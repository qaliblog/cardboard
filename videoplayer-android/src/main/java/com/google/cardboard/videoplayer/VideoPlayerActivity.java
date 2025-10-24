/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.cardboard.videoplayer;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import com.github.dhaval2404.imagepicker.ImagePicker;
import com.karumi.dexter.Dexter;
import com.karumi.dexter.MultiplePermissionsReport;
import com.karumi.dexter.PermissionToken;
import com.karumi.dexter.listener.PermissionRequest;
import com.karumi.dexter.listener.multi.MultiplePermissionsListener;

import java.util.List;

/**
 * Main activity for video file selection and VR video player launch.
 */
public class VideoPlayerActivity extends AppCompatActivity implements MultiplePermissionsListener {

    private static final int PERMISSIONS_REQUEST_CODE = 100;
    private static final int VIDEO_PICKER_REQUEST_CODE = 200;
    
    private Button selectVideoButton;
    private Uri selectedVideoUri;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_video_player);
        
        selectVideoButton = findViewById(R.id.select_video_button);
        selectVideoButton.setOnClickListener(v -> requestPermissionsAndSelectVideo());
    }

    private void requestPermissionsAndSelectVideo() {
        Dexter.withContext(this)
                .withPermissions(
                        Manifest.permission.READ_EXTERNAL_STORAGE,
                        Manifest.permission.READ_MEDIA_VIDEO
                )
                .withListener(this)
                .check();
    }

    @Override
    public void onPermissionsChecked(MultiplePermissionsReport report) {
        if (report.areAllPermissionsGranted()) {
            selectVideo();
        } else {
            Toast.makeText(this, "Permissions are required to select videos", Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onPermissionRationaleShouldBeShown(List<PermissionRequest> permissions, PermissionToken token) {
        token.continuePermissionRequest();
    }

    private void selectVideo() {
        ImagePicker.with(this)
                .galleryOnly()
                .galleryMimeTypes(new String[]{"video/*"})
                .start(VIDEO_PICKER_REQUEST_CODE);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == VIDEO_PICKER_REQUEST_CODE && resultCode == RESULT_OK && data != null) {
            selectedVideoUri = data.getData();
            if (selectedVideoUri != null) {
                launchVrVideoPlayer();
            }
        }
    }

    private void launchVrVideoPlayer() {
        Intent intent = new Intent(this, VrVideoActivity.class);
        intent.setData(selectedVideoUri);
        startActivity(intent);
    }
}