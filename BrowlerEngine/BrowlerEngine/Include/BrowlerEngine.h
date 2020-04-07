#pragma once

#include <mutex>
#include "Thread.h"

BRWL_NS

#include "Globals.h"


class Engine;
class TickProvider;
class Timer;


class MetaEngine 
{
public:
	typedef uint8_t EngineHandle;
	static const EngineHandle maxEngine = 4;
	enum class EngineRunMode : uint8_t
	{
		META_ENGINE_MAIN_THREAD = 0,
		SYNCHRONIZED,
		DETATCHED,
	};

	MetaEngine(TickProvider* tickProvider, PlatformGlobalsPtr globals);

	void initialize();

	// Run a single step of each engine in which is not in DETACHED mode
	void update();
	// Runs an independent loop for engines which are in DETACHED mode
	void detachedRun();

	bool createEngine(EngineHandle& handle, const char* settingsFile = nullptr);
	void SetEngineRunMode(EngineHandle handle, EngineRunMode runMode);
	EngineHandle getDefaultEngineHandle() { return defaultEngineHandle; }

private:
	bool isInitialized;
	std::mutex metaEngineLock;
	std::unique_ptr<TickProvider> tickProvider;
	EngineHandle defaultEngineHandle;
	std::unique_ptr<Engine> engines[maxEngine];
	std::unique_ptr<Thread<void>> frameThreads[maxEngine];
	
	PlatformGlobalsPtr globals;
};

class Engine
{
public:
	Engine(TickProvider* tickProvider, PlatformGlobals* globals);
	~Engine();

	bool init(const char* settingsFile);
	void update();
	void shutdown();
	bool shouldClose();
	// this one should acutally only be called by the main function
	void close();



	//// convenience functions for logging
	//void LogDebug(const char* msg) { if (logger) logger->debug(msg); }
	//void LogInfo(const char* msg) { if (logger) logger->info(msg); }
	//void LogWarning(const char* msg) { if (logger) logger->warning(msg); }
	//void LogError(const char* msg) { if (logger) logger->error(msg); }

	//std::shared_ptr<Settings> settings;
	//std::shared_ptr<Logger> logger;
	std::unique_ptr<Timer> time;
	//std::unique_ptr<Window> window;
	//std::unique_ptr<Renderer> renderer;
	//std::unique_ptr<InputManager> input;
	//std::unique_ptr<EventBus> eventBus;
	//std::unique_ptr<Hierarchy> hierarchy;
	//std::unique_ptr<MeshRegistry> meshRegistry;
	//std::unique_ptr<TextureRegistry> textureRegistry;

	MetaEngine::EngineRunMode getRunMode() const { return runMode; }

	struct WriteAccessMetaEngine {
		friend class MetaEngine;
		Engine* pThis;
	private:
		void setRunMode(MetaEngine::EngineRunMode mode) { pThis->runMode = mode; }
	};

	WriteAccessMetaEngine writeAccessMetaEngine() { return  { this }; }

protected:
	bool internalInit(const char* settingsFile);
	
	bool isInitialized;
	TickProvider* tickProvider;
	PlatformGlobals* globals;
	MetaEngine::EngineRunMode runMode;
};

BRWL_NS_END
