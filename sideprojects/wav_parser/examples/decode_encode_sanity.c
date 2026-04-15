#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// we use it as sanity check
#include "sndfile.h"

#define FRAMES_WINDOW_SIZE (1 << 5)

int main(int argc, char *argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Audio file path was not provided \n");
    return 1;
  }

  SF_INFO fileInfo;
  memset(&fileInfo, 0, sizeof(fileInfo));

  char *srcFilePath = argv[2];
  SNDFILE *srcFile = sf_open(srcFilePath, SFM_READ, &fileInfo);
  if (srcFile == NULL) {
    printf("Failed to open src file %s\n", sf_strerror(NULL));
    return -1;
  }

  printf("  fileInfo.channels: %d\n", fileInfo.channels);
  printf("  fileInfo.samplerate: %d\n", fileInfo.samplerate);
  printf("  fileInfo.format: %d\n", fileInfo.format);
  printf("  fileInfo.sections: %d\n", fileInfo.sections);
  printf("  fileInfo.seekable: %d\n", fileInfo.seekable);

  size_t framesRead;
  size_t numSamplesInFrameBuffer = FRAMES_WINDOW_SIZE * fileInfo.channels;
  float *frameBuffer = malloc(numSamplesInFrameBuffer * sizeof(float));

  char *distFilePath = argv[2];
  remove(distFilePath);
  SNDFILE *outputFile = sf_open(distFilePath, SFM_WRITE, &fileInfo);
  if (!outputFile) {
    printf("Error opening output file: %s\n", sf_strerror(NULL));
    sf_close(srcFile);
    return -1;
  }

  // we decode the source file
  while ((framesRead =
              sf_read_float(srcFile, frameBuffer, FRAMES_WINDOW_SIZE)) > 0) {

    // We write to the file
    if (sf_writef_float(outputFile, frameBuffer, FRAMES_WINDOW_SIZE) < 0) {
      printf("Failed to write to the file %s\n", sf_strerror(NULL));
      sf_close(outputFile);
      sf_close(srcFile);
      free(frameBuffer);
      return -1;
    }
  }

  printf("File written at: %s\n", distFilePath);

  sf_close(outputFile);
  sf_close(srcFile);
  free(frameBuffer);

  return 0;
}
