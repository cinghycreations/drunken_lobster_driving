#define SDL_MAIN_USE_CALLBACKS
#include <RmlUi/Core.h>
#include <RmlUi/Debugger.h>
#include <RmlUi_Platform_SDL.h>
#include <RmlUi_Renderer_SDL.h>
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>
#include <string_view>
#include <tileson.hpp>
#ifdef CC_HAS_DEVUI
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <imgui_stdlib.h>
#endif
#ifdef CC_HAS_EMBED_CONTENT
#include <cmrc/cmrc.hpp>
#endif

using float2 = glm::vec2;
using float3 = glm::vec3;
using float4 = glm::vec4;
using int2 = glm::ivec2;

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

float TimeUtils::microsecondsToFloatSeconds(const std::chrono::microseconds microseconds) {
	return float(microseconds.count()) / 1000000;
}

std::chrono::microseconds TimeUtils::floatSecondsToMicroseconds(const float seconds) {
	return std::chrono::microseconds(std::chrono::microseconds::rep(seconds * 1000000));
}

std::string TimeUtils::microsecondsToString(const std::chrono::microseconds microseconds) {
	using rep = std::chrono::microseconds::rep;
	static constexpr rep us_in_minute = 1000 * 1000 * 60;
	static constexpr rep us_in_second = 1000 * 1000;
	static constexpr rep us_in_milli = 1000;
	const rep us = microseconds.count();
	const rep mins = us / us_in_minute;
	const rep secs = (us % us_in_minute) / us_in_second;
	const rep millis = (us % us_in_second) / us_in_milli;
	return fmt::format("{}:{}.{}", mins, secs, millis);
}

/////////////
// Storage //
/////////////

class Storage {
public:
	bool exists(const std::filesystem::path& path) const;
	SDL_IOStream* open(const std::filesystem::path& path) const;

	nlohmann::json loadJson(const std::filesystem::path& path) const;
	void saveJson(const std::filesystem::path& path, const nlohmann::json& json) const;
};

#ifdef CC_HAS_EMBED_CONTENT
namespace cmrc::content {
cmrc::embedded_filesystem get_filesystem();
} // namespace cmrc::content

bool Storage::exists(const std::filesystem::path& path) const {
	return cmrc::content::get_filesystem().exists(path.string());
}

SDL_IOStream* Storage::open(const std::filesystem::path& path) const {
	cmrc::file file = cmrc::content::get_filesystem().open(path.string());
	return SDL_IOFromConstMem(file.begin(), file.size());
}
#else
bool Storage::exists(const std::filesystem::path& path) const {
	return SDL_GetPathInfo(path.string().c_str(), nullptr);
}

SDL_IOStream* Storage::open(const std::filesystem::path& path) const {
	return SDL_IOFromFile(path.string().c_str(), "rb");
}
#endif

nlohmann::json Storage::loadJson(const std::filesystem::path& path) const {
	SDL_IOStream* stream = open(path);
	size_t data_size = 0;
	void* data = SDL_LoadFile_IO(stream, &data_size, true);
	SDL_assert(data);

	nlohmann::json json = nlohmann::json::parse(static_cast<char*>(data), static_cast<char*>(data) + data_size);
	return json;
}

void Storage::saveJson(const std::filesystem::path& path, const nlohmann::json& json) const {
	SDL_IOStream* stream = SDL_IOFromFile(path.string().c_str(), "wb");
	SDL_assert(stream);

	const std::string data = json.dump(1, '\t');

	const bool save_ok = SDL_SaveFile_IO(stream, data.data(), data.size(), true);
	SDL_assert(save_ok);
}

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

struct RenderSettings {
	int skidTileResolution = 256;
	float4 skidColor = float4{0.5f};
	float skidSize = 2;
	float backgroundPeriod = 5;
	float backgroundScale = 0.2f;
	float backgroundDisplacement = 0.2f;
	float3 backgroundColor = float3{0.5f};

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, skidTileResolution, skidColor, skidSize, backgroundPeriod, backgroundScale, backgroundDisplacement,
												backgroundColor);
};

struct CarSettings {
	float size = 0.2f;
	float mouseSensibility = 0.01f;
	float grip = 1;
	float baseSpeed = 0.1f;
	float2 speedMultiplierRange = float2(1, 4);
	float2 speedMultiplierStackedRange = float2(1, 5);
	float speedMultiplierDecrease = 0.5f;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CarSettings, size, mouseSensibility, grip, baseSpeed, speedMultiplierRange, speedMultiplierStackedRange, speedMultiplierDecrease);
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

struct Settings {
	RenderSettings render;
	CameraSettings camera;
	CarSettings car;
	GameplaySettings gameplay;

	void update(const Storage& storage);

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Settings, render, camera, car, gameplay);
};

void Settings::update(const Storage& storage) {
#ifdef CC_HAS_DEVUI
	if (ImGui::Begin("Settings")) {
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_R);
		if (ImGui::Button("Reload") && storage.exists("settings.json")) {
			const nlohmann::json json = storage.loadJson("settings.json");
			*this = json;
		}
		ImGui::SameLine();
		ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_S);
		if (ImGui::Button("Save")) {
			const nlohmann::json json = *this;
			storage.saveJson("settings.json", json);
		}

		if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragInt("Skid Tile Resolution", &render.skidTileResolution, 1, 32, 512);
			ImGui::ColorEdit4("Skid Color", &render.skidColor.r);
			ImGui::DragFloat("Skid Size", &render.skidSize, 0.01f);
			ImGui::DragFloat("Background Period", &render.backgroundPeriod, 0.01f);
			ImGui::DragFloat("Background Scale", &render.backgroundScale, 0.01f);
			ImGui::DragFloat("Background Displacement", &render.backgroundDisplacement, 0.01f);
			ImGui::ColorEdit3("Background Color", &render.backgroundColor.r);
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Zoom", &camera.zoom, 0.01f, 0.01f, 10);
			ImGui::DragFloat2("Blast Range", &camera.shake.blastRange.x, 0.01f);
			ImGui::DragFloat2("Duration Range", &camera.shake.durationRange.x, 0.01f);
			ImGui::DragFloat2("Period Range", &camera.shake.periodRange.x, 0.01f);
			ImGui::DragFloat2("Strength Range", &camera.shake.strengthRange.x, 0.01f);
		}

		if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Size", &car.size, 0.01f, 0.01f, 5);
			ImGui::DragFloat("Mouse Sensibility", &car.mouseSensibility, 0.01f);
			ImGui::SliderFloat("Grip", &car.grip, 0, 1);
			ImGui::DragFloat("Base Speed", &car.baseSpeed, 0.01f);
			ImGui::DragFloat2("Speed Multiplier Range", &car.speedMultiplierRange.x, 0.01f);
			ImGui::DragFloat2("Speed Multiplier Stacked Range", &car.speedMultiplierStackedRange.x, 0.01f);
			ImGui::DragFloat("Speed Multiplier Decrease", &car.speedMultiplierDecrease, 0.01f);
		}

		if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int rank = 0; rank < gameplay.ranks.size(); ++rank) {
				const std::string caption = fmt::format("Rank {}", rank);
				ImGui::InputText(caption.c_str(), &gameplay.ranks.at(rank));
			}
			ImGui::Separator();

			ImGui::DragFloat("Sequence Time", &gameplay.sequenceTime, 0.01f);
			ImGui::DragFloat("Sequence Score Exponent", &gameplay.sequenceScoreExponent, 0.01f);
			ImGui::DragFloat("Skyscraper Score Multiplier", &gameplay.skyscraperScoreMultiplier, 0.01f);
			ImGui::DragFloat("Nuclear Score Multiplier", &gameplay.nuclearScoreMultiplier, 0.01f);
			ImGui::Separator();

			const std::array<std::pair<const char*, BuildingSettings*>, 5> entries = {{
				{"Garden", &gameplay.garden},
				{"House", &gameplay.house},
				{"Gaspump", &gameplay.gaspump},
				{"Skyscraper", &gameplay.skyscraper},
				{"Nuclear", &gameplay.nuclear},
			}};

			for (const auto& entry : entries) {
				ImGui::PushID(entry.first);
				ImGui::Text("%s", entry.first);
				ImGui::DragFloat("Speed Multiplier Bonus", &entry.second->speedMultiplierBonus, 0.01f);
				ImGui::Separator();
				ImGui::PopID();
			}
		}
	}
	ImGui::End();
#endif
}

//////////////
// Savegame //
//////////////

struct LevelRecord {
	float score = -1;
	float time = -1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(LevelRecord, score, time);
};

struct Savegame {
	int version = 0;
	std::vector<LevelRecord> records;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Savegame, version, records);
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

Flipbook::Flipbook(Storage& storage, SDL_Renderer* renderer, const char* pattern, const int starting_index) {
	int index = starting_index;
	while (true) {
		const std::filesystem::path path = fmt::vformat(pattern, fmt::vargs<int>{{index}});
		if (!storage.exists(path)) {
			break;
		}

		SDL_IOStream* stream = storage.open(path);
		SDL_assert(stream);

		SDL_Texture* texture = IMG_LoadTexture_IO(renderer, stream, true);
		SDL_assert(texture);

		textures.push_back(texture);

		++index;
	}
}

Flipbook::~Flipbook() {
	for (SDL_Texture* texture : textures) {
		SDL_DestroyTexture(texture);
	}
	textures.clear();
}

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

	Content(Storage& _storage, SDL_Renderer* renderer);
};

Content::Content(Storage& _storage, SDL_Renderer* renderer)
	: storage(_storage), carFlipbook(storage, renderer, "car/car{:04}.png", 1), groundFlipbook(storage, renderer, "roads/road{:04}.png", 1),
	  buildingsFlipbook(storage, renderer, "buildings/buildings{:04}.png", 1), gardenFlipbook(storage, renderer, "buildings/garden{:04}.png", 0),
	  gaspumpFlipbook(storage, renderer, "buildings/gaspump{:04}.png", 0), houseFlipbook(storage, renderer, "buildings/house{:04}.png", 0),
	  nuclearFlipbook(storage, renderer, "buildings/nuclear{:04}.png", 0), skyscraperFlipbook(storage, renderer, "buildings/skyscraper{:04}.png", 0) {}

/////////////
// Session //
/////////////

class CameraShaker {
public:
	void shake(const float _duration, const float _period, const float _strength);
	void update(const std::chrono::microseconds& delta_time);

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

void CameraShaker::shake(const float _duration, const float _period, const float _strength) {
	maxDuration = TimeUtils::floatSecondsToMicroseconds(_duration);
	durationLeft = maxDuration;
	period = _period;
	strength = _strength;
}

void CameraShaker::update(const std::chrono::microseconds& delta_time) {
	if (durationLeft >= delta_time) {
		durationLeft -= delta_time;
		const float time_secs = TimeUtils::microsecondsToFloatSeconds(durationLeft);
		const float progress = float(durationLeft.count()) / float(maxDuration.count());
		const float2 delta = shakeSinusoidal(time_secs, period) * strength;
		movement = glm::lerp(float2(0), delta, progress);
	} else {
		durationLeft = std::chrono::microseconds(0);
		movement = float2(0, 0);
	}
}

float2 CameraShaker::shakeSinusoidal(const float time, const float period) {
	const float angle = (time / period) * std::numbers::pi * 2;
	return float2(std::sin(angle), std::cos(angle));
}

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

	Session(const Settings& _settings, const Storage& _storage, const Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, const std::filesystem::path& level_path);
	~Session();

	void update(const std::chrono::microseconds delta_real_time);
	void render();

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

	static const std::string& getScoreRank(const Settings& settings, const float score, const float target_score);
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
	SDL_Window* window;
	SDL_Renderer* renderer;
	int2 worldSize;
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

	float2 getRenderTileSize() const;
	float2 getRenderDirectionX() const;
	float2 getRenderDirectionY() const;
	float2 worldToRender(const float2 world_coords) const;

	static float scoreSequence(const Settings& settings, const std::vector<BuildingType>& sequence);
	static std::string debugRenderSequence(const std::vector<Session::BuildingType>& sequence);

	void updateCarStats(const std::chrono::microseconds delta_time);
	void updateCarDrive(const std::chrono::microseconds delta_time);
	void updateCarCollisions(const std::chrono::microseconds total_time);
	bool isGameFinished() const;

	void renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture);
	void renderGrid();
};

Session::Session(const Settings& _settings, const Storage& _storage, const Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, const std::filesystem::path& level_path)
	: settings(_settings), storage(_storage), content(_content), window(_window), renderer(_renderer), sequenceTimeLeft(0), countdownTime(3000000 - 1), sessionTotalTime(0) {

	SDL_IOStream* level_stream = storage.open(level_path);
	size_t level_data_size = 0;
	void* level_data = SDL_LoadFile_IO(level_stream, &level_data_size, true);
	SDL_assert(level_data);

	tson::Tileson parser(std::unique_ptr<tson::NlohmannJson>(new tson::NlohmannJson));
	level = parser.parse(level_data, level_data_size);
	SDL_assert(level->getStatus() == tson::ParseStatus::OK);
	SDL_assert(level->getLayers().size() == 2);
	SDL_assert(level->getLayer("ground"));
	SDL_assert(level->getLayer("buildings"));
	SDL_assert(level->getLayer("ground")->getType() == tson::LayerType::TileLayer);
	SDL_assert(level->getLayer("buildings")->getType() == tson::LayerType::TileLayer);
	SDL_assert(level->getTilesets().size() == 2);
	SDL_assert(level->getTileset("ground"));
	SDL_assert(level->getTileset("buildings"));

	worldSize = int2(level->getSize().x, level->getSize().y);
	cells.resize(worldSize.x * worldSize.y);

	{
		const tson::Layer* ground_layer = level->getLayer("ground");
		const tson::Layer* buildings_layer = level->getLayer("buildings");
		const tson::Tileset* ground_tileset = level->getTileset("ground");
		const tson::Tileset* buildings_tileset = level->getTileset("buildings");

		for (int i = 0; i < worldSize.y; ++i) {
			for (int j = 0; j < worldSize.x; ++j) {
				CellState& cell = cells.at(i * worldSize.x + j);
				cell = CellState{.groundTile = -1,
								 .buildingTile = -1,
								 .buildingType = BuildingType::None,
								 .destructed = false,
								 .destructionTime = std::chrono::microseconds(0),
								 .skidTexture = nullptr};

				{
					SDL_assert(ground_layer->getTileData().count({j, i}));

					const tson::Tile* ground_tile = ground_layer->getTileData().at({j, i});
					SDL_assert(ground_tile->getTileset() == ground_tileset);

					cell.groundTile = ground_tile->getId() - 1;
					SDL_assert(cell.groundTile >= 0 && cell.groundTile < content.groundFlipbook.textures.size());
				}

				if (buildings_layer->getTileData().count({j, i})) {
					const tson::Tile* buildings_tile = buildings_layer->getTileData().at({j, i});
					SDL_assert(buildings_tile->getTileset() == buildings_tileset);

					cell.buildingTile = buildings_tile->getId() - 1;
					SDL_assert(cell.buildingTile >= 0 && cell.buildingTile < content.buildingsFlipbook.textures.size());

					if (buildings_tile->getClassType() == "impassable") {
						cell.buildingType = BuildingType::Impassable;
					} else if (buildings_tile->getClassType() == "garden") {
						cell.buildingType = BuildingType::Garden;
					} else if (buildings_tile->getClassType() == "house") {
						cell.buildingType = BuildingType::House;
					} else if (buildings_tile->getClassType() == "gaspump") {
						cell.buildingType = BuildingType::GasPump;
					} else if (buildings_tile->getClassType() == "skyscraper") {
						cell.buildingType = BuildingType::Skyscraper;
					} else if (buildings_tile->getClassType() == "nuclear") {
						cell.buildingType = BuildingType::Nuclear;
					} else {
						SDL_assert(false);
					}
				}

				{
					cell.skidTexture =
						SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, settings.render.skidTileResolution, settings.render.skidTileResolution / 2);
					SDL_assert(cell.skidTexture);

					const bool set_target_ok = SDL_SetRenderTarget(renderer, cell.skidTexture);
					SDL_assert(set_target_ok);

					const bool set_color_ok = SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
					SDL_assert(set_color_ok);

					const bool clear_ok = SDL_RenderClear(renderer);
					SDL_assert(clear_ok);

					const bool restore_target_ok = SDL_SetRenderTarget(renderer, nullptr);
					SDL_assert(restore_target_ok);
				}
			}
		}
	}

	SDL_assert(level->getProp("startX"));
	SDL_assert(level->getProp("startY"));

	worldStartingPosition = float2(level->getProp("startX")->getValue<int>(), level->getProp("startY")->getValue<int>()) + float2(0.5f);
	carPosition = worldStartingPosition;
}

Session::~Session() {
	for (auto& cell : cells) {
		SDL_DestroyTexture(cell.skidTexture);
		cell.skidTexture = nullptr;
	}
}

float2 Session::getRenderTileSize() const {
	int render_width = 0, render_height = 0;
	const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
	SDL_assert(output_size_ok);

	const float2 render_size(render_width, render_height);
	const float2 tile_size = float2(render_size.y * 0.5f * 2, render_size.y * 0.5f) * settings.camera.zoom;
	return tile_size;
}

float2 Session::getRenderDirectionX() const {
	return getRenderTileSize() * 0.5f;
}

float2 Session::getRenderDirectionY() const {
	return getRenderTileSize() * 0.5f * float2(-1, 1);
}

float2 Session::worldToRender(const float2 world_coords) const {
	int render_width = 0, render_height = 0;
	const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
	SDL_assert(output_size_ok);

	const float2 x_dir = getRenderDirectionX();
	const float2 y_dir = getRenderDirectionY();
	const float2 render_pos = float2(render_width, render_height) * 0.5f + (world_coords - cameraPosition).x * x_dir + (world_coords - cameraPosition).y * y_dir;
	return render_pos;
}

void Session::update(const std::chrono::microseconds real_delta_time) {
	bool debug_simulation = false;

#ifdef CC_HAS_DEVUI
	//---------- Dev Ui ----------//

	if (ImGui::Begin("Session")) {
		if (ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_G);
			ImGui::Checkbox("Grid", &renderEnableGrid);
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
			ImGui::Checkbox("Follow Car", &cameraFollowCar);
			ImGui::BeginDisabled(cameraFollowCar);
			ImGui::DragFloat2("Position##Camera", &cameraPosition.x, 0.01f);
			ImGui::EndDisabled();
			if (ImGui::Button("Try Minimum Shake")) {
				cameraShaker.shake(settings.camera.shake.durationRange.x, settings.camera.shake.periodRange.x, settings.camera.shake.strengthRange.x);
			}
			if (ImGui::Button("Try Maximum Shake")) {
				cameraShaker.shake(settings.camera.shake.durationRange.y, settings.camera.shake.periodRange.y, settings.camera.shake.strengthRange.y);
			}
		}

		if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_H);
			if (ImGui::Button("Restart")) {
				restart();
			}
			ImGui::SameLine();
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_D);
			if (ImGui::Button("Debug Simulation")) {
				debug_simulation = true;
			}
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_P);
			if (ImGui::Button("Pause/Unpause")) {
				pause(!isPaused());
			}
			ImGui::BeginDisabled(!(paused));
			ImGui::DragFloat2("Position##Car", &carPosition.x, 0.01f);
			ImGui::DragFloat2("Forward", &carForwardDirection.x, 0.01f);
			ImGui::DragFloat2("Steer", &carSteerDirection.x, 0.01f);
			ImGui::Checkbox("Skid", &carSkid);
			ImGui::EndDisabled();
		}
	}
	ImGui::End();
#endif

	//---------- Car ----------//

	if (!paused) {
		if (debug_simulation) {
			SDL_TriggerBreakpoint();
		}

		if (real_delta_time >= countdownTime) {
			countdownTime = std::chrono::microseconds(0);
		} else {
			countdownTime -= real_delta_time;
		}

		if (countdownTime.count() == 0) {
			sessionTotalTime += real_delta_time;
			updateCarStats(real_delta_time);
			updateCarDrive(real_delta_time);
			updateCarCollisions(sessionTotalTime);
			cameraShaker.update(real_delta_time);
		}
	}

	if (cameraFollowCar) {
		cameraPosition = carPosition + cameraShaker.getMovement();
	}

	//---------- End Game ----------//

	if (!finishTime.has_value() && isGameFinished()) {
		finishTime = sessionTotalTime;
		if (!sequence.empty()) {
			const float sequence_score = Session::scoreSequence(settings, sequence);
			sequences.emplace_back(Session::debugRenderSequence(sequence), sequence_score);
			carStats.score += sequence_score;
			sequence.clear();
		}
	}

#ifdef CC_HAS_DEVUI
	//---------- Stats ----------//

	if (ImGui::Begin("Car Stats")) {
		ImGui::BeginDisabled(paused);
		ImGui::Text("Speed Stacked Multiplier: %f", carStats.speedStackedMultiplier);
		ImGui::Text("Speed Multiplier: %f", carStats.speedMultiplier);
		ImGui::Text("Score: %f", carStats.score);
		ImGui::Text("Sequence Size: %d", sequence.size());
		ImGui::Text("Sequence Time Left: %f", TimeUtils::microsecondsToFloatSeconds(sequenceTimeLeft));
		ImGui::Text("Session Total Time: %.2fs", TimeUtils::microsecondsToFloatSeconds(sessionTotalTime));
		if (!finishTime.has_value()) {
			ImGui::Text("Finish Time: none");
		} else {
			ImGui::Text("Finish Time: %.2fs", TimeUtils::microsecondsToFloatSeconds(*finishTime));
		}
		for (int sequence_index = 0; sequence_index < sequences.size(); ++sequence_index) {
			ImGui::Text("Sequence %d: %s, %f", sequence_index, sequences.at(sequence_index).first.c_str(), sequences.at(sequence_index).second);
		}
		ImGui::EndDisabled();
	}
	ImGui::End();
#endif
}

float Session::scoreSequence(const Settings& settings, const std::vector<BuildingType>& sequence) {
	float base = 0;
	float mult = 1;
	int i = 0;

	for (const BuildingType& building : sequence) {
		SDL_assert(building != Session::BuildingType::None && building != Session::BuildingType::Impassable);
	}

	while (i < sequence.size()) {
		int sequence_len = 1;
		while (i + sequence_len < sequence.size() && sequence.at(i + sequence_len) != BuildingType::Skyscraper && sequence.at(i + sequence_len) != BuildingType::Nuclear &&
			   sequence.at(i + sequence_len) == sequence.at(i)) {
			++sequence_len;
		}

		if (sequence.at(i) == BuildingType::Skyscraper) {
			mult *= settings.gameplay.skyscraperScoreMultiplier;
			i += 1;
		} else if (sequence.at(i) == BuildingType::Nuclear) {
			mult *= settings.gameplay.nuclearScoreMultiplier;
			i += 1;
		} else if (sequence_len > 1) {
			base += int(std::pow(float(sequence_len), settings.gameplay.sequenceScoreExponent) * 10);
			i += sequence_len;
		} else {
			i += 1;
		}
	}

	return base * mult;
}

std::string Session::debugRenderSequence(const std::vector<Session::BuildingType>& sequence) {
	std::string result;
	result.reserve(sequence.size());

	for (const Session::BuildingType& building : sequence) {
		SDL_assert(building != Session::BuildingType::None && building != Session::BuildingType::Impassable);
		switch (building) {
		case Session::BuildingType::Garden:
			result.push_back('G');
			break;
		case Session::BuildingType::House:
			result.push_back('H');
			break;
		case Session::BuildingType::GasPump:
			result.push_back('P');
			break;
		case Session::BuildingType::Skyscraper:
			result.push_back('S');
			break;
		case Session::BuildingType::Nuclear:
			result.push_back('N');
			break;
		}
	}

	return result;
}

void Session::updateCarStats(const std::chrono::microseconds delta_time) {
	carStats.speedStackedMultiplier -= settings.car.speedMultiplierDecrease * TimeUtils::microsecondsToFloatSeconds(delta_time);
	carStats.speedStackedMultiplier = std::clamp<float>(carStats.speedStackedMultiplier, settings.car.speedMultiplierStackedRange.x, settings.car.speedMultiplierStackedRange.y);
	carStats.speedMultiplier = std::clamp<float>(carStats.speedStackedMultiplier, settings.car.speedMultiplierRange.x, settings.car.speedMultiplierRange.y);

	if (delta_time >= sequenceTimeLeft) {
		sequenceTimeLeft = std::chrono::microseconds(0);
		if (!sequence.empty()) {
			const float sequence_score = Session::scoreSequence(settings, sequence);
			sequences.emplace_back(Session::debugRenderSequence(sequence), sequence_score);
			carStats.score += sequence_score;
			sequence.clear();
		}
	} else {
		sequenceTimeLeft -= delta_time;
	}
}

void Session::updateCarDrive(const std::chrono::microseconds delta_time) {
	carPrevPosition = carPosition;

	float mouse_delta_x;
	SDL_GetRelativeMouseState(&mouse_delta_x, nullptr);

	const float steer_angle = mouse_delta_x * settings.car.mouseSensibility;
	carSteerDirection = glm::rotate(glm::normalize(carSteerDirection), steer_angle);

	const float delta_secs = TimeUtils::microsecondsToFloatSeconds(delta_time);
	const float wanted_angle = glm::orientedAngle(carForwardDirection, carSteerDirection);
	const float max_angle = glm::radians(settings.car.grip * 360) * delta_secs;
	carSkid = std::fabs(wanted_angle) >= max_angle;
	carForwardDirection = glm::rotate(carForwardDirection, std::clamp(wanted_angle, -max_angle, max_angle));
	carPosition += carForwardDirection * (settings.car.baseSpeed * carStats.speedMultiplier) * delta_secs;

	if (carSkid) {
		const int2 car_tile = carPosition;
		CellState& cell = cells.at(car_tile.y * worldSize.x + car_tile.x);

		const bool set_target_ok = SDL_SetRenderTarget(renderer, cell.skidTexture);
		SDL_assert(set_target_ok);

		const bool set_color_ok =
			SDL_SetRenderDrawColorFloat(renderer, settings.render.skidColor.r, settings.render.skidColor.g, settings.render.skidColor.b, settings.render.skidColor.a);
		SDL_assert(set_color_ok);

		for (const float2 wheel_delta : {float2(-0.25f, 0.1f), float2(-0.25f, -0.1f)}) {
			const float2 wheels_position = carPosition + carForwardDirection * settings.car.size * wheel_delta.x +
										   glm::rotate(carForwardDirection, glm::radians<float>(90)) * settings.car.size * wheel_delta.y;

			const float2 tile_topright = worldToRender(float2(car_tile) + float2(0.5f) - float2(1, 0));
			const float2 tile_bottomleft = worldToRender(float2(car_tile) + float2(0.5f) + float2(1, 0));
			const float2 skid_position = worldToRender(wheels_position);
			const float2 skid_position_in_tile = (skid_position - tile_topright) / (tile_bottomleft - tile_topright) * float2(cell.skidTexture->w, cell.skidTexture->h);
			const SDL_FRect skid_rect{.x = skid_position_in_tile.x - settings.render.skidSize / 2,
									  .y = skid_position_in_tile.y - settings.render.skidSize / 2,
									  .w = settings.render.skidSize,
									  .h = settings.render.skidSize};

			const bool render_rect_ok = SDL_RenderFillRect(renderer, &skid_rect);
			SDL_assert(render_rect_ok);
		}

		const bool restore_target_ok = SDL_SetRenderTarget(renderer, nullptr);
		SDL_assert(restore_target_ok);
	}
}

void Session::updateCarCollisions(const std::chrono::microseconds total_time) {
	const float2 delta = carPosition - carPrevPosition;
	int2 prev_tile = glm::floor(carPrevPosition);
	int2 curr_tile = glm::floor(carPosition);

	if (prev_tile == curr_tile) {
		return;
	}

	if (curr_tile == prev_tile + int2(-1, -1)) {
		curr_tile = prev_tile + (delta.x > delta.y ? int2(-1, 0) : int2(0, -1));
	}

	if (curr_tile == prev_tile + int2(-1, 1)) {
		curr_tile = prev_tile + (delta.x > delta.y ? int2(-1, 0) : int2(0, 1));
	}

	if (curr_tile == prev_tile + int2(1, -1)) {
		curr_tile = prev_tile + (delta.x > delta.y ? int2(1, 0) : int2(0, -1));
	}

	if (curr_tile == prev_tile + int2(1, 1)) {
		curr_tile = prev_tile + (delta.x > delta.y ? int2(-1, 0) : int2(0, 1));
	}

	bool bouncing = false;

	do {
		if (curr_tile.x < 0 || curr_tile.x >= worldSize.x || curr_tile.y < 0 || curr_tile.y >= worldSize.y) {
			bouncing = true;
			break;
		}

		CellState& cell = cells.at(curr_tile.y * worldSize.x + curr_tile.x);

		if (cell.buildingType == BuildingType::None || cell.destructed) {
			break;
		}

		if (cell.buildingType == BuildingType::Impassable) {
			bouncing = true;
			break;
		}

		const BuildingSettings* building_settings = nullptr;
		if (cell.buildingType == BuildingType::Garden) {
			building_settings = &settings.gameplay.garden;
		} else if (cell.buildingType == BuildingType::House) {
			building_settings = &settings.gameplay.house;
		} else if (cell.buildingType == BuildingType::GasPump) {
			building_settings = &settings.gameplay.gaspump;
		} else if (cell.buildingType == BuildingType::Skyscraper) {
			building_settings = &settings.gameplay.skyscraper;
		} else if (cell.buildingType == BuildingType::Nuclear) {
			building_settings = &settings.gameplay.nuclear;
		}

		cell.destructed = true;
		cell.destructionTime = total_time;

		carStats.speedStackedMultiplier += building_settings->speedMultiplierBonus;
		sequence.push_back(cell.buildingType);
		sequenceTimeLeft = TimeUtils::floatSecondsToMicroseconds(settings.gameplay.sequenceTime);

		const float blast = Session::scoreSequence(settings, sequence);
		const float blast_in_range =
			std::clamp<float>((blast - settings.camera.shake.blastRange.x) / (settings.camera.shake.blastRange.y - settings.camera.shake.blastRange.x), 0, 1);
		const float duration = glm::lerp<float>(settings.camera.shake.durationRange.x, settings.camera.shake.durationRange.y, blast_in_range);
		const float period = glm::lerp<float>(settings.camera.shake.periodRange.x, settings.camera.shake.periodRange.y, blast_in_range);
		const float strength = glm::lerp<float>(settings.camera.shake.strengthRange.x, settings.camera.shake.strengthRange.y, blast_in_range);
		cameraShaker.shake(duration, period, strength);
	} while (false);

	if (bouncing) {
		if (curr_tile == prev_tile + int2(1, 0)) {
			carPosition = float2(curr_tile.x, carPosition.y);
			carForwardDirection = glm::reflect(carForwardDirection, float2(-1, 0));
		}

		if (curr_tile == prev_tile + int2(-1, 0)) {
			carPosition = float2(prev_tile.x, carPosition.y);
			carForwardDirection = glm::reflect(carForwardDirection, float2(1, 0));
		}

		if (curr_tile == prev_tile + int2(0, -1)) {
			carPosition = float2(carPosition.x, prev_tile.y);
			carForwardDirection = glm::reflect(carForwardDirection, float2(0, 1));
		}

		if (curr_tile == prev_tile + int2(0, 1)) {
			carPosition = float2(carPosition.x, curr_tile.y);
			carForwardDirection = glm::reflect(carForwardDirection, float2(0, -1));
		}

		carStats.speedStackedMultiplier = 0;
	}
}

bool Session::isGameFinished() const {
	return std::all_of(cells.begin(), cells.end(),
					   [](const CellState& cell) { return cell.buildingType == BuildingType::None || cell.buildingType == BuildingType::Impassable || cell.destructed; });
}

void Session::pause(const bool _paused) {
	paused = _paused;
}

void Session::restart() {
	carPosition = worldStartingPosition;
	carPrevPosition = carPosition;
	carForwardDirection = {1, 0};
	carSteerDirection = {1, 0};
	carSkid = false;
	carStats = {};
	for (CellState& cell : cells) {
		cell.destructed = 0;
		cell.destructionTime = std::chrono::microseconds(0);
	}
	countdownTime = std::chrono::microseconds(3000000 - 1);
	sessionTotalTime = std::chrono::microseconds(0);
	finishTime.reset();
	sequence.clear();
	sequenceTimeLeft = std::chrono::microseconds(0);
	sequences.clear();
	paused = false;
}

void Session::renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture) {
	const float2 bottom_left = worldToRender(world_coords) + getRenderDirectionY() * world_size;
	const float2 bottom_right = worldToRender(world_coords) + getRenderDirectionX() * world_size;
	const float render_width = bottom_right.x - bottom_left.x;
	const float render_height = float(texture->h) / float(texture->w) * render_width;
	const SDL_FRect rect{.x = bottom_left.x, .y = bottom_left.y - render_height, .w = render_width, .h = render_height};
	const bool render_ok = SDL_RenderTexture(renderer, texture, nullptr, &rect);
	SDL_assert(render_ok);
}

void Session::render() {
	//---------- Ground ----------//

	for (int i = 0; i < worldSize.y; ++i) {
		for (int j = 0; j < worldSize.x; ++j) {
			const CellState& cell = cells.at(i * worldSize.x + j);
			SDL_Texture* ground_texture = content.groundFlipbook.textures.at(cell.groundTile);
			renderEntity(float2(j, i) + float2(0.5f), 1, ground_texture);
		}
	}

	//---------- Skids ----------//

	for (int i = 0; i < worldSize.y; ++i) {
		for (int j = 0; j < worldSize.x; ++j) {
			const CellState& cell = cells.at(i * worldSize.x + j);
			renderEntity(float2(j, i) + float2(0.5f), 1, cell.skidTexture);
		}
	}

	//---------- Buildings and Car ----------//
	{
		auto get_destructed_frame = [this](const CellState& cell, const Flipbook& flipbook) -> SDL_Texture* {
			if (cell.destructed) {
				const int usec_per_frame = 1000000 / 45;
				const int frame = (sessionTotalTime - cell.destructionTime).count() / usec_per_frame;
				return flipbook.textures.at(std::clamp<int>(frame, 0, flipbook.textures.size() - 1));
			} else {
				return flipbook.textures.front();
			}
		};

		for (int i = 0; i < worldSize.y; ++i) {
			for (int j = 0; j < worldSize.x; ++j) {
				const CellState& cell = cells.at(i * worldSize.x + j);

				//---------- Destroyed Building ----------//

				if (cell.buildingType != BuildingType::None && cell.destructed) {
					auto get_animation_frame = [session_time = sessionTotalTime](const CellState& cell, const Flipbook& flipbook) -> SDL_Texture* {
						if (cell.destructed) {
							const int usec_per_frame = 1000000 / 45;
							const int frame = (session_time - cell.destructionTime).count() / usec_per_frame;
							return flipbook.textures.at(std::clamp<int>(frame, 0, flipbook.textures.size() - 1));
						} else {
							return flipbook.textures.front();
						}
					};

					SDL_Texture* building_texture = nullptr;
					if (cell.buildingType == BuildingType::Garden) {
						building_texture = get_animation_frame(cell, content.gardenFlipbook);
					} else if (cell.buildingType == BuildingType::House) {
						building_texture = get_animation_frame(cell, content.houseFlipbook);
					} else if (cell.buildingType == BuildingType::GasPump) {
						building_texture = get_animation_frame(cell, content.gaspumpFlipbook);
					} else if (cell.buildingType == BuildingType::Nuclear) {
						building_texture = get_animation_frame(cell, content.nuclearFlipbook);
					} else if (cell.buildingType == BuildingType::Skyscraper) {
						building_texture = get_animation_frame(cell, content.skyscraperFlipbook);
					}

					SDL_assert(building_texture);
					renderEntity(float2(j, i) + float2(0.5f), 1, building_texture);
				}

				//---------- Car ----------//

				if (int2(carPosition) == int2(j, i)) {
					const float car_rotation = glm::orientedAngle(carSteerDirection, float2(1, 0));
					const float pi2 = std::numbers::pi * 2;
					const float rotation = car_rotation > 0 ? car_rotation : pi2 - std::fmod(-car_rotation, pi2);
					const int frame = int(rotation / pi2 * content.carFlipbook.textures.size()) % content.carFlipbook.textures.size();
					renderEntity(carPosition, settings.car.size, content.carFlipbook.textures.at(frame));
				}

				//---------- Non-Destroyed Building ----------//

				if (cell.buildingType != BuildingType::None && !cell.destructed) {
					SDL_Texture* building_texture = content.buildingsFlipbook.textures.at(cell.buildingTile);
					renderEntity(float2(j, i) + float2(0.5f), 1, building_texture);
				}
			}
		}
	}

	if (renderEnableGrid) {
		renderGrid();
	}
}

void Session::renderGrid() {
	const bool set_color_ok = SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1);
	SDL_assert(set_color_ok);

	for (int i = 0; i <= worldSize.y; ++i) {
		const float2 a = worldToRender(float2(0, i));
		const float2 b = worldToRender(float2(worldSize.x, i));
		const bool render_line_ok = SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
		SDL_assert(render_line_ok);
	}

	for (int j = 0; j <= worldSize.x; ++j) {
		const float2 a = worldToRender(float2(j, 0));
		const float2 b = worldToRender(float2(j, worldSize.y));
		const bool render_line_ok = SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
		SDL_assert(render_line_ok);
	}
}

const std::string& Session::getScoreRank(const Settings& settings, const float score, const float target_score) {
	SDL_assert(target_score > 0);
	SDL_assert(settings.gameplay.ranks.size() > 0);

	int rank = int((score / target_score) * settings.gameplay.ranks.size());
	rank = std::clamp<int>(rank, 0, settings.gameplay.ranks.size() - 1);

	return settings.gameplay.ranks.at(rank);
}

const std::string& Session::getTimeRank(const Settings& settings, const float time, const float target_time) {
	SDL_assert(target_time > 0);
	SDL_assert(settings.gameplay.ranks.size() > 0);

	int rank = int(((target_time * 2 - time) / target_time) * settings.gameplay.ranks.size());
	rank = std::clamp<int>(rank, 0, settings.gameplay.ranks.size() - 1);

	return settings.gameplay.ranks.at(rank);
}

//////////////
// Launcher //
//////////////

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

StorageFileInterface::StorageFileInterface(Storage& _storage) : storage(_storage) {}

Rml::FileHandle StorageFileInterface::Open(const Rml::String& path) {
	SDL_IOStream* stream = storage.open(path);
	return reinterpret_cast<Rml::FileHandle>(stream);
}

void StorageFileInterface::Close(Rml::FileHandle handle) {
	SDL_IOStream* stream = reinterpret_cast<SDL_IOStream*>(handle);
	SDL_CloseIO(stream);
}

size_t StorageFileInterface::Read(void* buffer, size_t size, Rml::FileHandle handle) {
	SDL_IOStream* stream = reinterpret_cast<SDL_IOStream*>(handle);
	return SDL_ReadIO(stream, buffer, size);
}

bool StorageFileInterface::Seek(Rml::FileHandle handle, long offset, int origin) {
	SDL_IOStream* stream = reinterpret_cast<SDL_IOStream*>(handle);
	const SDL_IOWhence whence = (origin == SEEK_SET) ? SDL_IO_SEEK_SET : (origin == SEEK_END) ? SDL_IO_SEEK_END : SDL_IO_SEEK_CUR;
	return SDL_SeekIO(stream, offset, whence) != -1;
}

size_t StorageFileInterface::Tell(Rml::FileHandle handle) {
	SDL_IOStream* stream = reinterpret_cast<SDL_IOStream*>(handle);
	return SDL_TellIO(stream);
}

class Launcher {
public:
	Launcher(const Settings& _settings, const Storage& _storage, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, Rml::Context* ui_context);
	~Launcher();

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

	static constexpr int expectedSavegameVersion = 1;

	const Settings& settings;
	const Storage& storage;
	Content& content;
	SDL_Window* window;
	SDL_Renderer* renderer;

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

	std::vector<float> levelTargetScores;
	std::vector<float> levelTargetTimes;
	Savegame savegame = Savegame{.version = expectedSavegameVersion};

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

Launcher::Launcher(const Settings& _settings, const Storage& _storage, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, Rml::Context* ui_context)
	: settings(_settings), storage(_storage), content(_content), window(_window), renderer(_renderer), uiContext(ui_context) {

	buildVersion = fmt::format("version {}+{}-{}.{:.8}.{}", CC_BUILD_VERSION, CC_BUILD_PLATFORM, CC_BUILD_CONFIG, CC_BUILD_COMMIT, CC_BUILD_NUMBER);

	{
		Rml::DataModelConstructor launcher_model_ctor = uiContext->CreateDataModel("launcher");
		launcher_model_ctor.RegisterArray<std::vector<Rml::String>>();
		launcher_model_ctor.BindEventCallback("selectLanguage", &Launcher::selectLanguage, this);
		launcher_model_ctor.BindEventCallback("goToHowToPlay", &Launcher::goToHowToPlay, this);
		launcher_model_ctor.BindEventCallback("goToDrive", &Launcher::goToDrive, this);
		launcher_model_ctor.BindEventCallback("goToCredits", &Launcher::goToCredits, this);
		launcher_model_ctor.BindEventCallback("goToHome", &Launcher::goToHome, this);
		launcher_model_ctor.BindEventCallback("quit", &Launcher::quit, this);
		launcher_model_ctor.BindEventCallback("playLevel", &Launcher::playLevel, this);
		launcher_model_ctor.BindEventCallback("switchMode", &Launcher::switchMode, this);
		launcher_model_ctor.Bind("buildVersion", &buildVersion);
		launcher_model_ctor.Bind("buildDate", &buildDate);
		launcher_model_ctor.Bind("levelNames", &levelNames);
		launcher_model_ctor.Bind("levelRecords", &levelRecords);
		launcher_model_ctor.Bind("levelRanks", &levelRanks);
		launcher_model_ctor.Bind("currentMode", &currentMode);
		launcherModel = launcher_model_ctor.GetModelHandle();
	}

	{
		Rml::DataModelConstructor session_model_ctor = uiContext->CreateDataModel("session");
		session_model_ctor.BindEventCallback("pauseSession", &Launcher::pauseSession, this);
		session_model_ctor.BindEventCallback("resumeSession", &Launcher::resumeSession, this);
		session_model_ctor.BindEventCallback("restartSession", &Launcher::restartSession, this);
		session_model_ctor.BindEventCallback("exitSession", &Launcher::exitSession, this);
		session_model_ctor.Bind("speedMultiplier", &speedMultiplier);
		session_model_ctor.Bind("sequence", &sequence);
		session_model_ctor.Bind("score", &score);
		session_model_ctor.Bind("time", &time);
		session_model_ctor.Bind("countdown", &countdown);
		session_model_ctor.Bind("finishedRank", &finishedRank);
		session_model_ctor.Bind("currentMode", &currentMode);
		sessionModel = session_model_ctor.GetModelHandle();
	}

	enumerateLevels();
	load();
	refreshGameMode(GameMode::Destruction);

	document = uiContext->LoadDocument("ui/home.html");
	SDL_assert(document);
	document->Show();
}

Launcher::~Launcher() {
	if (document) {
		uiContext->UnloadDocument(document);
		document = nullptr;
	}
}

void Launcher::enumerateLevels() {
	constexpr int max_level_count = 8;

	levelNames.clear();
	levelTargetScores.clear();
	levelTargetTimes.clear();

	tson::Tileson parser(std::unique_ptr<tson::NlohmannJson>(new tson::NlohmannJson));
	for (int level_index = 0; level_index < max_level_count; ++level_index) {
		const std::filesystem::path level_path = fmt::format("levels/level{}.tmj", level_index);
		if (storage.exists(level_path)) {
			SDL_IOStream* level_stream = storage.open(level_path);
			size_t level_data_size = 0;
			void* level_data = SDL_LoadFile_IO(level_stream, &level_data_size, true);
			SDL_assert(level_data);

			const std::unique_ptr<tson::Map> level = parser.parse(level_data, level_data_size);
			SDL_assert(level->getProp("name"));
			SDL_assert(level->getProp("targetScore"));
			SDL_assert(level->getProp("targetTime"));

			levelNames.push_back(level->getProp("name")->getValue<std::string>());
			levelTargetScores.push_back(level->getProp("targetScore")->getValue<float>());
			levelTargetTimes.push_back(level->getProp("targetTime")->getValue<float>());
		} else {
			levelNames.push_back("<none>");
			levelTargetScores.push_back(FLT_MAX);
			levelTargetTimes.push_back(FLT_MAX);
		}
	}
}

void Launcher::save() {
	SDL_Storage* storage = SDL_OpenUserStorage("Cinghy Creations", "Drunken Lobster Driving", 0);
	SDL_assert(storage);
	SDL_assert(SDL_StorageReady(storage));

	nlohmann::json json = savegame;
	const std::string data = json.dump();
	const bool write_ok = SDL_WriteStorageFile(storage, "savegame.json", data.data(), data.size());
	SDL_assert(write_ok);

	const bool close_ok = SDL_CloseStorage(storage);
	SDL_assert(close_ok);
}

void Launcher::load() {
	SDL_Storage* storage = SDL_OpenUserStorage("Cinghy Creations", "Drunken Lobster Driving", 0);
	SDL_assert(storage);
	SDL_assert(SDL_StorageReady(storage));

	const bool exists = SDL_GetStoragePathInfo(storage, "savegame.json", nullptr);
	if (exists) {
		uint64_t filesize = 0;
		const bool get_size_ok = SDL_GetStorageFileSize(storage, "savegame.json", &filesize);
		SDL_assert(get_size_ok);

		std::string data;
		data.resize(filesize);
		const bool read_ok = SDL_ReadStorageFile(storage, "savegame.json", data.data(), data.size());
		SDL_assert(read_ok);

		const nlohmann::json json = nlohmann::json::parse(data);
		const Savegame loaded_savegame = json;
		if (loaded_savegame.version == expectedSavegameVersion) {
			savegame = loaded_savegame;
		}
	} else {
		savegame = Savegame{};
	}

	const bool close_ok = SDL_CloseStorage(storage);
	SDL_assert(close_ok);
}

void Launcher::refreshGameMode(const GameMode game_mode) {
	sessionGameMode = game_mode;
	levelRecords.resize(levelNames.size());
	levelRanks.resize(levelNames.size());

	SDL_assert(sessionGameMode == GameMode::Destruction || sessionGameMode == GameMode::TimeAttack);
	if (sessionGameMode == GameMode::Destruction) {
		currentMode = "Sequence";

		for (int level_index = 0; level_index < levelNames.size(); ++level_index) {
			if (level_index < savegame.records.size() && savegame.records.at(level_index).score >= 0) {
				levelRecords.at(level_index) = fmt::format("{}", int(savegame.records.at(level_index).score));
				levelRanks.at(level_index) = Session::getScoreRank(settings, savegame.records.at(level_index).score, levelTargetScores.at(level_index));
			} else {
				levelRecords.at(level_index) = "---";
				levelRanks.at(level_index) = "---";
			}
		}
	} else if (sessionGameMode == GameMode::TimeAttack) {
		currentMode = "Time Attack";

		for (int level_index = 0; level_index < levelNames.size(); ++level_index) {
			if (level_index < savegame.records.size() && savegame.records.at(level_index).time >= 0) {
				levelRecords.at(level_index) = fmt::format("{:.01f}s", savegame.records.at(level_index).time);
				levelRanks.at(level_index) = Session::getTimeRank(settings, savegame.records.at(level_index).time, levelTargetTimes.at(level_index));
			} else {
				levelRecords.at(level_index) = "---";
				levelRanks.at(level_index) = "---";
			}
		}
	}

	launcherModel.DirtyVariable("currentMode");
	launcherModel.DirtyVariable("levelRecords");
	launcherModel.DirtyVariable("levelRanks");
	sessionModel.DirtyVariable("currentMode");
}

void Launcher::update(const std::chrono::microseconds real_total_time, const std::chrono::microseconds real_delta_time) {
	for (auto& deferred_update_func : deferredUpdates) {
		deferred_update_func();
	}
	deferredUpdates.clear();

	{
		int numkeys;
		const bool* keystates = SDL_GetKeyboardState(&numkeys);
		if (session && !session->isPaused() && SDL_SCANCODE_ESCAPE < numkeys && keystates[SDL_SCANCODE_ESCAPE]) {
			session->pause(true);
			captureMouse(false);
			goToPage("ui/pause.html");
		}
	}

	if (session) {
		session->update(real_delta_time);

		speedMultiplier = fmt::format("{:.01f}", session->getSpeedMultiplier());
		sessionModel.DirtyVariable("speedMultiplier");

		sequence = Launcher::renderSequence(session->getSequence());
		sessionModel.DirtyVariable("sequence");

		score = int(session->getScore());
		sessionModel.DirtyVariable("score");

		time = fmt::format("{:.01f}s", session->getGameTime());
		sessionModel.DirtyVariable("time");

		countdown = (session->getCountdownTime().count() + 999999) / 1000000;
		sessionModel.DirtyVariable("countdown");

		if (session->isFinished() && !resultsShown) {
			if (sessionLevelIndex >= savegame.records.size()) {
				savegame.records.resize(sessionLevelIndex + 1);
			}

			if (sessionGameMode == GameMode::Destruction) {
				if (savegame.records.at(sessionLevelIndex).score < 0 || session->getScore() > savegame.records.at(sessionLevelIndex).score) {
					savegame.records.at(sessionLevelIndex).score = session->getScore();
					save();
					refreshGameMode(GameMode::Destruction);
				}
				finishedRank = Session::getScoreRank(settings, session->getScore(), levelTargetScores.at(sessionLevelIndex));
			} else if (sessionGameMode == GameMode::TimeAttack) {
				if (savegame.records.at(sessionLevelIndex).time < 0 || session->getFinishTime() < savegame.records.at(sessionLevelIndex).time) {
					savegame.records.at(sessionLevelIndex).time = session->getFinishTime();
					save();
					refreshGameMode(GameMode::TimeAttack);
				}
				finishedRank = Session::getTimeRank(settings, session->getFinishTime(), levelTargetTimes.at(sessionLevelIndex));
			}
			sessionModel.DirtyVariable("finishedRank");

			resultsShown = true;
			captureMouse(false);
			goToPage("ui/results.html");
		}
	}

	backgroundTime = real_total_time;
}

void Launcher::render() {
	if (session) {
		session->render();
	} else {
		renderBackground();
	}
}

void Launcher::goToPage(const std::string& page) {
	if (document) {
		uiContext->UnloadDocument(document);
		document = nullptr;
	}

	document = uiContext->LoadDocument(page);
	SDL_assert(document);
	document->Show();
}

void Launcher::createSession(const int level_index) {
	const std::filesystem::path level_path = fmt::format("levels/level{}.tmj", level_index);

	sessionLevelIndex = level_index;
	session.reset();
	session.reset(new Session(settings, storage, content, window, renderer, level_path));
	resultsShown = false;
}

void Launcher::destroySession() {
	sessionLevelIndex = -1;
	session.reset();
	resultsShown = false;
}

void Launcher::selectLanguage(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	SDL_assert(params.size() == 1);
	SDL_assert(params.at(0).GetType() == Rml::Variant::STRING);

	const std::string& language = params.at(0).GetReference<std::string>();
	SDL_assert(language == "it" || language == "en" || language == "es");
}

void Launcher::goToHowToPlay(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/howto.html"));
}

void Launcher::goToDrive(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/drive.html"));
}

void Launcher::goToCredits(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/credits.html"));
}

void Launcher::goToHome(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/home.html"));
}

void Launcher::quit(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	shutdownRequested = true;
}

void Launcher::playLevel(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	SDL_assert(params.size() == 1);

	int level_index = 0;
	const bool param0_ok = params.at(0).GetInto(level_index);
	SDL_assert(param0_ok);

	deferredUpdates.emplace_back([this, level_index]() {
		createSession(level_index);
		captureMouse(true);
		goToPage("ui/ingame.html");
	});
}

void Launcher::switchMode(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	SDL_assert(sessionGameMode == GameMode::Destruction || sessionGameMode == GameMode::TimeAttack);
	if (sessionGameMode == GameMode::Destruction) {
		refreshGameMode(GameMode::TimeAttack);
	} else if (sessionGameMode == GameMode::TimeAttack) {
		refreshGameMode(GameMode::Destruction);
	}
}

void Launcher::pauseSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back([this]() {
		session->pause(true);
		captureMouse(false);
		goToPage("ui/pause.html");
	});
}

void Launcher::resumeSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back([this]() {
		session->pause(false);
		captureMouse(true);
		goToPage("ui/ingame.html");
	});
}

void Launcher::restartSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back([this]() {
		resultsShown = false;
		session->restart();
		captureMouse(true);
		goToPage("ui/ingame.html");
	});
}

void Launcher::exitSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back([this]() {
		destroySession();
		captureMouse(false);
		goToPage("ui/drive.html");
	});
}

void Launcher::renderBackground() {
	int render_width = 0, render_height = 0;
	const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
	SDL_assert(output_size_ok);

	SDL_Texture* texture = content.carFlipbook.textures.front();
	const std::chrono::microseconds sin_period = TimeUtils::floatSecondsToMicroseconds(settings.render.backgroundPeriod);
	const std::chrono::microseconds cos_period = TimeUtils::floatSecondsToMicroseconds(settings.render.backgroundPeriod * 1.3f);
	const float sin_angle = float(backgroundTime.count() % sin_period.count()) / float(sin_period.count()) * std::numbers::pi * 2;
	const float cos_angle = float(backgroundTime.count() % cos_period.count()) / float(cos_period.count()) * std::numbers::pi * 2;
	const float texture_unit = float(render_width) * settings.render.backgroundScale;
	const float displacement_unit = float(render_width) * settings.render.backgroundDisplacement;
	const float displacement_x = displacement_unit * std::cos(cos_angle);
	const float displacement_y = displacement_unit * std::sin(sin_angle);
	const float texture_scale = texture_unit / float(texture->w);
	const SDL_FRect dest_rect{
		.x = displacement_x - (std::floor(displacement_x / texture_unit) + 1) * texture_unit,
		.y = displacement_y - (std::floor(displacement_y / texture_unit) + 1) * texture_unit,
		.w = render_width + texture_unit,
		.h = render_height + texture_unit,
	};

	const bool set_color_ok = SDL_SetRenderDrawColorFloat(renderer, settings.render.backgroundColor.r, settings.render.backgroundColor.g, settings.render.backgroundColor.b, 1);
	SDL_assert(set_color_ok);

	const bool render_rect_ok = SDL_RenderFillRect(renderer, nullptr);
	SDL_assert(render_rect_ok);

	const bool render_texture_ok = SDL_RenderTextureTiled(renderer, texture, nullptr, texture_scale, &dest_rect);
	SDL_assert(render_texture_ok);
}

void Launcher::captureMouse(const bool capture) {
	const bool mouse_mode_ok = SDL_SetWindowRelativeMouseMode(window, capture);
	SDL_assert(mouse_mode_ok);
}

std::string Launcher::renderSequence(const std::vector<Session::BuildingType>& sequence) {
	std::stringstream stream;

	for (const Session::BuildingType& building : sequence) {
		SDL_assert(building != Session::BuildingType::None && building != Session::BuildingType::Impassable);
		switch (building) {
		case Session::BuildingType::Garden:
			stream << "<img src=\"/buildings/garden0000.png\"/>";
			break;
		case Session::BuildingType::House:
			stream << "<img src=\"/buildings/house0000.png\"/>";
			break;
		case Session::BuildingType::GasPump:
			stream << "<img src=\"/buildings/gaspump0000.png\"/>";
			break;
		case Session::BuildingType::Skyscraper:
			stream << "<img src=\"/buildings/skyscraper0000.png\"/>";
			break;
		case Session::BuildingType::Nuclear:
			stream << "<img src=\"/buildings/nuclear0000.png\"/>";
			break;
		}
	}

	return stream.str();
}

/////////
// App //
/////////

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
	std::unique_ptr<StorageFileInterface> uiStorage;
	std::unique_ptr<SystemInterface_SDL> uiSystem;
	std::unique_ptr<RenderInterface_SDL> uiRenderer;
	Rml::Context* uiContext;
	std::unique_ptr<Content> content;
	std::unique_ptr<Launcher> launcher;

	bool showDevUi = false;
	std::chrono::microseconds realTotalTime;
	std::chrono::microseconds realDeltaTime;
};

App::App(int argc, char* argv[]) {
	const bool hint_ok = SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
	SDL_assert(hint_ok);

	bool init_ok = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
	SDL_assert(init_ok);

	window = SDL_CreateWindow("Drunken Lobster Driving", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	SDL_assert(window);

	renderer = SDL_CreateRenderer(window, nullptr);
	SDL_assert(renderer);

	const bool vsync_ok = SDL_SetRenderVSync(renderer, 1);
	SDL_assert(vsync_ok);

	storage.reset(new Storage);

#ifdef CC_HAS_DEVUI
	{
		ImGuiContext* imgui_context = ImGui::CreateContext();
		SDL_assert(imgui_context);

		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		const float main_scale = SDL_GetWindowDisplayScale(window);
		ImGui::StyleColorsDark();
		ImGui::GetStyle().FontScaleDpi = main_scale;

		SDL_IOStream* font_stream = storage->open("fonts/NotoSans-Regular.ttf");
		size_t font_data_size = 0;
		void* font_data = SDL_LoadFile_IO(font_stream, &font_data_size, true);
		SDL_assert(font_data);

		const ImFont* base_font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(font_data, font_data_size, 18);
		SDL_assert(base_font);

		const bool imgui_init1_ok = ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
		SDL_assert(imgui_init1_ok);

		const bool imgui_init2_ok = ImGui_ImplSDLRenderer3_Init(renderer);
		SDL_assert(imgui_init2_ok);
	}
#endif

	if (storage->exists("settings.json")) {
		const nlohmann::json json = storage->loadJson("settings.json");
		settings = json;
	}

	{
		uiStorage.reset(new StorageFileInterface(*storage));
		uiSystem.reset(new SystemInterface_SDL);
		uiRenderer.reset(new RenderInterface_SDL(renderer));
		Rml::SetFileInterface(uiStorage.get());
		Rml::SetSystemInterface(uiSystem.get());
		Rml::SetRenderInterface(uiRenderer.get());

		const bool ui_initialize_ok = Rml::Initialise();
		SDL_assert(ui_initialize_ok);

		uiContext = Rml::CreateContext("RmlContext", Rml::Vector2i(1920, 1080));
		SDL_assert(uiContext);

		const bool debugger_init_ok = Rml::Debugger::Initialise(uiContext);
		SDL_assert(debugger_init_ok);

		const bool load_font1_ok = Rml::LoadFontFace("fonts/NotoSans-Regular.ttf", true);
		SDL_assert(load_font1_ok);

		const bool load_font2_ok = Rml::LoadFontFace("fonts/Lobster-Regular.ttf", true);
		SDL_assert(load_font2_ok);
	}

	content.reset(new Content(*storage, renderer));
	launcher.reset(new Launcher(settings, *storage, *content, window, renderer, uiContext));
	realTotalTime = std::chrono::microseconds(SDL_GetTicksNS() / 1000);
}

SDL_AppResult App::processEvent(SDL_Event* event) {
#ifdef CC_HAS_DEVUI
	if (showDevUi) {
		ImGui_ImplSDL3_ProcessEvent(event);
	}
#endif

	RmlSDL::InputEventHandler(uiContext, window, *event);

	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}
#ifdef CC_HAS_DEVUI
	else if (event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
		const float main_scale = SDL_GetWindowDisplayScale(window);
		ImGui::GetStyle().FontScaleDpi = main_scale;
	} else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_F1) {
		showDevUi = !showDevUi;
		if (showDevUi) {
			const bool mouse_mode_ok = SDL_SetWindowRelativeMouseMode(window, false);
			SDL_assert(mouse_mode_ok);
		}
	} else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_F12) {
		Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
	}
#endif

	return SDL_APP_CONTINUE;
}

SDL_AppResult App::iterate() {
	const std::chrono::microseconds now(SDL_GetTicksNS() / 1000);
	realDeltaTime = now - realTotalTime;
	realTotalTime = now;

#ifdef CC_HAS_DEVUI
	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
#endif

	settings.update(*storage);
	launcher->update(realTotalTime, realDeltaTime);

	const bool set_color_ok = SDL_SetRenderDrawColorFloat(renderer, 0, 0, 0, 1);
	SDL_assert(set_color_ok);

	const bool clear_ok = SDL_RenderClear(renderer);
	SDL_assert(clear_ok);

	launcher->render();

	{
		const bool ui_update_ok = uiContext->Update();
		SDL_assert(ui_update_ok);

		const bool ui_render_ok = uiContext->Render();
		SDL_assert(ui_render_ok);
	}

#ifdef CC_HAS_DEVUI
	if (ImGui::Begin("Frame Stats")) {
		const std::chrono::microseconds frame_delta_time = std::chrono::microseconds(SDL_GetTicksNS() / 1000) - now;
		ImGui::Text("Real Delta Time: %.3fms", TimeUtils::microsecondsToFloatSeconds(realDeltaTime) * 1000);
		ImGui::Text("Frame Delta Time: %.3fms", TimeUtils::microsecondsToFloatSeconds(frame_delta_time) * 1000);
	}
	ImGui::End();
#endif

#ifdef CC_HAS_DEVUI
	ImGui::Render();
	if (showDevUi) {
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
	}
#endif

	const bool present_ok = SDL_RenderPresent(renderer);
	SDL_assert(present_ok);

	return launcher->isShutdownRequested() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

App::~App() {
	launcher.reset();
	content.reset();

	Rml::Shutdown();
	uiRenderer.reset();
	uiSystem.reset();

	storage.reset();

#ifdef CC_HAS_DEVUI
	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();
#endif

	SDL_DestroyRenderer(renderer);
	renderer = nullptr;

	SDL_DestroyWindow(window);
	window = nullptr;

	SDL_Quit();
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	*appstate = new App(argc, argv);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	return static_cast<App*>(appstate)->processEvent(event);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	return static_cast<App*>(appstate)->iterate();
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	delete static_cast<App*>(appstate);
}
