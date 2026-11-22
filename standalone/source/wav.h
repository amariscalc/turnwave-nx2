#ifndef TW_WAV_H
#define TW_WAV_H
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    FILE *file;
    uint32_t data_bytes;
    uint32_t bytes_read;
    uint32_t sample_rate;
    long data_offset;
} TwWav;

/* The native output currently accepts 48 kHz, stereo, 16-bit PCM WAV only.
 * A 44.1 kHz file must be resampled before it is passed to audout. */
int tw_wav_open(TwWav *wav, const char *path, char *error, size_t error_size);
size_t tw_wav_read(TwWav *wav, void *out, size_t bytes);
int tw_wav_rewind(TwWav *wav);
void tw_wav_close(TwWav *wav);
#endif
