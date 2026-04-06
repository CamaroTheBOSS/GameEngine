#pragma once
#include "engine_common.h"
#include "engine_debug.h"

#define BITMAP_BYTES_PER_PIXEL 4
struct BitmapData {
	// Platform independent buffer to render graphics
	void* data;
	u32 width;
	u32 height;
	u32 pitch;
};

#define SOUND_SAMPLES_PER_SECOND 48000
struct SoundData {
	// Platform independent buffer to play sounds
	void* data;
	u32 nSamples;
	u32 nSamplesPerSec;
	u32 nChannels;
};

#define MAX_CONTROLLERS 5
#define KB_CONTROLLER_IDX (MAX_CONTROLLERS - 4)

struct Button {
	// NOTE: isDown says how many frames button is down,
	//		 wasDown says whether button wasDown in previous frame
	u32 isDown;
	bool wasDown;
};

#pragma warning(push)
#pragma warning(disable : 4201)
struct Controller {
	// Platform independent user input
	union {
		struct Buttons {
			Button kW;
			Button kS;
			Button kD;
			Button kA;
			Button kArrowUp;
			Button kArrowDown;
			Button kArrowLeft;
			Button kArrowRight;
			Button kSpace;
			Button kEsc;
			Button kShift;
			Button kAlt;
			Button mouseLeft;
			Button mouseRight;
			Button mouseMiddle;
			Button mouse1B;
			Button mouse2B;
		};
		Buttons B;
		Button E[17];
		static_assert(sizeof(B) == sizeof(E));
	};
	// Mouse data is in screen space <-0.5w + 0.5, 0.5w - 0.5> <-0.5h + 0.5, 0.5h - 0.5)
	V2 mouse;
};
#pragma warning(pop)

struct InputData {
	Controller controllers[MAX_CONTROLLERS];
	f32 dtFrame;
};

/* Functionalities served by the platform layer for program layer */
// DEBUG API
struct FileData {
	// Platform independent file content
	void* content;
	u64 size;
};
typedef FileData(*_DebugReadEntireFile)(const char* filename);
typedef bool		(*_DebugWriteFile)(const char* filename, void* buffer, u64 size);
typedef void		(*_DebugFreeFile)(FileData& file);
typedef void* (*_DebugAllocate)(u64 size);
typedef u32(*_DebugGetCurrentThreadId)();

// File API
struct PlatformFileHandle {
	u64 size;
};
struct PlatformFileGroup {
	PlatformFileHandle** files;
	u32 count;
};
struct PlatformFileGroupNames {
	char** names;
	u32 count;
};
typedef PlatformFileHandle* (*_PlatformFileOpen)(const char* filename);
typedef void					(*_PlatformFileClose)(PlatformFileHandle* file);
typedef PlatformFileGroup* (*_PlatformFileOpenAllWithExtension)(const char* extension);
typedef void					(*_PlatformFileCloseAllInGroup)(PlatformFileGroup* group);
typedef bool					(*_PlatformFileErrors)(PlatformFileHandle* file);
typedef void					(*_PlatformFileRead)(PlatformFileHandle* file, u32 offset, u32 size, void* dst);

// Memory API
typedef void* (*_PlatformMemoryAllocate)(u32 size);
typedef void	(*_PlatformMemoryFree)(void* memory);

// Queue API
struct PlatformQueue;
typedef void	(*PlatformQueueCallback)(void* data);
typedef void	(*_PlatformWaitForQueueCompletion)(PlatformQueue* queue);
typedef bool	(*_PlatformPushTaskToQueue)(PlatformQueue* queue, PlatformQueueCallback callback, void* args);

// System API
enum PlatformCommandState {
	CmdState_Failed,
	CmdState_Running,
	CmdState_Completed
};
struct PlatformCommandHandle {
	u64 processHandle;
	u64 threadHandle;
	PlatformCommandState state;
};

typedef PlatformCommandHandle(*_PlatformSystemExecuteCommand)(char* cwd, char* command);
typedef PlatformCommandState(*_PlatformSystemGetCommandState)(PlatformCommandHandle& cmdHandle);

struct DebugMemory {
	_DebugReadEntireFile ReadEntireFile;
	_DebugWriteFile WriteFile;
	_DebugFreeFile FreeFile;
	_DebugAllocate Allocate;
	_DebugGetCurrentThreadId GetCurrThreadId;
};
#if defined(INTERNAL_BUILD)
struct ProgramMemory;
extern ProgramMemory* debugGlobalMemory;
#endif

/*----------------------------------------------------------------*/
struct PlatformAPI {
	// Thread Queue API
	_PlatformWaitForQueueCompletion QueueWaitForCompletion;
	_PlatformPushTaskToQueue QueuePushTask;

	// File API
	_PlatformFileOpen FileOpen;
	_PlatformFileClose FileClose;
	_PlatformFileOpenAllWithExtension FileOpenAllWithExtension;
	_PlatformFileCloseAllInGroup FileCloseAllInGroup;
	_PlatformFileErrors FileErrors;
	_PlatformFileRead FileRead;

	// Memory API
	_PlatformMemoryAllocate MemoryAllocate;
	_PlatformMemoryFree MemoryFree;

	// System API
	_PlatformSystemExecuteCommand SystemExecuteCommand;
	_PlatformSystemGetCommandState SystemGetCommandState;
};
extern PlatformAPI* Platform;

struct ProgramMemory {
	// Platform independent memory arenas
	// NOTE: memoryBlock is whole memory block whereas the rest of the blocks are blocks inside memoryBlock
	void* memoryBlock;
	u64 memoryBlockSize;
	u64 permanentMemorySize;
	void* permanentMemory;
	u64 transientMemorySize;
	void* transientMemory;
	u64 debugMemorySize;
	void* debugMemory;
	bool executableReloaded;

	PlatformQueue* highPriorityQueue;
	PlatformQueue* lowPriorityQueue;

	DebugMemory debug;
	PlatformAPI platformAPI;
};

/* Functionalities served by the program layer for platform layer */
#define GAME_MAIN_LOOP_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, InputData& input)
#define GAME_FILL_SOUND_BUFFER(name) void name(ProgramMemory& memory, SoundData& soundData)
#define DEBUG_INIT(name) DebugGlobalState* name(ProgramMemory& memory)
#define DEBUG_FINISH_FRAME(name) void name(ProgramMemory& memory, BitmapData& bitmap, InputData& input)
typedef GAME_MAIN_LOOP_FRAME(_GameMainLoopFrame);
typedef GAME_FILL_SOUND_BUFFER(_GameFillSoundBuffer);
typedef DEBUG_INIT(_DebugInit);
typedef DEBUG_FINISH_FRAME(_DebugFinishFrame);
extern "C" GAME_MAIN_LOOP_FRAME(GameMainLoopFrameStub);
extern "C" GAME_FILL_SOUND_BUFFER(GameFillSoundBufferStub);
extern "C" DEBUG_INIT(DebugInitStub);
extern "C" DEBUG_FINISH_FRAME(DebugFinishFrameStub);