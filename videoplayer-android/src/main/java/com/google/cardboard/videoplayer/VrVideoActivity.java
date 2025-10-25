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
import android.annotation.SuppressLint;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.Toast;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.media3.common.MediaItem;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.ui.PlayerView;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.graphics.SurfaceTexture;
import android.view.Surface;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 * VR Video Activity that plays videos in split-screen mode with OpenCV effects.
 */
public class VrVideoActivity extends AppCompatActivity {
    
    private static final String TAG = VrVideoActivity.class.getSimpleName();
    private static final int PERMISSIONS_REQUEST_CODE = 2;
    
    // Native app pointer
    private long nativeApp;
    
    // UI components
    private GLSurfaceView glView;
    private PlayerView playerView;
    private ExoPlayer exoPlayer;
    private ImageButton settingsButton;
    
    // Video URI
    private Uri videoUri;
    
    // Gyroscope and sensors
    private SensorManager sensorManager;
    private Sensor gyroscopeSensor;
    private SensorEventListener gyroscopeListener;
    private boolean isHeadTrackingEnabled = false;
    
    // Touch handling
    private boolean isLongPress = false;
    private Handler longPressHandler = new Handler(Looper.getMainLooper());
    private Runnable longPressRunnable;
    private float initialTouchX = 0;
    private float initialTouchY = 0;
    
    // Video control
    private boolean isPlaying = true;
    private long lastSeekTime = 0;
    private static final long SEEK_THRESHOLD = 500; // ms
    
    // Video frame extraction
    private SurfaceTexture videoSurfaceTexture;
    private Surface videoSurface;

    static {
        System.loadLibrary("videoplayer_jni");
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Get video URI from intent
        videoUri = getIntent().getData();
        if (videoUri == null) {
            Toast.makeText(this, "No video selected", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        
        // Initialize native app
        nativeApp = nativeOnCreate(getAssets());
        
        // Set video URI
        if (videoUri != null) {
            nativeSetVideoUri(nativeApp, videoUri.toString());
        }
        
        // Load and apply effect settings
        loadAndApplyEffectSettings();
        
        setContentView(R.layout.activity_vr_video);
        
        // Setup OpenGL view
        glView = findViewById(R.id.gl_surface_view);
        glView.setEGLContextClientVersion(2);
        Renderer renderer = new Renderer();
        glView.setRenderer(renderer);
        glView.setRenderMode(GLSurfaceView.RENDERMODE_CONTINUOUSLY);
        
        // Setup video player
        setupVideoPlayer();
        
        // Setup settings button
        setupSettingsButton();
        
        // Setup touch handling
        setupTouchHandling();
        
        // Setup head tracking
        setupHeadTracking();
        
        // Setup immersive mode
        setupImmersiveMode();
        
        // Keep screen on
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    }
    
    private void setupVideoPlayer() {
        playerView = findViewById(R.id.player_view);
        exoPlayer = new ExoPlayer.Builder(this).build();
        playerView.setPlayer(exoPlayer);
        
        // Set up video frame extraction
        setupVideoFrameExtraction();
        
        MediaItem mediaItem = MediaItem.fromUri(videoUri);
        exoPlayer.setMediaItem(mediaItem);
        exoPlayer.prepare();
        exoPlayer.play();
    }
    
    private void setupVideoFrameExtraction() {
        // Create SurfaceTexture for video frame extraction
        videoSurfaceTexture = new SurfaceTexture(0);
        videoSurfaceTexture.setOnFrameAvailableListener(new SurfaceTexture.OnFrameAvailableListener() {
            @Override
            public void onFrameAvailable(SurfaceTexture surfaceTexture) {
                // Update the native texture
                nativeUpdateVideoTexture(nativeApp, videoSurfaceTexture);
            }
        });
        
        videoSurface = new Surface(videoSurfaceTexture);
        
        // Set the video surface for ExoPlayer
        exoPlayer.setVideoSurface(videoSurface);
    }
    
    private void setupSettingsButton() {
        settingsButton = findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(v -> {
            Intent settingsIntent = new Intent(this, SettingsActivity.class);
            startActivityForResult(settingsIntent, 100);
        });
    }
    
    private void loadAndApplyEffectSettings() {
        SettingsActivity.EffectSettings settings = SettingsActivity.getEffectSettings(this);
        nativeSetEffectSettings(nativeApp,
            settings.leftEyeEnabled, settings.leftEyeContrast, settings.leftEyeRedTint, 
            settings.leftEyeGreenTint, settings.leftEyeFogIntensity, settings.leftEyeDirectional,
            settings.rightEyeEnabled, settings.rightEyeContrast, settings.rightEyeRedTint, 
            settings.rightEyeGreenTint, settings.rightEyeFogIntensity, settings.rightEyeDirectional);
    }
    
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode == 100 && resultCode == RESULT_OK) {
            // Settings were updated, reload effect settings
            loadAndApplyEffectSettings();
        }
    }
    
    private void setupTouchHandling() {
        glView.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    initialTouchX = event.getX();
                    initialTouchY = event.getY();
                    isLongPress = false;
                    
                    // Start long press detection
                    longPressRunnable = () -> {
                        isLongPress = true;
                        Log.d(TAG, "Long press detected");
                    };
                    longPressHandler.postDelayed(longPressRunnable, 500); // 500ms for long press
                    return true;
                    
                case MotionEvent.ACTION_UP:
                    longPressHandler.removeCallbacks(longPressRunnable);
                    
                    if (!isLongPress) {
                        // Short press - toggle play/pause
                        togglePlayPause();
                    }
                    return true;
                    
                case MotionEvent.ACTION_MOVE:
                    if (isLongPress) {
                        handleHeadTiltSeek(event);
                    }
                    return true;
            }
            return false;
        });
    }
    
    private void setupHeadTracking() {
        sensorManager = (SensorManager) getSystemService(SENSOR_SERVICE);
        if (sensorManager != null) {
            gyroscopeSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
            if (gyroscopeSensor != null) {
                gyroscopeListener = new SensorEventListener() {
                    @Override
                    public void onSensorChanged(SensorEvent event) {
                        // Handle gyroscope data for head tracking
                        // This will be used for the tilt-based seeking
                    }
                    
                    @Override
                    public void onAccuracyChanged(Sensor sensor, int accuracy) {
                        // Handle accuracy changes
                    }
                };
                sensorManager.registerListener(gyroscopeListener, gyroscopeSensor, SensorManager.SENSOR_DELAY_GAME);
                isHeadTrackingEnabled = true;
            }
        }
    }
    
    private void setupImmersiveMode() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }
    
    private void togglePlayPause() {
        if (exoPlayer != null) {
            if (isPlaying) {
                exoPlayer.pause();
                isPlaying = false;
            } else {
                exoPlayer.play();
                isPlaying = true;
            }
        }
    }
    
    private void handleHeadTiltSeek(MotionEvent event) {
        if (exoPlayer == null) return;
        
        float deltaX = event.getX() - initialTouchX;
        float deltaY = event.getY() - initialTouchY;
        
        // Calculate tilt angle (simplified)
        float tiltAngle = (float) Math.toDegrees(Math.atan2(deltaY, deltaX));
        
        // Only process if significant movement
        if (Math.abs(tiltAngle) > 10) {
            long currentTime = System.currentTimeMillis();
            if (currentTime - lastSeekTime > SEEK_THRESHOLD) {
                lastSeekTime = currentTime;
                
                // Convert angle to seek time (40 degrees = 40 seconds)
                long seekTime = (long) (Math.abs(tiltAngle) * 1000); // Convert to milliseconds
                
                if (tiltAngle < 0) {
                    // Tilt left - seek backward
                    long currentPosition = exoPlayer.getCurrentPosition();
                    long newPosition = Math.max(0, currentPosition - seekTime);
                    exoPlayer.seekTo(newPosition);
                } else {
                    // Tilt right - seek forward
                    long currentPosition = exoPlayer.getCurrentPosition();
                    long duration = exoPlayer.getDuration();
                    long newPosition = Math.min(duration, currentPosition + seekTime);
                    exoPlayer.seekTo(newPosition);
                }
            }
        }
    }
    
    private void requestPermissions() {
        ActivityCompat.requestPermissions(this, 
                new String[]{Manifest.permission.CAMERA}, 
                PERMISSIONS_REQUEST_CODE);
    }
    
    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, 
                                         @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == PERMISSIONS_REQUEST_CODE) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                setupHeadTracking();
            } else {
                Toast.makeText(this, "Camera permission required for head tracking", Toast.LENGTH_LONG).show();
            }
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        if (exoPlayer != null) {
            exoPlayer.pause();
        }
        if (sensorManager != null && gyroscopeListener != null) {
            sensorManager.unregisterListener(gyroscopeListener);
        }
        nativeOnPause(nativeApp);
        glView.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (exoPlayer != null && isPlaying) {
            exoPlayer.play();
        }
        if (sensorManager != null && gyroscopeListener != null && isHeadTrackingEnabled) {
            sensorManager.registerListener(gyroscopeListener, gyroscopeSensor, SensorManager.SENSOR_DELAY_GAME);
        }
        nativeOnResume(nativeApp);
        glView.onResume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (exoPlayer != null) {
            exoPlayer.release();
        }
        if (sensorManager != null && gyroscopeListener != null) {
            sensorManager.unregisterListener(gyroscopeListener);
        }
        nativeOnDestroy(nativeApp);
        nativeApp = 0;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            setupImmersiveMode();
        }
    }

    private class Renderer implements GLSurfaceView.Renderer {
        @Override
        public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
            nativeOnSurfaceCreated(nativeApp);
        }

        @Override
        public void onSurfaceChanged(GL10 gl10, int width, int height) {
            nativeSetScreenParams(nativeApp, width, height);
        }

        @Override
        public void onDrawFrame(GL10 gl10) {
            nativeOnDrawFrame(nativeApp);
        }
    }

    // Native methods
    private native long nativeOnCreate(android.content.res.AssetManager assetManager);
    private native void nativeOnDestroy(long nativeApp);
    private native void nativeOnSurfaceCreated(long nativeApp);
    private native void nativeOnDrawFrame(long nativeApp);
    private native void nativeOnPause(long nativeApp);
    private native void nativeOnResume(long nativeApp);
    private native void nativeSetScreenParams(long nativeApp, int width, int height);
    private native void nativeSetVideoUri(long nativeApp, String videoUri);
    private native void nativeSetEffectSettings(long nativeApp, 
        boolean leftEnabled, float leftContrast, float leftRedTint, float leftGreenTint, 
        float leftFogIntensity, float leftDirectional,
        boolean rightEnabled, float rightContrast, float rightRedTint, float rightGreenTint, 
        float rightFogIntensity, float rightDirectional);
    private native void nativeUpdateVideoTexture(long nativeApp, SurfaceTexture surfaceTexture);
}