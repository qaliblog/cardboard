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

#include "video_player_app.h"

#include <android/log.h>
#include <array>
#include <cmath>
#include <memory>
#include <chrono>
#include <cstring>

#include "cardboard.h"

#define LOG_TAG "VideoPlayerApp"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace cardboard {

namespace {
// Vertex shader for rendering video frames
const char kVertexShader[] = R"(
attribute vec4 position;
attribute vec2 tex_coord;
varying vec2 v_tex_coord;
uniform mat4 mvp_matrix;

void main() {
  gl_Position = mvp_matrix * position;
  v_tex_coord = tex_coord;
}
)";

// Fragment shader for rendering video frames with 3D effects
const char kFragmentShader[] = R"(
precision mediump float;
varying vec2 v_tex_coord;
uniform sampler2D texture;
uniform float contrast;
uniform float red_tint;
uniform float green_tint;
uniform float fog_intensity;
uniform float directional_stretch;
uniform float time;
uniform bool is_left_eye;

void main() {
  vec2 tex_coord = v_tex_coord;
  
  // Apply directional stretch effect
  if (directional_stretch != 0.0) {
    float stretch_amount = directional_stretch * 0.1;
    if (is_left_eye) {
      tex_coord.x = tex_coord.x + stretch_amount * (tex_coord.x - 0.5);
    } else {
      tex_coord.x = tex_coord.x - stretch_amount * (tex_coord.x - 0.5);
    }
    tex_coord.x = clamp(tex_coord.x, 0.0, 1.0);
  }
  
  // Sample the texture
  vec4 color = texture2D(texture, tex_coord);
  
  // Apply contrast
  color.rgb = (color.rgb - 0.5) * contrast + 0.5;
  
  // Apply color tinting
  color.r += red_tint * 0.3;
  color.g += green_tint * 0.3;
  
  // Apply fog effect
  if (fog_intensity > 0.0) {
    float distance = length(tex_coord - 0.5);
    float fog_factor = 1.0 - smoothstep(0.0, 0.7, distance) * fog_intensity;
    color.rgb = mix(vec3(0.5, 0.5, 0.5), color.rgb, fog_factor);
  }
  
  // Add subtle 3D depth effect based on contrast
  float depth_factor = 1.0 + (contrast - 1.0) * 0.1;
  color.rgb *= depth_factor;
  
  gl_FragColor = color;
}
)";

// Quad vertices for rendering (two triangles forming a rectangle)
const float kQuadVertices[] = {
    // Left eye quad
    -1.0f, -1.0f, 0.0f,  // bottom left
     0.0f, -1.0f, 0.0f,  // bottom right
    -1.0f,  1.0f, 0.0f,  // top left
     0.0f,  1.0f, 0.0f,  // top right
    
    // Right eye quad
     0.0f, -1.0f, 0.0f,  // bottom left
     1.0f, -1.0f, 0.0f,  // bottom right
     0.0f,  1.0f, 0.0f,  // top left
     1.0f,  1.0f, 0.0f,  // top right
};

// Texture coordinates for the quad
const float kQuadTexCoords[] = {
    // Left eye texture coords
    0.0f, 1.0f,  // bottom left
    0.5f, 1.0f,  // bottom right
    0.0f, 0.0f,  // top left
    0.5f, 0.0f,  // top right
    
    // Right eye texture coords
    0.5f, 1.0f,  // bottom left
    1.0f, 1.0f,  // bottom right
    0.5f, 0.0f,  // top left
    1.0f, 0.0f,  // top right
};

// 180-degree FOV for static viewing

}  // namespace

VideoPlayerApp::VideoPlayerApp()
    : program_(0),
      vertex_buffer_(0),
      texture_id_(0),
      position_attrib_(0),
      tex_coord_attrib_(0),
      mvp_matrix_uniform_(0),
      texture_uniform_(0),
      contrast_uniform_(0),
      red_tint_uniform_(0),
      green_tint_uniform_(0),
      fog_intensity_uniform_(0),
      directional_stretch_uniform_(0),
      time_uniform_(0),
      is_left_eye_uniform_(0),
      lens_distortion_(nullptr),
      distortion_renderer_(nullptr),
      head_tracker_(nullptr),
      has_video_frame_(false),
      screen_width_(0),
      screen_height_(0),
      current_frame_index_(0),
      buffered_frames_count_(0),
      is_buffering_(false) {
  LOGD("VideoPlayerApp constructor");
  
  // Initialize video frame buffer
  video_frame_buffer_.resize(MAX_BUFFERED_FRAMES);
  frame_timestamps_.resize(MAX_BUFFERED_FRAMES);
  last_buffer_time_ = std::chrono::high_resolution_clock::now();
}

VideoPlayerApp::~VideoPlayerApp() {
  LOGD("VideoPlayerApp destructor");
  
  if (program_) {
    glDeleteProgram(program_);
  }
  if (vertex_buffer_) {
    glDeleteBuffers(1, &vertex_buffer_);
  }
  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
  }
  if (lens_distortion_) {
    CardboardLensDistortion_destroy(lens_distortion_);
  }
  if (distortion_renderer_) {
    CardboardDistortionRenderer_destroy(distortion_renderer_);
  }
  if (head_tracker_) {
    CardboardHeadTracker_destroy(head_tracker_);
  }
}

void VideoPlayerApp::OnSurfaceCreated() {
  LOGD("OnSurfaceCreated");
  InitializeGl();
  InitializeCardboard();
}

void VideoPlayerApp::OnDrawFrame() {
  if (!program_) {
    return;
  }

  // Clear the screen
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Get head pose (but don't use it for rendering - static view)
  float position[3] = {0, 0, 0};
  float orientation[4] = {0, 0, 0, 1};
  if (head_tracker_) {
    CardboardHeadTracker_getPose(head_tracker_, 0, kLandscapeLeft, position, orientation);
  }
  
  // Render video frame to texture
  RenderVideoFrame();
  
  // Render the texture to screen (simple quad rendering for now)
  RenderTextureToScreen();
}

void VideoPlayerApp::OnTriggerEvent() {
  LOGD("Trigger event");
  // This will be handled by the Java layer for play/pause
}

void VideoPlayerApp::OnPause() {
  LOGD("OnPause");
  if (head_tracker_) {
    CardboardHeadTracker_pause(head_tracker_);
  }
}

void VideoPlayerApp::OnResume() {
  LOGD("OnResume");
  if (head_tracker_) {
    CardboardHeadTracker_resume(head_tracker_);
  }
}

void VideoPlayerApp::SetScreenParams(int width, int height) {
  LOGD("SetScreenParams: %dx%d", width, height);
  screen_width_ = width;
  screen_height_ = height;
  
  if (lens_distortion_) {
    CardboardLensDistortion_destroy(lens_distortion_);
  }
  if (distortion_renderer_) {
    CardboardDistortionRenderer_destroy(distortion_renderer_);
  }
  
  InitializeCardboard();
}

void VideoPlayerApp::SetVideoUri(const std::string& video_uri) {
  LOGD("SetVideoUri: %s", video_uri.c_str());
  video_uri_ = video_uri;
}

void VideoPlayerApp::InitializeGl() {
  LOGD("InitializeGl");
  
  // Create shader program
  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  const char* vertex_shader_src = kVertexShader;
  glShaderSource(vertex_shader, 1, &vertex_shader_src, nullptr);
  glCompileShader(vertex_shader);
  
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  const char* fragment_shader_src = kFragmentShader;
  glShaderSource(fragment_shader, 1, &fragment_shader_src, nullptr);
  glCompileShader(fragment_shader);
  
  program_ = glCreateProgram();
  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, fragment_shader);
  glLinkProgram(program_);
  
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);
  
  // Get attribute and uniform locations
  position_attrib_ = glGetAttribLocation(program_, "position");
  tex_coord_attrib_ = glGetAttribLocation(program_, "tex_coord");
  mvp_matrix_uniform_ = glGetUniformLocation(program_, "mvp_matrix");
  texture_uniform_ = glGetUniformLocation(program_, "texture");
  contrast_uniform_ = glGetUniformLocation(program_, "contrast");
  red_tint_uniform_ = glGetUniformLocation(program_, "red_tint");
  green_tint_uniform_ = glGetUniformLocation(program_, "green_tint");
  fog_intensity_uniform_ = glGetUniformLocation(program_, "fog_intensity");
  directional_stretch_uniform_ = glGetUniformLocation(program_, "directional_stretch");
  time_uniform_ = glGetUniformLocation(program_, "time");
  is_left_eye_uniform_ = glGetUniformLocation(program_, "is_left_eye");
  
  // Create vertex buffer
  glGenBuffers(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  
  // Combine vertices and texture coordinates into one buffer
  std::vector<float> combined_data;
  for (int i = 0; i < 8; i++) {
    // Add vertex position (3 floats)
    combined_data.push_back(kQuadVertices[i * 3]);
    combined_data.push_back(kQuadVertices[i * 3 + 1]);
    combined_data.push_back(kQuadVertices[i * 3 + 2]);
    // Add texture coordinates (2 floats)
    combined_data.push_back(kQuadTexCoords[i * 2]);
    combined_data.push_back(kQuadTexCoords[i * 2 + 1]);
  }
  
  glBufferData(GL_ARRAY_BUFFER, combined_data.size() * sizeof(float), 
               combined_data.data(), GL_STATIC_DRAW);
  
  // Create texture
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  // Initialize with a test pattern texture
  std::vector<uint8_t> test_texture(512 * 512 * 3);
  for (int i = 0; i < 512 * 512; i++) {
    test_texture[i * 3] = 128;     // R
    test_texture[i * 3 + 1] = 128; // G  
    test_texture[i * 3 + 2] = 255; // B
  }
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, 
               test_texture.data());
  
  glUseProgram(program_);
  glUniform1i(texture_uniform_, 0);
}

void VideoPlayerApp::InitializeCardboard() {
  LOGD("InitializeCardboard");
  
  // Create head tracker
  head_tracker_ = CardboardHeadTracker_create();
  
  // For now, skip complex distortion rendering
  // We'll use simple split-screen rendering instead
  lens_distortion_ = nullptr;
  distortion_renderer_ = nullptr;
}

void VideoPlayerApp::RenderVideoFrame() {
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  
  if (has_video_frame_) {
    // Video frame is available - the SurfaceTexture should have updated the texture
    // No need to create patterns, just use the existing texture content
    LOGD("Rendering video frame from SurfaceTexture");
  } else {
    // Show animated test pattern when no video is available
    CreateTestPattern();
  }
}

void VideoPlayerApp::CreateTestPattern() {
  // Create an animated test pattern to verify rendering
  const int width = 512;
  const int height = 512;
  std::vector<uint8_t> test_pattern(width * height * 3);
  
  // Get current time for animation
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  float time = millis * 0.001f; // Convert to seconds
  
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int index = (y * width + x) * 3;
      
      // Create an animated pattern with moving colors
      float center_x = width / 2.0f;
      float center_y = height / 2.0f;
      float dx = x - center_x;
      float dy = y - center_y;
      float distance = sqrt(dx * dx + dy * dy);
      
      // Animated color based on position and time
      float angle = atan2(dy, dx) + time;
      
      // Create RGB values with animation
      test_pattern[index] = (uint8_t)(128 + 127 * sin(angle + time));           // R
      test_pattern[index + 1] = (uint8_t)(128 + 127 * sin(angle + time + 2.0f)); // G
      test_pattern[index + 2] = (uint8_t)(128 + 127 * sin(angle + time + 4.0f)); // B
    }
  }
  
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
                  GL_RGB, GL_UNSIGNED_BYTE, test_pattern.data());
}

void VideoPlayerApp::CreateVideoIndicatorPattern() {
  const int width = 512; const int height = 512;
  std::vector<uint8_t> pattern(width * height * 3);
  auto now = std::chrono::high_resolution_clock::now();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
  float time = millis * 0.001f;
  
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int index = (y * width + x) * 3;
      // Create a pulsing green pattern to indicate video is available
      float pulse = 0.5f + 0.5f * sin(time * 2.0f);
      pattern[index] = (uint8_t)(50 * pulse);     // Red - low
      pattern[index + 1] = (uint8_t)(255 * pulse); // Green - high
      pattern[index + 2] = (uint8_t)(50 * pulse);  // Blue - low
    }
  }
  
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pattern.data());
}

void VideoPlayerApp::RenderTextureToScreen() {
  // Use the shader program
  glUseProgram(program_);
  
  // Bind the texture
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glUniform1i(texture_uniform_, 0);
  
  // Set up vertex attributes
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glEnableVertexAttribArray(position_attrib_);
  glVertexAttribPointer(position_attrib_, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
  
  // Set up texture coordinates
  glEnableVertexAttribArray(tex_coord_attrib_);
  glVertexAttribPointer(tex_coord_attrib_, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 
                       (void*)(3 * sizeof(float)));
  
  // Get current time for animation
  auto now = std::chrono::high_resolution_clock::now();
  auto duration = now.time_since_epoch();
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  float time = millis * 0.001f;
  
  // Set time uniform
  glUniform1f(time_uniform_, time);
  
  // Set MVP matrix
  float mvp_matrix[16] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
  };
  glUniformMatrix4fv(mvp_matrix_uniform_, 1, GL_FALSE, mvp_matrix);
  
  // Render left eye
  glViewport(0, 0, screen_width_ / 2, screen_height_);
  glUniform1i(is_left_eye_uniform_, 1);
  glUniform1f(contrast_uniform_, effect_settings_.left_eye_contrast);
  glUniform1f(red_tint_uniform_, effect_settings_.left_eye_red_tint);
  glUniform1f(green_tint_uniform_, effect_settings_.left_eye_green_tint);
  glUniform1f(fog_intensity_uniform_, effect_settings_.left_eye_fog_intensity);
  glUniform1f(directional_stretch_uniform_, effect_settings_.left_eye_directional);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4); // Draw left eye quad
  
  // Render right eye
  glViewport(screen_width_ / 2, 0, screen_width_ / 2, screen_height_);
  glUniform1i(is_left_eye_uniform_, 0);
  glUniform1f(contrast_uniform_, effect_settings_.right_eye_contrast);
  glUniform1f(red_tint_uniform_, effect_settings_.right_eye_red_tint);
  glUniform1f(green_tint_uniform_, effect_settings_.right_eye_green_tint);
  glUniform1f(fog_intensity_uniform_, effect_settings_.right_eye_fog_intensity);
  glUniform1f(directional_stretch_uniform_, effect_settings_.right_eye_directional);
  glDrawArrays(GL_TRIANGLE_STRIP, 4, 4); // Draw right eye quad
  
  // Reset viewport
  glViewport(0, 0, screen_width_, screen_height_);
  
  // Disable vertex attributes
  glDisableVertexAttribArray(position_attrib_);
  glDisableVertexAttribArray(tex_coord_attrib_);
}

#ifdef OPENCV_AVAILABLE
void VideoPlayerApp::ProcessVideoFrame(const cv::Mat& input, cv::Mat& output) {
  if (input.empty()) {
    return;
  }
  
  // Create split-screen output
  output = cv::Mat::zeros(input.rows, input.cols, input.type());
  
  // Process left eye
  cv::Rect left_eye_rect(0, 0, input.cols / 2, input.rows);
  cv::Mat left_eye = input(left_eye_rect);
  cv::Mat processed_left_eye;
  if (effect_settings_.left_eye_enabled) {
    ApplyEffects(left_eye, processed_left_eye, true);
  } else {
    left_eye.copyTo(processed_left_eye);
  }
  processed_left_eye.copyTo(output(left_eye_rect));
  
  // Process right eye
  cv::Rect right_eye_rect(input.cols / 2, 0, input.cols / 2, input.rows);
  cv::Mat right_eye = input(right_eye_rect);
  cv::Mat processed_right_eye;
  if (effect_settings_.right_eye_enabled) {
    ApplyEffects(right_eye, processed_right_eye, false);
  } else {
    right_eye.copyTo(processed_right_eye);
  }
  processed_right_eye.copyTo(output(right_eye_rect));
}
#else
void VideoPlayerApp::ProcessVideoFrame(const uint8_t* input_data, int width, int height, uint8_t* output_data) {
  if (!input_data || !output_data) {
    return;
  }

  // For now, just copy the input to output without processing
  // This creates a basic split-screen effect
  int half_width = width / 2;
  int bytes_per_pixel = 4; // RGBA
  
  // Copy left half to both left and right output
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < half_width; x++) {
      int input_idx = (y * width + x) * bytes_per_pixel;
      int left_output_idx = (y * width + x) * bytes_per_pixel;
      int right_output_idx = (y * width + (x + half_width)) * bytes_per_pixel;
      
      // Copy to left eye
      output_data[left_output_idx] = input_data[input_idx];
      output_data[left_output_idx + 1] = input_data[input_idx + 1];
      output_data[left_output_idx + 2] = input_data[input_idx + 2];
      output_data[left_output_idx + 3] = input_data[input_idx + 3];
      
      // Copy to right eye (same as left for now)
      output_data[right_output_idx] = input_data[input_idx];
      output_data[right_output_idx + 1] = input_data[input_idx + 1];
      output_data[right_output_idx + 2] = input_data[input_idx + 2];
      output_data[right_output_idx + 3] = input_data[input_idx + 3];
    }
  }
}
#endif

void VideoPlayerApp::ApplyEffects(const cv::Mat& input, cv::Mat& output, bool is_left_eye) {
  if (input.empty()) {
    return;
  }
  
  const auto& settings = is_left_eye ? 
    EffectSettings{effect_settings_.left_eye_enabled, effect_settings_.left_eye_contrast,
                   effect_settings_.left_eye_red_tint, effect_settings_.left_eye_green_tint,
                   effect_settings_.left_eye_fog_intensity, effect_settings_.left_eye_directional} :
    EffectSettings{effect_settings_.right_eye_enabled, effect_settings_.right_eye_contrast,
                   effect_settings_.right_eye_red_tint, effect_settings_.right_eye_green_tint,
                   effect_settings_.right_eye_fog_intensity, effect_settings_.right_eye_directional};
  
  // Convert to float for processing
  cv::Mat float_input;
  input.convertTo(float_input, CV_32F, 1.0 / 255.0);
  
  cv::Mat result = float_input.clone();
  
  // Apply contrast
  float contrast = is_left_eye ? settings.left_eye_contrast : settings.right_eye_contrast;
  if (contrast != 1.0f) {
    result = result * contrast;
    result = cv::max(0.0f, cv::min(1.0f, result));
  }
  
  // Apply color tinting
  float red_tint = is_left_eye ? settings.left_eye_red_tint : settings.right_eye_red_tint;
  float green_tint = is_left_eye ? settings.left_eye_green_tint : settings.right_eye_green_tint;
  if (red_tint != 0.0f || green_tint != 0.0f) {
    std::vector<cv::Mat> channels;
    cv::split(result, channels);
    
    // Apply red tint
    if (red_tint != 0.0f) {
      channels[2] = channels[2] + red_tint * 0.3f; // Red channel
    }
    
    // Apply green tint
    if (green_tint != 0.0f) {
      channels[1] = channels[1] + green_tint * 0.3f; // Green channel
    }
    
    cv::merge(channels, result);
    result = cv::max(0.0f, cv::min(1.0f, result));
  }
  
  // Apply foggy effect
  float fog_intensity = is_left_eye ? settings.left_eye_fog_intensity : settings.right_eye_fog_intensity;
  if (fog_intensity > 0.0f) {
    cv::Mat fog_mask;
    cv::cvtColor(result, fog_mask, cv::COLOR_BGR2GRAY);
    
    // Apply Gaussian blur to create fog effect
    cv::Mat blurred;
    cv::GaussianBlur(fog_mask, blurred, cv::Size(15, 15), 0);
    
    // Create fog overlay
    cv::Mat fog_overlay;
    cv::cvtColor(blurred, fog_overlay, cv::COLOR_GRAY2BGR);
    
    // Blend with original image
    cv::addWeighted(result, 1.0f - fog_intensity, 
                   fog_overlay, fog_intensity, 0.0f, result);
  }
  
  // Apply directional stretch effect
  float directional = is_left_eye ? settings.left_eye_directional : settings.right_eye_directional;
  if (directional != 0.0f) {
    cv::Mat stretched;
    int stretch_amount = static_cast<int>(directional * 20); // Scale to pixels
    if (stretch_amount != 0) {
      cv::resize(result, stretched, cv::Size(result.cols + stretch_amount, result.rows));
      if (stretch_amount > 0) {
        // Stretch right
        result = stretched(cv::Rect(0, 0, result.cols, result.rows));
      } else {
        // Stretch left
        result = stretched(cv::Rect(-stretch_amount, 0, result.cols, result.rows));
      }
    }
  }
  
  // Convert back to uint8
  result.convertTo(output, CV_8U, 255.0);
}

void VideoPlayerApp::SetEffectSettings(const EffectSettings& settings) {
  effect_settings_ = settings;
}

int VideoPlayerApp::GetVideoTextureId() {
  return texture_id_;
}

void VideoPlayerApp::UpdateVideoTexture() {
  has_video_frame_ = true;
  LOGD("Video texture updated - frame available");
}

void VideoPlayerApp::BufferVideoFrame(const uint8_t* frame_data, int width, int height, int64_t timestamp) {
  if (!is_buffering_ || buffered_frames_count_ >= MAX_BUFFERED_FRAMES) {
    return;
  }
  
  // Calculate frame size
  int frame_size = width * height * 3; // RGB format
  
  // Store frame data
  video_frame_buffer_[buffered_frames_count_].resize(frame_size);
  memcpy(video_frame_buffer_[buffered_frames_count_].data(), frame_data, frame_size);
  frame_timestamps_[buffered_frames_count_] = timestamp;
  
  buffered_frames_count_++;
  last_buffer_time_ = std::chrono::high_resolution_clock::now();
  
  LOGD("Buffered frame %d at timestamp %lld", buffered_frames_count_, timestamp);
}

bool VideoPlayerApp::GetBufferedFrame(int64_t target_time, std::vector<uint8_t>& frame_data) {
  if (buffered_frames_count_ == 0) {
    return false;
  }
  
  // Find the closest frame to the target time
  int best_frame = 0;
  int64_t best_diff = std::abs(frame_timestamps_[0] - target_time);
  
  for (int i = 1; i < buffered_frames_count_; i++) {
    int64_t diff = std::abs(frame_timestamps_[i] - target_time);
    if (diff < best_diff) {
      best_diff = diff;
      best_frame = i;
    }
  }
  
  // If the difference is too large, don't use this frame
  if (best_diff > 1000000) { // 1 second in microseconds
    return false;
  }
  
  frame_data = video_frame_buffer_[best_frame];
  return true;
}

void VideoPlayerApp::StartBuffering() {
  is_buffering_ = true;
  buffered_frames_count_ = 0;
  current_frame_index_ = 0;
  last_buffer_time_ = std::chrono::high_resolution_clock::now();
  LOGD("Started video buffering");
}

void VideoPlayerApp::StopBuffering() {
  is_buffering_ = false;
  LOGD("Stopped video buffering");
}

}  // namespace cardboard