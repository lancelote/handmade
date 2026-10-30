#if !defined(WIN32_HANDMADE_H)

typedef struct win32_offscreen_buffer
{
    // pixels are always 32-bits wide, memory order BB GG RR XX
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
} win32_offscreen_buffer;

typedef struct win32_window_dimension
{
    int Width;
    int Height;
} win32_window_dimension;

typedef struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
} win32_sound_output;

#define WIN32_HANDMADE_H
#endif