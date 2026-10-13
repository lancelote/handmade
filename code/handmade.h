#if !defined(HANDMADE_H)

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

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

#define HANDMADE_H
#endif