#include "engine.h"

#include <windows.h>
#include <Xinput.h>
#include <Audioclient.h>
#include <mmdeviceapi.h>
#include <comdef.h>
#include <sys/stat.h>


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

struct Win32GameCode {
	char pathToDll[MY_MAX_PATH] = "engine.dll";
	char pathToTempDll[MY_MAX_PATH] = "engine_temp.dll";

	HMODULE dll = nullptr;
	u64 lastWriteTimestamp = 0;
	bool isValid = false;
	bool reloaded = false;

	game_main_loop_frame* GameMainLoopFrame = GameMainLoopFrameStub;
};

struct DebugLoopRecord {
	const char* inputFilename = "loop.hmi";
	const char* stateFilename = "loop.hmm";
	HANDLE inputFileHandle = nullptr;
	bool recording;
	bool replaying;
	void* stateMemoryBlock = nullptr;
};

struct Win32State {
	HWND window;

	// TODO change to some structure with X and Y access like screen dim
	u32 bltOffsetX;
	u32 bltOffsetY;
	u32 displayWidth;
	u32 displayHeight;

	char exeFilePath[MY_MAX_PATH];
	char exeDirectory[MY_MAX_PATH];
	char* exeFileName;

	/* Debug state */
	DebugLoopRecord dLoopRecord;
};

noapi 
struct ScreenDimension {
	int width;
	int height;
};

static bool globalRunning;
static BitmapData globalBitmap;
static Win32State globalWin32State;
static BITMAPINFO globalBitmapInfo;
static SoundRenderData globalSoundData;
static LARGE_INTEGER globalPerformanceFreq;
WINDOWPLACEMENT globalWindowPos = { sizeof(globalWindowPos) };

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
void Win32OutputPerformanceCounters(DebugMemory& memory) {
	char buffer[256] = "Debug Counters:\n";
	OutputDebugStringA(buffer);
	for (u32 counterIndex = 0; counterIndex < ArrayCount(memory.performanceCounters); counterIndex++) {
		DebugPerformanceCounters* counter = memory.performanceCounters + counterIndex;
		if (counter->counts == 0) {
			continue;
		}
		sprintf_s(buffer, "\t%d:   %dc,  %dn,  %dc/n\n",
			counterIndex,
			u4(counter->cycles), 
			u4(counter->counts), 
			u4(counter->cycles / counter->counts)
		);
		OutputDebugStringA(buffer);
	}

}

internal
u64 Win32GetLastWriteTime(const char* filename) {
	struct _stat64i32 stats;
	_stat(filename, &stats);
	return stats.st_mtime;
}

internal
bool Win32ReloadGameCode(Win32GameCode& gameCode) {
	u64 lastWriteTime = Win32GetLastWriteTime(gameCode.pathToDll);
	gameCode.reloaded = false;
	if (lastWriteTime != gameCode.lastWriteTimestamp) {
		Win32UnloadGameCode(gameCode);
		if (Win32LoadGameCode(gameCode)) {
			gameCode.lastWriteTimestamp = lastWriteTime;
			gameCode.reloaded = true;
		}
	}
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
		int err = GetLastError();
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
void Win32DebugStartRecordingInput(Win32State& state, ProgramMemory& memory) {
	Assert(!state.dLoopRecord.recording);
	Assert(!state.dLoopRecord.replaying);
	HANDLE inputFile = CreateFileA(state.dLoopRecord.inputFilename, GENERIC_WRITE, FILE_SHARE_WRITE, 0,
		OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	if (!inputFile) {
		// TODO: LOGGING
		return;
	}
	SetEndOfFile(inputFile);
	CopyMemory(state.dLoopRecord.stateMemoryBlock, memory.memoryBlock, memory.memoryBlockSize);
	state.dLoopRecord.inputFileHandle = inputFile;
	state.dLoopRecord.recording = true;
	state.dLoopRecord.replaying = false;
}

internal
void Win32DebugRecordInput(Win32State& state, Controller& data) {
	DWORD bytesWritten;
	Assert(state.dLoopRecord.recording);
	Assert(!state.dLoopRecord.replaying);
	Assert(state.dLoopRecord.inputFileHandle);
	if (!state.dLoopRecord.inputFileHandle) {
		return;
	}
	if (!WriteFile(state.dLoopRecord.inputFileHandle, &data, sizeof(data), &bytesWritten, 0)) {
		// TOOD: LOGGING
		return;
	}
	Assert(bytesWritten == sizeof(data));
}

internal
void Win32DebugEndRecordingInput(Win32State& state) {
	Assert(state.dLoopRecord.recording);
	Assert(!state.dLoopRecord.replaying);
	CloseHandle(state.dLoopRecord.inputFileHandle);
	state.dLoopRecord.inputFileHandle = nullptr;
	state.dLoopRecord.recording = false;
}

internal
void Win32DebugStartReplayingInput(Win32State& state, ProgramMemory& memory) {
	Assert(!state.dLoopRecord.recording);
	Assert(!state.dLoopRecord.replaying);
	HANDLE inputFile = CreateFileA(state.dLoopRecord.inputFilename, GENERIC_READ, FILE_SHARE_READ, 0,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	if (!inputFile) {
		// TODO: LOGGING
		return;
	}
	CopyMemory(memory.memoryBlock, state.dLoopRecord.stateMemoryBlock, memory.memoryBlockSize);
	state.dLoopRecord.inputFileHandle = inputFile;
	state.dLoopRecord.replaying = true;
}

internal
void Win32DebugReplayInput(Win32State& state, ProgramMemory& memory, Controller& data) {
	DWORD bytesRead;
	Assert(!state.dLoopRecord.recording);
	Assert(state.dLoopRecord.replaying);
	Assert(state.dLoopRecord.inputFileHandle);
	if (!state.dLoopRecord.inputFileHandle) {
		return;
	}
	if (!ReadFile(state.dLoopRecord.inputFileHandle, &data, sizeof(data), &bytesRead, 0)) {
		// TODO logging
		return;
	}
	if (bytesRead != sizeof(data)) {
		CopyMemory(memory.memoryBlock, state.dLoopRecord.stateMemoryBlock, memory.memoryBlockSize);
		SetFilePointer(state.dLoopRecord.inputFileHandle, 0, 0, FILE_BEGIN);
		return;
	}
	Assert(bytesRead == sizeof(data));
}

internal
void Win32DebugEndReplayingInput(Win32State& state) {
	Assert(!state.dLoopRecord.recording);
	Assert(state.dLoopRecord.replaying);
	CloseHandle(state.dLoopRecord.inputFileHandle);
	state.dLoopRecord.inputFileHandle = nullptr;
	state.dLoopRecord.replaying = false;
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
	bitmap.pitch = BITMAP_BYTES_PER_PIXEL * bitmap.width;

	if (bitmap.data) {
		VirtualFree(bitmap.data, 0, MEM_RELEASE);
	}

	globalBitmapInfo.bmiHeader.biSize = sizeof(globalBitmapInfo.bmiHeader);
	globalBitmapInfo.bmiHeader.biWidth = bitmap.width;
	globalBitmapInfo.bmiHeader.biHeight = static_cast<int>(bitmap.height);
	globalBitmapInfo.bmiHeader.biPlanes = 1;
	globalBitmapInfo.bmiHeader.biBitCount = 32;
	globalBitmapInfo.bmiHeader.biCompression = BI_RGB;

	int allocSize = 4 * newWidth * newHeight; // NOTE: align each pixel to DWORD
	bitmap.data = VirtualAlloc(0, allocSize, MEM_COMMIT, PAGE_READWRITE);
}

internal
void Win32DisplayWindow(HDC deviceContext, Win32State& state, BitmapData bitmap, int width, int height) {
	// TODO change that to more shipping quality version of fullscreen
	if (state.bltOffsetX || state.bltOffsetY) {
		PatBlt(deviceContext, 0, 0, state.bltOffsetX, 600, BLACKNESS);
		PatBlt(deviceContext, state.bltOffsetX, 0, 1000, state.bltOffsetY, BLACKNESS);
		PatBlt(deviceContext, state.bltOffsetX + bitmap.width, 0, 1920, 1080, BLACKNESS);
		PatBlt(deviceContext, 0, state.bltOffsetY + bitmap.height, 1000, 600, BLACKNESS);
	}
	StretchDIBits(
		deviceContext,
		state.bltOffsetX, state.bltOffsetY, state.displayWidth, state.displayHeight,
		0, 0, bitmap.width, bitmap.height,
		bitmap.data,
		&globalBitmapInfo,
		DIB_RGB_COLORS, SRCCOPY
	);
}

internal
void ToggleFullscreen(HWND window) {
	DWORD style = GetWindowLong(window, GWL_STYLE);
	if (style & WS_OVERLAPPEDWINDOW) {
		MONITORINFO monitorInfo = { sizeof(monitorInfo) };
		if (GetWindowPlacement(window, &globalWindowPos) &&
			GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
			SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(window, HWND_TOP,
				monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED
			);
		}
		ScreenDimension dim = GetWindowDimension(globalWin32State.window);
		// TODO: change to function parameter
		globalWin32State.bltOffsetX = 0;
		globalWin32State.bltOffsetY = 0;
		globalWin32State.displayWidth = dim.width;
		globalWin32State.displayHeight = dim.height;
	}
	else {
		SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(window, &globalWindowPos);
		SetWindowPos(window, NULL, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
			SWP_NOOWNERZORDER | SWP_FRAMECHANGED
		);
		globalWin32State.bltOffsetX = 10;
		globalWin32State.bltOffsetY = 10;
		globalWin32State.displayWidth = globalBitmap.width;
		globalWin32State.displayHeight = globalBitmap.height;
	}
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
		Win32DisplayWindow(deviceContext, globalWin32State, globalBitmap, dim.width, dim.height);
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
void Win32ProcessOSMessages(Win32State& state, ProgramMemory& memory, Controller& controller) {
	MSG msg = {};
	while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
		switch (msg.message) {
		case WM_QUIT: {
			globalRunning = false;
		} break;
		//case WM_MOUSEDOWN:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			u32 vkCode = static_cast<u32>(msg.wParam);
			bool wasDown = msg.lParam & (scast(LPARAM, 1) << 30);
			bool isDown = !(msg.lParam & (scast(LPARAM, 1) << 31));
			bool altIsDown = (msg.lParam & (scast(LPARAM, 1) << 29));
			if (vkCode == 'W') {
				controller.isWDown = isDown;
			}
			else if (vkCode == 'A') {
				controller.isADown = isDown;
			}
			else if (vkCode == 'S') {
				controller.isSDown = isDown;
			}
			else if (vkCode == 'D') {
				controller.isDDown = isDown;
			}
			else if (vkCode == VK_UP) {
				controller.isUpDown = isDown;
			}
			else if (vkCode == VK_LEFT) {
				controller.isLeftDown = isDown;
			}
			else if (vkCode == VK_DOWN) {
				controller.isDownDown = isDown;
			}
			else if (vkCode == VK_RIGHT) {
				controller.isRightDown = isDown;
			}
			else if (vkCode == VK_SPACE) {
				controller.isSpaceDown = isDown;
			}
			else if (vkCode == VK_ESCAPE) {
				controller.isEscDown = isDown;
			}
			else if (vkCode == 'L') {
				if (!wasDown) {
					if (state.dLoopRecord.recording == 0) {
						if (state.dLoopRecord.replaying) {
							Win32DebugEndReplayingInput(state);
						}
						Win32DebugStartRecordingInput(state, memory);
					}
					else {
						if (state.dLoopRecord.recording) {
							Win32DebugEndRecordingInput(state);
						}
						Win32DebugStartReplayingInput(state, memory);
					}
				}
			}
			else if (vkCode == 'P') {
				controller = {};
				if (state.dLoopRecord.replaying) {
					Win32DebugEndReplayingInput(state);
				}
				if (state.dLoopRecord.recording) {
					Win32DebugEndRecordingInput(state);
				}
			}
			else if (vkCode == VK_RETURN && altIsDown) {
				if (!wasDown) {
					ToggleFullscreen(msg.hwnd);
				}
			}
		} break;
		default: {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		}
	}
	POINT point;
	GetCursorPos(&point);
	ScreenToClient(state.window, &point);
	controller.mouseX = f4(point.x - state.bltOffsetX) / state.displayWidth;
	controller.mouseY = 1.f - f4(point.y - state.bltOffsetY) / state.displayHeight;
	controller.isMouseMDown = GetKeyState(VK_MBUTTON) >> 15;
	controller.isMouseRDown = GetKeyState(VK_RBUTTON) >> 15;
	controller.isMouseLDown = GetKeyState(VK_LBUTTON) >> 15;
	controller.isMouse1BDown = GetKeyState(VK_XBUTTON1) >> 15;
	controller.isMouse2BDown = GetKeyState(VK_XBUTTON2) >> 15;
	if (controller.isMouseLDown) {
		int breakhere = 0;
	}
}

internal
void Win32GatherGamepadInput(Controller& controller, DWORD cIndex) {
		XINPUT_STATE state = {};
		auto errCode = XInputGetState(cIndex, &state);
		if (errCode != ERROR_SUCCESS) {
			//TODO log errCode with error listed in Winerror.h
			return;
		}
		controller.isWDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP;
		controller.isSDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
		controller.isADown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
		controller.isDDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
		controller.isSpaceDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_START;
		controller.isEscDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK;
		controller.isLeftDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_A;
		controller.isRightDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_B;
		controller.isDownDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_X;
		controller.isUpDown = state.Gamepad.wButtons & XINPUT_GAMEPAD_Y;
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

internal
ProgramMemory Win32InitProgramMemory(Win32State& state) {
	ProgramMemory programMemory = {};
	programMemory.debug.freeFile = DebugFreeFile;
	programMemory.debug.readEntireFile = DebugReadEntireFile;
	programMemory.debug.writeFile = DebugWriteToFile;
	programMemory.permanentMemorySize = MB(64);
	programMemory.transientMemorySize = GB(static_cast<u64>(3));
	programMemory.memoryBlockSize = programMemory.permanentMemorySize + programMemory.transientMemorySize;
	programMemory.memoryBlock = VirtualAlloc(
		MEM_ALLOC_START,
		programMemory.permanentMemorySize + programMemory.transientMemorySize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
	);
	if (!programMemory.memoryBlock) {
		// TODO log error
		DWORD err = GetLastError();
		return {};
	}
	programMemory.permanentMemory = programMemory.memoryBlock;
	programMemory.transientMemory = reinterpret_cast<void*>(
		reinterpret_cast<u8*>(programMemory.permanentMemory) + programMemory.permanentMemorySize
		);
	Assert(programMemory.permanentMemorySize >= sizeof(ProgramState));
	state.dLoopRecord.stateMemoryBlock = VirtualAlloc(
		0, programMemory.permanentMemorySize + programMemory.transientMemorySize,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE
	);
	return programMemory;
}

u64 StringLength(char* str) {
	u64 result = 0;
	while (*str != '\0') {
		result++;
	}
	return result;
}

u64 CopyString(char* src, u64 srcSize, char* dst, u64 dstSize) {
	u32 copied = 0;
	while (*src != '\0' && copied < dstSize - 1 && copied < srcSize) {
		*dst++ = *src++;
		copied++;
	}
	*dst = '\0';
	return copied;
}

u64 ConcatenateString(char* first, u64 firstSize, char* second, u64 secondSize, char* dst, u64 dstSize) {
	u64 length = CopyString(first, firstSize, dst, dstSize);
	dst += length;
	length += CopyString(second, secondSize, dst, dstSize - length);
	return length;
}

struct ThreadContext {
	u32 threadId;
};

typedef void (*PlatformQueueCallback)(void* data, ThreadContext& context);

struct PlatformQueueTask {
	PlatformQueueCallback callback;
	void* args;
	u32 done;
};

struct PlatformQueue {
	volatile u32 writeIndex;
	volatile u32 readIndex;
	PlatformQueueTask tasks[16];
	HANDLE semaphore;
};

struct ThreadData {
	PlatformQueue* queue;
	ThreadContext context;
};

struct PrintStringArgs {
	u32 number;
};

void PrintString(void* args, ThreadContext& context) {
	PrintStringArgs* data = ptrcast(PrintStringArgs, args);
	char buf[256];
	sprintf_s(buf, "[Thread %d] START Task with number: %d\n", context.threadId, data->number);
	OutputDebugStringA(buf);
	Sleep(1000);
}

bool TryPopAndExecuteTaskFromQueue(PlatformQueue* queue, ThreadContext& context) {
	bool workDone = false;
	u32 visibleReadIndex = queue->readIndex;
	if (visibleReadIndex != queue->writeIndex) {
		u32 nextReadIndex = (visibleReadIndex + 1) % ArrayCount(queue->tasks);
		u32 actualReadIndex = InterlockedCompareExchange(
			ptrcast(volatile LONG, &queue->readIndex),
			nextReadIndex,
			visibleReadIndex
		);
		if (visibleReadIndex == actualReadIndex) {
			PlatformQueueTask* task = queue->tasks + visibleReadIndex;
			task->callback(task->args, context);
			InterlockedExchange(ptrcast(volatile LONG, &task->done), 1);
		}
		workDone = true;
	}
	return workDone;
}

DWORD ThreadProc(LPVOID params) {
	ThreadData* data = ptrcast(ThreadData, params);
	PlatformQueue* queue = data->queue;
	ThreadContext& context = data->context;
	while (true) {
		if (!TryPopAndExecuteTaskFromQueue(queue, context)) {
			WaitForSingleObjectEx(queue->semaphore, INFINITE, FALSE);
		}
		
	}
	return 0;
}

bool PushTask(PlatformQueue& queue, PlatformQueueCallback callback, void* args) {
	u32 taskIndex = queue.writeIndex;
	PlatformQueueTask* existingTask = queue.tasks + taskIndex;
	if (!InterlockedCompareExchange(ptrcast(volatile LONG, &existingTask->done), 0, 0)) 
	{
		Assert(false); // No space for new task!
		return false;
	}
	PlatformQueueTask* newTask = queue.tasks + taskIndex;
	newTask->callback = callback;
	newTask->args = args;
	newTask->done = 0;
	// TODO: Should I just use InterlockedIncrement()? 
	_WriteBarrier();
	_mm_sfence();
	queue.writeIndex = (taskIndex + 1) % ArrayCount(queue.tasks);
	ReleaseSemaphore(queue.semaphore, 1, 0);
	return true;
}

bool WorkInQueueIsDone(PlatformQueue& queue) {
	return queue.readIndex == queue.writeIndex;
}

void WaitForQueueCompletion(PlatformQueue& queue) {
	ThreadContext context = {};
	context.threadId = 69;
	while (!WorkInQueueIsDone(queue)) {
		TryPopAndExecuteTaskFromQueue(&queue, context);
	};
}

void InitializeQueue(PlatformQueue& queue) {
	// TODO: Could it be done better than that? For now it is required to exist, because
	// I need to check whether specific slot is free or not to avoid overriding task which might be
	// done in the background!!!
	for (u32 taskIndex = 0; taskIndex < ArrayCount(queue.tasks); taskIndex++) {
		PlatformQueueTask* task = queue.tasks + taskIndex;
		task->done = true;
	}
}

PlatformQueue globalQueue = {};

int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prevInstance,
	LPSTR cmdLine,
	int showCmd
) {
	
	ThreadData datas[5] = {};
	HANDLE threads[5] = {};
	u32 initialCount = 0;
	u32 threadCount = ArrayCount(threads);
	InitializeQueue(globalQueue);
	globalQueue.semaphore = CreateSemaphoreExA(0, 0, threadCount, 0, 0, EVENT_ALL_ACCESS);
	for (u32 threadIndex = 0; threadIndex < ArrayCount(datas); threadIndex++) {
		ThreadData* data = datas + threadIndex;
		data->queue = &globalQueue;
		LPVOID params = ptrcast(void, data);
		threads[threadIndex] = CreateThread(0, 0, ThreadProc, params, 0, ptrcast(DWORD, &data->context.threadId));
	}
	PrintStringArgs args[20] = {};
	for (u32 argsIndex = 0; argsIndex < ArrayCount(args); argsIndex++) {
		PrintStringArgs* argsEntry = args + argsIndex;
		argsEntry->number = argsIndex + 1;
		if ((argsIndex + 1) > 10) {
			argsEntry->number = argsIndex + 81;
		}
	}
	OutputDebugStringA("Sleeping for 1500ms before adding tasks\n");
	Sleep(1500);
	OutputDebugStringA("Adding tasks\n");
	PushTask(globalQueue, PrintString, &args[0]);
	PushTask(globalQueue, PrintString, &args[1]);
	PushTask(globalQueue, PrintString, &args[2]);
	PushTask(globalQueue, PrintString, &args[3]);
	PushTask(globalQueue, PrintString, &args[4]);
	PushTask(globalQueue, PrintString, &args[5]);
	PushTask(globalQueue, PrintString, &args[6]);
	PushTask(globalQueue, PrintString, &args[7]);
	PushTask(globalQueue, PrintString, &args[8]);
	PushTask(globalQueue, PrintString, &args[9]);
	Sleep(1500);
	OutputDebugStringA("Adding tasks 2\n");
	PushTask(globalQueue, PrintString, &args[10]);
	PushTask(globalQueue, PrintString, &args[11]);
	PushTask(globalQueue, PrintString, &args[12]);
	PushTask(globalQueue, PrintString, &args[13]);
	PushTask(globalQueue, PrintString, &args[14]);
	PushTask(globalQueue, PrintString, &args[15]);
	PushTask(globalQueue, PrintString, &args[16]);
	PushTask(globalQueue, PrintString, &args[17]);
	PushTask(globalQueue, PrintString, &args[18]);
	PushTask(globalQueue, PrintString, &args[19]);
	WaitForQueueCompletion(globalQueue);

#if 1
	u32 globalBitmapWidth = 960;
	u32 globalBitmapHeight = 540;
#else
	u32 globalBitmapWidth = 1920;
	u32 globalBitmapHeight = 1080;
#endif
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
	windowClass.hCursor = LoadCursor(0, IDC_ARROW);
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
	RECT rect;
	GetClientRect(window, &rect);
	MoveWindow(window, rect.left, rect.top, 1024, 600, true);

	// PART: Initializing program memory
	// // TODO change globalWin32State to win32State
	// Win32State win32State = {};
	globalWin32State.window = window;
	globalWin32State.bltOffsetX = 10;
	globalWin32State.bltOffsetY = 10;
	globalWin32State.displayWidth = globalBitmapWidth;
	globalWin32State.displayHeight = globalBitmapHeight;
	ProgramMemory programMemory = Win32InitProgramMemory(globalWin32State);
	if (!programMemory.memoryBlock) {
		// TODO: Logging
		return 0;
	}
	Win32GameCode gameCode = {};
	SoundData soundData = {};
	InputData inputData = {};

	DWORD length = GetModuleFileNameA(0, globalWin32State.exeFilePath, MY_MAX_PATH);
	char* tmpChar = globalWin32State.exeFilePath;
	for (u32 charIndex = 0; charIndex < length; charIndex++) {
		if (*tmpChar == '\\') {
			globalWin32State.exeFileName = tmpChar + 1;
		}
		tmpChar++;
	}
	CopyString(globalWin32State.exeFilePath, globalWin32State.exeFileName - globalWin32State.exeFilePath, globalWin32State.exeDirectory, MY_MAX_PATH);
	char tmpStr[MY_MAX_PATH];
	CopyString(gameCode.pathToDll, MY_MAX_PATH, tmpStr, MY_MAX_PATH);
	ConcatenateString(globalWin32State.exeDirectory, MY_MAX_PATH, tmpStr, MY_MAX_PATH, gameCode.pathToDll, MY_MAX_PATH);
	CopyString(gameCode.pathToTempDll, MY_MAX_PATH, tmpStr, MY_MAX_PATH);
	ConcatenateString(globalWin32State.exeDirectory, MY_MAX_PATH, tmpStr, MY_MAX_PATH, gameCode.pathToTempDll, MY_MAX_PATH);

	// NOTE: We can use one devicecontext because we specified CS_OWNDC so we dont share context with anyone
	HDC deviceContext = GetDC(window);
	Win32ResizeBitmapMemory(globalBitmap, globalBitmapWidth, globalBitmapHeight);
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
		Win32ProcessOSMessages(globalWin32State, programMemory, inputData.controllers[KB_CONTROLLER_IDX]);
		for (DWORD cIndex = 0; cIndex < XUSER_MAX_COUNT; cIndex++) {
			Win32GatherGamepadInput(inputData.controllers[cIndex], cIndex);
		}
		if (globalWin32State.dLoopRecord.recording) {
			Win32DebugRecordInput(globalWin32State, inputData.controllers[KB_CONTROLLER_IDX]);
		}
		else if (globalWin32State.dLoopRecord.replaying) {
			Win32DebugReplayInput(globalWin32State, programMemory, inputData.controllers[KB_CONTROLLER_IDX]);
		}
		inputData.dtFrame = targetFrameRefreshSeconds;
		inputData.executableReloaded = gameCode.reloaded;
		

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
		ZeroMemory(
			programMemory.debug.performanceCounters, 
			ArrayCount(programMemory.debug.performanceCounters) * sizeof(DebugPerformanceCounters)
		);
		gameCode.GameMainLoopFrame(programMemory, globalBitmap, soundData, inputData);
		Win32OutputPerformanceCounters(programMemory.debug);

	
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
			//Assert(secondsElapsedForFrame < targetFrameRefreshSeconds);
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
#if 0
		char buffer[256];
		sprintf_s(buffer, "%0.2fms/f,  %0.2ffps/f,  %0.2fMc/f,   frames_av %d\n", msElapsed, fps, megaCycles, framesAvailable);
		OutputDebugStringA(buffer);
#endif

		// PART: Displaying window
		auto dim = GetWindowDimension(window);
		Win32DisplayWindow(deviceContext, globalWin32State, globalBitmap, dim.width, dim.height);
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