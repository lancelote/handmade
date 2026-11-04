#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz)
{
    local_persist real32 tSine;
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;

    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; ++SampleIndex)
    {
        real32 SineValue = sinf(tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f*Pi32*1.0f/(real32)WavePeriod;
    }
}

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
GameUpdateAndRender(
    game_input *Input,
    game_offscreen_buffer *Buffer,
    game_sound_output_buffer *SoundBuffer)
{
    local_persist int BlueOffset = 0;
    local_persist int GreenOffset = 0;
    local_persist int ToneHz = 256;

    game_controller_input *Input0 = &Input->Controllers[0];

    if(Input0->IsAnalog)
    {
        // TODO: use analog movement tuning
        BlueOffset += (int)(4.0f * Input0->EndX);
        ToneHz = 256 + (int)(128.0f * Input0->EndY);
    }
    else
    {
        // TODO: use digital movement tuning
    }

    if(Input0->Down.EndedDown)
    {
        GreenOffset += 1;
    }

    // TODO: allow sample offsets here for more robust platform options
    GameOutputSound(SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, BlueOffset, GreenOffset);
}
