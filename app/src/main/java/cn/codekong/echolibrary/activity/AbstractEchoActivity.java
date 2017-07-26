package cn.codekong.echolibrary.activity;

import android.os.Bundle;
import android.os.Handler;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.text.TextUtils;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.TextView;

import cn.codekong.echolibrary.R;

/**
 * Created by 尚振鸿 on 17-7-26. 14:24
 * mail:szh@codekong.cn
 */

public abstract class AbstractEchoActivity extends AppCompatActivity implements View.OnClickListener {
    //端口号
    protected EditText mPortEditText;
    //开启服务按钮
    protected Button mStartButton;
    //日志滚动布局
    protected ScrollView mLogScrollView;
    //日志显示视图
    protected TextView mLogTextView;
    //布局id
    private final int mLayoutId;

    static {
        System.loadLibrary("echo-lib");
    }

    public AbstractEchoActivity(int layoutId){
        this.mLayoutId = layoutId;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(mLayoutId);
        //初始化布局
        initView();
    }

    /**
     * 初始化布局
     */
    private void initView() {
        mPortEditText = (EditText) findViewById(R.id.port_edit);
        mStartButton = (Button) findViewById(R.id.start_button);
        mLogScrollView = (ScrollView) findViewById(R.id.log_scroll);
        mLogTextView = (TextView) findViewById(R.id.log_view);


        mStartButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View view) {
        switch (view.getId()){
            case R.id.start_button:
                onStartButtonClicked();
                break;
            default:
                break;
        }
    }

    //点击开启服务按钮触发
    protected abstract void onStartButtonClicked();


    /**
     * 获得端口号
     * @return
     */
    protected Integer getPort(){
        Integer port;
        if (!TextUtils.isEmpty(mPortEditText.getText())){
            port = Integer.valueOf(mPortEditText.getText().toString());
        }else{
            port = null;
        }

        return port;
    }

    /**
     * 记录给定的日志消息
     * @param message
     */
    protected void logMessage(final String message){
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                logMessageDirect(message);
            }
        });
    }

    /**
     * 直接记录给定的日志消息(显示在滚动布局中)
     * @param message
     */
    private void logMessageDirect(final String message) {
        mLogTextView.append(message);
        mLogTextView.append("\n");
        mLogScrollView.fullScroll(View.FOCUS_DOWN);
    }

    /**
     * 抽象异步Echo任务
     */
    protected abstract class AbstractEchoTask extends Thread{
        //Handler对象
        private final Handler handler;

        public AbstractEchoTask(){
            handler = new Handler();
        }

        /**
         * 在调用线程中先执行回调
         */
        protected void onPreExecute(){
            mStartButton.setEnabled(false);
            mLogTextView.setText("");
        }

        public synchronized void start(){
            onPreExecute();
            super.start();
        }

        @Override
        public void run() {
            onBackground();
            handler.post(new Runnable() {
                @Override
                public void run() {
                    onPostExecute();
                }
            });
        }

        //新线程中的背景回调
        protected abstract void onBackground();

        //在调用线程后执行回调
        protected void onPostExecute(){
            mStartButton.setEnabled(true);
        }
    }



}
