#include "../../dependencies/miniaudio.h"
#include "../wave.h"

#include <stddef.h>
#include <stdio.h>

ma_format decodedFormat(WaveFileHeader h) {
  switch (h.bitsPerSample) {
  case 8:
    return ma_format_u8;
  case 16:
    return ma_format_s16;
  case 32:
    return ma_format_s32;
  default:
    return ma_format_unknown;
  }
}

void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                   ma_uint32 frameCount) {

  WaveFileHeader *fHeader = (WaveFileHeader *)pDevice->pUserData;

  size_t framesRead = readWaveFile_CInt(fHeader, (int *)pOutput, frameCount);
  if (framesRead < 0) {
    // error happened
    return;
  }

  (void)pInput;
}

int main(int argc, char **argv) {
  ma_device_config deviceConfig;
  ma_device device;

  if (argc < 2) {
    printf("No input file.\n");
    return -1;
  }

  WaveFileHeader fHeader = {0};
  if (openWaveFile(argv[1], &fHeader) < 0) {
    return -1;
  }

  deviceConfig = ma_device_config_init(ma_device_type_playback);
  deviceConfig.playback.format = decodedFormat(fHeader);
  deviceConfig.playback.channels = fHeader.numOfChannels;
  deviceConfig.sampleRate = fHeader.sampleRate;
  deviceConfig.dataCallback = data_callback;
  deviceConfig.pUserData = &fHeader;

  if (ma_device_init(NULL, &deviceConfig, &device) != MA_SUCCESS) {
    printf("Failed to open playback device.\n");
    closeFile(&fHeader);
    return -3;
  }

  if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start playback device.\n");
    ma_device_uninit(&device);
    closeFile(&fHeader);
    return -4;
  }

  printf("Press Enter to quit...");
  getchar();

  ma_device_uninit(&device);
  closeFile(&fHeader);

  return 0;
}