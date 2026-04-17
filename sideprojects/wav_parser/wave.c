#include "wave.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/_types/_u_int16_t.h>

int openWaveFile(char *waveFilePath, WaveFileHeader *header) {
  if (header == NULL) {
    printf("Provided header must be initialized before passing\n");
    return -1;
  }

  FILE *wavFile = fopen(waveFilePath, "rb");
  if (wavFile == NULL) {
    printf("Failed to open the audio file due to %s\n", strerror(errno));
    return -1;
  }
  int ret = 0;

  header->file = wavFile;

  uint8_t riffID[4];
  uint8_t riffType[4];

  fread(riffID, 4, 1, wavFile);
  fread(&header->fileSize, 4, 1, wavFile);
  fread(riffType, 4, 1, wavFile);

  if (memcmp(riffID, "RIFF", 4) != 0 || memcmp(riffType, "WAVE", 4)) {
    printf("Invalid file formate, could not parse\n");
    ret = -1;
    goto end;
  }

  uint8_t id[4];
  uint32_t size;
  while (fread(id, 4, 1, wavFile) == 1 && fread(&size, 4, 1, wavFile) == 1) {
    if (memcmp(id, "fmt ", 4) == 0) {
      header->fmtChunkSize = size;

      fread(&header->formatTag, 2, 1, wavFile);
      fread(&header->numOfChannels, 2, 1, wavFile);
      fread(&header->sampleRate, 4, 1, wavFile);
      fread(&header->byteRate, 4, 1, wavFile);
      fread(&header->blockAlign, 2, 1, wavFile);
      fread(&header->bitsPerSample, 2, 1, wavFile);
      header->bytesPerSample = header->bitsPerSample / 8;

      if (header->formatTag != WAV_FORMAT_PCM) {
        printf("Failed to parse, PCM is the only supported\n");
        goto end;
      }

      uint32_t expectedByteRate =
          header->sampleRate * header->numOfChannels * header->bytesPerSample;
      if (header->byteRate != expectedByteRate) {
        printf("The audio file is corrupted\n");
        goto end;
      }

      uint32_t expectedBlockAlign =
          header->numOfChannels * header->bytesPerSample;
      if (header->blockAlign != expectedBlockAlign) {
        printf("The audio file is corrupted\n");
        goto end;
      }

      // Skip any extra bytes in fmt chunk if size > 16
      if (size > 16)
        fseek(wavFile, size - 16, SEEK_CUR);

    } else if (memcmp(id, "data", 4) == 0) {
      header->dataChunkSize = size;
      header->numberOfSamples = size / header->blockAlign;

      if (header->fmtChunkSize == 0) {
        printf(
            "Audio file is corrupted, retrieved data without the fmt first\n");
        goto end;
      }
      break;
    } else {
      // skip unsupported chunk
      fseek(wavFile, (size + 1) & ~1, SEEK_CUR);
    }
  }

end:
  return ret;
}

static int normalizeSamples_float(void *tempBuffer, float *outBuffer,
                                  uint16_t bitsPerSample,
                                  uint32_t numberOfSamples) {
  if (bitsPerSample == 32) {
    int32_t *samples = (int32_t *)tempBuffer;
    for (size_t i = 0; i < numberOfSamples; i++) {
      outBuffer[i] = samples[i] / WAV_INT32_FMAX;
    }
  } else if (bitsPerSample == 16) {
    int16_t *samples = (int16_t *)tempBuffer;
    for (size_t i = 0; i < numberOfSamples; i++) {
      outBuffer[i] = samples[i] / (float)WAV_INT16_FMAX;
    }
  } else if (bitsPerSample == 8) {
    uint8_t *samples = (uint8_t *)tempBuffer;
    for (size_t i = 0; i < numberOfSamples; i++) {
      outBuffer[i] = (samples[i] - WAV_INT8_MAX) / WAV_INT8_FMAX;
    }
  } else {
    return -1;
  }
  return 0;
}

size_t readWaveFile_float(WaveFileHeader *header, float *ptr) {
  if (ptr == NULL) {
    printf("An allocated float array is required\n");
    return -1;
  }
  size_t ret = -1;

  uint32_t size = header->dataChunkSize;
  void *rawData = malloc(size);
  if (rawData == NULL) {
    printf("Failed to allocate memory for raw data due to %s\n",
           strerror(errno));
    goto clean_and_exit;
  }

  if (fread(rawData, 1, size, header->file) != size) {
    printf("Failed to read all data frames due to %s\n", strerror(errno));
    goto clean_and_exit;
  }

  if (normalizeSamples_float(rawData, ptr, header->bitsPerSample,
                             header->numberOfSamples) < 0) {
    printf("The file bitsPerSample: %d is not supported\n",
           header->bitsPerSample);
    goto clean_and_exit;
  }

  // return the read frames count in case of success
  ret = header->numberOfSamples * header->numOfChannels;

clean_and_exit:
  free(rawData);
  return ret;
}

size_t readWaveFile_Cfloat(WaveFileHeader *header, float *ptr,
                           uint32_t frames) {
  if (ptr == NULL) {
    printf("An allocated float array is required\n");
    return 1;
  }
  size_t ret = -1;

  // Calculate remaining bytes in the data chunk
  uint32_t remainingBytes = header->dataChunkSize - header->currentDataOffset;
  uint32_t reqNumOfSamples = frames * header->numOfChannels;
  uint32_t reqChunkSize = reqNumOfSamples * header->bytesPerSample;
  // Clamp request to file boundaries
  if (reqChunkSize > remainingBytes) {
    reqChunkSize = remainingBytes;
    frames = reqChunkSize / (header->numOfChannels * header->bytesPerSample);
    reqNumOfSamples = frames * header->numOfChannels;
  }

  void *rawData = malloc(reqChunkSize);
  if (rawData == NULL) {
    printf("Failed to allocate memory for raw data due to %s\n",
           strerror(errno));
    goto clean_and_exit;
  }

  if (remainingBytes <= 0) {
    ret = 0;
    goto clean_and_exit;
  }

#ifdef WAV_DEBUG
  printf("  remainingBytes: %d\n", remainingBytes);
  printf("  frames: %d\n", frames);
  printf("  reqNumOfSamples: %d\n", reqNumOfSamples);
  printf("  reqChunkSize: %d\n", reqChunkSize);
#endif

  size_t bytesRead = fread(rawData, 1, reqChunkSize, header->file);

#ifdef WAV_DEBUG
  printf("  File READ BYTES: %zu\n", bytesRead);
#endif

  if (bytesRead <= 0) {
    printf(
        "Failed to read chunk size: %d [%d frames] from the file due to %s\n",
        reqChunkSize, frames, strerror(errno));
    goto clean_and_exit;
  }

  if (normalizeSamples_float(rawData, ptr, header->bitsPerSample,
                             reqNumOfSamples) < 0) {
    printf("The file bitsPerSample: %d is not supported\n",
           header->bitsPerSample);
    goto clean_and_exit;
  }

  header->currentDataOffset += reqChunkSize;
  ret = frames;

  remainingBytes = header->dataChunkSize - header->currentDataOffset;

#ifdef WAV_DEBUG
  printf("  AFT: remainingBytes %d\n", remainingBytes);
#endif

clean_and_exit:
  free(rawData);
  return ret;
}

size_t readWaveFile_CInt(WaveFileHeader *header, int *ptr, uint32_t frames) {
  if (ptr == NULL) {
    printf("An allocated float array is required\n");
    return 1;
  }
  size_t ret = -1;

  // Calculate remaining bytes in the data chunk
  uint32_t remainingBytes = header->dataChunkSize - header->currentDataOffset;
  uint32_t reqNumOfSamples = frames * header->numOfChannels;
  uint32_t reqChunkSize = reqNumOfSamples * header->bytesPerSample;
  // Clamp request to file boundaries
  if (reqChunkSize > remainingBytes) {
    reqChunkSize = remainingBytes;
    frames = reqChunkSize / (header->numOfChannels * header->bytesPerSample);
    reqNumOfSamples = frames * header->numOfChannels;
  }

  if (remainingBytes <= 0) {
    ret = 0;
    goto clean_and_exit;
  }

#ifdef WAV_DEBUG
  printf("  remainingBytes: %d\n", remainingBytes);
  printf("  frames: %d\n", frames);
  printf("  reqNumOfSamples: %d\n", reqNumOfSamples);
  printf("  reqChunkSize: %d\n", reqChunkSize);
#endif

  size_t bytesRead = fread(ptr, 1, reqChunkSize, header->file);

#ifdef WAV_DEBUG
  printf("  File READ BYTES: %zu\n", bytesRead);
#endif

  if (bytesRead <= 0) {
    printf(
        "Failed to read chunk size: %d [%d frames] from the file due to %s\n",
        reqChunkSize, frames, strerror(errno));
    goto clean_and_exit;
  }

  header->currentDataOffset += reqChunkSize;
  ret = frames;

  remainingBytes = header->dataChunkSize - header->currentDataOffset;

#ifdef WAV_DEBUG
  printf("  AFT: remainingBytes %d\n", remainingBytes);
#endif

clean_and_exit:
  return ret;
}

void closeFile(const WaveFileHeader *header) { fclose(header->file); }

void printWaveInfo(const WaveFileHeader *header) {
  printf("WAV Header Information:\n");
  printf("  File Size:        %d bytes\n", header->fileSize);
  printf("  Chunk 1 Size:     %d\n", header->fmtChunkSize);
  printf("  Format Tag:       %u (0x%X)\n", header->formatTag,
         header->formatTag);
  printf("  Channels:         %u\n", header->numOfChannels);
  printf("  Sample Rate:      %d Hz\n", header->sampleRate);
  printf("  Byte Rate:        %d bytes/sec\n", header->byteRate);
  printf("  Block Align:      %u\n", header->blockAlign);
  printf("  Bits Per Sample:  %u\n", header->bitsPerSample);
  printf("  Bytes Per Sample: %u\n", header->bytesPerSample);
  printf("  Data Size:        %d bytes (%d samples) (%d frames)\n",
         header->dataChunkSize, header->numberOfSamples,
         header->numberOfSamples / header->numOfChannels);
}