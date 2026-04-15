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

void dumpDataArrToFile(float *arr, size_t size, const char *filename) {
  printf("SIZE: %zu - NAME: %s\n", size, filename);
  remove(filename);
  FILE *fp = fopen(filename, "w");
  if (!fp) {
    fprintf(stderr, "Failed to open file %s for writing\n", filename);
    return;
  }
  for (size_t i = 0; i < size; ++i) {
    fprintf(fp, "%.5f\n", arr[i]);
  }
  fclose(fp);
}

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

    size_t dataArrSize = header.numberOfSamples * sizeof(float);
    float *dataArr = malloc(dataArrSize);

    int framesRead = readWaveFile_float(&header, dataArr);
    if (framesRead < 0) {
      free(dataArr);
      closeFile(&header);
      return -1;
    }
    printf("[Custom] Frames read: %d\n", framesRead);
    dumpDataArrToFile(dataArr, header.numberOfSamples,
                      "./output/custom_ra.txt");

    free(dataArr);
    closeFile(&header);

    // return 0;
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

    printf("  fileInfo.channels: %d\n", fileInfo.channels);
    printf("  fileInfo.samplerate: %d\n", fileInfo.samplerate);
    printf("  fileInfo.format: %d\n", fileInfo.format);
    printf("  fileInfo.sections: %d\n", fileInfo.sections);
    printf("  fileInfo.seekable: %d\n", fileInfo.seekable);

    size_t dataArrSize = fileInfo.frames * sizeof(float);
    float *dataArr = malloc(dataArrSize);

    sf_count_t framesRead = sf_readf_float(file, dataArr, fileInfo.frames);
    printf("[PROF] Frames read: %lld\n", framesRead);

    dumpDataArrToFile(dataArr, fileInfo.frames, "./output/prof_ra.txt");

    free(dataArr);
    sf_close(file);
  }

  return 0;
}
