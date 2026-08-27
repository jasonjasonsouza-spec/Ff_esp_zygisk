package com.example.application;
import android.app.Activity;
import android.os.Bundle;
public class MainActivity extends Activity {
    static { System.loadLibrary("zygisk_esp"); }
    @Override protected void onCreate(Bundle b) {
        super.onCreate(b);
        finish();
    }
}
