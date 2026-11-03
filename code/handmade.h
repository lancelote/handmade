#if !defined(HANDMADE_H)

#define ArrayCount(Array) (sizeof(Array) / sizeof(Array[0]))

/*
  services that the platform layer provides to the game
*/

/*
  services that the game provides to the platform layer
  (this may expand in the future - sound on a separate thread, etc.)
*/

/*
  takes four things

  - timing
  - controller / keyboard input
  - bitmap buffer to use
  - sound buffer to use
*/
// todo: in the future, rendering _specifically_ will become a three-tiered abstraction
typedef struct game_offscreen_buffer
{
    // pixels are always 32-bits wide, memory order BB GG RR XX
    void *Memory;
    int Width;
    int Height;
    int Pitch;
} game_offscreen_buffer;

typedef struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16 *Samples;
} game_sound_output_buffer;

typedef struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
} game_button_state;

typedef struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;

    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union
    {
        game_button_state Buttons[6];
        struct
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
} game_controller_input;

typedef struct game_input
{
    game_controller_input Controllers[4];
} game_input;

internal void
GameUpdateAndRender(
    game_input *Input,
    game_offscreen_buffer *Buffer,
    game_sound_output_buffer *SoundBuffer);

#define HANDMADE_H
#endif