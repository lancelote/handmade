#include "handmade.h"

internal void
RenderWeirdGradient(
    game_offscreen_buffer *Buffer,
    int BlueOffset,
    int GreenOffset)
{
    /*
    MEMORY ORDER:   RR GG BB xx
    LOADED IN:      xx BB GG RR
    WANTED:         xx RR GG BB
    MEMORY ORDER:   BB GG RR xx
    */

    /*
            WIDTH ->
            Width & BytesPerPixel -> pointer to the next row
    BitmapMemory -> 0 BB GG RR xx BB GG RR xx BB GG RR xx ...
    BM + Pitch      1 BB GG RR xx ...
            2 BB GG RR xx ...
            ...
    */

    // TODO: see what the optimizer does, and whether Buffer should be a pointer
    uint8 *Row = (uint8 *)Buffer->Memory;  // should be uint8 to add pitch (in bytes)
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            /*
            Pixel (32-bits)

                   0  1  2  3  - byte order number (Pixel + 1, etc.)
            memory:   BB GG RR xx
            register: xx RR GG BB
            */
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);
            *Pixel++ = (Green << 8) | Blue; // will be incremented by 4 bytes
        }

        Row += Buffer->Pitch;
    }

}

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
