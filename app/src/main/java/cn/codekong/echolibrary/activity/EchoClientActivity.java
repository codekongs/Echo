package cn.codekong.echolibrary.activity;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.widget.EditText;

import cn.codekong.echolibrary.R;

public class EchoClientActivity extends AbstractEchoActivity {

    private EditText ipEdit;
    private EditText messageEdit;

    public EchoClientActivity() {
        super(R.layout.activity_echo_client);
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ipEdit = (EditText) findViewById(R.id.ip_edit);
        messageEdit = (EditText) findViewById(R.id.message_edit);
    }

    @Override
    protected void onStartButtonClicked() {
        String ip = ipEdit.getText().toString();
        String message = messageEdit.getText().toString();
        Integer port = getPort();
        if ((0 != ip.length()) && (port != null) && (0 != message.length())) {
            ClientTask clientTask = new ClientTask(ip, port, message);
            clientTask.start();
        }
    }

    private native void nativeStartTcpClient(String ip, int port, String message);

    private class ClientTask extends AbstractEchoTask {

        private final String ip;
        private final int port;
        private final String message;

        public ClientTask(String ip, int port, String message) {
            this.ip = ip;
            this.port = port;
            this.message = message;
        }

        @Override
        protected void onBackground() {
            logMessage("Starting client.");
            try {
                nativeStartTcpClient(ip, port, message);
            } catch (Throwable e) {
                logMessage(e.getMessage());
            }
            logMessage("Client terminated.");
        }
    }
}
