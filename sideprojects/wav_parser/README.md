# WAV Parser

A lightweight, dependency-free C library for parsing and reading WAV audio files. This project provides a simple API to extract header information and read sample data as normalized floating-point values.

## Features

- **Header Parsing**: Extract sample rate, channel count, bit depth, and more.
- **Normalized Output**: Automatically converts 8-bit, 16-bit, and 32-bit PCM data to `float` (-1.0 to 1.0).
- **Flexible Reading**:
  - `readWaveFile_float`: Read the entire file at once.
  - `readWaveFile_Cfloat`: Read data in chunks (useful for streaming or large files).
- **Zero Dependencies**: Standard C library only.

## Project Structure

```text
.
├── wave.h          # Library header
├── wave.c          # Library implementation
└── examples/       # Usage examples
    ├── read_all.c    # Example: Reading an entire file
    └── read_chunks.c # Example: Reading in chunks (streaming)
```

## Usage

### 1. Basic Example (Read All)

```c
#include "wave.h"

int main() {
    WaveFileHeader header;
    if (openWaveFile("audio.wav", &header) == 0) {
        printWaveInfo(&header);

        float *data = malloc(header.numberOfSamples * sizeof(float));
        readWaveFile_float(&header, data);

        // Process audio data...

        free(data);
        closeFile(&header);
    }
    return 0;
}
```

### 2. Chunked Reading

```c
float buffer[1024];
while (readWaveFile_Cfloat(&header, buffer, 1024) > 0) {
    // Process 1024 frames...
}
```

## Building

You can compile the library along with your project:

```bash
gcc -o my_app main.c wave.c
```

To build the provided examples, check the `examples/Makefile`.

## Supported Formats

- **Format**: PCM (Uncompressed)
- **Bit Depths**: 8-bit, 16-bit, 32-bit.
- **Channels**: Mono, Stereo, and Multi-channel.

## License

MIT
