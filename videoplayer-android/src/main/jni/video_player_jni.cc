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

#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <memory>

#include "video_player_app.h"

using cardboard::VideoPlayerApp;

namespace {
std::unique_ptr<VideoPlayerApp> g_video_player_app;
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnCreate(
    JNIEnv* env, jobject obj, jobject asset_manager) {
  g_video_player_app = std::make_unique<VideoPlayerApp>();
  return reinterpret_cast<jlong>(g_video_player_app.get());
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnDestroy(
    JNIEnv* env, jobject obj, jlong native_app) {
  g_video_player_app.reset();
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnSurfaceCreated(
    JNIEnv* env, jobject obj, jlong native_app) {
  if (g_video_player_app) {
    g_video_player_app->OnSurfaceCreated();
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnDrawFrame(
    JNIEnv* env, jobject obj, jlong native_app) {
  if (g_video_player_app) {
    g_video_player_app->OnDrawFrame();
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnPause(
    JNIEnv* env, jobject obj, jlong native_app) {
  if (g_video_player_app) {
    g_video_player_app->OnPause();
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeOnResume(
    JNIEnv* env, jobject obj, jlong native_app) {
  if (g_video_player_app) {
    g_video_player_app->OnResume();
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeSetScreenParams(
    JNIEnv* env, jobject obj, jlong native_app, jint width, jint height) {
  if (g_video_player_app) {
    g_video_player_app->SetScreenParams(width, height);
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeSetVideoUri(
    JNIEnv* env, jobject obj, jlong native_app, jstring video_uri) {
  if (g_video_player_app) {
    const char* uri_str = env->GetStringUTFChars(video_uri, nullptr);
    g_video_player_app->SetVideoUri(std::string(uri_str));
    env->ReleaseStringUTFChars(video_uri, uri_str);
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeSetEffectSettings(
    JNIEnv* env, jobject obj, jlong native_app, 
    jboolean left_enabled, jfloat left_contrast, jfloat left_red_tint, jfloat left_green_tint, 
    jfloat left_fog_intensity, jfloat left_directional,
    jboolean right_enabled, jfloat right_contrast, jfloat right_red_tint, jfloat right_green_tint, 
    jfloat right_fog_intensity, jfloat right_directional) {
  if (g_video_player_app) {
    cardboard::EffectSettings settings;
    settings.left_eye_enabled = left_enabled;
    settings.left_eye_contrast = left_contrast;
    settings.left_eye_red_tint = left_red_tint;
    settings.left_eye_green_tint = left_green_tint;
    settings.left_eye_fog_intensity = left_fog_intensity;
    settings.left_eye_directional = left_directional;
    
    settings.right_eye_enabled = right_enabled;
    settings.right_eye_contrast = right_contrast;
    settings.right_eye_red_tint = right_red_tint;
    settings.right_eye_green_tint = right_green_tint;
    settings.right_eye_fog_intensity = right_fog_intensity;
    settings.right_eye_directional = right_directional;
    
    g_video_player_app->SetEffectSettings(settings);
  }
}

JNIEXPORT void JNICALL
Java_com_google_cardboard_videoplayer_VrVideoActivity_nativeUpdateVideoTexture(
    JNIEnv* env, jobject obj, jlong native_app, jobject surface_texture) {
  if (g_video_player_app) {
    // For now, just mark that we have a video frame
    // TODO: Extract actual texture from SurfaceTexture
    g_video_player_app->UpdateVideoTexture();
  }
}

}  // extern "C"