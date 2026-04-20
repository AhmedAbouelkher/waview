#ifndef FLAC_H
#define FLAC_H

#include <stddef.h>
#include <stdint.h>

/*
 * Minimal sequential FLAC decoder API.
 *
 * The decoder currently targets a practical subset of the FLAC format:
 * - file-backed input only
 * - full file is loaded into memory by flac_open()
 * - sequential forward decoding only
 * - PCM output as interleaved int32 or normalized float
 *
 * A frame is one sample for each channel. For stereo, reading N frames writes
 * 2 * N samples into the destination buffer in LRLR... order.
 */

/* Decoder status codes. Positive values are non-fatal states, negatives are errors. */
typedef enum {
  FLAC_STATUS_OK = 0,
  FLAC_STATUS_END_OF_STREAM = 1,
  FLAC_STATUS_INVALID_ARGUMENT = -1,
  FLAC_STATUS_IO_ERROR = -2,
  FLAC_STATUS_INVALID_FORMAT = -3,
  FLAC_STATUS_UNSUPPORTED = -4,
  FLAC_STATUS_MEMORY_ERROR = -5,
  FLAC_STATUS_CRC_MISMATCH = -6
} FlacStatus;

/* Basic stream properties parsed from the mandatory STREAMINFO metadata block. */
typedef struct {
  uint32_t sample_rate;
  uint16_t channels;
  uint16_t bits_per_sample;
  uint16_t min_block_size;
  uint16_t max_block_size;
  uint64_t total_frames;
} FlacStreamInfo;

/*
 * Decoder instance.
 *
 * Initialize by zeroing the struct or by declaring it as `{0}` before the
 * first flac_open() call. All fields below the public status/stream_info
 * section are internal and should not be modified by callers.
 */
typedef struct {
  /* Public state. */
  FlacStreamInfo stream_info;
  FlacStatus last_error;
  const char *error_message;

  // internal
  uint8_t *file_data;
  size_t file_size;
  size_t data_offset;
  size_t next_frame_offset;
  uint64_t decoded_frames;
  uint32_t magic;

  // internal
  int32_t *block_samples;
  uint32_t block_size;
  uint32_t block_index;
  uint32_t block_capacity;
} FlacDecoder;

/*
 * Opens and parses a FLAC file.
 *
 * On success, stream_info is filled and the decoder is ready for sequential
 * reads from the first audio frame. On failure, returns 0 and last_error can
 * be inspected with flac_last_error().
 */
int flac_open(FlacDecoder *decoder, const char *path);

/* Releases all memory owned by the decoder and resets it to the closed state. */
void flac_close(FlacDecoder *decoder);

/*
 * Reads up to frame_count frames as interleaved float samples in the range
 * roughly [-1.0, 1.0). Returns the number of frames written.
 *
 * A return value smaller than frame_count means either end of stream or a
 * decode error. Check decoder->last_error for the exact reason.
 */
size_t flac_read_f32(FlacDecoder *decoder, size_t frame_count, float *frame_buffer);

/*
 * Reads up to frame_count frames as interleaved signed 32-bit PCM samples.
 *
 * Samples preserve the decoded integer sample values from the FLAC stream.
 * Narrower source bit depths are sign-extended into int32_t.
 */
size_t flac_read_s32(
    FlacDecoder *decoder,
    size_t frame_count,
    int32_t *frame_buffer);

/* Returns a stable human-readable string for a status code. */
const char *flac_status_string(FlacStatus status);

/* Returns the decoder's most recent error string, or a default status string. */
const char *flac_last_error(const FlacDecoder *decoder);

#endif
