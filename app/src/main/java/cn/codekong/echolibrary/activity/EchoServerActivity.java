package cn.codekong.echolibrary.activity;


import cn.codekong.echolibrary.R;

public class EchoServerActivity extends AbstractEchoActivity {

    public EchoServerActivity() {
        super(R.layout.activity_echo_server);
    }

    @Override
    protected void onStartButtonClicked() {
        Integer port = getPort();
        if (port!=null){
            ServerTask serverTask = new ServerTask(port);
            serverTask.start();
        }
    }

    private native void nativeStartTcpServer(int port) throws Exception;

    private class ServerTask extends AbstractEchoTask{

        //端口号
        private final int port;

        public ServerTask(int port) {
            this.port = port;
        }

        @Override
        protected void onBackground() {
            logMessage("Starting server.");
            try{
                nativeStartTcpServer(port);
            }catch (Exception e){
                logMessage(e.getMessage());
            }
            logMessage("Server terminated.");
        }
    }
}
