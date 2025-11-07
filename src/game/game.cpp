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
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>
#include <string_view>
#include <tileson.hpp>

using float2 = glm::vec2;
using float4 = glm::vec4;
using int2 = glm::ivec2;

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

bool Storage::exists(const std::filesystem::path& path) const {
	return SDL_GetPathInfo(path.string().c_str(), nullptr);
}

SDL_IOStream* Storage::open(const std::filesystem::path& path) const {
	return SDL_IOFromFile(path.string().c_str(), "rb");
}

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
	float shakeTime = 0.5f;
	float shakePeriod = 0.1f;
	float shakeStrength = 0.2f;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(ShakeSettings, shakeTime, shakePeriod, shakeStrength);
};

struct CameraSettings {
	float zoom = 1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraSettings, zoom);
};

struct RenderSettings {
	int skidTileResolution = 256;
	float4 skidColor = float4{0.5f};
	float skidSize = 2;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(RenderSettings, skidTileResolution, skidColor, skidSize);
};

struct CarSettings {
	float size = 0.2f;
	float baseSpeed = 0.1f;
	float superSpeed = 0.2f;
	float mouseSensibility = 0.01f;
	float grip = 1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CarSettings, size, baseSpeed, superSpeed, mouseSensibility, grip);
};

struct BuildingSettings {
	float minMayhemToDestroy = 0;
	float minSpeedToDestroy = 0;
	float perkDuration = 0;
	float perkMayhem = 0;
	float superSpeedDuration = 0;
	float scoreMultiplier = 1;
	ShakeSettings shake;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(BuildingSettings, minMayhemToDestroy, minSpeedToDestroy, perkMayhem, perkDuration, superSpeedDuration, scoreMultiplier, shake);
};

struct GameplaySettings {
	BuildingSettings garden;
	BuildingSettings house;
	BuildingSettings gaspump;
	BuildingSettings skyscraper;
	BuildingSettings nuclear;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(GameplaySettings, garden, house, gaspump, skyscraper, nuclear);
};

struct Settings {
	RenderSettings render;
	CameraSettings camera;
	CarSettings car;
	GameplaySettings gameplay;

	void update(const Storage& storage);

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Settings, camera, car, gameplay);
};

void Settings::update(const Storage& storage) {
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
		}

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Zoom", &camera.zoom, 0.01f, 0.01f, 10);
		}

		if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat("Size", &car.size, 0.01f, 0.01f, 5);
			ImGui::DragFloat("Base Speed", &car.baseSpeed, 0.01f);
			ImGui::DragFloat("Super Speed", &car.superSpeed, 0.01f);
			ImGui::DragFloat("Mouse Sensibility", &car.mouseSensibility, 0.01f);
			ImGui::SliderFloat("Grip", &car.grip, 0, 1);
		}

		if (ImGui::CollapsingHeader("Gameplay", ImGuiTreeNodeFlags_DefaultOpen)) {
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
				ImGui::DragFloat("Min Mayhem To Destroy", &entry.second->minMayhemToDestroy, 0.01f);
				ImGui::DragFloat("Min Speed To Destroy", &entry.second->minSpeedToDestroy, 0.01f);
				ImGui::DragFloat("Perk Duration", &entry.second->perkDuration, 0.01f);
				ImGui::DragFloat("Perk Mayhem", &entry.second->perkMayhem, 0.01f);
				ImGui::DragFloat("SuperSpeed Duration", &entry.second->superSpeedDuration, 0.01f);
				ImGui::DragFloat("Score Multiplier", &entry.second->scoreMultiplier, 0.01f);
				ImGui::DragFloat("Shake Time", &entry.second->shake.shakeTime, 0.01f);
				ImGui::DragFloat("Shake Period", &entry.second->shake.shakePeriod, 0.01f);
				ImGui::DragFloat("Shake Strength", &entry.second->shake.shakeStrength, 0.01f);
				ImGui::Separator();
				ImGui::PopID();
			}
		}
	}
	ImGui::End();
}

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
	void shake(const ShakeSettings& shake_settings);
	void update(const std::chrono::microseconds& delta_time);

	float2 getMovement() const {
		return movement;
	}

private:
	std::chrono::microseconds maxDuration = std::chrono::microseconds(0);
	std::chrono::microseconds durationLeft = std::chrono::microseconds(0);
	float period = 1;
	float strength = 0;

	float2 movement;

	static float2 shakeSinusoidal(const float time, const float period);
};

void CameraShaker::shake(const ShakeSettings& shake_settings) {
	maxDuration = TimeUtils::floatSecondsToMicroseconds(shake_settings.shakeTime);
	durationLeft = maxDuration;
	period = shake_settings.shakePeriod;
	strength = shake_settings.shakeStrength;
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
	Session(const Settings& _settings, const Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, const std::filesystem::path& level_path);
	~Session();

	void update(const std::chrono::microseconds delta_real_time);
	void render();

	void pause(const bool _paused) {
		paused = _paused;
	}

	bool isPaused() const {
		return paused;
	}

	std::chrono::microseconds getSessionTime() {
		return sessionTime;
	}

	int getScore() const {
		return score;
	}

private:
	enum class BuildingType {
		None,
		Impassable,
		Garden,
		House,
		GasPump,
		Skyscraper,
		Nuclear,
	};

	struct CellState {
		int groundTile;
		int buildingTile;
		BuildingType buildingType;
		bool destructed;
		std::chrono::microseconds destructionTime;
		SDL_Texture* skidTexture;
	};

	struct CarPerk {
		std::chrono::microseconds durationLeft;
		float mayhem;
	};

	struct CarStats {
		float mayhem;
		float speed;
	};

	const Settings& settings;
	const Content& content;
	SDL_Window* window;
	SDL_Renderer* renderer;
	int2 worldSize;
	float2 worldStartingPosition = {0.5f, 0.5f};
	bool renderEnableGrid = false;
	float2 cameraPosition = {0, 0};
	bool cameraFollowCar = true;
	CameraShaker cameraShaker;
	bool inputCaptureMouse = false;

	float2 carPrevPosition = {0.5f, 0.5f};
	float2 carPosition = {0.5f, 0.5f};
	float2 carForwardDirection = {1, 0};
	float2 carSteerDirection = {1, 0};
	bool carSkid = false;

	std::list<CarPerk> carPerks;
	std::chrono::microseconds carSuperSpeedDuration;

	std::unique_ptr<tson::Map> level;
	std::vector<CellState> cells;

	std::chrono::microseconds sessionTime;
	std::optional<std::chrono::microseconds> finishTime;
	bool paused = false;
	float score = 0;

	float2 getRenderTileSize() const;
	float2 getRenderDirectionX() const;
	float2 getRenderDirectionY() const;
	float2 worldToRender(const float2 world_coords) const;

	CarStats updateCarPerks(const std::chrono::microseconds delta_time);
	void updateCarDrive(const std::chrono::microseconds delta_time, const CarStats& car_stats);
	void updateCarCollisions(const std::chrono::microseconds game_time, const CarStats& stats);
	bool isGameFinished() const;

	void renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture, SDL_Renderer* renderer);
	void renderGrid();
};

Session::Session(const Settings& _settings, const Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, const std::filesystem::path& level_path)
	: settings(_settings), content(_content), window(_window), renderer(_renderer), sessionTime(0) {

	tson::Tileson parser(std::unique_ptr<tson::NlohmannJson>(new tson::NlohmannJson));
	level = parser.parse(level_path);
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

	if (level->getProp("startX") && level->getProp("startY")) {
		worldStartingPosition = float2(level->getProp("startX")->getValue<int>(), level->getProp("startY")->getValue<int>()) + float2(0.5f);
	}
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

void Session::update(const std::chrono::microseconds delta_real_time) {
	bool debug_simulation = false;

	//---------- Debug Ui ----------//

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
		}

		if (ImGui::CollapsingHeader("Input", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_M);
			if (ImGui::Checkbox("Capture Mouse", &inputCaptureMouse)) {
				const bool mouse_mode_ok = SDL_SetWindowRelativeMouseMode(window, inputCaptureMouse);
				SDL_assert(mouse_mode_ok);
			}
		}

		if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_H);
			if (ImGui::Button("Return Home")) {
				carPosition = worldStartingPosition;
				carForwardDirection = {1, 0};
				carSteerDirection = {1, 0};
				carSkid = false;
				carPerks.clear();
				carSuperSpeedDuration = std::chrono::microseconds(0);
				for (CellState& cell : cells) {
					cell.destructed = 0;
					cell.destructionTime = std::chrono::microseconds(0);
				}
				sessionTime = std::chrono::microseconds(0);
				finishTime.reset();
				score = 0;
			}
			ImGui::SameLine();
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_D);
			if (ImGui::Button("Debug Simulation")) {
				debug_simulation = true;
			}
			ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_P);
			ImGui::Checkbox("Pause", &paused);

			ImGui::BeginDisabled(!(paused));
			ImGui::DragFloat2("Position##Car", &carPosition.x, 0.01f);
			ImGui::DragFloat2("Forward", &carForwardDirection.x, 0.01f);
			ImGui::DragFloat2("Steer", &carSteerDirection.x, 0.01f);
			ImGui::Checkbox("Skid", &carSkid);
			ImGui::EndDisabled();
		}
	}
	ImGui::End();

	//---------- Car ----------//

	CarStats car_stats{.mayhem = 0, .speed = 0};

	if (!paused) {
		if (debug_simulation) {
			__debugbreak();
		}

		sessionTime += delta_real_time;
		car_stats = updateCarPerks(delta_real_time);
		updateCarDrive(delta_real_time, car_stats);
		updateCarCollisions(sessionTime, car_stats);
		cameraShaker.update(delta_real_time);
	}

	if (cameraFollowCar) {
		cameraPosition = carPosition + cameraShaker.getMovement();
	}

	//---------- End Game ----------//

	if (!finishTime.has_value() && isGameFinished()) {
		finishTime = sessionTime;
	}

	//---------- Stats ----------//

	if (ImGui::Begin("Car Stats")) {
		ImGui::BeginDisabled(paused);
		ImGui::Text("Mayhem: %f", car_stats.mayhem);
		ImGui::Text("Speed: %f", car_stats.speed);
		ImGui::Text("Super Speed Duration: %d", carSuperSpeedDuration.count());
		ImGui::Text("Perks: %d", carPerks.size());
		ImGui::Text("Session Time: %.2fs", TimeUtils::microsecondsToFloatSeconds(sessionTime));
		if (!finishTime.has_value()) {
			ImGui::Text("Finish Time: none");
		} else {
			ImGui::Text("Finish Time: %.2fs", TimeUtils::microsecondsToFloatSeconds(*finishTime));
		}
		ImGui::Text("Score: %f", score);
		ImGui::EndDisabled();
	}
	ImGui::End();
}

Session::CarStats Session::updateCarPerks(const std::chrono::microseconds delta_time) {
	CarStats stats;

	stats.mayhem = 0;
	for (auto perk_iter = carPerks.begin(); perk_iter != carPerks.end();) {
		if (perk_iter->durationLeft < delta_time) {
			perk_iter = carPerks.erase(perk_iter);
		} else {
			perk_iter->durationLeft -= delta_time;
			stats.mayhem += perk_iter->mayhem;
			++perk_iter;
		}
	}

	if (carSuperSpeedDuration > delta_time) {
		stats.speed = settings.car.superSpeed;
		carSuperSpeedDuration -= delta_time;
	} else {
		stats.speed = settings.car.baseSpeed;
		carSuperSpeedDuration = std::chrono::microseconds(0);
	}

	return stats;
}

void Session::updateCarDrive(const std::chrono::microseconds delta_time, const CarStats& car_stats) {
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
	carPosition += carForwardDirection * car_stats.speed * delta_secs;

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

void Session::updateCarCollisions(const std::chrono::microseconds game_time, const CarStats& stats) {
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

		if (stats.mayhem >= building_settings->minMayhemToDestroy && stats.speed >= building_settings->minSpeedToDestroy) {
			cell.destructed = true;
			cell.destructionTime = game_time;
			carPerks.emplace_back(CarPerk{.durationLeft = TimeUtils::floatSecondsToMicroseconds(building_settings->perkDuration), .mayhem = building_settings->perkMayhem});
			carSuperSpeedDuration += TimeUtils::floatSecondsToMicroseconds(building_settings->superSpeedDuration);
			cameraShaker.shake(building_settings->shake);

			score += stats.mayhem * stats.speed * building_settings->scoreMultiplier;
		} else {
			bouncing = true;
		}
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
	}
}

bool Session::isGameFinished() const {
	return std::all_of(cells.begin(), cells.end(), [](const CellState& cell) { return cell.buildingType == BuildingType::None || cell.destructed; });
}

void Session::renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture, SDL_Renderer* renderer) {
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
			renderEntity(float2(j, i) + float2(0.5f), 1, ground_texture, renderer);
		}
	}

	//---------- Skids ----------//

	for (int i = 0; i < worldSize.y; ++i) {
		for (int j = 0; j < worldSize.x; ++j) {
			const CellState& cell = cells.at(i * worldSize.x + j);
			renderEntity(float2(j, i) + float2(0.5f), 1, cell.skidTexture, renderer);
		}
	}

	//---------- Buildings and Car ----------//
	{
		auto get_destructed_frame = [this](const CellState& cell, const Flipbook& flipbook) -> SDL_Texture* {
			if (cell.destructed) {
				const int usec_per_frame = 1000000 / 45;
				const int frame = (sessionTime - cell.destructionTime).count() / usec_per_frame;
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
					auto get_animation_frame = [session_time = sessionTime](const CellState& cell, const Flipbook& flipbook) -> SDL_Texture* {
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
					renderEntity(float2(j, i) + float2(0.5f), 1, building_texture, renderer);
				}

				//---------- Car ----------//

				if (int2(carPosition) == int2(j, i)) {
					const float car_rotation = glm::orientedAngle(carSteerDirection, float2(1, 0));
					const float pi2 = std::numbers::pi * 2;
					const float rotation = car_rotation > 0 ? car_rotation : pi2 - std::fmod(-car_rotation, pi2);
					const int frame = int(rotation / pi2 * content.carFlipbook.textures.size()) % content.carFlipbook.textures.size();
					renderEntity(carPosition, settings.car.size, content.carFlipbook.textures.at(frame), renderer);
				}

				//---------- Non-Destroyed Building ----------//

				if (cell.buildingType != BuildingType::None && !cell.destructed) {
					SDL_Texture* building_texture = content.buildingsFlipbook.textures.at(cell.buildingTile);
					renderEntity(float2(j, i) + float2(0.5f), 1, building_texture, renderer);
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

class Launcher {
public:
	Launcher(const Settings& _settings, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, Rml::Context* ui_context);
	~Launcher();

	void update(const std::chrono::microseconds delta_real_time);
	void render();

	bool isShutdownRequested() const {
		return shutdownRequested;
	}

private:
	const Settings& settings;
	Content& content;
	SDL_Window* window;
	SDL_Renderer* renderer;

	Rml::Context* uiContext;
	Rml::DataModelHandle launcherModel;
	Rml::DataModelHandle sessionModel;
	std::string buildVersion = "v0";
	std::string buildDate = __DATE__ " " __TIME__;
	int mayhem = 0;
	int speed = 0;
	std::string time;
	int score = 0;

	Rml::ElementDocument* document = nullptr;

	std::unique_ptr<Session> session;
	std::list<std::function<void()>> deferredUpdates;
	bool shutdownRequested = false;

	void goToPage(const std::string& page);
	void createSession();
	void destroySession();

	void selectLanguageItalian(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void selectLanguageEnglish(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void selectLanguageSpanish(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToHowToPlay(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToDrive(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToCredits(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void goToHome(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void quit(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void pauseSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void resumeSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
	void exitSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params);
};

Launcher::Launcher(const Settings& _settings, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer, Rml::Context* ui_context)
	: settings(_settings), content(_content), window(_window), renderer(_renderer), uiContext(ui_context) {
	{
		Rml::DataModelConstructor launcher_model_ctor = uiContext->CreateDataModel("launcher");
		launcher_model_ctor.BindEventCallback("selectLanguageItalian", &Launcher::selectLanguageItalian, this);
		launcher_model_ctor.BindEventCallback("selectLanguageEnglish", &Launcher::selectLanguageEnglish, this);
		launcher_model_ctor.BindEventCallback("selectLanguageSpanish", &Launcher::selectLanguageSpanish, this);
		launcher_model_ctor.BindEventCallback("goToHowToPlay", &Launcher::goToHowToPlay, this);
		launcher_model_ctor.BindEventCallback("goToDrive", &Launcher::goToDrive, this);
		launcher_model_ctor.BindEventCallback("goToCredits", &Launcher::goToCredits, this);
		launcher_model_ctor.BindEventCallback("goToHome", &Launcher::goToHome, this);
		launcher_model_ctor.BindEventCallback("quit", &Launcher::quit, this);
		launcher_model_ctor.Bind("buildVersion", &buildVersion);
		launcher_model_ctor.Bind("buildDate", &buildDate);
		launcherModel = launcher_model_ctor.GetModelHandle();
	}

	{
		Rml::DataModelConstructor session_model_ctor = uiContext->CreateDataModel("session");
		session_model_ctor.BindEventCallback("pauseSession", &Launcher::pauseSession, this);
		session_model_ctor.BindEventCallback("resumeSession", &Launcher::resumeSession, this);
		session_model_ctor.BindEventCallback("exitSession", &Launcher::exitSession, this);
		session_model_ctor.Bind("mayhem", &mayhem);
		session_model_ctor.Bind("speed", &speed);
		session_model_ctor.Bind("time", &time);
		session_model_ctor.Bind("score", &score);
		sessionModel = session_model_ctor.GetModelHandle();
	}

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

void Launcher::update(const std::chrono::microseconds delta_real_time) {
	for (auto& deferred_update_func : deferredUpdates) {
		deferred_update_func();
	}
	deferredUpdates.clear();

	if (session) {
		session->update(delta_real_time);

		score = session->getScore();
		sessionModel.DirtyVariable("score");

		time = TimeUtils::microsecondsToString(session->getSessionTime());
		sessionModel.DirtyVariable("time");
	}
}

void Launcher::render() {
	if (session) {
		session->render();
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

void Launcher::createSession() {
	session.reset();
	session.reset(new Session(settings, content, window, renderer, "levels/testbed.tmj"));
}

void Launcher::destroySession() {
	session.reset();
}

void Launcher::selectLanguageItalian(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {}
void Launcher::selectLanguageEnglish(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {}
void Launcher::selectLanguageSpanish(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {}

void Launcher::goToHowToPlay(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/howto.html"));
}

void Launcher::goToDrive(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/ingame.html"));
	deferredUpdates.emplace_back(std::bind(&Launcher::createSession, this));
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

void Launcher::pauseSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back([this]() { session->pause(true); });
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/pause.html"));
}

void Launcher::resumeSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/ingame.html"));
	deferredUpdates.emplace_back([this]() { session->pause(false); });
}

void Launcher::exitSession(Rml::DataModelHandle handle, Rml::Event& event, const Rml::VariantList& params) {
	deferredUpdates.emplace_back(std::bind(&Launcher::destroySession, this));
	deferredUpdates.emplace_back(std::bind(&Launcher::goToPage, this, "ui/home.html"));
}

/////////
// App //
/////////

struct App {
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::unique_ptr<Storage> storage;
	Settings settings;
	std::unique_ptr<SystemInterface_SDL> uiSystem;
	std::unique_ptr<RenderInterface_SDL> uiRenderer;
	Rml::Context* uiContext;
	std::unique_ptr<Content> content;
	std::unique_ptr<Launcher> launcher;

	bool showDebugUi = false;
	std::chrono::microseconds currentTime;
	std::chrono::microseconds deltaTime;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	bool init_ok = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
	SDL_assert(init_ok);

	App* app = new App;
	app->window = SDL_CreateWindow("Drunken Lobster Driving", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	SDL_assert(app->window);

	app->renderer = SDL_CreateRenderer(app->window, nullptr);
	SDL_assert(app->renderer);

	const bool vsync_ok = SDL_SetRenderVSync(app->renderer, 1);
	SDL_assert(vsync_ok);

	{
		ImGuiContext* imgui_context = ImGui::CreateContext();
		SDL_assert(imgui_context);

		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		const float main_scale = SDL_GetWindowDisplayScale(app->window);
		ImGui::StyleColorsDark();
		ImGui::GetStyle().FontScaleDpi = main_scale;

		const ImFont* base_font = ImGui::GetIO().Fonts->AddFontFromFileTTF("fonts/NotoSans-Regular.ttf", 18);
		SDL_assert(base_font);

		const bool imgui_init1_ok = ImGui_ImplSDL3_InitForSDLRenderer(app->window, app->renderer);
		SDL_assert(imgui_init1_ok);

		const bool imgui_init2_ok = ImGui_ImplSDLRenderer3_Init(app->renderer);
		SDL_assert(imgui_init2_ok);
	}

	app->storage.reset(new Storage);

	if (app->storage->exists("settings.json")) {
		const nlohmann::json json = app->storage->loadJson("settings.json");
		app->settings = json;
	}

	{
		app->uiSystem.reset(new SystemInterface_SDL);
		app->uiRenderer.reset(new RenderInterface_SDL(app->renderer));
		Rml::SetSystemInterface(app->uiSystem.get());
		Rml::SetRenderInterface(app->uiRenderer.get());

		const bool ui_initialize_ok = Rml::Initialise();
		SDL_assert(ui_initialize_ok);

		app->uiContext = Rml::CreateContext("RmlContext", Rml::Vector2i(1920, 1080));
		SDL_assert(app->uiContext);

		const bool debugger_init_ok = Rml::Debugger::Initialise(app->uiContext);
		SDL_assert(debugger_init_ok);

		const bool load_font1_ok = Rml::LoadFontFace("fonts/NotoSans-Regular.ttf", true);
		SDL_assert(load_font1_ok);

		const bool load_font2_ok = Rml::LoadFontFace("fonts/Lobster-Regular.ttf", true);
		SDL_assert(load_font2_ok);
	}

	app->content.reset(new Content(*app->storage, app->renderer));
	app->launcher.reset(new Launcher(app->settings, *app->content, app->window, app->renderer, app->uiContext));
	app->currentTime = std::chrono::microseconds(SDL_GetTicksNS() / 1000);

	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	App* app = static_cast<App*>(appstate);

	if (app->showDebugUi) {
		ImGui_ImplSDL3_ProcessEvent(event);
	}
	RmlSDL::InputEventHandler(app->uiContext, app->window, *event);

	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	} else if (event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
		const float main_scale = SDL_GetWindowDisplayScale(app->window);
		ImGui::GetStyle().FontScaleDpi = main_scale;
	} else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_F1) {
		app->showDebugUi = !app->showDebugUi;
	} else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_F12) {
		Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	App* app = static_cast<App*>(appstate);

	const std::chrono::microseconds now(SDL_GetTicksNS() / 1000);
	app->deltaTime = now - app->currentTime;
	app->currentTime = now;

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	app->settings.update(*app->storage);
	app->launcher->update(app->deltaTime);

	const bool set_color_ok = SDL_SetRenderDrawColorFloat(app->renderer, 0, 0, 0, 1);
	SDL_assert(set_color_ok);

	const bool clear_ok = SDL_RenderClear(app->renderer);
	SDL_assert(clear_ok);

	app->launcher->render();

	{
		const bool ui_update_ok = app->uiContext->Update();
		SDL_assert(ui_update_ok);

		const bool ui_render_ok = app->uiContext->Render();
		SDL_assert(ui_render_ok);
	}

	ImGui::Render();
	if (app->showDebugUi) {
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app->renderer);
	}

	const bool present_ok = SDL_RenderPresent(app->renderer);
	SDL_assert(present_ok);

	return app->launcher->isShutdownRequested() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	App* app = static_cast<App*>(appstate);

	app->launcher.reset();
	app->content.reset();

	Rml::Shutdown();
	app->uiRenderer.reset();
	app->uiSystem.reset();

	app->storage.reset();

	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext();

	SDL_DestroyRenderer(app->renderer);
	app->renderer = nullptr;

	SDL_DestroyWindow(app->window);
	app->window = nullptr;

	delete app;

	SDL_Quit();
}
