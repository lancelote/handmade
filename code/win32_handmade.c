#include <windows.h>
#include <stdint.h>
#include <xinput.h>
#include <dsound.h>

#define bool int
#define true 1
#define false 0
#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

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

// todo: global for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

// controller support is optional, so we need a trick in case we didn't compile
// with required libraries for it
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)

typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);

X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}

X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}

global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;

#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

// sound is optional
#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDEvice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void
Win32LoadXInput(void)
{
    // TODO: Windows 7 only has 1.3
    HMODULE XInputLibrary = LoadLibraryA("XInput1_4.dll");
    if(!XInputLibrary)
    {
        // TODO: diagnostic
        XInputLibrary = LoadLibraryA("XInput1_3.dll");
    }

    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        if(!XInputGetState) {XInputGetState = XInputGetStateStub;}

        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
        if(!XInputSetState) {XInputSetState = XInputSetStateStub;}

        // TODO: diagnostic
    }
    else
    {
        // TODO: diagnostic
    }
}

internal void
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

    if(DSoundLibrary)
    {
        // get a DirectSound object
        direct_sound_create *DirectSoundCreate = (direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

        // TODO: double-check if it works on XP - DirectSound8 or 7?
        LPDIRECTSOUND DirectSound;
        if(DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            WaveFormat.cbSize = 0;

            if(SUCCEEDED(DirectSound->lpVtbl->SetCooperativeLevel(DirectSound, Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                // TODO: DSBCAPS_GLOBALFOCUS?
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

                // "create" a primary buffer
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if(SUCCEEDED(DirectSound->lpVtbl->CreateSoundBuffer(DirectSound, &BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->lpVtbl->SetFormat(PrimaryBuffer, &WaveFormat);
                    if(SUCCEEDED(Error))
                    {
                        OutputDebugStringA("primary buffer format was set\n");
                    }
                    else
                    {
                        // TODO: diagnostic
                    }
                }
                else
                {
                    // TODO: diagnostic
                }
            }
            else
            {
                // TODO: diagnostic
            }

            // "create" a secondary buffer
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(BufferDescription);
            // TODO: DSBCAPS_GETCURRENTPOSITION2
            BufferDescription.dwFlags = 0;
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;

            HRESULT Error = DirectSound->lpVtbl->CreateSoundBuffer(
                DirectSound,
                &BufferDescription,
                &GlobalSecondaryBuffer,
                0);
            if(SUCCEEDED(Error))
            {
                OutputDebugStringA("secondary buffer created successfully\n");
            }
            else
            {
                // TODO: diagnostic
            }
        }
        else
        {
            // TODO: diagnostic
        }
    }
}

internal win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;

    RECT ClientRect; // writable region of a Window
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;

    return(Result);
}

internal void
RenderWeirdGradient(
    win32_offscreen_buffer *Buffer,
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
Win32ResizeDIBSection(
    win32_offscreen_buffer *Buffer,
    int Width,
    int Height)
{
    // TODO: play with stretch modes
    // TODO: maybe don't free first, free after, then free first if that fails
    if (Buffer->Memory)
    {
        VirtualFree(
            Buffer->Memory,
            0, // size, must be 0 for `MEM_RELEASE`
               // as it frees the entire region allocated with `VirtualAlloc`
            MEM_RELEASE);
    }

    Buffer->Width = Width;
    Buffer->Height = Height;

    int BytesPerPixel = 4;

    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;

    // negative for top-down DIB, meaning the first three bytes of the image
    // are the color of the top left pixel in the beatmap
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = Buffer->Width * Buffer->Height * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    // how many bytes the pointer should go for the next row
    Buffer->Pitch = Width * BytesPerPixel;

    // TODO: probably clear this to black
}

internal void
Win32DisplayBufferInWindow(
    win32_offscreen_buffer *Buffer,
    HDC DeviceContext,
    int WindowWidth,
    int WindowHeight)
{
    // TODO: correct the aspect ration
    StretchDIBits(
        DeviceContext,
        0, 0, WindowWidth, WindowHeight,   // destination
        0, 0, Buffer->Width, Buffer->Height, // source
        Buffer->Memory,
        &Buffer->Info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

LRESULT CALLBACK
Win32MainWindowCallback(
    HWND Window,
    UINT Message,
    WPARAM WParam,
    LPARAM LParam)
{
    LRESULT Result = 0;

    switch (Message) {
        case WM_SIZE:
        {
        } break;

        case WM_DESTROY:
        {
            // TODO: handle this as an error - recreate window?
            GlobalRunning = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 31)) == 0);

            if(WasDown != IsDown)  // eat key-repeat (press and hold)
            {
                if(VKCode == 'W')
                {
                }
                else if(VKCode == 'A')
                {
                }
                else if(VKCode == 'S')
                {
                }
                else if(VKCode == 'D')
                {
                }
                else if(VKCode == 'Q')
                {
                }
                else if(VKCode == 'E')
                {
                }
                else if(VKCode == VK_UP)
                {
                }
                else if(VKCode == VK_LEFT)
                {
                }
                else if(VKCode == VK_DOWN)
                {
                }
                else if(VKCode == VK_RIGHT)
                {
                }
                else if(VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if(IsDown)
                    {
                        OutputDebugStringA("IsDown");
                    }
                    if(WasDown)
                    {
                        OutputDebugStringA("WasDown");
                    }
                    OutputDebugStringA("\n");
                }
                else if(VKCode == VK_SPACE)
                {
                }
                else
                {
                }
            }

            bool AltKeyWasDown = ((LParam & (1 << 29)) != 0);
            if((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
        } break;

        case WM_CLOSE:
        {
            // TODO: handle this with a message to the user?
            GlobalRunning = false;
        } break;

        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;

        case WM_PAINT:
        {
            // Windows asks us explicitly to repaint
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);

            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            Win32DisplayBufferInWindow(
                &GlobalBackBuffer,
                DeviceContext,
                Dimension.Width,
                Dimension.Height);
            EndPaint(Window, &Paint);
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}

int CALLBACK
WinMain(
    HINSTANCE Instance,
    HINSTANCE PrevInstance,
    LPSTR CommandLine,
    int ShowCode)
{
    Win32LoadXInput();

    WNDCLASSA WindowClass = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    // CS_HREDRAW | CS_VREDRAW - redraw the whole window on resize
    // CS_OWNDC                - each window has its own device context
    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = Win32MainWindowCallback; // what to call for message processing
    WindowClass.hInstance = Instance;                  // in which process is the callback
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    // Windows will make its own copy of WindowClass
    // hence - no need to worry about it going out of scope
    // not true for any object you pass by pointer
    if (RegisterClass(&WindowClass)) {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            // WS_OVERLAPPEDWINDOW - the window has title bar, close cl
            // WS_VISIBLE          - visible on startup
            WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        if (Window) {
            // ask Windows to give us a handle so we can issue draw calls
            //
            // since we specified CS_OWNDC, we can get one device context
            // and use it forever as it is not shared
            HDC DeviceContext = GetDC(Window);

            // graphics test
            int XOffset = 0;
            int YOffset = 0;

            // sound test
            int SamplesPerSecond = 48000;
            int ToneHz = 256;
            uint32 RunningSampleIndex = 0;
            int SquareWaveCounter = 0;
            int SquareWavePeriod = SamplesPerSecond/ToneHz;
            int HalfSquareWavePeriod = SquareWavePeriod/2;
            int BytesPerSample = sizeof(int16)*2;
            int SecondaryBufferSize = SamplesPerSecond*BytesPerSample;

            Win32InitDSound(Window, SamplesPerSecond, SecondaryBufferSize);

            GlobalRunning = true;
            while(GlobalRunning) {
                MSG Message;
                while(PeekMessage(
                    &Message,
                    0,        // any window which belongs to us
                    0,        // get all messages
                    0,        // get all messages
                    PM_REMOVE // remove message from queue
                ))
                {
                    if(Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }

                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }

                // TODO: should we poll this more frequently?
                for(DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ++ControllerIndex)
                {
                    XINPUT_STATE ControllerState;
                    if(XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // this controller is plugged in
                        // TODO: see if `ControllerState.dwPacketNumber` increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Bacl = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);

                        int16 StickX = Pad->sThumbLX;
                        int16 StickY = Pad->sThumbLY;

                        XOffset += StickX >> 12;
                        YOffset += StickY >> 12;
                    }
                    else
                    {
                        // the controller is not available
                    }
                }

                RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

                // DirectSound output test

                // the buffer is ...
                // [int16 int16] int16 int16 ...
                // [LEFT  RIGHT] LEFT  RIGHT ...
                //  ^^^^^^^^^^^ is a single sample

                DWORD PlayCursor;
                DWORD WriteCursor;
                if(SUCCEEDED(GlobalSecondaryBuffer->lpVtbl->GetCurrentPosition(
                    GlobalSecondaryBuffer,
                    &PlayCursor,
                    &WriteCursor)))
                {
                    DWORD ByteToLock = RunningSampleIndex*BytesPerSample % SecondaryBufferSize;
                    DWORD BytesToWrite;

                    if(ByteToLock > PlayCursor)
                    {
                        BytesToWrite = (SecondaryBufferSize - ByteToLock);
                        BytesToWrite += PlayCursor;
                    }
                    else
                    {
                        BytesToWrite = PlayCursor - ByteToLock;
                    }

                    VOID *Region1;
                    DWORD Region1Size;
                    VOID *Region2;
                    DWORD Region2Size;

                    if(SUCCEEDED(GlobalSecondaryBuffer->lpVtbl->Lock(
                        GlobalSecondaryBuffer,
                        ByteToLock,
                        BytesToWrite,
                        &Region1,
                        &Region1Size,
                        &Region2,
                        &Region2Size,
                        0 // flags
                    )))
                    {
                        // TODO: assert that Region1Size & Region2Size are valid
                        DWORD Region1SampleCount = Region1Size/BytesPerSample;
                        int16 *SampleOut = (int16 *)Region1;

                        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? 16000 : -16000;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }

                        DWORD Region2SampleCount = Region2Size/BytesPerSample;
                        SampleOut = (int16 *)Region2;

                        for(DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; ++SampleIndex)
                        {
                            int16 SampleValue = ((RunningSampleIndex++ / HalfSquareWavePeriod) % 2) ? 16000 : -16000;
                            *SampleOut++ = SampleValue;
                            *SampleOut++ = SampleValue;
                        }
                    }
                }

                win32_window_dimension Dimension = Win32GetWindowDimension(Window);

                Win32DisplayBufferInWindow(
                    &GlobalBackBuffer,
                    DeviceContext,
                    Dimension.Width,
                    Dimension.Height);
            }
        } else {
            // TODO: logging
        }
    } else {
        // TODO: logging
    }

    return(0);
}
