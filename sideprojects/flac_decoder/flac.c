#include "flac.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Implementation notes
 *
 * This decoder keeps the design intentionally small:
 * - read the entire file into memory once
 * - parse metadata up front
 * - decode one FLAC frame at a time into a reusable PCM block buffer
 * - serve public reads by draining that decoded block buffer
 *
 * The implementation follows the FLAC bitstream model closely. Audio payloads
 * are not byte-aligned structs, so a dedicated bit reader is used throughout
 * frame and subframe parsing.
 */

typedef struct {
  const uint8_t *data;
  size_t size;
  size_t byte_pos;
  uint8_t bit_pos;
} FlacBitReader;

/* Normalized view of one frame header after all coded values are resolved. */
typedef struct {
  uint32_t block_size;
  uint32_t sample_rate;
  uint8_t channels;
  uint8_t bits_per_sample;
  uint8_t channel_assignment;
} FlacFrameHeader;

#define FLAC_DECODER_MAGIC 0x464C4143U

static const uint8_t flac_crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36,
    0x31, 0x24, 0x23, 0x2A, 0x2D, 0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B,
    0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D, 0xE0,
    0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1,
    0xC4, 0xC3, 0xCA, 0xCD, 0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82,
    0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD, 0xC7, 0xC0,
    0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3,
    0xE4, 0xED, 0xEA, 0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A, 0x27, 0x20, 0x29,
    0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04,
    0x0D, 0x0A, 0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F,
    0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A, 0x89, 0x8E, 0x87, 0x80,
    0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3,
    0xA4, 0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6,
    0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4, 0x69, 0x6E, 0x67, 0x60, 0x75,
    0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F,
    0x28, 0x3D, 0x3A, 0x33, 0x34, 0x4E, 0x49, 0x40, 0x47, 0x52, 0x55,
    0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63, 0x3E,
    0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F,
    0x1A, 0x1D, 0x14, 0x13, 0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC,
    0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83, 0xDE, 0xD9,
    0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA,
    0xFD, 0xF4, 0xF3};

static const uint16_t flac_crc16_table[256] = {
    0x0000, 0x8005, 0x800F, 0x000A, 0x801B, 0x001E, 0x0014, 0x8011, 0x8033,
    0x0036, 0x003C, 0x8039, 0x0028, 0x802D, 0x8027, 0x0022, 0x8063, 0x0066,
    0x006C, 0x8069, 0x0078, 0x807D, 0x8077, 0x0072, 0x0050, 0x8055, 0x805F,
    0x005A, 0x804B, 0x004E, 0x0044, 0x8041, 0x80C3, 0x00C6, 0x00CC, 0x80C9,
    0x00D8, 0x80DD, 0x80D7, 0x00D2, 0x00F0, 0x80F5, 0x80FF, 0x00FA, 0x80EB,
    0x00EE, 0x00E4, 0x80E1, 0x00A0, 0x80A5, 0x80AF, 0x00AA, 0x80BB, 0x00BE,
    0x00B4, 0x80B1, 0x8093, 0x0096, 0x009C, 0x8099, 0x0088, 0x808D, 0x8087,
    0x0082, 0x8183, 0x0186, 0x018C, 0x8189, 0x0198, 0x819D, 0x8197, 0x0192,
    0x01B0, 0x81B5, 0x81BF, 0x01BA, 0x81AB, 0x01AE, 0x01A4, 0x81A1, 0x01E0,
    0x81E5, 0x81EF, 0x01EA, 0x81FB, 0x01FE, 0x01F4, 0x81F1, 0x81D3, 0x01D6,
    0x01DC, 0x81D9, 0x01C8, 0x81CD, 0x81C7, 0x01C2, 0x0140, 0x8145, 0x814F,
    0x014A, 0x815B, 0x015E, 0x0154, 0x8151, 0x8173, 0x0176, 0x017C, 0x8179,
    0x0168, 0x816D, 0x8167, 0x0162, 0x8123, 0x0126, 0x012C, 0x8129, 0x0138,
    0x813D, 0x8137, 0x0132, 0x0110, 0x8115, 0x811F, 0x011A, 0x810B, 0x010E,
    0x0104, 0x8101, 0x8303, 0x0306, 0x030C, 0x8309, 0x0318, 0x831D, 0x8317,
    0x0312, 0x0330, 0x8335, 0x833F, 0x033A, 0x832B, 0x032E, 0x0324, 0x8321,
    0x0360, 0x8365, 0x836F, 0x036A, 0x837B, 0x037E, 0x0374, 0x8371, 0x8353,
    0x0356, 0x035C, 0x8359, 0x0348, 0x834D, 0x8347, 0x0342, 0x03C0, 0x83C5,
    0x83CF, 0x03CA, 0x83DB, 0x03DE, 0x03D4, 0x83D1, 0x83F3, 0x03F6, 0x03FC,
    0x83F9, 0x03E8, 0x83ED, 0x83E7, 0x03E2, 0x83A3, 0x03A6, 0x03AC, 0x83A9,
    0x03B8, 0x83BD, 0x83B7, 0x03B2, 0x0390, 0x8395, 0x839F, 0x039A, 0x838B,
    0x038E, 0x0384, 0x8381, 0x0280, 0x8285, 0x828F, 0x028A, 0x829B, 0x029E,
    0x0294, 0x8291, 0x82B3, 0x02B6, 0x02BC, 0x82B9, 0x02A8, 0x82AD, 0x82A7,
    0x02A2, 0x82E3, 0x02E6, 0x02EC, 0x82E9, 0x02F8, 0x82FD, 0x82F7, 0x02F2,
    0x02D0, 0x82D5, 0x82DF, 0x02DA, 0x82CB, 0x02CE, 0x02C4, 0x82C1, 0x8243,
    0x0246, 0x024C, 0x8249, 0x0258, 0x825D, 0x8257, 0x0252, 0x0270, 0x8275,
    0x827F, 0x027A, 0x826B, 0x026E, 0x0264, 0x8261, 0x0220, 0x8225, 0x822F,
    0x022A, 0x823B, 0x023E, 0x0234, 0x8231, 0x8213, 0x0216, 0x021C, 0x8219,
    0x0208, 0x820D, 0x8207, 0x0202};

/* Initializes a reader at an arbitrary byte offset. Bit position always starts at 0. */
static void flac_bit_reader_init(
    FlacBitReader *reader,
    const uint8_t *data,
    size_t size,
    size_t offset) {
  reader->data = data;
  reader->size = size;
  reader->byte_pos = offset;
  reader->bit_pos = 0;
}

/* Reads up to 56 bits MSB-first from the FLAC bitstream. */
static int flac_read_bits(FlacBitReader *reader, uint8_t count, uint64_t *value) {
  uint64_t out = 0;

  if (count > 56) {
    return 0;
  }

  while (count > 0) {
    uint8_t remaining;
    uint8_t take;
    uint8_t shift;
    uint8_t mask;

    if (reader->byte_pos >= reader->size) {
      return 0;
    }

    remaining = (uint8_t)(8U - reader->bit_pos);
    take = count < remaining ? count : remaining;
    shift = (uint8_t)(remaining - take);
    mask = (uint8_t)(((1U << take) - 1U) << shift);

    out = (out << take) | ((reader->data[reader->byte_pos] & mask) >> shift);

    reader->bit_pos = (uint8_t)(reader->bit_pos + take);
    if (reader->bit_pos == 8) {
      reader->bit_pos = 0;
      reader->byte_pos++;
    }

    count = (uint8_t)(count - take);
  }

  *value = out;
  return 1;
}

/* Reads a signed two's-complement value with arbitrary bit width. */
static int flac_read_signed_bits(FlacBitReader *reader, uint8_t count, int32_t *value) {
  uint64_t raw;

  if (count == 0) {
    *value = 0;
    return 1;
  }

  if (!flac_read_bits(reader, count, &raw)) {
    return 0;
  }

  if (count < 32 && (raw & (1ULL << (count - 1))) != 0) {
    raw |= ~((1ULL << count) - 1ULL);
  }

  *value = (int32_t)raw;
  return 1;
}

/* FLAC uses unary prefixes in Rice-coded residuals. */
static int flac_read_unary(FlacBitReader *reader, uint32_t *value) {
  uint32_t count = 0;
  uint64_t bit;

  while (1) {
    if (!flac_read_bits(reader, 1, &bit)) {
      return 0;
    }
    if (bit != 0) {
      break;
    }
    count++;
  }

  *value = count;
  return 1;
}

/* Subframes end on byte boundaries before the frame footer CRC16. */
static void flac_align_to_byte(FlacBitReader *reader) {
  if (reader->bit_pos != 0) {
    reader->bit_pos = 0;
    reader->byte_pos++;
  }
}

static uint8_t flac_crc8(const uint8_t *data, size_t size) {
  uint8_t crc = 0;
  size_t i;

  for (i = 0; i < size; ++i) {
    crc = flac_crc8_table[crc ^ data[i]];
  }

  return crc;
}

static uint16_t flac_crc16(const uint8_t *data, size_t size) {
  uint16_t crc = 0;
  size_t i;

  for (i = 0; i < size; ++i) {
    crc = (uint16_t)((crc << 8) ^ flac_crc16_table[((crc >> 8) ^ data[i]) & 0xFF]);
  }

  return crc;
}

/* Centralized error helper so the caller can keep the first useful failure reason. */
static const char *flac_set_error(FlacDecoder *decoder, FlacStatus status, const char *message) {
  decoder->last_error = status;
  decoder->error_message = message;
  return message;
}

static uint16_t flac_read_be16(const uint8_t *data) {
  return (uint16_t)((data[0] << 8) | data[1]);
}

static uint32_t flac_read_be24(const uint8_t *data) {
  return ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8) | data[2];
}

/*
 * Some files start with an ID3v2 tag before the FLAC marker. Skip it if
 * present so the decoder can still find the mandatory "fLaC" signature.
 */
static int flac_skip_id3v2(const uint8_t *data, size_t size, size_t *offset) {
  uint32_t tag_size;
  uint8_t flags;

  *offset = 0;
  if (size < 10 || memcmp(data, "ID3", 3) != 0) {
    return 1;
  }

  flags = data[5];
  tag_size = ((uint32_t)(data[6] & 0x7F) << 21) | ((uint32_t)(data[7] & 0x7F) << 14) |
             ((uint32_t)(data[8] & 0x7F) << 7) | (uint32_t)(data[9] & 0x7F);
  *offset = (size_t)(10U + tag_size + ((flags & 0x10U) != 0 ? 10U : 0U));
  return *offset <= size;
}

/*
 * Frame/sample numbers in FLAC headers are stored in a UTF-8-like variable
 * length encoding. This helper decodes that integer representation.
 */
static int flac_read_utf8_uint64(FlacBitReader *reader, uint64_t *value) {
  uint64_t raw;
  uint64_t out;
  uint8_t first;
  uint8_t extra;
  uint8_t i;

  if (reader->bit_pos != 0) {
    return 0;
  }

  if (!flac_read_bits(reader, 8, &raw)) {
    return 0;
  }

  first = (uint8_t)raw;
  if ((first & 0x80U) == 0) {
    *value = first;
    return 1;
  }

  if ((first & 0xE0U) == 0xC0U) {
    extra = 1;
    out = first & 0x1FU;
  } else if ((first & 0xF0U) == 0xE0U) {
    extra = 2;
    out = first & 0x0FU;
  } else if ((first & 0xF8U) == 0xF0U) {
    extra = 3;
    out = first & 0x07U;
  } else if ((first & 0xFCU) == 0xF8U) {
    extra = 4;
    out = first & 0x03U;
  } else if ((first & 0xFEU) == 0xFCU) {
    extra = 5;
    out = first & 0x01U;
  } else if (first == 0xFEU) {
    extra = 6;
    out = 0;
  } else {
    return 0;
  }

  for (i = 0; i < extra; ++i) {
    if (!flac_read_bits(reader, 8, &raw) || (raw & 0xC0U) != 0x80U) {
      return 0;
    }
    out = (out << 6) | (raw & 0x3FU);
  }

  *value = out;
  return 1;
}

/* STREAMINFO is the only mandatory metadata block and defines the decode format. */
static int flac_parse_streaminfo(FlacDecoder *decoder, const uint8_t *data, size_t size) {
  uint64_t total_frames;

  if (size != 34) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid STREAMINFO size");
    return 0;
  }

  decoder->stream_info.min_block_size = flac_read_be16(data);
  decoder->stream_info.max_block_size = flac_read_be16(data + 2);
  decoder->stream_info.sample_rate =
      ((uint32_t)data[10] << 12) | ((uint32_t)data[11] << 4) | ((uint32_t)data[12] >> 4);
  decoder->stream_info.channels = (uint16_t)(((data[12] & 0x0EU) >> 1) + 1U);
  decoder->stream_info.bits_per_sample =
      (uint16_t)((((uint32_t)data[12] & 0x01U) << 4) | ((uint32_t)data[13] >> 4));
  decoder->stream_info.bits_per_sample++;

  total_frames = ((uint64_t)(data[13] & 0x0FU) << 32) | ((uint64_t)data[14] << 24) |
                 ((uint64_t)data[15] << 16) | ((uint64_t)data[16] << 8) | data[17];
  decoder->stream_info.total_frames = total_frames;

  if (decoder->stream_info.channels == 0 || decoder->stream_info.channels > 8 ||
      decoder->stream_info.bits_per_sample == 0 || decoder->stream_info.bits_per_sample > 32 ||
      decoder->stream_info.max_block_size == 0 || decoder->stream_info.sample_rate == 0) {
    flac_set_error(decoder, FLAC_STATUS_UNSUPPORTED, "unsupported STREAMINFO values");
    return 0;
  }

  return 1;
}

/*
 * Metadata parsing stops at the first audio frame. Unknown metadata blocks are
 * skipped because only STREAMINFO is required for sequential decoding.
 */
static int flac_parse_metadata(FlacDecoder *decoder) {
  size_t offset;
  int saw_streaminfo = 0;

  if (!flac_skip_id3v2(decoder->file_data, decoder->file_size, &offset)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid ID3 tag");
    return 0;
  }

  if (offset + 4 > decoder->file_size ||
      memcmp(decoder->file_data + offset, "fLaC", 4) != 0) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing fLaC marker");
    return 0;
  }

  offset += 4;
  while (offset + 4 <= decoder->file_size) {
    uint8_t header = decoder->file_data[offset];
    uint32_t length = flac_read_be24(decoder->file_data + offset + 1);
    uint8_t type = (uint8_t)(header & 0x7FU);
    int is_last = (header & 0x80U) != 0;

    offset += 4;
    if (offset + length > decoder->file_size) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated metadata block");
      return 0;
    }

    if (type == 0) {
      if (saw_streaminfo || !flac_parse_streaminfo(decoder, decoder->file_data + offset, length)) {
        if (decoder->last_error == FLAC_STATUS_OK) {
          flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid STREAMINFO block");
        }
        return 0;
      }
      saw_streaminfo = 1;
    }

    offset += length;
    if (is_last) {
      decoder->data_offset = offset;
      decoder->next_frame_offset = offset;
      return saw_streaminfo;
    }
  }

  flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "metadata terminator missing");
  return 0;
}

/* Grows the reusable decoded PCM buffer to hold one full frame per channel. */
static int flac_reserve_block(FlacDecoder *decoder, uint32_t block_size) {
  size_t samples;
  int32_t *buffer;

  if (block_size <= decoder->block_capacity) {
    return 1;
  }

  samples = (size_t)block_size * decoder->stream_info.channels;
  buffer = (int32_t *)realloc(decoder->block_samples, samples * sizeof(*buffer));
  if (buffer == NULL) {
    flac_set_error(decoder, FLAC_STATUS_MEMORY_ERROR, "out of memory");
    return 0;
  }

  decoder->block_samples = buffer;
  decoder->block_capacity = block_size;
  return 1;
}

/* Each channel owns a contiguous slice inside block_samples. */
static int32_t *flac_channel_buffer(FlacDecoder *decoder, uint32_t channel) {
  return decoder->block_samples + ((size_t)channel * decoder->block_capacity);
}

/* FLAC residuals map signed integers to unsigned Rice codewords with zigzag folding. */
static int32_t flac_unfold_signed(uint32_t value) {
  return (int32_t)((value >> 1) ^ (uint32_t)-(int32_t)(value & 1U));
}

/*
 * Residuals are entropy-coded per partition. The first partition is shorter
 * because warm-up samples for the predictor are stored explicitly.
 */
static int flac_decode_residual(
    FlacBitReader *reader,
    int32_t *samples,
    uint32_t block_size,
    uint8_t predictor_order) {
  uint64_t raw;
  uint8_t method;
  uint8_t param_bits;
  uint8_t escape_value;
  uint8_t partition_order;
  uint32_t partitions;
  uint32_t partition_size;
  uint32_t sample_index;
  uint32_t partition;

  if (!flac_read_bits(reader, 2, &raw)) {
    return 0;
  }

  method = (uint8_t)raw;
  if (method > 1) {
    return 0;
  }

  param_bits = method == 0 ? 4 : 5;
  escape_value = method == 0 ? 15 : 31;

  if (!flac_read_bits(reader, 4, &raw)) {
    return 0;
  }

  partition_order = (uint8_t)raw;
  partitions = 1U << partition_order;
  if (block_size < predictor_order || partitions == 0 || (block_size % partitions) != 0) {
    return 0;
  }

  partition_size = block_size / partitions;
  sample_index = predictor_order;

  for (partition = 0; partition < partitions; ++partition) {
    uint32_t count = partition_size;
    uint32_t i;
    uint8_t rice_parameter;

    if (partition == 0) {
      if (count < predictor_order) {
        return 0;
      }
      count -= predictor_order;
    }

    if (!flac_read_bits(reader, param_bits, &raw)) {
      return 0;
    }

    rice_parameter = (uint8_t)raw;
    if (rice_parameter == escape_value) {
      uint8_t width;

      if (!flac_read_bits(reader, 5, &raw)) {
        return 0;
      }

      width = (uint8_t)raw;
      for (i = 0; i < count; ++i) {
        if (!flac_read_signed_bits(reader, width, &samples[sample_index++])) {
          return 0;
        }
      }
      continue;
    }

    for (i = 0; i < count; ++i) {
      uint32_t quotient;
      uint32_t folded;

      if (!flac_read_unary(reader, &quotient)) {
        return 0;
      }

      folded = quotient << rice_parameter;
      if (rice_parameter != 0) {
        if (!flac_read_bits(reader, rice_parameter, &raw)) {
          return 0;
        }
        folded |= (uint32_t)raw;
      }

      samples[sample_index++] = flac_unfold_signed(folded);
    }
  }

  return sample_index == block_size;
}

/* Reconstructs samples for fixed predictors of order 0..4. */
static void flac_restore_fixed(
    int32_t *samples,
    uint32_t block_size,
    uint8_t predictor_order) {
  uint32_t i;

  for (i = predictor_order; i < block_size; ++i) {
    int64_t prediction = 0;

    switch (predictor_order) {
      case 0:
        prediction = 0;
        break;
      case 1:
        prediction = samples[i - 1];
        break;
      case 2:
        prediction = (2LL * samples[i - 1]) - samples[i - 2];
        break;
      case 3:
        prediction =
            (3LL * samples[i - 1]) - (3LL * samples[i - 2]) + samples[i - 3];
        break;
      case 4:
        prediction = (4LL * samples[i - 1]) - (6LL * samples[i - 2]) +
                     (4LL * samples[i - 3]) - samples[i - 4];
        break;
    }

    samples[i] = (int32_t)(samples[i] + prediction);
  }
}

/* Reconstructs LPC-coded samples using decoded coefficients and residuals. */
static int flac_restore_lpc(
    FlacBitReader *reader,
    int32_t *samples,
    uint32_t block_size,
    uint8_t predictor_order,
    uint8_t bits_per_sample) {
  int32_t coefficients[32];
  int32_t shift_value;
  uint64_t raw;
  uint8_t precision;
  uint8_t i;
  int8_t shift;
  uint32_t sample;

  if (!flac_read_bits(reader, 4, &raw) || raw == 15) {
    return 0;
  }

  precision = (uint8_t)raw + 1;
  if (!flac_read_signed_bits(reader, 5, &shift_value)) {
    return 0;
  }
  shift = (int8_t)shift_value;

  (void)bits_per_sample;

  for (i = 0; i < predictor_order; ++i) {
    if (!flac_read_signed_bits(reader, precision, &coefficients[i])) {
      return 0;
    }
  }

  if (!flac_decode_residual(reader, samples, block_size, predictor_order)) {
    return 0;
  }

  for (sample = predictor_order; sample < block_size; ++sample) {
    int64_t prediction = 0;
    uint8_t coefficient;

    for (coefficient = 0; coefficient < predictor_order; ++coefficient) {
      prediction +=
          (int64_t)coefficients[coefficient] * samples[sample - coefficient - 1];
    }

    if (shift >= 0) {
      prediction >>= shift;
    } else {
      prediction <<= -shift;
    }

    samples[sample] = (int32_t)(samples[sample] + prediction);
  }

  return 1;
}

/*
 * Decodes one subframe for one channel. This implementation supports the
 * subframe types needed by common FLAC files: constant, verbatim, fixed and LPC.
 */
static int flac_decode_subframe(
    FlacBitReader *reader,
    int32_t *samples,
    uint32_t block_size,
    uint8_t bits_per_sample) {
  uint64_t raw;
  uint8_t type;
  uint8_t wasted_bits = 0;
  uint32_t i;

  if (!flac_read_bits(reader, 1, &raw) || raw != 0) {
    return 0;
  }
  if (!flac_read_bits(reader, 6, &raw)) {
    return 0;
  }
  type = (uint8_t)raw;
  if (!flac_read_bits(reader, 1, &raw)) {
    return 0;
  }

  if (raw != 0) {
    uint32_t zeros;
    if (!flac_read_unary(reader, &zeros)) {
      return 0;
    }
    wasted_bits = (uint8_t)(zeros + 1);
    if (wasted_bits >= bits_per_sample) {
      return 0;
    }
    bits_per_sample = (uint8_t)(bits_per_sample - wasted_bits);
  }

  if (type == 0) {
    int32_t sample_value;
    if (!flac_read_signed_bits(reader, bits_per_sample, &sample_value)) {
      return 0;
    }
    for (i = 0; i < block_size; ++i) {
      samples[i] = sample_value;
    }
  } else if (type == 1) {
    for (i = 0; i < block_size; ++i) {
      if (!flac_read_signed_bits(reader, bits_per_sample, &samples[i])) {
        return 0;
      }
    }
  } else if ((type & 0x38U) == 0x08U && (type & 0x07U) <= 4) {
    uint8_t order = (uint8_t)(type & 0x07U);
    for (i = 0; i < order; ++i) {
      if (!flac_read_signed_bits(reader, bits_per_sample, &samples[i])) {
        return 0;
      }
    }
    if (!flac_decode_residual(reader, samples, block_size, order)) {
      return 0;
    }
    flac_restore_fixed(samples, block_size, order);
  } else if ((type & 0x20U) != 0) {
    uint8_t order = (uint8_t)((type & 0x1FU) + 1U);
    if (order > 32 || order > block_size) {
      return 0;
    }
    for (i = 0; i < order; ++i) {
      if (!flac_read_signed_bits(reader, bits_per_sample, &samples[i])) {
        return 0;
      }
    }
    if (!flac_restore_lpc(reader, samples, block_size, order, bits_per_sample)) {
      return 0;
    }
  } else {
    return 0;
  }

  if (wasted_bits != 0) {
    for (i = 0; i < block_size; ++i) {
      samples[i] <<= wasted_bits;
    }
  }

  return 1;
}

/*
 * Parses and validates a frame header, including its CRC8. Coded values such
 * as block-size and sample-rate codes are resolved into concrete numbers here.
 */
static int flac_parse_frame_header(
    FlacDecoder *decoder,
    FlacBitReader *reader,
    size_t frame_start,
    FlacFrameHeader *header) {
  static const uint32_t sample_rates[12] = {
      0, 88200, 176400, 192000, 8000, 16000, 22050, 24000, 32000, 44100, 48000, 96000};
  uint64_t raw;
  uint64_t frame_index;
  uint8_t block_size_code;
  uint8_t sample_rate_code;
  uint8_t sample_size_code;
  uint8_t header_crc;

  if (!flac_read_bits(reader, 14, &raw) || raw != 0x3FFEU) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid frame sync code");
    return 0;
  }
  if (!flac_read_bits(reader, 1, &raw) || raw != 0) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "reserved frame bit is set");
    return 0;
  }
  if (!flac_read_bits(reader, 1, &raw)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame header");
    return 0;
  }
  if (!flac_read_bits(reader, 4, &raw)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame header");
    return 0;
  }
  block_size_code = (uint8_t)raw;
  if (!flac_read_bits(reader, 4, &raw)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame header");
    return 0;
  }
  sample_rate_code = (uint8_t)raw;
  if (!flac_read_bits(reader, 4, &raw)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame header");
    return 0;
  }
  header->channel_assignment = (uint8_t)raw;
  if (!flac_read_bits(reader, 3, &raw)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame header");
    return 0;
  }
  sample_size_code = (uint8_t)raw;
  if (!flac_read_bits(reader, 1, &raw) || raw != 0) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "reserved frame bit is set");
    return 0;
  }
  if (!flac_read_utf8_uint64(reader, &frame_index)) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid frame number");
    return 0;
  }

  (void)frame_index;

  switch (block_size_code) {
    case 1:
      header->block_size = 192;
      break;
    case 2:
    case 3:
    case 4:
    case 5:
      header->block_size = 576U << (block_size_code - 2U);
      break;
    case 6:
      if (!flac_read_bits(reader, 8, &raw)) {
        flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing block size");
        return 0;
      }
      header->block_size = (uint32_t)raw + 1U;
      break;
    case 7:
      if (!flac_read_bits(reader, 16, &raw)) {
        flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing block size");
        return 0;
      }
      header->block_size = (uint32_t)raw + 1U;
      break;
    default:
      if (block_size_code < 8) {
        flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid block size code");
        return 0;
      }
      header->block_size = 256U << (block_size_code - 8U);
      break;
  }

  if (sample_rate_code <= 11) {
    header->sample_rate = sample_rates[sample_rate_code];
  } else if (sample_rate_code == 12) {
    if (!flac_read_bits(reader, 8, &raw)) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing sample rate");
      return 0;
    }
    header->sample_rate = (uint32_t)raw * 1000U;
  } else if (sample_rate_code == 13) {
    if (!flac_read_bits(reader, 16, &raw)) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing sample rate");
      return 0;
    }
    header->sample_rate = (uint32_t)raw;
  } else if (sample_rate_code == 14) {
    if (!flac_read_bits(reader, 16, &raw)) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "missing sample rate");
      return 0;
    }
    header->sample_rate = (uint32_t)raw * 10U;
  } else {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid sample rate code");
    return 0;
  }

  switch (sample_size_code) {
    case 0:
      header->bits_per_sample = (uint8_t)decoder->stream_info.bits_per_sample;
      break;
    case 1:
      header->bits_per_sample = 8;
      break;
    case 2:
      header->bits_per_sample = 12;
      break;
    case 4:
      header->bits_per_sample = 16;
      break;
    case 5:
      header->bits_per_sample = 20;
      break;
    case 6:
      header->bits_per_sample = 24;
      break;
    default:
      flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid sample size code");
      return 0;
  }

  if (header->channel_assignment <= 7) {
    header->channels = (uint8_t)(header->channel_assignment + 1U);
  } else if (header->channel_assignment <= 10) {
    header->channels = 2;
  } else {
    flac_set_error(decoder, FLAC_STATUS_UNSUPPORTED, "unsupported channel assignment");
    return 0;
  }

  if (header->sample_rate == 0) {
    header->sample_rate = decoder->stream_info.sample_rate;
  }
  if (header->sample_rate != decoder->stream_info.sample_rate ||
      header->channels != decoder->stream_info.channels ||
      header->bits_per_sample != decoder->stream_info.bits_per_sample) {
    flac_set_error(decoder, FLAC_STATUS_UNSUPPORTED, "mid-stream format changes are unsupported");
    return 0;
  }

  if (reader->bit_pos != 0 || reader->byte_pos >= reader->size) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "invalid frame header alignment");
    return 0;
  }

  header_crc = reader->data[reader->byte_pos];
  if (flac_crc8(reader->data + frame_start, reader->byte_pos - frame_start) != header_crc) {
    flac_set_error(decoder, FLAC_STATUS_CRC_MISMATCH, "frame header CRC mismatch");
    return 0;
  }

  reader->byte_pos++;
  return 1;
}

/* Undo FLAC stereo decorrelation after both coded channels have been decoded. */
static void flac_apply_channel_assignment(
    FlacFrameHeader *header,
    int32_t *channel0,
    int32_t *channel1) {
  uint32_t i;

  if (header->channel_assignment == 8) {
    for (i = 0; i < header->block_size; ++i) {
      channel1[i] = channel0[i] - channel1[i];
    }
  } else if (header->channel_assignment == 9) {
    for (i = 0; i < header->block_size; ++i) {
      channel0[i] += channel1[i];
    }
  } else if (header->channel_assignment == 10) {
    for (i = 0; i < header->block_size; ++i) {
      int64_t mid = channel0[i];
      int64_t side = channel1[i];
      mid = (mid << 1) | (side & 1LL);
      channel0[i] = (int32_t)((mid + side) >> 1);
      channel1[i] = (int32_t)((mid - side) >> 1);
    }
  }
}

/*
 * Decodes the next full FLAC frame into the reusable block buffer and verifies
 * the frame footer CRC16 before exposing samples to public read calls.
 */
static int flac_decode_next_frame(FlacDecoder *decoder) {
  FlacBitReader reader;
  FlacFrameHeader header;
  uint32_t channel;
  size_t frame_start;
  size_t crc_offset;
  uint16_t expected_crc;

  if (decoder->next_frame_offset >= decoder->file_size) {
    decoder->last_error = FLAC_STATUS_END_OF_STREAM;
    decoder->error_message = flac_status_string(FLAC_STATUS_END_OF_STREAM);
    return 0;
  }

  frame_start = decoder->next_frame_offset;
  flac_bit_reader_init(&reader, decoder->file_data, decoder->file_size, frame_start);
  if (!flac_parse_frame_header(decoder, &reader, frame_start, &header)) {
    return 0;
  }
  if (!flac_reserve_block(decoder, header.block_size)) {
    return 0;
  }

  for (channel = 0; channel < header.channels; ++channel) {
    uint8_t channel_bps = header.bits_per_sample;

    if ((header.channel_assignment == 8 && channel == 1) ||
        (header.channel_assignment == 9 && channel == 0) ||
        (header.channel_assignment == 10 && channel == 1)) {
      channel_bps++;
    }

    if (!flac_decode_subframe(
            &reader,
            flac_channel_buffer(decoder, channel),
            header.block_size,
            channel_bps)) {
      flac_set_error(decoder, FLAC_STATUS_UNSUPPORTED, "unsupported subframe");
      return 0;
    }
  }

  flac_align_to_byte(&reader);
  crc_offset = reader.byte_pos;
  if (crc_offset + 2 > decoder->file_size) {
    flac_set_error(decoder, FLAC_STATUS_INVALID_FORMAT, "truncated frame footer");
    return 0;
  }

  expected_crc = flac_read_be16(decoder->file_data + crc_offset);
  if (flac_crc16(decoder->file_data + frame_start, crc_offset - frame_start) != expected_crc) {
    flac_set_error(decoder, FLAC_STATUS_CRC_MISMATCH, "frame CRC mismatch");
    return 0;
  }

  if (header.channel_assignment >= 8 && header.channel_assignment <= 10) {
    flac_apply_channel_assignment(
        &header,
        flac_channel_buffer(decoder, 0),
        flac_channel_buffer(decoder, 1));
  }

  decoder->block_size = header.block_size;
  decoder->block_index = 0;
  decoder->next_frame_offset = crc_offset + 2;
  return 1;
}

/* Public status strings are intentionally short and stable for debugging. */
const char *flac_status_string(FlacStatus status) {
  switch (status) {
    case FLAC_STATUS_OK:
      return "ok";
    case FLAC_STATUS_END_OF_STREAM:
      return "end of stream";
    case FLAC_STATUS_INVALID_ARGUMENT:
      return "invalid argument";
    case FLAC_STATUS_IO_ERROR:
      return "i/o error";
    case FLAC_STATUS_INVALID_FORMAT:
      return "invalid format";
    case FLAC_STATUS_UNSUPPORTED:
      return "unsupported feature";
    case FLAC_STATUS_MEMORY_ERROR:
      return "out of memory";
    case FLAC_STATUS_CRC_MISMATCH:
      return "crc mismatch";
  }

  return "unknown";
}

/* Returns the decoder's last stored error string, falling back to the status text. */
const char *flac_last_error(const FlacDecoder *decoder) {
  if (decoder == NULL) {
    return flac_status_string(FLAC_STATUS_INVALID_ARGUMENT);
  }

  return decoder->error_message != NULL ? decoder->error_message
                                        : flac_status_string(decoder->last_error);
}

/* Safe to call on both initialized and zeroed decoders. */
void flac_close(FlacDecoder *decoder) {
  if (decoder == NULL) {
    return;
  }

  if (decoder->magic != FLAC_DECODER_MAGIC) {
    memset(decoder, 0, sizeof(*decoder));
    decoder->last_error = FLAC_STATUS_OK;
    decoder->error_message = flac_status_string(FLAC_STATUS_OK);
    return;
  }

  free(decoder->block_samples);
  free(decoder->file_data);
  memset(decoder, 0, sizeof(*decoder));
  decoder->last_error = FLAC_STATUS_OK;
  decoder->error_message = flac_status_string(FLAC_STATUS_OK);
}

/*
 * Opens a file, loads it into memory, parses metadata and allocates the decode
 * buffer sized for the maximum frame block size reported by STREAMINFO.
 */
int flac_open(FlacDecoder *decoder, const char *path) {
  FILE *file;
  long size;

  if (decoder == NULL || path == NULL) {
    if (decoder != NULL) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_ARGUMENT, "invalid argument");
    }
    return 0;
  }

  if (decoder->magic == FLAC_DECODER_MAGIC) {
    flac_close(decoder);
  } else {
    memset(decoder, 0, sizeof(*decoder));
  }
  decoder->last_error = FLAC_STATUS_OK;
  decoder->error_message = flac_status_string(FLAC_STATUS_OK);

  file = fopen(path, "rb");
  if (file == NULL) {
    flac_set_error(decoder, FLAC_STATUS_IO_ERROR, "failed to open file");
    return 0;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    flac_set_error(decoder, FLAC_STATUS_IO_ERROR, "failed to seek file");
    return 0;
  }

  size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    flac_set_error(decoder, FLAC_STATUS_IO_ERROR, "failed to read file size");
    return 0;
  }

  decoder->file_size = (size_t)size;
  decoder->file_data = (uint8_t *)malloc(decoder->file_size);
  if (decoder->file_data == NULL) {
    fclose(file);
    flac_set_error(decoder, FLAC_STATUS_MEMORY_ERROR, "out of memory");
    return 0;
  }

  if (decoder->file_size != 0 &&
      fread(decoder->file_data, 1, decoder->file_size, file) != decoder->file_size) {
    fclose(file);
    flac_close(decoder);
    flac_set_error(decoder, FLAC_STATUS_IO_ERROR, "failed to read file");
    return 0;
  }

  fclose(file);

  if (!flac_parse_metadata(decoder)) {
    FlacStatus status = decoder->last_error;
    const char *message = decoder->error_message;
    flac_close(decoder);
    flac_set_error(decoder, status, message);
    return 0;
  }
  if (!flac_reserve_block(decoder, decoder->stream_info.max_block_size)) {
    FlacStatus status = decoder->last_error;
    const char *message = decoder->error_message;
    flac_close(decoder);
    flac_set_error(decoder, status, message);
    return 0;
  }

  decoder->block_size = 0;
  decoder->block_index = 0;
  decoder->decoded_frames = 0;
  decoder->magic = FLAC_DECODER_MAGIC;
  decoder->last_error = FLAC_STATUS_OK;
  decoder->error_message = flac_status_string(FLAC_STATUS_OK);
  return 1;
}

/*
 * Public reads are satisfied by draining the current decoded block buffer. When
 * the block is exhausted, the next FLAC frame is decoded on demand.
 */
size_t flac_read_s32(FlacDecoder *decoder, size_t frame_count, int32_t *frame_buffer) {
  size_t written = 0;
  size_t channels;

  if (decoder == NULL || frame_buffer == NULL) {
    if (decoder != NULL) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_ARGUMENT, "invalid argument");
    }
    return 0;
  }

  channels = decoder->stream_info.channels;
  while (written < frame_count) {
    size_t available;
    size_t frames;
    size_t frame;
    size_t channel;

    if (decoder->block_index >= decoder->block_size && !flac_decode_next_frame(decoder)) {
      break;
    }

    available = decoder->block_size - decoder->block_index;
    frames = frame_count - written;
    if (frames > available) {
      frames = available;
    }

    for (frame = 0; frame < frames; ++frame) {
      for (channel = 0; channel < channels; ++channel) {
        frame_buffer[(written + frame) * channels + channel] =
            flac_channel_buffer(decoder, (uint32_t)channel)[decoder->block_index + frame];
      }
    }

    written += frames;
    decoder->block_index += (uint32_t)frames;
    decoder->decoded_frames += frames;
  }

  return written;
}

/* Float reads reuse the same decode path and only apply final sample scaling. */
size_t flac_read_f32(FlacDecoder *decoder, size_t frame_count, float *frame_buffer) {
  size_t written = 0;
  size_t channels;
  double scale;

  if (decoder == NULL || frame_buffer == NULL) {
    if (decoder != NULL) {
      flac_set_error(decoder, FLAC_STATUS_INVALID_ARGUMENT, "invalid argument");
    }
    return 0;
  }

  channels = decoder->stream_info.channels;
  scale = decoder->stream_info.bits_per_sample == 32
              ? 1.0 / 2147483648.0
              : 1.0 / (double)(1ULL << (decoder->stream_info.bits_per_sample - 1U));

  while (written < frame_count) {
    size_t available;
    size_t frames;
    size_t frame;
    size_t channel;

    if (decoder->block_index >= decoder->block_size && !flac_decode_next_frame(decoder)) {
      break;
    }

    available = decoder->block_size - decoder->block_index;
    frames = frame_count - written;
    if (frames > available) {
      frames = available;
    }

    for (frame = 0; frame < frames; ++frame) {
      for (channel = 0; channel < channels; ++channel) {
        frame_buffer[(written + frame) * channels + channel] =
            (float)(flac_channel_buffer(decoder, (uint32_t)channel)[decoder->block_index + frame] *
                    scale);
      }
    }

    written += frames;
    decoder->block_index += (uint32_t)frames;
    decoder->decoded_frames += frames;
  }

  return written;
}
