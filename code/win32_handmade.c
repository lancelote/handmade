#include <windows.h>

#define bool int
#define true 1
#define false 0
#define internal static
#define local_persist static
#define global_variable static

// todo: global for now
global_variable bool Running;

LRESULT CALLBACK MainWindowCallback(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam
) {
	LRESULT Result = 0;

	switch(Message) {
		case WM_SIZE:
		{
			OutputDebugStringA("WM_SIZE\n");
		} break;

		case WM_DESTROY:
		{
			// todo: handle this as an error - recreate window?
			Running = false;
		} break;

		case WM_CLOSE:
		{
			// todo: handle this with a message to the user?
			Running = false;
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

			local_persist DWORD Operation = WHITENESS;
			PatBlt(DeviceContext, X, Y, Width, Height, Operation);
			EndPaint(Window, &Paint);
		} break;

		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}

int CALLBACK WinMain(
	HINSTANCE Instance,
	HINSTANCE PrevInstance,
	LPSTR CommandLine,
	int ShowCode
) {
	WNDCLASS WindowClass = {};

	WindowClass.lpfnWndProc = MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&WindowClass)) {
		HWND WindowHandle = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW|WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0
		);
		if (WindowHandle) {
			Running = true;
			while(Running) {
				MSG Message;
				BOOL MessageResult = GetMessageA(
					&Message,
					0, // any Window which belongs to us
					0, // get all messages
					0  // get all messages
				);

				if (MessageResult > 0) {
					TranslateMessage(&Message);
					DispatchMessageA(&Message);
				} else {
					break;
				}
			}
		} else {
			// todo: logging
		}
	} else {
		// todo: logging
	}

	return(0);
}
