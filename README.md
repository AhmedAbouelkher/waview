# Waview

A simple, high-performance audio visualizer built with C and Raylib.

> **Note**: This is a passion project and is currently a work in progress. I'm still figuring things out as I go!

## V2 Preview (Latest)

<https://github.com/user-attachments/assets/f3ea5287-acdc-46e1-8847-e9f54917c250>

## V1 Preview

<https://github.com/user-attachments/assets/85f8772e-2d8e-4708-841b-8ba9e71560ec>

## Features

- **Drag & Drop**: Easily load new audio files by dragging them into the window.
- **Real-time FFT**: Fast Fourier Transform visualization with Hamming windowing.
- **Multiple View Modes**: Toggle between Horizontal and Radial (Circular) visualizations.
- **Smooth Animations**: Easing functions and linear interpolation for fluid visual transitions.
- **Audio Controls**: Play, pause, seek, and volume management.
- **Format Support**: Supports various audio formats via Raylib (mp3, wav, ogg, etc.).
- **macOS App Bundle**: Includes a `Makefile` target to build a native `.app` bundle.

## Prerequisites

- [Raylib](https://www.raylib.com/)
- A C compiler (GCC/Clang)
- `make` (optional)

## Building

### Standard Build

If you have `make` installed:

```bash
make build
```

The executable will be located in the `build/` directory.

Otherwise, compile manually linking raylib:

```bash
cc main.c -lraylib -lm -o waview
```

### macOS App Bundle

To create a native macOS application bundle:

```bash
make macos
```

This will generate `Waview.app` in the `build/` directory.

## Usage

Run the executable and provide the path to an audio file:

```bash
./waview path/to/your/music.mp3
```

## Controls

- `SPACE`: Play/Pause
- `L`: Toggle View Mode (Horizontal/Radial)
- `UP/DOWN`: Volume +/-
- `LEFT/RIGHT`: Seek +/- 5s
- `M`: Mute/Unmute
- `Q` / `ESC`: Quit
- **Drag & Drop**: Drop any supported audio file to play it immediately.

## License

This project is licensed under the [MIT License](LICENSE).

---

## Sideprojects

### [WAV Parser](./sideprojects/wav_parser)

A lightweight, dependency-free C library for parsing and reading WAV audio files. This project provides a simple API to extract header information and read sample data as normalized floating-point values.

#### Features

- **Header Parsing**: Extract sample rate, channel count, bit depth, and more.
- **Normalized Output**: Automatically converts 8-bit, 16-bit, and 32-bit PCM data to `float` (-1.0 to 1.0).
- **Flexible Reading**:
  - `readWaveFile_float`: Read the entire file at once.
  - `readWaveFile_Cfloat`: Read data in chunks (useful for streaming or large files).
- **Zero Dependencies**: Standard C library only.
