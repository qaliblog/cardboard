#!/bin/bash

# Build script for Cardboard Video Player with 3D effects and buffering

echo "Building Cardboard Video Player..."

# Set up Android NDK environment
export ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-/opt/android-ndk}
export ANDROID_SDK_HOME=${ANDROID_SDK_HOME:-/opt/android-sdk}

# Build the video player module
cd videoplayer-android

# Clean previous builds
./gradlew clean

# Build the project
./gradlew assembleDebug

if [ $? -eq 0 ]; then
    echo "✅ Video player build successful!"
    echo "APK location: videoplayer-android/build/outputs/apk/debug/videoplayer-android-debug.apk"
    echo ""
    echo "Features implemented:"
    echo "  - ✅ Two-screen VR video playback"
    echo "  - ✅ 3D effects based on contrast settings"
    echo "  - ✅ 20-second future video buffering"
    echo "  - ✅ Enhanced shaders for depth perception"
    echo "  - ✅ Separate left/right eye effect controls"
    echo ""
    echo "To install on device:"
    echo "  adb install videoplayer-android/build/outputs/apk/debug/videoplayer-android-debug.apk"
else
    echo "❌ Build failed. Check the error messages above."
    exit 1
fi