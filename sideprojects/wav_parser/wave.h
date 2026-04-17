#ifndef WAVE_H
#define WAVE_H

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WAV_FORMAT_PCM 0x0001
#define WAV_FORMAT_IEEE_FLOAT 0x0003
#define WAV_FORMAT_EXTENSIBLE 0xFFFE

#define WAV_INT8_MAX 128
#define WAV_INT8_FMAX 128.f
#define WAV_INT16_FMAX 32767.f
#define WAV_INT32_FMAX 2147483647.f

typedef struct {
  uint32_t fileSize;
  uint32_t fmtChunkSize;
  uint16_t formatTag;
  uint16_t numOfChannels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  uint16_t bytesPerSample;
  uint32_t numberOfSamples;
  uint32_t dataChunkSize;

  // internal
  FILE *file;
  // internal
  uint32_t currentDataOffset;
} WaveFileHeader;

/**
 * @brief Opens a WAV audio file and reads its header information.
 *
 * @param waveFilePath The path to the WAV file to open.
 * @param header Pointer to a WaveFileHeader struct that will be filled with
 * parsed header information.
 * @return Returns 0 on success, or a negative integer on error.
 */
int openWaveFile(char *waveFilePath, WaveFileHeader *header);

/**
 * @brief Reads the entire sample data from a WAV file and converts it to
 * normalized floating-point values.
 *
 * @param header Pointer to an already populated WaveFileHeader structure
 * representing the open WAV file.
 * @param ptr Pointer to a pre-allocated float array with space for at least
 * header->numberOfSamples elements.
 * @return Returns frames read count on success, or a negative integer on error.
 */
size_t readWaveFile_float(WaveFileHeader *header, float *ptr);

/**
 * @brief Reads a chunk of sample data (up to 'frames' frames) from a WAV file
 * and converts it to normalized floating-point values.
 *
 * @param header Pointer to an already populated WaveFileHeader structure
 * representing the open WAV file.
 * @param ptr Pointer to a pre-allocated float array with space for at least
 * 'frames' elements.
 * @param frames The number of frames to read and convert.
 * @return Returns frames read count on success, or a negative integer on error.
 */
size_t readWaveFile_Cfloat(WaveFileHeader *header, float *ptr, uint32_t frames);

/**
 * @brief Reads a chunk of sample data (up to 'frames' frames) from a WAV file
 * and converts it to normalized integer values.
 *
 * @param header Pointer to an already populated WaveFileHeader structure
 * representing the open WAV file.
 * @param ptr Pointer to a pre-allocated int array with space for at least
 * 'frames' elements.
 * @param frames The number of frames to read and convert.
 * @return Returns frames read count on success, or a negative integer on error.
 */
size_t readWaveFile_CInt(WaveFileHeader *header, int *ptr, uint32_t frames);

/**
 * @brief Closes the open WAV file associated with the provided header.
 *
 * @param header Pointer to the WaveFileHeader structure containing the open
 * file handle.
 */
void closeFile(const WaveFileHeader *header);

/**
 * @brief Prints parsed header information about the WAV file to stdout.
 *
 * @param header Pointer to the WaveFileHeader structure containing the parsed
 * information.
 */
void printWaveInfo(const WaveFileHeader *header);

#endif // WAVE_H
