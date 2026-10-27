#pragma once
#include <RmlUi/Core.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_SDL.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_render.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <functional>
#include <glm/glm.hpp>
#include <list>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <tileson.hpp>
#include <vector>

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;
using int2 = glm::ivec2;

///////////////////////////
// Compile-time features //
///////////////////////////

namespace Features {
constexpr bool enableSkidAudio = false;
};

///////////////////////
// GLM serialization //
///////////////////////

namespace glm {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec2, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec3, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(vec4, x, y, z, w);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec2, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec3, x, y, z);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ivec4, x, y, z, w);
}; // namespace glm

///////////
// Utils //
///////////

struct TimeUtils {
	static float microsecondsToFloatSeconds(const std::chrono::microseconds microseconds);
	static std::chrono::microseconds floatSecondsToMicroseconds(const float seconds);
	static std::string microsecondsToString(const std::chrono::microseconds microseconds);
};

/////////////
// Storage //
/////////////

class Storage {
public:
	bool exists(const std::filesystem::path& path) const;
	SDL_IOStream* open(const std::filesystem::path& path) const;

	nlohmann::json loadJson(const std::filesystem::path& path) const;
	void saveJson(const std::filesystem::path& path, const nlohmann::json& json) const;

private:
	static std::string normalizePath(const std::filesystem::path& path);
};

//////////////
// Settings //
//////////////

struct ShakeSettings {
	float2 blastRange = {1000, 10000};
	float2 durationRange = {0.5f, 1.0f};
	float2 periodRange = {0.1f, 0.2f};
	float2 strengthRange = {0.2f, 0.2f};

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ShakeSettings, blastRange, durationRange, periodRange, strengthRange);
};

struct CameraSettings {
	float zoom = 1;
	ShakeSettings shake;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraSettings, zoom, shake);
};

enum class FramerateLimit : int {
	Off,
	Fps30,
	Fps60,
	Fps120,
};

NLOHMANN_JSON_SERIALIZE_ENUM(FramerateLimit, {
												 {FramerateLimit::Off, "Off"},
												 {FramerateLimit::Fps30, "Fps30"},
												 {FramerateLimit::Fps60, "Fps60"},
												 {FramerateLimit::Fps120, "Fps120"},
											 });

struct RenderSettings {
	int skidTileResolution = 256;
	float4 skidColor = float4{0.5f};
	float skidSize = 2;
	float backgroundPeriod = 5;
	float backgroundScale = 0.2f;
	float backgroundDisplacement = 0.2f;
	float3 backgroundColor = float3{0.5f};
	FramerateLimit framerateLimit = FramerateLimit::Off;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, skidTileResolution, skidColor, skidSize, backgroundPeriod, backgroundScale, backgroundDisplacement, backgroundColor,
												framerateLimit);
};

struct CarSettings {
	float size = 0.2f;
	float mouseSensibility = 0.01f;
	float gamepadSensibility = 0.01f;
	float keyboardSensibility = 0.01f;
	float grip = 1;
	float baseSpeed = 0.1f;
	float2 speedMultiplierRange = float2(1, 4);
	float2 speedMultiplierStackedRange = float2(1, 5);
	float speedMultiplierDecrease = 0.5f;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CarSettings, size, mouseSensibility, gamepadSensibility, keyboardSensibility, grip, baseSpeed, speedMultiplierRange,
												speedMultiplierStackedRange, speedMultiplierDecrease);
};

struct BuildingSettings {
	float speedMultiplierBonus = 0;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(BuildingSettings, speedMultiplierBonus);
};

struct GameplaySettings {
	std::array<std::string, 5> ranks = {"Lame Lobster", "Annoying Lobster", "Infamous Lobster", "Terrorist Lobster", "Worst Lobster Ever"};
	float sequenceTime = 2.0f;
	float sequenceScoreExponent = 1.5f;
	float skyscraperScoreMultiplier = 2;
	float nuclearScoreMultiplier = 5;
	BuildingSettings garden;
	BuildingSettings house;
	BuildingSettings gaspump;
	BuildingSettings skyscraper;
	BuildingSettings nuclear;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GameplaySettings, sequenceTime, sequenceScoreExponent, skyscraperScoreMultiplier, nuclearScoreMultiplier, ranks, garden, house,
												gaspump, skyscraper, nuclear);
};

struct SystemSettings {
	bool updateTranslations = false;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(SystemSettings, updateTranslations);
};

struct AudioSettings {
	float musicGain = 1;
	float effectsGain = 1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(AudioSettings, musicGain, effectsGain);
};

struct Settings {
	RenderSettings render;
	CameraSettings camera;
	CarSettings car;
	GameplaySettings gameplay;
	SystemSettings system;
	AudioSettings audio;

	void update(const Storage& storage);

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Settings, render, camera, car, gameplay, system, audio);
};

//////////////
// Savegame //
//////////////

struct LevelRecord {
	int score1 = -1;
	float time = -1;

	int& score() {
		return score1;
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LevelRecord, score1, time);
};

struct Savegame {
	std::string language = "en";
	std::vector<LevelRecord> records;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Savegame, language, records);
};

///////////
// Audio //
///////////

class Audio;

struct Sound {
	Audio& audio;
	SDL_AudioSpec spec;
	short* data = nullptr;
	int dataSize = 0;
	SDL_AudioStream* audioStream;

	Sound(Storage& storage, Audio& _audio, const std::filesystem::path& path);
	~Sound();

	void setGain(const float gain);
};

class Audio {
public:
	SDL_AudioDeviceID deviceId = 0;

	Audio();
	~Audio();

	void pause();
	void resume();

	void playSound(const Sound& sound, const bool looping);
	void stopSound(const Sound& sound);

private:
	static void loopingCallback(void* userdata, SDL_AudioStream* stream, int additional_amount, int total_amount);
};

/////////////
// Content //
/////////////

struct Flipbook {
	std::vector<SDL_Texture*> textures;

	Flipbook() {}
	Flipbook(const Flipbook&) = delete;
	Flipbook(Flipbook&&) = default;
	Flipbook& operator=(const Flipbook&) = delete;
	Flipbook& operator=(Flipbook&&) = default;

	Flipbook(Storage& storage, SDL_Renderer* renderer, const char* pattern, const int starting_index = 0);
	~Flipbook();
};

struct Content {
	Storage& storage;
	Flipbook carFlipbook;
	Flipbook groundFlipbook;
	Flipbook buildingsFlipbook;
	Flipbook gardenFlipbook;
	Flipbook gaspumpFlipbook;
	Flipbook houseFlipbook;
	Flipbook nuclearFlipbook;
	Flipbook skyscraperFlipbook;
	Flipbook backgroundFlipbook;
	Sound menuSound;
	Sound ingameSound;
	Sound crashSound;
	Sound skidSound;

	Content(Storage& _storage, Audio& audio, SDL_Renderer* renderer);
};

/////////////
// Session //
/////////////

class CameraShaker {
public:
	void shake(const float _duration, const float _period, const float _strength);
	void update(const std::chrono::microseconds& delta_time);

	void reset();

	float2 getMovement() const {
		return movement;
	}

private:
	std::chrono::microseconds maxDuration = std::chrono::microseconds(0);
	std::chrono::microseconds durationLeft = std::chrono::microseconds(0);
	float period = 1;
	float strength = 0;

	float2 movement = float2{0};

	static float2 shakeSinusoidal(const float time, const float period);
};

class Session {
public:
	enum class BuildingType {
		None,
		Impassable,
		Garden,
		House,
		GasPump,
		Skyscraper,
		Nuclear,
	};

	Session(const Settings& _settings, const Storage& _storage, Audio& _audio, const Content& _content, SDL_Window* _window, SDL_Renderer* _renderer,
			const std::filesystem::path& level_path);
	~Session();

	void update(const std::chrono::microseconds delta_real_time);
	void render();

	float4 getBackgroundColor() const {
		return backgroundColor;
	}

	void pause(const bool _paused);

	bool isPaused() const {
		return paused;
	}

	void restart();

	bool isFinished() const {
		return finishTime.has_value();
	}

	float getGameTime() const {
		return TimeUtils::microsecondsToFloatSeconds(finishTime.has_value() ? *finishTime : sessionTotalTime);
	}

	float getFinishTime() const {
		return TimeUtils::microsecondsToFloatSeconds(*finishTime);
	}

	std::chrono::microseconds getCountdownTime() const {
		return countdownTime;
	}

	float getSpeedMultiplier() const {
		return carStats.speedMultiplier;
	}

	float getScore() const {
		return carStats.score;
	}

	const std::vector<BuildingType> getSequence() const {
		return sequence;
	}

	static const std::string& getScoreRank(const Settings& settings, const int score, const int target_score);
	static const std::string& getTimeRank(const Settings& settings, const float time, const float target_time);

private:
	struct CellState {
		int groundTile;
		int buildingTile;
		BuildingType buildingType;
		bool destructed;
		std::chrono::microseconds destructionTime;
		SDL_Texture* skidTexture;
	};

	struct CarStats {
		float speedStackedMultiplier = 0;
		float speedMultiplier = 0;
		float score = 0;
	};

	const Settings& settings;
	const Storage& storage;
	const Content& content;
	Audio& audio;
	SDL_Window* window;
	SDL_Renderer* renderer;
	int2 worldSize;
	float4 backgroundColor;
	float2 worldStartingPosition = {0.5f, 0.5f};
	bool renderEnableGrid = false;
	float2 cameraPosition = {0, 0};
	bool cameraFollowCar = true;
	CameraShaker cameraShaker;

	CarStats carStats;
	std::vector<BuildingType> sequence;
	std::chrono::microseconds sequenceTimeLeft;
	std::vector<std::pair<std::string, float>> sequences;

	float2 carPrevPosition = {0.5f, 0.5f};
	float2 carPosition = {0.5f, 0.5f};
	float2 carForwardDirection = {1, 0};
	float2 carSteerDirection = {1, 0};
	bool carSkid = false;

	std::unique_ptr<tson::Map> level;
	std::vector<CellState> cells;

	std::chrono::microseconds countdownTime;
	std::chrono::microseconds sessionTotalTime;
	std::optional<std::chrono::microseconds> finishTime;
	bool paused = false;

	std::optional<SDL_Gamepad*> gamepad;

	float2 getRenderTileSize() const;
	float2 getRenderDirectionX() const;
	float2 getRenderDirectionY() const;
	float2 worldToRender(const float2 world_coords) const;

	static float scoreSequence(const Settings& settings, const std::vector<BuildingType>& sequence);
	static std::string debugRenderSequence(const std::vector<Session::BuildingType>& sequence);

	float getPlayerInput(const std::chrono::microseconds delta_time);

	void updateCarStats(const std::chrono::microseconds delta_time);
	void updateCarDrive(const std::chrono::microseconds delta_time, const float player_input);
	void updateCarCollisions(const std::chrono::microseconds total_time);
	bool isGameFinished() const;

	void renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture);
	void renderGrid();
};

//////////////
// Launcher //
//////////////

class Translator {
public:
	Translator(const Settings& _settings, Storage& _storage) : settings(_settings), storage(_storage) {}

	void selectLanguage(const std::string_view language);
	bool translate(const std::string_view input, std::string_view& output);

	static std::filesystem::path getTranslationPath(const std::string_view language);

private:
	const Settings& settings;
	Storage& storage;

	std::string currentLanguage = "en";
	nlohmann::json translation;
};

class StorageFileInterface : public Rml::FileInterface {
public:
	Storage& storage;

	StorageFileInterface(Storage& _storage);

	virtual Rml::FileHandle Open(const Rml::String& path) override;
	virtual void Close(Rml::FileHandle handle) override;
	virtual size_t Read(void* buffer, size_t size, Rml::FileHandle handle) override;
	virtual bool Seek(Rml::FileHandle handle, long offset, int origin) override;
	virtual size_t Tell(Rml::FileHandle handle) override;
};

class Launcher {
public:
	Launcher(const Settings& _settings, const Storage& _storage, Audio& _audio, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, Translator& _translator,
			 Rml::Context* ui_context);
	~Launcher();

	float4 getBackgroundColor() const;

	void update(const std::chrono::microseconds real_total_time, const std::chrono::microseconds delta_real_time);
	void render();

	bool isShutdownRequested() const {
		return shutdownRequested;
	}

private:
	enum class GameMode {
		Destruction,
		TimeAttack,
	};

	const Settings& settings;
	const Storage& storage;
	Audio& audio;
	Content& content;
	SDL_Window* window;
	SDL_Renderer* renderer;

	Translator& translator;
	Rml::Context* uiContext;
	Rml::DataModelHandle launcherModel;
	Rml::DataModelHandle sessionModel;
	std::string buildVersion = "v0";
	std::string buildDate = __DATE__ " " __TIME__;
	std::vector<Rml::String> levelNames;
	std::vector<Rml::String> levelRecords;
	std::vector<Rml::String> levelRanks;
	std::string currentMode;

	std::string speedMultiplier;
	std::string sequence;
	int score;
	std::string time;
	int countdown = 0;
	std::string finishedRank;

	std::vector<int> levelTargetScores;
	std::vector<float> levelTargetTimes;
	Savegame savegame;

	std::unique_ptr<Session> session;
	GameMode sessionGameMode;
	int sessionLevelIndex = -1;

	Rml::ElementDocument* document = nullptr;
	std::list<std::function<void()>> deferredUpdates;
	bool resultsShown = false;
	bool shutdownRequested = false;

	std::chrono::microseconds backgroundTime;

	void enumerateLevels();
	void save();
	void load();
	void refreshGameMode(const GameMode game_mode);

	void goToPage(const std::string& page);
	void createSession(const int level_index);
	void destroySession();

	void selectLanguage(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToHowToPlay(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToDrive(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToCredits(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToHome(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void quit(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);

	void playLevel(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void switchMode(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void pauseSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void resumeSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void restartSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void exitSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);

	void renderBackground();
	void captureMouse(const bool capture);

	static std::string renderSequence(const std::vector<Session::BuildingType>& sequence);
};

/////////
// App //
/////////

class SystemInterface : public SystemInterface_SDL {
public:
	SystemInterface(SDL_Window* window, Translator& _translator) : SystemInterface_SDL(window), translator(_translator) {}

	int TranslateString(Rml::String& translated, const Rml::String& input) override;

private:
	Translator& translator;
};

class App {
public:
	App(int argc, char* argv[]);
	~App();

	SDL_AppResult processEvent(SDL_Event* event);
	SDL_AppResult iterate();

private:
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::unique_ptr<Storage> storage;
	Settings settings;
	std::unique_ptr<Translator> translator;
	std::unique_ptr<StorageFileInterface> uiStorage;
	std::unique_ptr<SystemInterface> uiSystem;
	std::unique_ptr<RenderInterface_SDL> uiRenderer;
	Rml::Context* uiContext;
	std::unique_ptr<Audio> audio;
	std::unique_ptr<Content> content;
	std::unique_ptr<Launcher> launcher;

	bool showDevUi = false;
	std::chrono::microseconds realTotalTime;
	std::chrono::microseconds realDeltaTime;
};
