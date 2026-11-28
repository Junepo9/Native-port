package com.junepo9.nativeport;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    private static final int PICK_GAME = 1001;
    private static final int HEADER_BYTES = 512;

    private Button selectButton;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File header = new File(getFilesDir(), "header.bin");
        if (!header.exists()) {
            addSelectButton();
        }
    }

    private void addSelectButton() {
        selectButton = new Button(this);
        selectButton.setText("Select game file");
        selectButton.setAllCaps(false);
        selectButton.setOnClickListener(v -> openPicker());

        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
        lp.bottomMargin = 64;
        addContentView(selectButton, lp);
    }

    private void openPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, PICK_GAME);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != PICK_GAME || resultCode != RESULT_OK || data == null) {
            return;
        }
        final Uri uri = data.getData();
        if (uri == null) {
            return;
        }
        new Thread(() -> copyHeader(uri)).start();
    }

    private void copyHeader(Uri uri) {
        try (InputStream in = getContentResolver().openInputStream(uri)) {
            if (in == null) {
                return;
            }
            byte[] buf = new byte[HEADER_BYTES];
            int total = 0;
            while (total < HEADER_BYTES) {
                int n = in.read(buf, total, HEADER_BYTES - total);
                if (n < 0) {
                    break;
                }
                total += n;
            }
            File tmp = new File(getFilesDir(), "header.tmp");
            try (FileOutputStream out = new FileOutputStream(tmp)) {
                out.write(buf, 0, total);
            }
            if (tmp.renameTo(new File(getFilesDir(), "header.bin"))) {
                runOnUiThread(() -> {
                    if (selectButton != null) {
                        selectButton.setVisibility(View.GONE);
                    }
                });
            }
        } catch (Exception e) {
            Log.e("NativePort", "copyHeader failed", e);
        }
    }
}
