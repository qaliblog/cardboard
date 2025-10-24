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

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.MenuItem;
import android.widget.SeekBar;
import android.widget.Switch;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import com.google.android.material.card.MaterialCardView;
import com.google.android.material.slider.Slider;

/**
 * Settings activity for configuring visual effects for each screen.
 */
public class SettingsActivity extends AppCompatActivity {
    
    private static final String PREFS_NAME = "VideoPlayerSettings";
    
    // Left eye settings
    private Switch leftEyeEnabledSwitch;
    private Slider leftEyeContrastSlider;
    private Slider leftEyeRedTintSlider;
    private Slider leftEyeGreenTintSlider;
    private Slider leftEyeFogIntensitySlider;
    private Slider leftEyeDirectionalSlider;
    private TextView leftEyeContrastValue;
    private TextView leftEyeRedTintValue;
    private TextView leftEyeGreenTintValue;
    private TextView leftEyeFogValue;
    private TextView leftEyeDirectionalValue;
    
    // Right eye settings
    private Switch rightEyeEnabledSwitch;
    private Slider rightEyeContrastSlider;
    private Slider rightEyeRedTintSlider;
    private Slider rightEyeGreenTintSlider;
    private Slider rightEyeFogIntensitySlider;
    private Slider rightEyeDirectionalSlider;
    private TextView rightEyeContrastValue;
    private TextView rightEyeRedTintValue;
    private TextView rightEyeGreenTintValue;
    private TextView rightEyeFogValue;
    private TextView rightEyeDirectionalValue;
    
    private SharedPreferences preferences;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);
        
        preferences = getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        
        setupToolbar();
        initializeViews();
        loadSettings();
        setupListeners();
    }
    
    private void setupToolbar() {
        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
            getSupportActionBar().setTitle("Effect Settings");
        }
    }
    
    private void initializeViews() {
        // Left eye controls
        leftEyeEnabledSwitch = findViewById(R.id.left_eye_enabled_switch);
        leftEyeContrastSlider = findViewById(R.id.left_eye_contrast_slider);
        leftEyeRedTintSlider = findViewById(R.id.left_eye_red_tint_slider);
        leftEyeGreenTintSlider = findViewById(R.id.left_eye_green_tint_slider);
        leftEyeFogIntensitySlider = findViewById(R.id.left_eye_fog_intensity_slider);
        leftEyeDirectionalSlider = findViewById(R.id.left_eye_directional_slider);
        
        leftEyeContrastValue = findViewById(R.id.left_eye_contrast_value);
        leftEyeRedTintValue = findViewById(R.id.left_eye_red_tint_value);
        leftEyeGreenTintValue = findViewById(R.id.left_eye_green_tint_value);
        leftEyeFogValue = findViewById(R.id.left_eye_fog_value);
        leftEyeDirectionalValue = findViewById(R.id.left_eye_directional_value);
        
        // Right eye controls
        rightEyeEnabledSwitch = findViewById(R.id.right_eye_enabled_switch);
        rightEyeContrastSlider = findViewById(R.id.right_eye_contrast_slider);
        rightEyeRedTintSlider = findViewById(R.id.right_eye_red_tint_slider);
        rightEyeGreenTintSlider = findViewById(R.id.right_eye_green_tint_slider);
        rightEyeFogIntensitySlider = findViewById(R.id.right_eye_fog_intensity_slider);
        rightEyeDirectionalSlider = findViewById(R.id.right_eye_directional_slider);
        
        rightEyeContrastValue = findViewById(R.id.right_eye_contrast_value);
        rightEyeRedTintValue = findViewById(R.id.right_eye_red_tint_value);
        rightEyeGreenTintValue = findViewById(R.id.right_eye_green_tint_value);
        rightEyeFogValue = findViewById(R.id.right_eye_fog_value);
        rightEyeDirectionalValue = findViewById(R.id.right_eye_directional_value);
    }
    
    private void loadSettings() {
        // Load left eye settings
        leftEyeEnabledSwitch.setChecked(preferences.getBoolean("left_eye_enabled", true));
        leftEyeContrastSlider.setValue(preferences.getFloat("left_eye_contrast", 1.0f));
        leftEyeRedTintSlider.setValue(preferences.getFloat("left_eye_red_tint", 0.0f));
        leftEyeGreenTintSlider.setValue(preferences.getFloat("left_eye_green_tint", 0.0f));
        leftEyeFogIntensitySlider.setValue(preferences.getFloat("left_eye_fog_intensity", 0.3f));
        leftEyeDirectionalSlider.setValue(preferences.getFloat("left_eye_directional", 0.0f));
        
        // Load right eye settings
        rightEyeEnabledSwitch.setChecked(preferences.getBoolean("right_eye_enabled", false));
        rightEyeContrastSlider.setValue(preferences.getFloat("right_eye_contrast", 1.0f));
        rightEyeRedTintSlider.setValue(preferences.getFloat("right_eye_red_tint", 0.0f));
        rightEyeGreenTintSlider.setValue(preferences.getFloat("right_eye_green_tint", 0.0f));
        rightEyeFogIntensitySlider.setValue(preferences.getFloat("right_eye_fog_intensity", 0.0f));
        rightEyeDirectionalSlider.setValue(preferences.getFloat("right_eye_directional", 0.0f));
        
        updateValueLabels();
    }
    
    private void setupListeners() {
        // Left eye listeners
        leftEyeEnabledSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            saveSetting("left_eye_enabled", isChecked);
            updateEffectAvailability();
        });
        
        setupSliderListener(leftEyeContrastSlider, "left_eye_contrast", leftEyeContrastValue, "%.2f");
        setupSliderListener(leftEyeRedTintSlider, "left_eye_red_tint", leftEyeRedTintValue, "%.2f");
        setupSliderListener(leftEyeGreenTintSlider, "left_eye_green_tint", leftEyeGreenTintValue, "%.2f");
        setupSliderListener(leftEyeFogIntensitySlider, "left_eye_fog_intensity", leftEyeFogValue, "%.2f");
        setupSliderListener(leftEyeDirectionalSlider, "left_eye_directional", leftEyeDirectionalValue, "%.2f");
        
        // Right eye listeners
        rightEyeEnabledSwitch.setOnCheckedChangeListener((buttonView, isChecked) -> {
            saveSetting("right_eye_enabled", isChecked);
            updateEffectAvailability();
        });
        
        setupSliderListener(rightEyeContrastSlider, "right_eye_contrast", rightEyeContrastValue, "%.2f");
        setupSliderListener(rightEyeRedTintSlider, "right_eye_red_tint", rightEyeRedTintValue, "%.2f");
        setupSliderListener(rightEyeGreenTintSlider, "right_eye_green_tint", rightEyeGreenTintValue, "%.2f");
        setupSliderListener(rightEyeFogIntensitySlider, "right_eye_fog_intensity", rightEyeFogValue, "%.2f");
        setupSliderListener(rightEyeDirectionalSlider, "right_eye_directional", rightEyeDirectionalValue, "%.2f");
        
        updateEffectAvailability();
    }
    
    private void setupSliderListener(Slider slider, String key, TextView valueLabel, String format) {
        slider.addOnChangeListener((slider1, value, fromUser) -> {
            if (fromUser) {
                saveSetting(key, value);
                valueLabel.setText(String.format(format, value));
            }
        });
    }
    
    private void updateValueLabels() {
        leftEyeContrastValue.setText(String.format("%.2f", leftEyeContrastSlider.getValue()));
        leftEyeRedTintValue.setText(String.format("%.2f", leftEyeRedTintSlider.getValue()));
        leftEyeGreenTintValue.setText(String.format("%.2f", leftEyeGreenTintSlider.getValue()));
        leftEyeFogValue.setText(String.format("%.2f", leftEyeFogIntensitySlider.getValue()));
        leftEyeDirectionalValue.setText(String.format("%.2f", leftEyeDirectionalSlider.getValue()));
        
        rightEyeContrastValue.setText(String.format("%.2f", rightEyeContrastSlider.getValue()));
        rightEyeRedTintValue.setText(String.format("%.2f", rightEyeRedTintSlider.getValue()));
        rightEyeGreenTintValue.setText(String.format("%.2f", rightEyeGreenTintSlider.getValue()));
        rightEyeFogValue.setText(String.format("%.2f", rightEyeFogIntensitySlider.getValue()));
        rightEyeDirectionalValue.setText(String.format("%.2f", rightEyeDirectionalSlider.getValue()));
    }
    
    private void updateEffectAvailability() {
        boolean leftEnabled = leftEyeEnabledSwitch.isChecked();
        boolean rightEnabled = rightEyeEnabledSwitch.isChecked();
        
        // Enable/disable left eye controls
        leftEyeContrastSlider.setEnabled(leftEnabled);
        leftEyeRedTintSlider.setEnabled(leftEnabled);
        leftEyeGreenTintSlider.setEnabled(leftEnabled);
        leftEyeFogIntensitySlider.setEnabled(leftEnabled);
        leftEyeDirectionalSlider.setEnabled(leftEnabled);
        
        // Enable/disable right eye controls
        rightEyeContrastSlider.setEnabled(rightEnabled);
        rightEyeRedTintSlider.setEnabled(rightEnabled);
        rightEyeGreenTintSlider.setEnabled(rightEnabled);
        rightEyeFogIntensitySlider.setEnabled(rightEnabled);
        rightEyeDirectionalSlider.setEnabled(rightEnabled);
    }
    
    private void saveSetting(String key, Object value) {
        SharedPreferences.Editor editor = preferences.edit();
        if (value instanceof Boolean) {
            editor.putBoolean(key, (Boolean) value);
        } else if (value instanceof Float) {
            editor.putFloat(key, (Float) value);
        } else if (value instanceof Integer) {
            editor.putInt(key, (Integer) value);
        } else if (value instanceof String) {
            editor.putString(key, (String) value);
        }
        editor.apply();
    }
    
    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
    
    @Override
    public void onBackPressed() {
        // Save settings and return to video player
        Intent resultIntent = new Intent();
        setResult(RESULT_OK, resultIntent);
        super.onBackPressed();
    }
    
    public static EffectSettings getEffectSettings(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE);
        EffectSettings settings = new EffectSettings();
        
        // Left eye settings
        settings.leftEyeEnabled = prefs.getBoolean("left_eye_enabled", true);
        settings.leftEyeContrast = prefs.getFloat("left_eye_contrast", 1.0f);
        settings.leftEyeRedTint = prefs.getFloat("left_eye_red_tint", 0.0f);
        settings.leftEyeGreenTint = prefs.getFloat("left_eye_green_tint", 0.0f);
        settings.leftEyeFogIntensity = prefs.getFloat("left_eye_fog_intensity", 0.3f);
        settings.leftEyeDirectional = prefs.getFloat("left_eye_directional", 0.0f);
        
        // Right eye settings
        settings.rightEyeEnabled = prefs.getBoolean("right_eye_enabled", false);
        settings.rightEyeContrast = prefs.getFloat("right_eye_contrast", 1.0f);
        settings.rightEyeRedTint = prefs.getFloat("right_eye_red_tint", 0.0f);
        settings.rightEyeGreenTint = prefs.getFloat("right_eye_green_tint", 0.0f);
        settings.rightEyeFogIntensity = prefs.getFloat("right_eye_fog_intensity", 0.0f);
        settings.rightEyeDirectional = prefs.getFloat("right_eye_directional", 0.0f);
        
        return settings;
    }
    
    public static class EffectSettings {
        public boolean leftEyeEnabled;
        public float leftEyeContrast;
        public float leftEyeRedTint;
        public float leftEyeGreenTint;
        public float leftEyeFogIntensity;
        public float leftEyeDirectional;
        
        public boolean rightEyeEnabled;
        public float rightEyeContrast;
        public float rightEyeRedTint;
        public float rightEyeGreenTint;
        public float rightEyeFogIntensity;
        public float rightEyeDirectional;
    }
}