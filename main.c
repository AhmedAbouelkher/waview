#include <complex.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raylib.h"

#define RAYGUI_IMPLEMENTATION
#include "./libs/raygui/src/raygui.h"

#if defined(PLATFORM_WEB)
#include <emscripten/emscripten.h>
#endif

#define NUM_OF_CHANNELS 2
#define N (1 << 9)

float in_arr[N] = {0};
float complex out_raw_arr[N] = {0};
float out_arr[N] = {0};
float out_smooth_arr[N];

Music music = {0};

bool global_isSoundMuted = false;
#ifdef global_isSoundMuted
float global_currentVolume = 0.0f;
#else
float global_currentVolume = 0.3f;
#endif

enum ViewMode { VIEWMODE_RADIAL, VIEW_MODE_HORIZONTAL };
enum ViewMode global_currentViewMode = VIEWMODE_RADIAL;

typedef enum {
  CONTROL_PLAY_PAUSE,
  CONTROL_MUTE,
  CONTROL_VOL_UP,
  CONTROL_VOL_DOWN,
  CONTROL_SEEK_BACK,
  CONTROL_SEEK_FORWARD,
  CONTROL_TOGGLE_VIEW,
} ControlAction;

float amp(float complex z) { return log10f(1.f + cabs(z) * 9.f); }

float ampInv(float a) { return (powf(10.0f, a) - 1.0f) / 9.0f; }

float easeOutBounce(float x) {
  const float n1 = 7.5625f;
  const float d1 = 2.75f;

  if (x < 1.0f / d1) {
    return n1 * x * x;
  } else if (x < 2.0f / d1) {
    x -= 1.5f / d1;
    return n1 * x * x + 0.75f;
  } else if (x < 2.5f / d1) {
    x -= 2.25f / d1;
    return n1 * x * x + 0.9375f;
  } else {
    x -= 2.625f / d1;
    return n1 * x * x + 0.984375f;
  }
}

float easeInOutCubic(float x) {
  if (x < 0.5f) {
    return 4.0f * x * x * x;
  } else {
    float t = -2.0f * x + 2.0f;
    return 1.0f - (t * t * t) / 2.0f;
  }
}

float easeOutQuad(float x) {
  float t = 1.0f - x;
  return 1.0f - t * t;
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
    float left = samples[i * NUM_OF_CHANNELS]; // Left channel
    // float right = samples[i * NUM_OF_CHANNELS + 1]; // Right channel
    // float chAvg = (left + right) / 2;

    memmove(in_arr, in_arr + 1, (N - 1) * sizeof(in_arr[0]));
    in_arr[N - 1] = left;
    // in_arr[i] = left;
  }

  // Apply Hamming window befor FFT
  for (size_t n = 0; n < N; n++) {
    float t = (float)n / (N - 1);
    float coef = 0.54 - 0.46 * cosf(2 * PI * t);
    in_arr[n] *= coef;
  }

  // APPLY FFT
  fft(in_arr, 1, out_raw_arr, N);
}

void startFreshAndPlay(Music music) {
  memset(in_arr, 0, sizeof(in_arr));
  memset(out_raw_arr, 0, sizeof(out_raw_arr));
  memset(out_arr, 0, sizeof(out_arr));
  memset(out_smooth_arr, 0, sizeof(out_smooth_arr));

  music.looping = true;
  PlayMusicStream(music);
  SetMusicVolume(music, global_currentVolume);
  AttachAudioStreamProcessor(music.stream, audioProcessorCallback);
}

// MARK:- UI Helpers
void formatTime(float time, char *formattedTime, size_t bufSize) {
  int minutes = (int)time / 60;
  int seconds = (int)time % 60;
  snprintf(formattedTime, bufSize, "%02d:%02d", minutes, seconds);
}

void handleControlAction(ControlAction action) {
  if (!IsMusicValid(music))
    return;

  switch (action) {
  case CONTROL_PLAY_PAUSE:
    if (IsMusicStreamPlaying(music)) {
      PauseMusicStream(music);
    } else {
      ResumeMusicStream(music);
    }
    break;
  case CONTROL_MUTE:
    global_isSoundMuted = !global_isSoundMuted;
    SetMusicVolume(music, global_isSoundMuted ? 0 : global_currentVolume);
    break;
  case CONTROL_VOL_UP:
    global_currentVolume = fminf(1.0f, global_currentVolume + 0.05f);
    global_isSoundMuted = global_currentVolume < 0.05f;
    SetMusicVolume(music, global_currentVolume);
    break;
  case CONTROL_VOL_DOWN:
    global_currentVolume = fmaxf(0.0f, global_currentVolume - 0.05f);
    global_isSoundMuted = global_currentVolume < 0.05f;
    SetMusicVolume(music, global_currentVolume);
    break;
  case CONTROL_SEEK_FORWARD:
    SeekMusicStream(music, GetMusicTimePlayed(music) + 5);
    break;
  case CONTROL_SEEK_BACK: {
    float current = GetMusicTimePlayed(music);
    if (current >= 5)
      SeekMusicStream(music, current - 5);
    break;
  }
  case CONTROL_TOGGLE_VIEW:
    global_currentViewMode =
        global_currentViewMode == VIEW_MODE_HORIZONTAL ? VIEWMODE_RADIAL
                                                       : VIEW_MODE_HORIZONTAL;
    break;
  }
}

static void UpdateDrawFrame(void); // Update and draw one frame

// MARK:- main
int main(int argc, char *argv[]) {
  char *filePath = NULL;
  if (argc > 1) {
    filePath = argv[1];
    printf("Playing the audio file: %s\n", filePath);
  }

  // SetTraceLogLevel(LOG_DEBUG | LOG_FATAL | LOG_ERROR);
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1200, 900, "Waview");

  InitAudioDevice();

  if (filePath != NULL) {
    music = LoadMusicStream(filePath);
  }

#if defined(PLATFORM_WEB)
  music = LoadMusicStream("resources/alexgrohl-energetic-action-sport.mp3");
#endif

  if (IsMusicValid(music)) {
    startFreshAndPlay(music);
    printf("Music is valid and playing\n");
  }

#if defined(PLATFORM_WEB)
  GuiSetStyle(DEFAULT, TEXT_SIZE, 20);
#endif

#if defined(PLATFORM_WEB)
  emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
  SetTargetFPS(60); // Set our game to run at 60 frames-per-second
  //--------------------------------------------------------------------------------------

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    UpdateDrawFrame();
  }
#endif

  CloseAudioDevice();
  CloseWindow();

  return 0;
}

static void UpdateDrawFrame(void) {
  if (IsFileDropped()) {
    FilePathList droppedFiles = LoadDroppedFiles();
    if (droppedFiles.count > 0) {
      if (IsMusicValid(music)) {
        StopMusicStream(music);
        DetachAudioStreamProcessor(music.stream, audioProcessorCallback);
        UnloadMusicStream(music);
      }

      music = LoadMusicStream(droppedFiles.paths[0]);
      if (IsMusicValid(music)) {
        startFreshAndPlay(music);
        printf("Playing: %s\n", droppedFiles.paths[0]);
      }
    }
    UnloadDroppedFiles(droppedFiles);
  }

  if (IsMusicValid(music)) {
    UpdateMusicStream(music);
  }

  float w = GetScreenWidth();
  float h = GetScreenHeight();
  float dt = GetFrameTime();

  if (IsKeyPressed(KEY_SPACE)) {
    handleControlAction(CONTROL_PLAY_PAUSE);
  } else if (IsKeyPressed(KEY_M)) {
    handleControlAction(CONTROL_MUTE);
  } else if (IsKeyPressed(KEY_UP)) {
    handleControlAction(CONTROL_VOL_UP);
  } else if (IsKeyPressed(KEY_DOWN)) {
    handleControlAction(CONTROL_VOL_DOWN);
  } else if (IsKeyPressed(KEY_RIGHT)) {
    handleControlAction(CONTROL_SEEK_FORWARD);
  } else if (IsKeyPressed(KEY_LEFT)) {
    handleControlAction(CONTROL_SEEK_BACK);
  } else if (IsKeyPressed(KEY_L)) {
    handleControlAction(CONTROL_TOGGLE_VIEW);
  }

  BeginDrawing();

  ClearBackground(BLACK);

  // >> START: DRAW VISUALS <<
  size_t m = 0;
  const float stepSize = 1.01;
  float lowf = 3.0f;
  float maxAmp = 1.0f;
  int n = N / (2 * 3);
  for (float f = lowf; (size_t)f < n; f = ceilf(f * stepSize)) {
    float f1 = ceilf(f * stepSize);
    float a = 0.0f;
    for (size_t q = (size_t)f; q < n && q < (size_t)f1; ++q) {
      float b = amp(out_raw_arr[q]);
      if (b > a)
        a = b;
    }
    if (maxAmp < a)
      maxAmp = a;
    out_arr[m++] = a;
  }
  for (size_t i = 0; i < m; ++i) {
    out_arr[i] /= maxAmp;
  }
  for (int i = 0; i < m; i++) {
    out_smooth_arr[i] += (out_arr[i] - out_smooth_arr[i]) * 4 * dt;
  }

  if (global_currentViewMode == VIEW_MODE_HORIZONTAL) {
    float cellWidth = ceilf((float)w / m);

    for (int i = 0; i < m; i++) {
      // float t = out_smooth_arr[i];
      float progress = (float)i / m;
      float weight = 1.0f + (progress * progress * (3 - 2 * progress)) * 1.2f;
      float t = out_smooth_arr[i] * weight;

      float hue = (float)i / m;
      Color color = ColorFromHSV(hue * 360, .75f, 1);
      // Adjust Y if you want it centered vertically
      float radius = (cellWidth / 2) * sqrt(t);
      // Compute maximum bar height as half the screen height
      float maxBarHeight = h / 2.0f;
      float barHeight = maxBarHeight * t;

      Vector2 startPos = {
          i * cellWidth - cellWidth / 2,
          .y = h / 2 - barHeight / 2,
      };
      Vector2 endPos = {
          i * cellWidth - cellWidth / 2,
          .y = h / 2 + barHeight / 2,
      };
      DrawLineEx(startPos, endPos, radius, color);
    }
  } else if (global_currentViewMode == VIEWMODE_RADIAL) {

    Vector2 center = {w / 2, h / 2};

    static float rotation = 0;
    if (IsMusicStreamPlaying(music)) {
      rotation += 10.f * dt;
    }

    // Use the first frequency bin (bass) to drive the radius
    float bigCircleRadius = 40.0f + (out_smooth_arr[0] * 20.0f);

    float baseRadius = (w * 2 / 5) - bigCircleRadius;
    float baseAngle = 360.0f / m;
    float maxOuterRadius = baseRadius - bigCircleRadius;

    float thickness = ceilf((3.f * PI * bigCircleRadius / m) * 1.5f);

    for (int i = 0; i < m; i++) {
      float progress = (float)i / m;
      float weight = 1.0f + (progress * progress * (3 - 2 * progress)) * 1.2f;
      float t = out_smooth_arr[i] * weight;

      float hue = (float)i / m;

      float angle = baseAngle * i + rotation;
      Vector2 dir = {cosf(angle * DEG2RAD), sinf(-angle * DEG2RAD)};
      Vector2 endPos = {
          center.x + dir.x * (bigCircleRadius + t * maxOuterRadius),
          center.y + dir.y * (bigCircleRadius + t * maxOuterRadius)};

      Color color = ColorFromHSV(hue * 360, .75f, easeOutQuad(t));
      DrawLineEx(center, endPos, thickness / 3, color);
      DrawCircleV(endPos, thickness / 1.5, color);
    }
    DrawCircleV(center, bigCircleRadius, WHITE);
  } else {
    printf("View mode is not supported\n");
  }

  // >> END: DRAW VISUALS <<

  // >> START: DRAW PLAYER CONTROLS <<

  const float fontSize = 25;

  // Keyboard shortcuts legend (top right)
  const char *shortcuts[] = {"[SPACE] Play/Pause", "[M] Mute/Unmute",
                             "[UP/DOWN] Vol +/-",  "[LEFT/RIGHT] Seek",
                             "[L] View Mode",      "[Q] Quit"};
  int numShortcuts = sizeof(shortcuts) / sizeof(shortcuts[0]);
  int legendFontSize = 20;
  int padding = 10;
  for (int i = 0; i < numShortcuts; i++) {
    int textWidth = MeasureText(shortcuts[i], legendFontSize);
    DrawText(shortcuts[i], w - textWidth - padding,
             padding + i * (legendFontSize + 5), legendFontSize, GRAY);
  }

  if (IsMusicValid(music)) {
    char timePlayed[64];
    char timeLength[64];
    formatTime(GetMusicTimeLength(music), timeLength, 64);
    formatTime(GetMusicTimePlayed(music), timePlayed, 64);
    DrawText(TextFormat("%s/%s", timePlayed, timeLength), 10, 10, fontSize,
             WHITE);

    char *playingStatus;
    Color statusColor;
    if (IsMusicStreamPlaying(music)) {
      playingStatus = "Playing";
      statusColor = GREEN;
    } else {
      playingStatus = "Paused";
      statusColor = ORANGE;
    }
    DrawText(playingStatus, 10, 35, fontSize, statusColor);

    if (global_isSoundMuted) {
      char *mutedStatus = "Muted";
      DrawText(mutedStatus, 10, 65, fontSize, RED);
    } else {
      const char *volumeLevel =
          TextFormat("Volume: %d", (int)roundf(global_currentVolume * 100));
      DrawText(volumeLevel, 10, 65, fontSize, ORANGE);
    }
  } else {
    DrawText("Drag and drop a music file to start", 10, 10, fontSize, GRAY);
  }

#if defined(PLATFORM_WEB)
  const float btnH = 50;
  const float btnW = 140;
  const float gap = 8;
  const float startX = 10;
  const float y = h - btnH - 12;
  typedef struct {
    const char *label;
    ControlAction action;
  } ControlButton;
  const ControlButton buttons[] = {
      {"Play/Pause", CONTROL_PLAY_PAUSE}, {"Mute", CONTROL_MUTE},
      {"Vol -", CONTROL_VOL_DOWN},         {"Vol +", CONTROL_VOL_UP},
      {"Seek -", CONTROL_SEEK_BACK},       {"Seek +", CONTROL_SEEK_FORWARD},
      {"Change View", CONTROL_TOGGLE_VIEW},
  };
  for (int i = 0; i < (int)(sizeof(buttons) / sizeof(buttons[0])); i++) {
    Rectangle r = {startX + i * (btnW + gap), y, btnW, btnH};
    if (GuiButton(r, buttons[i].label)) {
      handleControlAction(buttons[i].action);
    }
  }
#endif

  // >> END: DRAW PLAYER CONTROLS <<

  EndDrawing();
}