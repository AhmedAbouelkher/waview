#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// we use it as sanity check
#include "sndfile.h"

#include "../wave.h"

#define FRAMES_WINDOW_SIZE (1 << 20)

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Audio file path was not provided\n");
    return 1;
  }
  char *srcFilePath = argv[1];

  // OUR CUSTOM IMPLEMENTATION
  if (1) {
    WaveFileHeader header;
    memset(&header, 0, sizeof(WaveFileHeader));

    if (openWaveFile(srcFilePath, &header) < 0) {
      closeFile(&header);
      return -1;
    }
    printWaveInfo(&header);

    // LOG FILE
    FILE *fp = fopen("./output/custom_chunks.txt", "w");
    if (!fp) {
      fprintf(stderr, "Failed to open file %s for writing\n",
              "custom_chunks.txt");
      return -1;
    }

    size_t framesRead;
    size_t numSamplesInFrameBuffer = FRAMES_WINDOW_SIZE * header.numOfChannels;
    float *frameBuffer = malloc(numSamplesInFrameBuffer * sizeof(float));

    printf("## Allocated Frame buffer samples: %zu\n", numSamplesInFrameBuffer);

    size_t totalSamplesProcessed = 0;
    while ((framesRead = readWaveFile_Cfloat(&header, frameBuffer,
                                             FRAMES_WINDOW_SIZE)) > 0) {
      size_t samplesInWindow = framesRead * header.numOfChannels;
      for (size_t i = 0; i < samplesInWindow; i++) {
        fprintf(fp, "[%zu] VAL: %.5f\n", totalSamplesProcessed + i,
                frameBuffer[i]);
      }
      fprintf(fp, "\n\n");
      totalSamplesProcessed += samplesInWindow;
    }

    free(frameBuffer);
    closeFile(&header);
    fclose(fp);

    // return 0;
  } else {
    printf("SKIPPING THE CUSTOM IMPL.\n");
  }

  printf("++++++++++++++++++++++++++++++++++++\n");

  // PROFESSIONAL IMPLEMENTATION
  if (1) {
    SF_INFO fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));

    SNDFILE *file = sf_open(srcFilePath, SFM_READ, &fileInfo);
    if (!file) {
      printf("Error opening file: %s\n", sf_strerror(NULL));
      return -1;
    }

    // LOG FILE
    FILE *fp = fopen("./output/prof_chunks.txt", "w");
    if (!fp) {
      fprintf(stderr, "Failed to open file %s for writing\n",
              "prof_chunks.txt");
      return -1;
    }

    sf_count_t framesRead;
    size_t samplesCount = FRAMES_WINDOW_SIZE * fileInfo.channels;
    float *frameBuffer = malloc(samplesCount * sizeof(float));

    printf("## [PROF] Allocated Frame buffer samples: %zu\n", samplesCount);

    size_t totalSamplesProcessed = 0;
    while ((framesRead =
                sf_readf_float(file, frameBuffer, FRAMES_WINDOW_SIZE)) > 0) {
      size_t samplesInWindow = framesRead * fileInfo.channels;
      for (size_t i = 0; i < samplesInWindow; i++) {
        fprintf(fp, "[%zu] VAL: %.5f\n", totalSamplesProcessed + i,
                frameBuffer[i]);
      }
      fprintf(fp, "\n\n");
      totalSamplesProcessed += samplesInWindow;
    }

    free(frameBuffer);
    sf_close(file);
    fclose(fp);
  } else {
    printf("SKIPPING THE PROF IMPL.\n");
  }

  return 0;
}
