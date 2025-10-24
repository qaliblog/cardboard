# VR Video Player

A Google Cardboard Android application that converts normal videos into a 3D-like experience using OpenCV effects.

## Features

- **Split-screen Video Playback**: Converts single videos into side-by-side stereo view
- **OpenCV Effects**: Applies foggy effects to the left frame for enhanced 3D perception
- **180-degree Static View**: Optimized for Cardboard viewing without head tracking
- **Gyroscope Controls**: 
  - Tap to pause/resume
  - Hold and tilt left/right to seek through video
- **Modern UI**: Clean file picker and video player interface
- **OpenCV Integration**: Real-time video processing with computer vision effects

## Technical Details

### Architecture
- Built on Google Cardboard SDK
- Uses Media3 ExoPlayer for video playback
- OpenCV for real-time video processing
- Native C++ rendering with OpenGL ES

### Video Processing
- Splits input video into two frames (left/right eye)
- Applies foggy effect to left frame using OpenCV
- Real-time processing with minimal latency
- 180-degree field of view for immersive experience

### Controls
- **Single Tap**: Pause/Resume video
- **Long Press + Tilt Left**: Seek backward (40° = 40 seconds)
- **Long Press + Tilt Right**: Seek forward (40° = 40 seconds)

## Building

### Prerequisites
- Android Studio Arctic Fox or later
- Android SDK 26+
- NDK 21+
- OpenCV for Android 4.8.0+

### Build Steps
1. Clone the repository
2. Open in Android Studio
3. Sync Gradle files
4. Build the project

### Dependencies
- Google Cardboard SDK
- OpenCV for Android
- Media3 ExoPlayer
- Material Design Components
- Dexter (Permissions)

## Usage

1. Launch the app
2. Select a video file using the file picker
3. The app will automatically switch to VR mode
4. Insert your phone into a Cardboard viewer
5. Use touch controls to pause/resume and seek

## Configuration

The app is configured for:
- 180-degree field of view
- Static viewing (no head tracking)
- Split-screen stereo rendering
- OpenCV foggy effect on left frame

## License

Copyright 2024 Google LLC

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.