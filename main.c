#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define NUM_OF_CHANNELS 2
#define N (1 << 9)

float in_arr[N] = {0};
float complex out_arr[N] = {0};

void hammingWindow(float in[]) {
  for (size_t n = 0; n < N; n++) {
    float t = (float)n / (N - 1);
    float coef = 0.54 - 0.46 * cosf(2 * PI * t);
    in[n] *= coef;
  }
}

float amp(float complex z) {
  float mag = hypotf(crealf(z), cimagf(z));
  return logf(1 + mag);
}

// Ported from https://rosettacode.org/wiki/Fast_Fourier_transform#Python
void fft(float in[], size_t stride, float complex out[], size_t n) {
  if (n == 1) {
    out[0] = in[0];
    return;
  }
  fft(in, stride * 2, out, n / 2);
  fft(in + stride, stride * 2, out + n / 2, n / 2);
  for (size_t k = 0; k < n / 2; ++k) {
    float t = (float)k / n;
    float complex v = cexp(-2 * I * PI * t) * out[k + n / 2];
    float complex e = out[k];
    out[k] = e + v;
    out[k + n / 2] = e - v;
  }
}

void audioProcessorCallback(void *bufferData, unsigned int frames) {
  if (frames > N)
    frames = N;

  float *samples = (float *)bufferData; // Cast to float pointer

  for (unsigned int i = 0; i < frames; i++) {
    float left = samples[i * NUM_OF_CHANNELS];      // Left channel
    float right = samples[i * NUM_OF_CHANNELS + 1]; // Right channel
    float chAvg = (left + right) / 2;

    memmove(in_arr, in_arr + 1, (N - 1) * sizeof(in_arr[0]));
    in_arr[N - 1] = chAvg;
    // in_arr[i] = left;
  }

  // Use only the latest half of in_arr for processing
  hammingWindow(in_arr);
  fft(in_arr, 1, out_arr, N);
}

// MARK:- UI Helpers
void formatTime(float time, char *formattedTime, size_t bufSize) {
  int minutes = (int)time / 60;
  int seconds = (int)time % 60;
  snprintf(formattedTime, bufSize, "%02d:%02d", minutes, seconds);
}

// MARK:- main
int main(int argc, char *argv[]) {
  if (argc != 2) {
    printf("You should provide the file path\n");
    return -1;
  }
  char *filePath = argv[1];
  printf("Playing the audio file: %s\n", filePath);

  // SetTraceLogLevel(LOG_DEBUG | LOG_FATAL | LOG_ERROR);
  InitWindow(800, 600, "Waview");
  SetTargetFPS(60);

  InitAudioDevice();
  Music music = LoadMusicStream(filePath);

  if (!IsMusicValid(music)) {
    printf("Music file was not valid for some reason\n");
    return -1;
  }

  PlayMusicStream(music);
  SetMusicVolume(music, 0.2f);

  AttachAudioStreamProcessor(music.stream, audioProcessorCallback);

  while (!WindowShouldClose()) {
    UpdateMusicStream(music);

    BeginDrawing();

    ClearBackground(BLACK);

    int w = GetRenderWidth();
    int h = GetRenderHeight();
    int hs = h / 2;

    float step = 1.06;
    size_t n = (float)N / 2;

    size_t samplesNum = 0;

    float maxAmp = 0.0f;
    for (size_t i = 0; i < n; ++i) {
      float a = amp(out_arr[i]);
      if (maxAmp < a)
        maxAmp = a;
    }

    int cell_width = w / n;

    for (size_t i = 0; i < n; i++) {
      float t = amp(out_arr[i]) / maxAmp;
      Color c = BLUE;
      if (i % 2 == 0) {
        c = RED;
      }
      DrawRectangle(i * cell_width, hs - hs * t, cell_width, hs * t, c);
    }

    char timePlayed[128];
    formatTime(GetMusicTimePlayed(music), timePlayed, 255);
    DrawText(timePlayed, 10, 10, 20, WHITE);
    if (IsKeyPressed(KEY_SPACE)) {
      if (IsMusicStreamPlaying(music)) {
        PauseMusicStream(music);
      } else {
        ResumeMusicStream(music);
      }
    }
    if (IsMusicStreamPlaying(music)) {
      char status[32] = "Playing";
      DrawText(status, 10, 35, 20, GREEN);
    } else {
      char status[32] = "Paused";
      DrawText(status, 10, 35, 20, RED);
    }

    EndDrawing();
  }

  CloseAudioDevice();
  CloseWindow();

  return 0;
}
