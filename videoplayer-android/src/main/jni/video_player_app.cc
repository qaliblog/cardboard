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

// Fragment shader for rendering video frames
const char kFragmentShader[] = R"(
precision mediump float;
varying vec2 v_tex_coord;
uniform sampler2D texture;

void main() {
  gl_FragColor = texture2D(texture, v_tex_coord);
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
const float kFovDegrees = 180.0f;

}  // namespace

VideoPlayerApp::VideoPlayerApp(AAssetManager* asset_manager)
    : program_(0),
      vertex_buffer_(0),
      texture_id_(0),
      position_attrib_(0),
      tex_coord_attrib_(0),
      mvp_matrix_uniform_(0),
      texture_uniform_(0),
      lens_distortion_(nullptr),
      distortion_renderer_(nullptr),
      head_tracker_(nullptr),
      frame_updated_(false),
      screen_width_(0),
      screen_height_(0),
      fov_degrees_(kFovDegrees),
      asset_manager_(asset_manager) {
  LOGD("VideoPlayerApp constructor");
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
  if (!program_ || !distortion_renderer_) {
    return;
  }

  // Get head pose (but don't use it for rendering - static view)
  CardboardHeadTracker_getPose(head_tracker_, 0, &CardboardHeadTracker_getPose);
  
  // Render video frame
  RenderVideoFrame();
  
  // Render distortion mesh
  CardboardDistortionRenderer_renderEyeToDisplay(
      distortion_renderer_, texture_id_, 0, 0, 0, screen_width_, screen_height_,
      0, 0, screen_width_, screen_height_);
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
  glShaderSource(vertex_shader, 1, &kVertexShader, nullptr);
  glCompileShader(vertex_shader);
  
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &kFragmentShader, nullptr);
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
  
  // Create vertex buffer
  glGenBuffers(1, &vertex_buffer_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);
  
  // Create texture
  glGenTextures(1, &texture_id_);
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  
  // Initialize with a black texture
  std::vector<uint8_t> black_texture(512 * 512 * 3, 0);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 512, 0, GL_RGB, GL_UNSIGNED_BYTE, 
               black_texture.data());
  
  glUseProgram(program_);
  glUniform1i(texture_uniform_, 0);
}

void VideoPlayerApp::InitializeCardboard() {
  LOGD("InitializeCardboard");
  
  // Create head tracker
  head_tracker_ = CardboardHeadTracker_create();
  
  // Create lens distortion (using default parameters for now)
  CardboardLensDistortionParams lens_params = {};
  lens_params.left_eye_field_of_view_angles[0] = fov_degrees_ * M_PI / 180.0f;
  lens_params.left_eye_field_of_view_angles[1] = fov_degrees_ * M_PI / 180.0f;
  lens_params.right_eye_field_of_view_angles[0] = fov_degrees_ * M_PI / 180.0f;
  lens_params.right_eye_field_of_view_angles[1] = fov_degrees_ * M_PI / 180.0f;
  
  lens_distortion_ = CardboardLensDistortion_create(&lens_params);
  
  // Create distortion renderer
  distortion_renderer_ = CardboardDistortionRenderer_create();
}

void VideoPlayerApp::RenderVideoFrame() {
  if (!frame_updated_) {
    return;
  }
  
  // Update texture with current frame
  glBindTexture(GL_TEXTURE_2D, texture_id_);
  
  if (!processed_frame_.empty()) {
    // Convert OpenCV Mat to OpenGL texture
#ifdef OPENCV_AVAILABLE
    cv::Mat gl_frame;
    cv::cvtColor(processed_frame_, gl_frame, cv::COLOR_BGR2RGB);
#else
    // Process raw frame data without OpenCV
    // For now, just use the raw data directly
#endif
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, gl_frame.cols, gl_frame.rows,
                    GL_RGB, GL_UNSIGNED_BYTE, gl_frame.data);
  }
  
  frame_updated_ = false;
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
  if (settings.contrast != 1.0f) {
    result = result * settings.contrast;
    result = cv::max(0.0f, cv::min(1.0f, result));
  }
  
  // Apply color tinting
  if (settings.red_tint != 0.0f || settings.green_tint != 0.0f) {
    std::vector<cv::Mat> channels;
    cv::split(result, channels);
    
    // Apply red tint
    if (settings.red_tint != 0.0f) {
      channels[2] = channels[2] + settings.red_tint * 0.3f; // Red channel
    }
    
    // Apply green tint
    if (settings.green_tint != 0.0f) {
      channels[1] = channels[1] + settings.green_tint * 0.3f; // Green channel
    }
    
    cv::merge(channels, result);
    result = cv::max(0.0f, cv::min(1.0f, result));
  }
  
  // Apply foggy effect
  if (settings.fog_intensity > 0.0f) {
    cv::Mat fog_mask;
    cv::cvtColor(result, fog_mask, cv::COLOR_BGR2GRAY);
    
    // Apply Gaussian blur to create fog effect
    cv::Mat blurred;
    cv::GaussianBlur(fog_mask, blurred, cv::Size(15, 15), 0);
    
    // Create fog overlay
    cv::Mat fog_overlay;
    cv::cvtColor(blurred, fog_overlay, cv::COLOR_GRAY2BGR);
    
    // Blend with original image
    cv::addWeighted(result, 1.0f - settings.fog_intensity, 
                   fog_overlay, settings.fog_intensity, 0.0f, result);
  }
  
  // Apply directional stretch effect
  if (settings.directional != 0.0f) {
    cv::Mat stretched;
    int stretch_amount = static_cast<int>(settings.directional * 20); // Scale to pixels
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

}  // namespace cardboard