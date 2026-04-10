# Waview

A simple, high-performance audio visualizer built with C and Raylib.

> **Note**: This is a passion project and is currently a work in progress. I'm still figuring things out as I go!

## V1 Preview

<https://github.com/user-attachments/assets/85f8772e-2d8e-4708-841b-8ba9e71560ec>

## Features

- **Real-time FFT**: Fast Fourier Transform visualization with Hamming windowing.
- **Multiple View Modes**: Toggle between Horizontal and Radial (Circular) visualizations.
- **Smooth Animations**: Easing functions for fluid visual transitions.
- **Audio Controls**: Play, pause, seek, and volume management.
- **Format Support**: Supports various audio formats via Raylib (mp3, wav, ogg, etc.).

## Prerequisites

- [Raylib](https://www.raylib.com/)
- A C compiler (GCC/Clang)
- `make` (optional)

## Building

If you have `make` installed:

```bash
make
```

Otherwise, compile manually linking raylib:

```bash
cc main.c -lraylib -lm -o waview
```

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

## License

This project is licensed under the [MIT License](LICENSE).
