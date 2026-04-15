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
  if (argc != 3) {
    fprintf(stderr, "Audio file path was not provided \n");
    return 1;
  }

  WaveFileHeader header;
  memset(&header, 0, sizeof(WaveFileHeader));

  char *srcFilePath = argv[1];
  if (openWaveFile(srcFilePath, &header) < 0) {
    printf("Failed to decode the input file: %s\n", srcFilePath);
    closeFile(&header);
    return -1;
  }
  printWaveInfo(&header);

  size_t framesRead;
  size_t numSamplesInFrameBuffer = FRAMES_WINDOW_SIZE * header.numOfChannels;
  float *frameBuffer = malloc(numSamplesInFrameBuffer * sizeof(float));

  printf("## Allocated Frame buffer samples: %zu\n", numSamplesInFrameBuffer);

  SF_INFO fileInfo;
  memset(&fileInfo, 0, sizeof(fileInfo));
  fileInfo.channels = header.numOfChannels;
  fileInfo.frames = header.numberOfSamples * header.numOfChannels;
  fileInfo.samplerate = header.sampleRate;
  fileInfo.format = 65538;
  fileInfo.seekable = 1;
  fileInfo.sections = 1;

  printf("  [OUT] fileInfo.channels: %d\n", fileInfo.channels);
  printf("  [OUT] fileInfo.samplerate: %d\n", fileInfo.samplerate);
  printf("  [OUT] fileInfo.format: %d\n", fileInfo.format);
  printf("  [OUT] fileInfo.sections: %d\n", fileInfo.sections);
  printf("  [OUT] fileInfo.seekable: %d\n", fileInfo.seekable);

  char *distFilePath = argv[2];
  remove(distFilePath);
  SNDFILE *outputFile = sf_open(distFilePath, SFM_WRITE, &fileInfo);
  if (!outputFile) {
    printf("Error opening output file: %s\n", sf_strerror(NULL));
    closeFile(&header);
    return -1;
  }

  // we decode the source file
  while ((framesRead = readWaveFile_Cfloat(&header, frameBuffer,
                                           FRAMES_WINDOW_SIZE)) > 0) {

    // We write to the file
    if (sf_writef_float(outputFile, frameBuffer, FRAMES_WINDOW_SIZE) < 0) {
      printf("Failed to write to the file %s\n", sf_strerror(NULL));
      free(frameBuffer);
      closeFile(&header);
      return -1;
    }
  }

  printf("File written at: %s\n", distFilePath);

  sf_close(outputFile);
  closeFile(&header);
  free(frameBuffer);

  return 0;
}
