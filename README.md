# Waview

A simple audio visualizer built with C and Raylib.

> **Note**: This is a passion project and is currently a work in progress. I'm still figuring things out as I go!

https://github.com/user-attachments/assets/85f8772e-2d8e-4708-841b-8ba9e71560ec

## Features

- Real-time FFT (Fast Fourier Transform) visualization.
- Supports various audio formats (via Raylib).
- Play/Pause functionality with `SPACE`.
- Basic time display.

## Prerequisites

- [Raylib](https://www.raylib.com/)
- A C compiler (GCC/Clang)

## Building

If you have `make` installed, you can simply run:

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
- `ESC`: Close window
