// binplay.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <time.h>

#include <portaudio.h>

#define PROG "binplay"
#define CC "gcc"
#define C_FLAGS "-Wall -O3 -pedantic -lportaudio"

#define MAX_COMMAND_SIZE  512
#define MAX_FILE_SIZE     512
#define FRAMES_PER_BUFFER 512
#define SAMPLE_RATE       44100
#define CHANNEL_COUNT     2

#define NoError (0)
#define Error (-1)

typedef int32_t i32;
typedef uint32_t u32;
typedef int16_t i16;
typedef uint16_t u16;
typedef int8_t i8;
typedef uint8_t u8;

typedef struct Binplay {
  FILE* fp;
  u32 file_size;
  u32 file_cursor;
  u32 frames_per_buffer;
  u32 sample_rate;
} Binplay;

Binplay binplay = {0};
PaStream* stream = NULL;
PaStreamParameters output_port;

static i32 rebuild_program();
static void exec_command(const char* fmt, ...);
static i32 binplay_init(Binplay* b, const char* path);
static i32 binplay_process_audio(void* output);
static i32 stereo_callback(const void* in_buffer, void* out_buffer, unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user_data);
static i32 binplay_open_stream(Binplay* b);
static i32 binplay_start_stream(Binplay* b);
static void binplay_exit(Binplay* b);

i32 main(i32 argc, char** argv) {
  if (rebuild_program()) {
    return 0;
  }
#if 0
  if (argc <= 1) {
    fprintf(stdout, "USAGE:\n  ./%s <filename>\n", PROG);
    return 0;
  }
#endif
  if (binplay_init(&binplay, "audio.wav") == NoError) {
    if (binplay_open_stream(&binplay) == NoError) {
      binplay_start_stream(&binplay);
      fprintf(stdout, "Press ENTER to exit\n");
      getc(stdin);
    }
    binplay_exit(&binplay);
  }
  return 0;
}

// Compare modify dates between executable and source file
// Recompile and run program again if they differ.
i32 rebuild_program() {
  struct stat source_stat;
  struct stat bin_stat;

  char filename[MAX_FILE_SIZE] = {0};
  snprintf(filename, MAX_FILE_SIZE, "%s.c", PROG);
  if (stat(filename, &source_stat) < 0) {
    return 0;
  }
  snprintf(filename, MAX_FILE_SIZE, "%s", PROG);
  if (stat(filename, &bin_stat) < 0) {
    return 0;
  }

  time_t time_diff = source_stat.st_ctime - bin_stat.st_ctime;

  // Negative time diffs means that the executable file is up to date to the source code
  if (time_diff > 0) {
    exec_command("set -xe");
    exec_command("%s %s.c -o %s %s && ./%s", CC, PROG, PROG, C_FLAGS, PROG);
    return 1;
  }
  return 0;
}

void exec_command(const char* fmt, ...) {
  char command[MAX_COMMAND_SIZE] = {0};
  va_list args;
  va_start(args, fmt);
  vsnprintf(command, MAX_COMMAND_SIZE, fmt, args);
  va_end(args);

  FILE* fp = popen(command, "w");
  fclose(fp);
}

i32 binplay_init(Binplay* b, const char* path) {
  if (!(b->fp = fopen(path, "r"))) {
    fprintf(stderr, "Failed to open '%s'\n", path);
    return Error;
  }
  fseek(b->fp, 0, SEEK_END);
  b->file_size = ftell(b->fp);
  fseek(b->fp, 0, SEEK_SET);
  b->file_cursor = 0;
  b->frames_per_buffer = FRAMES_PER_BUFFER;
  b->sample_rate = SAMPLE_RATE;
  return NoError;
}

i32 stereo_callback(const void* in_buffer, void* out_buffer, unsigned long frames_per_buffer, const PaStreamCallbackTimeInfo* time_info, PaStreamCallbackFlags flags, void* user_data) {
  if (binplay_process_audio(out_buffer) == NoError) {
    return paContinue;
  }
  return paComplete;
}

i32 binplay_open_stream(Binplay* b) {
  PaError err = Pa_Initialize();
  if (err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "PortAudio Error: %s\n", Pa_GetErrorText(err));
    return Error;
  }

  i32 output_device = Pa_GetDefaultOutputDevice();
  output_port.device = output_device;
  output_port.channelCount = CHANNEL_COUNT;
  output_port.sampleFormat = paInt16;
  output_port.suggestedLatency = Pa_GetDeviceInfo(output_port.device)->defaultHighOutputLatency;
  output_port.hostApiSpecificStreamInfo = NULL;

  if ((err = Pa_IsFormatSupported(NULL, &output_port, b->sample_rate)) != paFormatIsSupported) {
    fprintf(stderr, "PortAudio Error: %s\n", Pa_GetErrorText(err));
    return Error;
  }

  err = Pa_OpenStream(
    &stream,
    NULL,
    &output_port,
    b->sample_rate,
    b->frames_per_buffer,
    paNoFlag,
    stereo_callback,
    NULL
  );
  if (err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "PortAudio Error: %s\n", Pa_GetErrorText(err));
    return Error;
  }
  return NoError;
}

i32 binplay_start_stream(Binplay* b) {
  PaError err = Pa_StartStream(stream);
  if (err != paNoError) {
    Pa_Terminate();
    fprintf(stderr, "PortAudio Error: %s\n", Pa_GetErrorText(err));
    return Error;
  }
  return NoError;
}

i32 binplay_process_audio(void* output) {
  Binplay* b = &binplay;
  i16* buffer = (i16*)output;
  for (u32 i = 0; i < b->frames_per_buffer; ++i) {
    *buffer++ = 0;
    *buffer++ = 0;
  }
  return NoError;
}

void binplay_exit(Binplay* b) {
  fclose(b->fp);
  Pa_CloseStream(stream);
  Pa_Terminate();
}
