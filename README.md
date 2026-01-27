# OculusSonar

OculusSonar SDK with enhanced debugging and AI-powered object detection capabilities.

## Features

### YOLO Object Detection
- Real-time underwater object detection using YOLO v8
- Toggle detection on/off with checkbox in top-left corner
- Automatic bounding box visualization on sonar display
- Configurable confidence threshold (default: 0.9)
- ~100ms inference time on 512x512 sonar images

### Hex Viewer Integration
- Dedicated hex viewer panel on the right side (toggle with 'X' hotkey)
- Real-time packet data visualization in hexadecimal format with ASCII representation
- Integrated control panel with action buttons:
  - **Send Ping**: Transmits current sonar settings
  - **Clear**: Clears the hex viewer display
  - **Exit**: Toggles hex viewer visibility
- Professional dark theme with green monospace text
- Auto-scrolling to track latest packet data

### Technical Details
- ONNX Runtime 1.18.0 for ML inference
- OpenCV integration for image preprocessing
- Qt 6.10 based UI with OpenGL rendering
- Supports multiple sonar formats (512x415, 256x415, etc.)
- Automatic pixel-to-meter coordinate conversion

## Requirements
- ONNX Runtime 1.18.0+
- OpenCV 4.10+
- Qt 6.10+
- YOLO model file: `sonar.onnx` (place in executable directory)

## Usage
1. Place `sonar.onnx` model file in the same directory as the executable
2. Launch OculusSonar
3. Enable/disable object detection using the checkbox (top-left)
4. Toggle hex viewer with 'X' key for debugging

Detection boxes are automatically displayed in red on the sonar display with confidence scores.
