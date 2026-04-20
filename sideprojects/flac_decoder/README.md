# FLAC Decoder

A lightweight C library for opening and decoding FLAC audio files. The library exposes a small sequential API for reading decoded PCM frames as either normalized `float` samples or signed `int32_t` samples.

The decoder itself has no external dependencies. A playback example is included under `examples/` and uses `miniaudio` from the shared `dependencies/` directory.

## Features

- **FLAC Metadata Parsing**: Reads the mandatory `STREAMINFO` block and exposes sample rate, channels, bit depth, block sizes, and total frame count.
- **Sequential Decoding**: Opens a FLAC file and decodes frames on demand.
- **Two Output Formats**:
  - `flac_read_f32`: Read interleaved PCM as normalized `float`
  - `flac_read_s32`: Read interleaved PCM as signed `int32_t`
- **Common FLAC Subframes**: Supports constant, verbatim, fixed predictor, and LPC subframes.
- **Stereo Decorrelation**: Supports independent stereo, left-side, right-side, and mid-side coding.
- **Zero Library Dependencies**: Only the standard C library is required for the decoder itself.

## Project Structure

```text
.
├── flac.h                 # Public decoder API
├── flac.c                 # Decoder implementation
├── README.md              # Project documentation
└── examples/
    ├── Makefile           # Example build helper
    └── simple_parser.c    # Playback example using miniaudio
```

## Usage

### 1. Open a FLAC File

```c
#include "flac.h"

int main(void) {
    FlacDecoder decoder = {0};

    if (!flac_open(&decoder, "audio.flac")) {
        return 1;
    }

    // Use decoder.stream_info here.

    flac_close(&decoder);
    return 0;
}
```

### 2. Read Float Samples

```c
float buffer[1024 * 2]; // 1024 stereo frames
size_t frames = flac_read_f32(&decoder, 1024, buffer);

while (frames > 0) {
    // Process interleaved float PCM...
    frames = flac_read_f32(&decoder, 1024, buffer);
}
```

### 3. Read Integer Samples

```c
int32_t buffer[1024 * 2]; // 1024 stereo frames
size_t frames = flac_read_s32(&decoder, 1024, buffer);

while (frames > 0) {
    // Process interleaved int PCM...
    frames = flac_read_s32(&decoder, 1024, buffer);
}
```

## Building

Build the decoder together with your own application:

```bash
cc -std=c99 -Wall -Wextra -Werror -o my_app main.c flac.c
```

## Example

The playback example is in `examples/simple_parser.c`.

It depends on:

- `../flac.c`
- `../../dependencies/miniaudio.h`
- `../../dependencies/miniaudio.c`

One way to build it from the project root is:

```bash
cc -std=c99 -Wall -Wextra -Werror \
  -o examples/flac_player \
  examples/simple_parser.c \
  flac.c \
  ../dependencies/miniaudio.c
```

Then run it with:

```bash
./examples/flac_player "/path/to/file.flac"
```

## Supported Scope

This implementation currently targets a practical sequential decoder:

- **Input**: File-backed FLAC streams
- **Read Mode**: Forward-only sequential reads
- **Output**: Interleaved `float` or `int32_t` PCM
- **Channels**: Mono, stereo, and multi-channel streams handled by the current decoder path

## Current Limitations

- The whole FLAC file is loaded into memory by `flac_open()`
- No seeking API yet
- No streaming/network input yet
- No metadata exposure beyond `STREAMINFO`
- Mid-stream format changes are currently rejected

## License

MIT
