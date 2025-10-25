#include "main_header.h"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>

#include "program_layer.cpp"

struct SoundRenderData {
	IMMDevice* device;
	IAudioClient* audio;
	IAudioRenderClient* renderer;
	WAVEFORMATEX* mixFormat;
	WAVEFORMATEXTENSIBLE dataFormat;
	REFERENCE_TIME requestedDuration;
	float tSine = 0;
	int bufferSizeInSamples;
	u64 runningSampleIndex = 0;
};

noapi 
struct ScreenDimension {
	int width;
	int height;
};

static bool globalRunning;
static BitmapData globalBitmap;
static BITMAPINFO globalBitmapInfo;
static SoundRenderData globalSoundData;
static InputData globalInputData;

/* Some explanation on this code (for dynamic loading XInputGet/SetState() functions from XInput lib):
	1. Defines are defining actual signature of these functions
	2. Typedefs defines types for these functions, now we can declare pointer with such type as these functions
	3. Then we define stubs with the same signature, so dummy functions which do nothing in case functions we're not found
	4. We declare global ptrs which will be used as our functions, but initialize them with dummy stubs at first
	5. Last stem is to define normal name for the function like original names so we can use them with these defines
	6. We need also to call Win32LoadXInput() at the start of the program to replace stubs with actual implementation
	if dlls exists
-------------------------------------------------------------------------------*/
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return 1; }
X_INPUT_SET_STATE(XInputSetStateStub) { return 1; }
static x_input_get_state* XInputGetStatePtr = XInputGetStateStub;
static x_input_set_state* XInputSetStatePtr = XInputSetStateStub;
#define XInputGetState XInputGetStatePtr
#define XInputSetState XInputSetStatePtr

internal
void Win32LoadXInput() {
	HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (xInputLibrary) {
		XInputGetState = reinterpret_cast<x_input_get_state*>(GetProcAddress(xInputLibrary, "XInputGetState"));
		XInputSetState = reinterpret_cast<x_input_set_state*>(GetProcAddress(xInputLibrary, "XInputSetState"));
	}
}
/* -----------------------------------------------------------------------------*/
internal
void Win32InitAudioClient() {
	// TODO: Decrease latency, current latency=22ms which is greater than 16ms

	// TODO: FIX MEMORY LEAKS WHEN TAKING EARLY RETURN
	IMMDeviceEnumerator* deviceEnum = nullptr;
	HRESULT hr = CoCreateInstance(
		__uuidof(MMDeviceEnumerator), 
		0,
		CLSCTX_ALL, 
		__uuidof(IMMDeviceEnumerator),
		reinterpret_cast<void**>(&deviceEnum)
	);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	IMMDevice* renderDevice = nullptr;
	hr = deviceEnum->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &renderDevice);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	IAudioClient3* audioClient = nullptr;
	void** clientPtr = reinterpret_cast<void**>(&audioClient);
	hr = renderDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, 0, clientPtr);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}
	hr = renderDevice->Activate(__uuidof(IAudioClient2), CLSCTX_ALL, 0, clientPtr);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}
	hr = renderDevice->Activate(__uuidof(IAudioClient3), CLSCTX_ALL, 0, clientPtr);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}


	WAVEFORMATEX* mixFormat = nullptr;
	hr = audioClient->GetMixFormat(&mixFormat);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	// ?????? doesnt work
	REFERENCE_TIME min = 1080;
	REFERENCE_TIME max = 2000;
	hr = audioClient->GetBufferSizeLimits(mixFormat, 0, &min, &max);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		_com_error err(hr);
		LPCTSTR errMsg = err.ErrorMessage();
		min = -1;
		max = -1;
	}

	UINT32 defaultPeriod = 0;
	UINT32 fundamentalPeriod = 0;
	UINT32 minPeriod = 0;
	UINT32 maxPeriod = 0;
	// TODO: Check InitializeSharedAudioStream() and do audio streaming myself
	hr = audioClient->GetSharedModeEnginePeriod(mixFormat, &defaultPeriod, &fundamentalPeriod, &minPeriod, &maxPeriod);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		_com_error err(hr);
		LPCTSTR errMsg = err.ErrorMessage();
	}
	
	REFERENCE_TIME requestedDuration = 1;  // NOTE: 1'000'000 * 100ns = 0.1sec
	hr = audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		0,
		requestedDuration,
		0,		// NOTE: always 0 in shared mode
		mixFormat,
		0
	);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}
	IAudioRenderClient* renderClient = nullptr;
	hr = audioClient->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(&renderClient));
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	WAVEFORMATEXTENSIBLE format = {};
	if (mixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
		format = *reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mixFormat);
	}
	else {
		format.Format = *mixFormat;
		format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
		INIT_WAVEFORMATEX_GUID(&format.SubFormat, mixFormat->wFormatTag);
		format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;
		format.dwChannelMask = 0;
	}

	UINT32 bufferSizeInSamples;
	hr = audioClient->GetBufferSize(&bufferSizeInSamples);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	REFERENCE_TIME latency;
	hr = audioClient->GetStreamLatency(&latency);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return;
	}

	globalSoundData.audio = audioClient;
	globalSoundData.device = renderDevice;
	globalSoundData.mixFormat = mixFormat;
	globalSoundData.dataFormat = std::move(format);
	globalSoundData.renderer = renderClient;
	globalSoundData.requestedDuration = requestedDuration;
	globalSoundData.bufferSizeInSamples = bufferSizeInSamples;
}

internal
ScreenDimension GetWindowDimension(HWND window) {
	RECT clientRect = {};
	GetClientRect(window, &clientRect);
	return {
		clientRect.right - clientRect.left,
		clientRect.bottom - clientRect.top
	};
}

internal
void Win32ResizeBitmapMemory(BitmapData& bitmap, int newWidth, int newHeight) {
	bitmap.width = newWidth;
	bitmap.height = newHeight;
	bitmap.bytesPerPixel = 4;
	bitmap.pitch = bitmap.bytesPerPixel * bitmap.width;

	if (bitmap.data) {
		VirtualFree(bitmap.data, 0, MEM_RELEASE);
	}

	globalBitmapInfo.bmiHeader.biSize = sizeof(globalBitmapInfo.bmiHeader);
	globalBitmapInfo.bmiHeader.biWidth = bitmap.width;
	globalBitmapInfo.bmiHeader.biHeight = -bitmap.height;
	globalBitmapInfo.bmiHeader.biPlanes = 1;
	globalBitmapInfo.bmiHeader.biBitCount = 32;
	globalBitmapInfo.bmiHeader.biCompression = BI_RGB;

	int allocSize = 4 * newWidth * newHeight; // NOTE: align each pixel to DWORD
	bitmap.data = VirtualAlloc(0, allocSize, MEM_COMMIT, PAGE_READWRITE);
}

internal
void Win32DisplayWindow(HDC deviceContext, BitmapData bitmap, int width, int height) {
	StretchDIBits(
		deviceContext,
		0, 0, width, height,
		0, 0, bitmap.width, bitmap.height,
		bitmap.data,
		&globalBitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

internal
LRESULT CALLBACK Win32MainWindowCallback(
	HWND window,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam
) {
	LRESULT result = 0;
	switch (msg) {
	case WM_SIZE: {
		/*auto dim = GetWindowDimension(window);
		Win32ResizeBitmapMemory(globalBitmap, dim.width, dim.height);*/
	} break;
	case WM_CLOSE:
	case WM_DESTROY: {
		globalRunning = false;
	} break;
	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC deviceContext = BeginPaint(window, &paint);
		auto dim = GetWindowDimension(window);
		Win32DisplayWindow(deviceContext, globalBitmap, dim.width, dim.height);
		EndPaint(window, &paint);

	} break;
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP: 
	case WM_KEYDOWN: 
	case WM_KEYUP: {
		u32 vkCode = wParam;
		bool wasDown = lParam & (static_cast<LPARAM>(1) << 30);
		bool isDown = !(lParam & (static_cast<LPARAM>(1) << 31));
		if (wParam == 'Q') {

		}
		if (vkCode == 'W') {
			globalInputData.isWDown = isDown;
			if (wasDown) {
				OutputDebugStringA("was down W\n");
			}
			else if (isDown) {
				OutputDebugStringA("is down W\n");
			}
		}
		else if (vkCode == 'A') {
			globalInputData.isADown = isDown;
		}
		else if (vkCode == 'S') {
			globalInputData.isSDown = isDown;
		}
		else if (vkCode == 'D') {
			globalInputData.isDDown = isDown;
		}
		else if (vkCode == VK_UP) {
			globalInputData.isUpDown = isDown;
		}
		else if (vkCode == VK_LEFT) {

		}
		else if (vkCode == VK_DOWN) {
			globalInputData.isDownDown = isDown;
		}
		else if (vkCode == VK_RIGHT) {

		}
		else if (vkCode == VK_SPACE) {

		}
		else if (vkCode == VK_ESCAPE) {

		}
	} break;
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("Window activated\n");
	} break;
	default: {
		return DefWindowProcA(window, msg, wParam, lParam);
	}
	}
	return result;
}

internal
int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prevInstance,
	LPSTR cmdLine,
	int showCmd
) {
	Win32LoadXInput();

	CoInitialize(nullptr);
	Win32InitAudioClient();

	HRESULT hr = globalSoundData.audio->Start();
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return 0;
	}

	WNDCLASSEXA windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	windowClass.lpfnWndProc = Win32MainWindowCallback;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = "HandmadeWindowClass";

	if (!RegisterClassExA(&windowClass)) {
		//TODO log GetLastError()
		//std::cout << "Error when registering class: " << GetLastError() << std::endl;
		return -1;
	}
	HWND window = CreateWindowExA(
		0,
		windowClass.lpszClassName,
		"Handmade application",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		0,
		0,
		instance,
		0
	);
	if (!window) {
		//TODO log GetLastError()
		return -1;
	}

	// NOTE: We can use one devicecontext because we specified CS_OWNDC so we dont share context with anyone
	HDC deviceContext = GetDC(window);
	Win32ResizeBitmapMemory(globalBitmap, 1400, 900);
	globalRunning = true;
	int xOffset = 0;
	int yOffset = 0;
	LARGE_INTEGER performanceCounterStart = {};
	LARGE_INTEGER performanceFrequency = {};
	u64 rdtscStart = __rdtsc();
	QueryPerformanceCounter(&performanceCounterStart);
	QueryPerformanceFrequency(&performanceFrequency);
	while (globalRunning) {

		// PART: Processing all messages from windows OS
		MSG msg = {};
		while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				globalRunning = false;
			}
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		// PART: Gathering user gamepad input
		for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++) {
			XINPUT_STATE state = {};
			auto errCode = XInputGetState(controllerIndex, &state);
			if (errCode != ERROR_SUCCESS) {
				//TODO log errCode with error listed in Winerror.h
				continue;
			}
			bool btnUp = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
			bool btnDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
			bool btnLeft = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
			bool btnRight = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
			bool btnStart = state.Gamepad.wButtons & XINPUT_GAMEPAD_START;
			bool btnBack = state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
			bool btnA = state.Gamepad.wButtons & XINPUT_GAMEPAD_A;
			bool btnB = state.Gamepad.wButtons & XINPUT_GAMEPAD_B;
			bool btnX = state.Gamepad.wButtons & XINPUT_GAMEPAD_X;
			bool btnY = state.Gamepad.wButtons & XINPUT_GAMEPAD_Y;
			if (btnUp) {
				yOffset++;
			}
		}
		// PART: Preparing SoundData structure for game main loop
		UINT32 padding = 0;
		globalSoundData.audio->GetCurrentPadding(&padding);
		UINT32 framesAvailable = globalSoundData.bufferSizeInSamples - padding;
		if (framesAvailable == 0) {
			// TODO: log error
		}
		SoundData soundData = {};
		hr = globalSoundData.renderer->GetBuffer(framesAvailable, reinterpret_cast<BYTE**>(&soundData.data));
		if (!SUCCEEDED(hr)) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
		}
		soundData.nSamples = framesAvailable;
		soundData.nSamplesPerSec = globalSoundData.dataFormat.Format.nSamplesPerSec;
		soundData.nChannels = globalSoundData.dataFormat.Format.nChannels;

		// PART: Game main loop
		GameMainLoopFrame(globalBitmap, soundData, globalInputData);

		// PART: Playing sound
		hr = globalSoundData.renderer->ReleaseBuffer(framesAvailable, 0);
		if (!SUCCEEDED(hr)) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
		}

		// PART: Displaying window
		auto dim = GetWindowDimension(window);
		Win32DisplayWindow(deviceContext, globalBitmap, dim.width, dim.height);
		ReleaseDC(window, deviceContext);

		// PART: Timing stuff
		u64 rdtscEnd = __rdtsc();
		LARGE_INTEGER performanceCounterEnd = {};
		QueryPerformanceCounter(&performanceCounterEnd);
		float msElapsed = 1'000. * static_cast<float>(performanceCounterEnd.QuadPart - performanceCounterStart.QuadPart) / static_cast<float>(performanceFrequency.QuadPart);
		float fps = 1'000 / msElapsed;
		float megaCycles = static_cast<float>(rdtscEnd - rdtscStart) / 1'000'000.;
		rdtscStart = rdtscEnd;
		performanceCounterStart = performanceCounterEnd;
		char buffer[256];
		sprintf(buffer, "%0.2fms/f,  %0.2ffps/f,  %0.2fMc/f\n", msElapsed, fps, megaCycles);
		OutputDebugStringA(buffer);
	}

	return 0;
}