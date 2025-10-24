/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CARDBOARD_VIDEO_PLAYER_APP_H_
#define CARDBOARD_VIDEO_PLAYER_APP_H_

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <android/asset_manager.h>
#include <jni.h>
#include <memory>
#ifdef OPENCV_AVAILABLE
#include <opencv2/opencv.hpp>
#endif

#include "cardboard.h"

namespace cardboard {

class VideoPlayerApp {
 public:
  VideoPlayerApp(AAssetManager* asset_manager);
  ~VideoPlayerApp();

  void OnSurfaceCreated();
  void OnDrawFrame();
  void OnTriggerEvent();
  void OnPause();
  void OnResume();
  void SetScreenParams(int width, int height);
  void SetVideoUri(const std::string& video_uri);
  void SetEffectSettings(const EffectSettings& settings);

 private:
  void InitializeGl();
  void InitializeCardboard();
  void RenderVideoFrame();
#ifdef OPENCV_AVAILABLE
  void ProcessVideoFrame(const cv::Mat& input, cv::Mat& output);
  void ApplyEffects(const cv::Mat& input, cv::Mat& output, bool is_left_eye);
#else
  void ProcessVideoFrame(const uint8_t* input_data, int width, int height, uint8_t* output_data);
#endif
  
  // OpenGL resources
  GLuint program_;
  GLuint vertex_buffer_;
  GLuint texture_id_;
  GLint position_attrib_;
  GLint tex_coord_attrib_;
  GLint mvp_matrix_uniform_;
  GLint texture_uniform_;
  
  // Cardboard resources
  CardboardLensDistortion* lens_distortion_;
  CardboardDistortionRenderer* distortion_renderer_;
  CardboardHeadTracker* head_tracker_;
  
  // Video processing
#ifdef OPENCV_AVAILABLE
  cv::Mat current_frame_;
  cv::Mat processed_frame_;
#else
  uint8_t* current_frame_data_;
  uint8_t* processed_frame_data_;
  int frame_width_;
  int frame_height_;
#endif
  bool frame_updated_;
  
  // Rendering parameters
  int screen_width_;
  int screen_height_;
  float fov_degrees_;
  
  // Asset manager
  AAssetManager* asset_manager_;
  
  // Video URI (passed from Java)
  std::string video_uri_;
  
  // Effect settings
  struct EffectSettings {
    bool left_eye_enabled = true;
    float left_eye_contrast = 1.0f;
    float left_eye_red_tint = 0.0f;
    float left_eye_green_tint = 0.0f;
    float left_eye_fog_intensity = 0.3f;
    float left_eye_directional = 0.0f;
    
    bool right_eye_enabled = false;
    float right_eye_contrast = 1.0f;
    float right_eye_red_tint = 0.0f;
    float right_eye_green_tint = 0.0f;
    float right_eye_fog_intensity = 0.0f;
    float right_eye_directional = 0.0f;
  } effect_settings_;
};

}  // namespace cardboard

#endif  // CARDBOARD_VIDEO_PLAYER_APP_H_