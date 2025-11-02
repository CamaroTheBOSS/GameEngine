#include "main_header.h"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>

#if defined(HANDMADE_INTERNAL_BUILD)
LPVOID MEM_ALLOC_START = reinterpret_cast<void*>(TB(static_cast<u64>(10)));
#else
LPVOID MEM_ALLOC_START = reinterpret_cast<void*>(0);
#endif

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
static LARGE_INTEGER globalPerformanceFreq;

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

struct Win32GameCode {
	const char* pathToDll = "program_layer.dll";
	const char* pathToTempDll = "program_layer_temp.dll";

	HMODULE dll = nullptr;
	u64 lastWriteTimestamp = 0;
	bool isValid = false;

	game_main_loop_frame* GameMainLoopFrame = GameMainLoopFrameStub;
};

internal
bool Win32LoadGameCode(Win32GameCode& gameCode) {
	bool success = CopyFileA(gameCode.pathToDll, gameCode.pathToTempDll, false);
	gameCode.dll = LoadLibraryA(gameCode.pathToTempDll);
	if (gameCode.dll) {
		gameCode.GameMainLoopFrame = reinterpret_cast<game_main_loop_frame*>(GetProcAddress(gameCode.dll, "GameMainLoopFrame"));
		gameCode.isValid = gameCode.GameMainLoopFrame != nullptr;
		Assert(gameCode.isValid);
	}
	if (!gameCode.isValid) {
		gameCode.GameMainLoopFrame = GameMainLoopFrameStub;
	}
	return success;
}

internal
void Win32UnloadGameCode(Win32GameCode& gameCode) {
	if (gameCode.dll) {
		Assert(FreeLibrary(gameCode.dll));
		gameCode.dll = nullptr;
	}
	Assert(!gameCode.dll);
	gameCode.GameMainLoopFrame = GameMainLoopFrameStub;
	gameCode.isValid = false;
}

internal
u64 Win32GetLastWriteTime(const char* filename) {
	struct _stat64i32 stats;
	_stat(filename, &stats);
	return stats.st_mtime;
}

internal
bool Win32ReloadGameCode(Win32GameCode& gameCode) {
#if 0
	WIN32_FILE_ATTRIBUTE_DATA data;
	GetFileAttributesExA(gameCode.pathToDll, GetFileExInfoStandard, &data);
	if (CompareFileTime(&data.ftLastWriteTime, &gameCode.fileStatsData.ftLastWriteTime)) {
		Win32UnloadGameCode(gameCode);
		Win32LoadGameCode(gameCode);
		gameCode.fileStatsData.ftLastWriteTime = data.ftLastWriteTime;
	}
#else
	u64 lastWriteTime = Win32GetLastWriteTime(gameCode.pathToDll);
	if (lastWriteTime != gameCode.lastWriteTimestamp) {
		Win32UnloadGameCode(gameCode);
		if (Win32LoadGameCode(gameCode)) {
			gameCode.lastWriteTimestamp = lastWriteTime;
		}
	}
#endif
	return true;
}

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
	
	REFERENCE_TIME requestedDuration = 500000;  // NOTE: 1'000'000 * 100ns = 0.1sec
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

FileData DebugReadEntireFile(const char* filename) {
	HANDLE file = CreateFileA(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (!file) {
		// TODO: LOGGING
		return FileData{ nullptr, 0 };
	}
	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(file, &fileSize)) {
		// TODO: LOGGING
		CloseHandle(file);
		return FileData{ nullptr, 0 };
	}
	// NOTE: For now support only reading files up to 4gb
	Assert(fileSize.QuadPart < UINT32_MAX);
	void* fileContent = VirtualAlloc(nullptr, fileSize.QuadPart, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (!fileContent) {
		// TODO: LOGGING
		CloseHandle(file);
		return FileData{ nullptr, 0 };
	}
	DWORD readBytes = 0;
	if (!ReadFile(file, fileContent, static_cast<DWORD>(fileSize.QuadPart), &readBytes, nullptr) && readBytes != fileSize.QuadPart) {
		// TODO: LOGGING
		VirtualFree(fileContent, 0, MEM_RELEASE);
		CloseHandle(file);
		return FileData{ nullptr, 0 };
	}
	CloseHandle(file);
	return FileData{ fileContent, static_cast<u64>(fileSize.QuadPart) };
}

bool DebugWriteToFile(const char* filename, void* buffer, u64 size) {
	HANDLE file = CreateFileA(filename, GENERIC_WRITE, FILE_SHARE_WRITE, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (!file) {
		// TODO: LOGGING
		return false;
	}
	// NOTE: For now support only reading files up to 4gb
	Assert(size < UINT32_MAX);
	DWORD writtenBytes = 0;
	if (!WriteFile(file, buffer, static_cast<DWORD>(size), &writtenBytes, nullptr) && writtenBytes != size) {
		// TODO: LOGGING
		CloseHandle(file);
		return false;
	}
	CloseHandle(file);
	return true;
}

void DebugFreeFile(FileData& file) {
	VirtualFree(file.content, 0, MEM_RELEASE);
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
		Assert(!"User input should be collected in Win32ProcessOSMessages directly!");
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
void Win32ProcessOSMessages(InputData& inputData) {
	MSG msg = {};
	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
		case WM_QUIT: {
			globalRunning = false;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			u32 vkCode = static_cast<u32>(msg.wParam);
			bool wasDown = msg.lParam & (static_cast<LPARAM>(1) << 30);
			bool isDown = !(msg.lParam & (static_cast<LPARAM>(1) << 31));
			if (vkCode == 'W') {
				inputData.isWDown = isDown;
			}
			else if (vkCode == 'A') {
				inputData.isADown = isDown;
			}
			else if (vkCode == 'S') {
				inputData.isSDown = isDown;
			}
			else if (vkCode == 'D') {
				inputData.isDDown = isDown;
			}
			else if (vkCode == VK_UP) {
				inputData.isUpDown = isDown;
			}
			else if (vkCode == VK_LEFT) {
				inputData.isLeftDown = isDown;
			}
			else if (vkCode == VK_DOWN) {
				inputData.isDownDown = isDown;
			}
			else if (vkCode == VK_RIGHT) {
				inputData.isRightDown = isDown;
			}
			else if (vkCode == VK_SPACE) {
				inputData.isSpaceDown = isDown;
			}
			else if (vkCode == VK_ESCAPE) {
				inputData.isEscDown = isDown;
			}
		} break;
		default: {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		}
	}
}

internal
void Win32GatherGamepadInput(InputData& inputData) {
	for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; controllerIndex++) {
		XINPUT_STATE state = {};
		auto errCode = XInputGetState(controllerIndex, &state);
		if (errCode != ERROR_SUCCESS) {
			//TODO log errCode with error listed in Winerror.h
			continue;
		}
		inputData.isWDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
		inputData.isSDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
		inputData.isADown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
		inputData.isDDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
		inputData.isSpaceDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_START;
		inputData.isEscDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
		inputData.isLeftDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_A;
		inputData.isRightDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_B;
		inputData.isDownDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_X;
		inputData.isUpDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_Y;
	}
}

inline internal
u64 Win32GetCurrentTimestamp() {
	LARGE_INTEGER timestamp = {};
	QueryPerformanceCounter(&timestamp);
	return timestamp.QuadPart;
}

inline internal
f32 Win32CalculateTimeElapsed(u64 startTime, u64 endTime) {
	return static_cast<f32>(endTime - startTime) / static_cast<f32>(globalPerformanceFreq.QuadPart);
}

int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prevInstance,
	LPSTR cmdLine,
	int showCmd
) {
	Win32LoadXInput();

	HRESULT hr = CoInitialize(nullptr);
	if (!SUCCEEDED(hr)) {
		// TODO log error with _com_error err.ErrorMessage()
		return 0;
	}
	Win32InitAudioClient();

	globalSoundData.audio->Start();
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

	// PART: Initializing program memory
	ProgramMemory programMemory = {};
	programMemory.debugFreeFile = DebugFreeFile;
	programMemory.debugReadEntireFile = DebugReadEntireFile;
	programMemory.debugWriteFile = DebugWriteToFile;
	programMemory.permanentMemorySize = MB(64);
	programMemory.transientMemorySize = GB(static_cast<u64>(4));
	programMemory.permanentMemory = VirtualAlloc(
		MEM_ALLOC_START,
		programMemory.permanentMemorySize + programMemory.transientMemorySize,
		MEM_RESERVE | MEM_COMMIT, 
		PAGE_READWRITE
	);
	programMemory.transientMemory = reinterpret_cast<void*>(
		reinterpret_cast<u8*>(programMemory.permanentMemory) + programMemory.permanentMemorySize
	);
	if (!programMemory.permanentMemory || !programMemory.transientMemory) {
		// TODO log error
		DWORD err = GetLastError();
		return 0;
	}
	Assert(programMemory.permanentMemorySize >= sizeof(ProgramState));
	ProgramState* state = reinterpret_cast<ProgramState*>(programMemory.permanentMemory);
	state->toneHz = 255.;
	Win32GameCode gameCode = {};
	SoundData soundData = {};

	// NOTE: We can use one devicecontext because we specified CS_OWNDC so we dont share context with anyone
	HDC deviceContext = GetDC(window);
	Win32ResizeBitmapMemory(globalBitmap, 1400, 900);
	globalRunning = true;

	UINT schedulerGranularityMs = 1;
	bool sleepIsGranular = timeBeginPeriod(schedulerGranularityMs) == NO_ERROR;
	u32 frameRefreshHz = 60;
	f32 targetFrameRefreshSeconds = 1.0f / static_cast<f32>(frameRefreshHz);
	u32 soundSamplesToWriteEachFrame = 12u + static_cast<u32>(globalSoundData.dataFormat.Format.nSamplesPerSec * targetFrameRefreshSeconds);
	u64 rdtscStart = __rdtsc();
	u64 frameStartTime = Win32GetCurrentTimestamp();
	QueryPerformanceFrequency(&globalPerformanceFreq);
	while (globalRunning) {
		Win32ReloadGameCode(gameCode);
		Win32ProcessOSMessages(globalInputData);
		Win32GatherGamepadInput(globalInputData);

		// PART: Preparing SoundData structure for game main loop
		UINT32 padding = 0;
		globalSoundData.audio->GetCurrentPadding(&padding);
		UINT32 framesAvailable = globalSoundData.bufferSizeInSamples - padding;
		if (framesAvailable > soundSamplesToWriteEachFrame) {
			framesAvailable = soundSamplesToWriteEachFrame;
		}
		if (framesAvailable == 0) {
			// TODO: log error
		}
		hr = globalSoundData.renderer->GetBuffer(framesAvailable, reinterpret_cast<BYTE**>(&soundData.data));
		if (!SUCCEEDED(hr)) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
		}
		soundData.nSamples = framesAvailable;
		soundData.nSamplesPerSec = globalSoundData.dataFormat.Format.nSamplesPerSec;
		soundData.nChannels = globalSoundData.dataFormat.Format.nChannels;

		// PART: Game main loop
		gameCode.GameMainLoopFrame(programMemory, globalBitmap, soundData, globalInputData);

	
		// PART: Timing stuff
		u64 rdtscEnd = __rdtsc();
		u64 frameEndTime = Win32GetCurrentTimestamp();
		f32 secondsElapsedForFrame = Win32CalculateTimeElapsed(frameStartTime, frameEndTime);
		f32 desiredSleepTimeMs = 1000.0f * (targetFrameRefreshSeconds - secondsElapsedForFrame) - 5;
		if (sleepIsGranular && desiredSleepTimeMs > schedulerGranularityMs) {
			u64 start = Win32GetCurrentTimestamp();
			Sleep(static_cast<DWORD>(desiredSleepTimeMs));
			u64 end = Win32GetCurrentTimestamp();
			f32 elapsed = Win32CalculateTimeElapsed(start, end);
			frameEndTime = Win32GetCurrentTimestamp();
			secondsElapsedForFrame = Win32CalculateTimeElapsed(frameStartTime, frameEndTime);
			Assert(secondsElapsedForFrame < targetFrameRefreshSeconds);
		}
		while (secondsElapsedForFrame < targetFrameRefreshSeconds) {
			YieldProcessor();
			frameEndTime = Win32GetCurrentTimestamp();
			secondsElapsedForFrame = Win32CalculateTimeElapsed(frameStartTime, frameEndTime);
		}
		float msElapsed = 1'000.f * secondsElapsedForFrame;
		float fps = 1'000.f / msElapsed;
		float megaCycles = static_cast<float>(rdtscEnd - rdtscStart) / 1'000'000.f;
		rdtscStart = rdtscEnd;
		frameStartTime = frameEndTime;
		char buffer[256];
		sprintf_s(buffer, "%0.2fms/f,  %0.2ffps/f,  %0.2fMc/f,   frames_av %d\n", msElapsed, fps, megaCycles, framesAvailable);
		OutputDebugStringA(buffer);

		// PART: Displaying window
		auto dim = GetWindowDimension(window);
		Win32DisplayWindow(deviceContext, globalBitmap, dim.width, dim.height);
		ReleaseDC(window, deviceContext);

		// PART: Playing sound
		hr = globalSoundData.renderer->ReleaseBuffer(framesAvailable, 0);
		if (!SUCCEEDED(hr)) {
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
		}
	}

	return 0;
}