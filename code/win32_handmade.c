#include <windows.h>
#include <stdint.h>

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
	BITMAPINFO Info;
	void *Memory;
	int Width;
	int Height;
	int Pitch;
	int BytesPerPixel;
} win32_offscreen_buffer;

// todo: global for now
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

typedef struct win32_window_dimension
{
	int Width;
	int Height;
} win32_window_dimension;

win32_window_dimension
Win32GetWindowDimension(HWND Window)
{
	win32_window_dimension Result;
	
	RECT ClientRect;
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;
	
	return(Result);
}

internal void
RenderWeirdGradient(
	win32_offscreen_buffer Buffer,
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

	// todo: see what the optimizer does, and whether Buffer should be a pointer
	uint8 *Row = (uint8 *)Buffer.Memory;
	for (int Y = 0; Y < Buffer.Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer.Width; ++X)
		{
			/*
			Pixel (32-bits)

				   0  1  2  3  - byte order number (Pixel + 1, etc.)
			memory:   BB GG RR xx
			register: xx RR GG BB
			*/
			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);
			*Pixel++ = (Green << 8) | Blue;
		}
		
		Row += Buffer.Pitch;
	}

}

internal void
Win32ResizeDIBSection(
	win32_offscreen_buffer *Buffer,
	int Width,
	int Height)
{
	// todo: maybe don't free first, free after, then free first if that fails
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
	Buffer->BytesPerPixel = 4;

	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	
	// negative for top-down DIB, meaning the first three bytes of the image
	// are the color of the top left pixel in the beatmap
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BitmapMemorySize = Buffer->Width * Buffer->Height * Buffer->BytesPerPixel;
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);

	// how many bytes the pointer should go for the next row
	Buffer->Pitch = Width * Buffer->BytesPerPixel;

	// todo: probably clear this to black
}

internal void
Win32DisplayBufferInWindow(
	HDC DeviceContext,
	int WindowWidth,
	int WindowHeight,
	win32_offscreen_buffer Buffer,
	int X,
	int Y,
	int Width,
	int Height)
{
	// todo: correct the aspect ration
	StretchDIBits(
		DeviceContext,
		0, 0, WindowWidth, WindowHeight,   // destination
		0, 0, Buffer.Width, Buffer.Height, // source
		Buffer.Memory,
		&Buffer.Info,
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
			// todo: handle this as an error - recreate window?
			GlobalRunning = false;
		} break;

		case WM_CLOSE:
		{
			// todo: handle this with a message to the user?
			GlobalRunning = false;
		} break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;

		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);

			int X = Paint.rcPaint.left;
			int Y = Paint.rcPaint.top;
			int Width = Paint.rcPaint.right - Paint.rcPaint.left;
			int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

			win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			Win32DisplayBufferInWindow(
				DeviceContext,
				Dimension.Width,
				Dimension.Height,
				GlobalBackBuffer,
				X,
				Y,
				Width,
				Height);
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
	WNDCLASS WindowClass = {};
	
	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW;       // redraw the whole window on resize
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
			int XOffset = 0;
			int YOffset = 0;

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

				RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);

				HDC DeviceContext = GetDC(Window);
				
				win32_window_dimension Dimension = Win32GetWindowDimension(Window);

				Win32DisplayBufferInWindow(
					DeviceContext,
					Dimension.Width,
					Dimension.Height,
					GlobalBackBuffer,
					0,
					0,
					Dimension.Width,
					Dimension.Height);

				ReleaseDC(Window, DeviceContext);

				++XOffset;
			}
		} else {
			// todo: logging
		}
	} else {
		// todo: logging
	}

	return(0);
}
