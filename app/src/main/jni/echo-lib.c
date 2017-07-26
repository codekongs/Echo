//
// Created by szh on 17-7-26.
//
#include <jni.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/un.h>
#include <stddef.h>

//最大日志消息长度
#define MAX_LOG_MESSAGE_LENGTH 256
//最大数据缓冲区大小
#define MAX_BUFFER_SIZE 80

/**
 * 将给定的消息记录到应用程序
 * @param env
 * @param obj
 * @param format
 */
static void LogMessage(JNIEnv *env, jobject obj, const char *format, ...) {
    //缓存日志方法Id
    static jmethodID methodId = NULL;
    if (NULL == methodId) {
        jclass class = (*env)->GetObjectClass(env, obj);
        methodId = (*env)->GetMethodID(env, class, "logMessage", "(Ljava/lang/String;)V");
        (*env)->DeleteLocalRef(env, class);
    }
    if (NULL != methodId) {
        char buffer[MAX_LOG_MESSAGE_LENGTH];

        //实例化可变长度参数列表
        va_list ap;
        //初始化可变长度参数列表，设置format为可变长度列表的起始点
        va_start(ap, format);
        vsnprintf(buffer, MAX_LOG_MESSAGE_LENGTH, format, ap);
        //关闭ap指针
        va_end(ap);

        //将缓冲区转换为Java字符串
        jstring message = (*env)->NewStringUTF(env, buffer);

        if (NULL != message) {
            //记录消息
            (*env)->CallVoidMethod(env, obj, methodId, message);
            //释放消息引用
            (*env)->DeleteLocalRef(env, message);
        }
    }
}

/**
 * 用给定的异常类和异常消息抛出新的异常
 * @param env
 * @param className
 * @param message
 */
static void ThrowException(JNIEnv *env, const char *className, const char *message) {
    jclass class = (*env)->FindClass(env, className);
    if (NULL != class) {
        //抛出异常
        (*env)->ThrowNew(env, class, message);
        (*env)->DeleteLocalRef(env, class);
    }
}

/**
 * 给定的异常类和基于错误消息抛出新异常
 * @param env
 * @param className
 * @param errnum
 */
static void ThrowErrorException(JNIEnv *env, const char *className, int errnum) {
    char buffer[MAX_LOG_MESSAGE_LENGTH];
    if (-1 == strerror_r(errnum, buffer, MAX_LOG_MESSAGE_LENGTH)) {
        strerror_r(errno, buffer, MAX_LOG_MESSAGE_LENGTH);
    }
    ThrowException(env, className, buffer);
}

/**
 * 创建新的TCP socket
 * @param env
 * @param obj
 * @return
 */
static int NewTcpSocket(JNIEnv *env, jobject obj) {
    LogMessage(env, obj, "Constructing a new TCP socket...");
    //构建socket
    int tcpSocket = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == tcpSocket) {
        ThrowErrorException(env, "java/io/IOException", errno);
    }
    return tcpSocket;
}

/**
 * 将socket绑定到某一端口号
 * @param env
 * @param obj
 * @param sd
 * @param port
 */
static void BindSocketToPort(JNIEnv *env, jobject obj, int sd, unsigned short port) {
    struct sockaddr_in address;
    //绑定socket的地址
    memset(&address, 0, sizeof(address));
    address.sin_family = PF_INET;
    //绑定到所有地址
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    //将端口转换为网络字节顺序
    address.sin_port = htons(port);

    LogMessage(env, obj, "Binding to port %hu.", port);
    //绑定socket
    if (-1 == bind(sd, (struct sockaddr *) &address, sizeof(address))) {
        ThrowErrorException(env, "java/io/IOException", errno);
    }
}

static unsigned short GetSocketPort(JNIEnv *env, jobject obj, int sd) {
    unsigned short port = 0;
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);
    //获取socket地址
    if (-1 == getsockname(sd, (struct sockaddr *) &address, &addressLength)) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        //将端口转换为主机字节顺序
        port = ntohs(address.sin_port);
        LogMessage(env, obj, "Binded to random port %hu.", port);
    }
    return port;
}

/**
 * 监听指定的待处理连接的backlog的socket，当backlog已满时拒绝新的连接
 * @param env
 * @param obj
 * @param sd
 * @param backlog
 */
static void ListenOnSocket(JNIEnv *env, jobject obj, int sd, int backlog) {
    LogMessage(env, obj, "Listening on socket with a backlog of %d pending connections.", backlog);
    if (-1 == listen(sd, backlog)) {
        ThrowErrorException(env, "java/io/IOException", errno);
    }
}

static void
LogAddress(JNIEnv *env, jobject obj, const char *message, const struct sockaddr_in *address) {
    char ip[INET_ADDRSTRLEN];
    if (NULL == inet_ntop(PF_INET, &(address->sin_addr), ip, INET_ADDRSTRLEN)) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        unsigned short port = ntohs(address->sin_port);
        LogMessage(env, obj, "%s %s:%hu.", message, ip, port);
    }
}

/**
 * 在给定的socket上阻塞和等待进来的客户连接
 * @param env
 * @param obj
 * @param sd
 * @return
 */
static int AcceptOnSocket(JNIEnv *env, jobject obj, int sd) {
    struct sockaddr_in address;
    socklen_t addressLength = sizeof(address);
    LogMessage(env, obj, "Waiting for a client connection...");
    int clientSocket = accept(sd, (struct sockaddr *) &address, &addressLength);
    if (-1 == clientSocket) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        LogAddress(env, obj, "Client connection from", &address);
    }
    return clientSocket;
}

/**
 * 阻塞并接收来自socket的数据放在缓冲区
 * @param env
 * @param obj
 * @param sd
 * @param buffer
 * @param bufferSize
 * @return
 */
static size_t ReceiveFromSocket(JNIEnv *env, jobject obj, int sd, char *buffer, size_t bufferSize) {
    LogMessage(env, obj, "Receiving from the socket...");
    size_t recvSize = recv(sd, buffer, bufferSize - 1, 0);
    if (-1 == recvSize) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        buffer[recvSize] = NULL;
        if (recvSize > 0) {
            LogMessage(env, obj, "Received %d bytes: %s", recvSize, buffer);
        } else {
            LogMessage(env, obj, "Client disconnected.");
        }
    }
    return recvSize;
}

static size_t
SendToSocket(JNIEnv *env, jobject obj, int sd, const char *buffer, size_t bufferSize) {
    LogMessage(env, obj, "Sending to the socket...");
    size_t sendSize = send(sd, buffer, bufferSize, 0);
    if (-1 == sendSize) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        if (sendSize > 0) {
            LogMessage(env, obj, "Send %d bytes: %s", sendSize, buffer);
        } else {
            LogMessage(env, obj, "Client disconnected.");
        }
    }
    return sendSize;
}

/**
 * 连接到给定的IP地址和给定的端口号
 * @param env
 * @param obj
 * @param sd
 * @param ip
 * @param port
 */
static void
ConnectToAddress(JNIEnv *env, jobject obj, int sd, const char *ip, unsigned short port) {
    LogMessage(env, obj, "Connecting to %s:%hu...", ip, port);
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = PF_INET;
    //将IP地址字符串转换为网络地址
    if (0 == inet_aton(ip, &(address.sin_addr))) {
        ThrowErrorException(env, "java/io/IOException", errno);
    } else {
        address.sin_port = htons(port);
        if (-1 == connect(sd, (const struct sockaddr *) &address, sizeof(address))) {
            ThrowErrorException(env, "java/io/IOException", errno);
        } else {
            LogMessage(env, obj, "Connected.");
        }
    }
}

JNIEXPORT void JNICALL
Java_cn_codekong_echolibrary_activity_EchoClientActivity_nativeStartTcpClient(JNIEnv *env,
                                                                              jobject obj,
                                                                              jstring ip_,
                                                                              jint port,
                                                                              jstring message_) {
    const char *ip = (*env)->GetStringUTFChars(env, ip_, 0);
    const char *message = (*env)->GetStringUTFChars(env, message_, 0);

    int clientSocket = NewTcpSocket(env, obj);
    if (NULL == (*env)->ExceptionOccurred(env)) {
        if (NULL == ip)
            goto exit;
        //连接到IP地址和端口
        ConnectToAddress(env, obj, clientSocket, ip, (unsigned short) port);
        if (NULL != (*env)->ExceptionOccurred(env))
            goto exit;
        if (NULL == message)
            goto exit;
        jsize messageSize = (*env)->GetStringUTFLength(env, message_);
        //发送消息给socket
        SendToSocket(env, obj, clientSocket, message, messageSize);
        if (NULL != (*env)->ExceptionOccurred(env))
            goto exit;
        char buffer[MAX_BUFFER_SIZE];
        //从socket接收
        ReceiveFromSocket(env, obj, clientSocket, buffer, MAX_BUFFER_SIZE);
    }

    (*env)->ReleaseStringUTFChars(env, ip_, ip);
    (*env)->ReleaseStringUTFChars(env, message_, message);

    exit:
    if (clientSocket > -1) {
        close(clientSocket);
    }

}

JNIEXPORT void JNICALL
Java_cn_codekong_echolibrary_activity_EchoServerActivity_nativeStartTcpServer(JNIEnv *env,
                                                                              jobject obj,
                                                                              jint port) {

    //创建新的TCP socket
    int serverSocket = NewTcpSocket(env, obj);
    if (NULL == (*env)->ExceptionOccurred(env)) {
        //将socket绑定到某端口号
        BindSocketToPort(env, obj, serverSocket, (unsigned short) port);
        if (NULL != (*env)->ExceptionOccurred(env))
            goto exit;
        if (0 == port) {
            //获取当前绑定的端口号
            GetSocketPort(env, obj, serverSocket);
            if (NULL != (*env)->ExceptionOccurred(env))
                goto exit;
        }
        //监听4个等待连接的backlog的socket
        ListenOnSocket(env, obj, serverSocket, 4);
        if (NULL != (*env)->ExceptionOccurred(env))
            goto exit;
        //接受socket的一个客户连接
        int clientSocket = AcceptOnSocket(env, obj, serverSocket);
        if (NULL != (*env)->ExceptionOccurred(env))
            goto exit;
        char buffer[MAX_BUFFER_SIZE];
        size_t recvSize;
        size_t sendSize;
        //接收并发送回数据
        while (1) {
            //从socket中接收
            recvSize = ReceiveFromSocket(env, obj, clientSocket, buffer, MAX_BUFFER_SIZE);
            if ((0 == recvSize) || (NULL != (*env)->ExceptionOccurred(env)))
                break;
            //发送给socket
            sendSize = SendToSocket(env, obj, clientSocket, buffer, (size_t) recvSize);
            if ((0 == sendSize) || (NULL != (*env)->ExceptionOccurred(env)))
                break;
        }
        //关闭客户端socket
        close(clientSocket);
    }
    exit:
    if (serverSocket > 0) {
        close(serverSocket);
    }
}