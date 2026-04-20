#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../flac.h"
#include "../../dependencies/miniaudio.h"

typedef struct {
  FlacDecoder decoder;
  int finished;
  int failed;
} AppState;

static void sleep_ms(unsigned int milliseconds) {
  struct timespec duration;

  duration.tv_sec = milliseconds / 1000U;
  duration.tv_nsec = (long)(milliseconds % 1000U) * 1000000L;
  nanosleep(&duration, NULL);
}

static void data_callback(ma_device *device, void *output, const void *input,
                          ma_uint32 frame_count) {
  AppState *app = (AppState *)device->pUserData;
  float *samples = (float *)output;
  size_t channels = app->decoder.stream_info.channels;
  size_t read_frames;

  (void)input;

  if (app->finished || app->failed) {
    memset(samples, 0, (size_t)frame_count * channels * sizeof(*samples));
    return;
  }

  read_frames = flac_read_f32(&app->decoder, frame_count, samples);
  if (read_frames < frame_count) {
    size_t remaining = frame_count - read_frames;
    memset(samples + (read_frames * channels), 0,
           remaining * channels * sizeof(*samples));

    if (app->decoder.last_error != FLAC_STATUS_OK &&
        app->decoder.last_error != FLAC_STATUS_END_OF_STREAM) {
      app->failed = 1;
    }

    app->finished = 1;
  }
}

int main(int argc, char **argv) {
  AppState app = {0};
  ma_device_config config;
  ma_device device;
  ma_result result;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <file.flac>\n", argv[0]);
    return 1;
  }

  if (!flac_open(&app.decoder, argv[1])) {
    fprintf(stderr, "flac_open() failed: %s\n", flac_last_error(&app.decoder));
    return 1;
  }

  printf("Playing %s\nsample_rate=%u channels=%u bits_per_sample=%u "
         "total_frames=%llu\n",
         argv[1], app.decoder.stream_info.sample_rate,
         app.decoder.stream_info.channels,
         app.decoder.stream_info.bits_per_sample,
         (unsigned long long)app.decoder.stream_info.total_frames);

  config = ma_device_config_init(ma_device_type_playback);
  config.playback.format = ma_format_f32;
  config.playback.channels = app.decoder.stream_info.channels;
  config.sampleRate = app.decoder.stream_info.sample_rate;
  config.dataCallback = data_callback;
  config.pUserData = &app;

  result = ma_device_init(NULL, &config, &device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ma_device_init() failed: %d\n", (int)result);
    flac_close(&app.decoder);
    return 1;
  }

  result = ma_device_start(&device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "ma_device_start() failed: %d\n", (int)result);
    ma_device_uninit(&device);
    flac_close(&app.decoder);
    return 1;
  }

  while (!app.finished && !app.failed) {
    sleep_ms(50);
  }

  ma_device_stop(&device);
  ma_device_uninit(&device);

  if (app.failed) {
    fprintf(stderr, "decode failed: %s\n", flac_last_error(&app.decoder));
    flac_close(&app.decoder);
    return 1;
  }

  flac_close(&app.decoder);
  return 0;
}
