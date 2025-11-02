#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <numbers>
#include <string_view>

using float2 = glm::vec2;
using int2 = glm::ivec2;

class Storage {
public:
	bool exists(const std::filesystem::path& path) const {
		return SDL_GetPathInfo(path.string().c_str(), nullptr);
	}

	SDL_IOStream* open(const std::filesystem::path& path) const {
		return SDL_IOFromFile(path.string().c_str(), "rb");
	}

	nlohmann::json loadJson(const std::filesystem::path& path) const {
		SDL_IOStream* stream = open(path);
		size_t data_size = 0;
		void* data = SDL_LoadFile_IO(stream, &data_size, true);
		SDL_assert(data);

		nlohmann::json json = nlohmann::json::parse(static_cast<char*>(data), static_cast<char*>(data) + data_size);
		return json;
	}

	void saveJson(const std::filesystem::path& path, const nlohmann::json& json) const {
		SDL_IOStream* stream = SDL_IOFromFile(path.string().c_str(), "wb");
		SDL_assert(stream);

		const std::string data = json.dump(1, '\t');

		const bool save_ok = SDL_SaveFile_IO(stream, data.data(), data.size(), true);
		SDL_assert(save_ok);
	}
};

struct CameraSettings {
	float zoom = 1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CameraSettings, zoom);
};

struct CarSettings {
	float size = 0.2f;
	float speed = 0.1f;
	float mouseSensibility = 0.01f;
	float grip = 1;

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(CarSettings, size, speed, mouseSensibility, grip);
};

struct Settings {
	CameraSettings camera;
	CarSettings car;

	void update(const Storage& storage) {
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

			if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat("Zoom", &camera.zoom, 0.01f, 0.01f, 10);
			}

			if (ImGui::CollapsingHeader("Car", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat("Size", &car.size, 0.01f, 0.01f, 5);
				ImGui::DragFloat("Speed", &car.speed, 0.01f);
				ImGui::DragFloat("Mouse Sensibility", &car.mouseSensibility, 0.01f);
				ImGui::SliderFloat("Grip", &car.grip, 0, 1);
			}
		}
		ImGui::End();
	}

	NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Settings, camera, car);
};

struct Flipbook {
	std::vector<SDL_Texture*> textures;

	Flipbook() {}
	Flipbook(const Flipbook&) = delete;
	Flipbook(Flipbook&&) = default;
	Flipbook& operator=(const Flipbook&) = delete;
	Flipbook& operator=(Flipbook&&) = default;

	Flipbook(Storage& storage, SDL_Renderer* renderer, const char* pattern, const int starting_index = 0) {
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

	~Flipbook() {
		for (SDL_Texture* texture : textures) {
			SDL_DestroyTexture(texture);
		}
		textures.clear();
	}
};

struct Content {
	Flipbook carFlipbook;
	Flipbook roadsFlipbook;

	Content(Storage& storage, SDL_Renderer* renderer) : carFlipbook(storage, renderer, "car/car{:04}.png", 1), roadsFlipbook(storage, renderer, "roads/road{:04}.png", 1) {}
};

struct Session {
	const Settings& settings;
	Content& content;
	SDL_Window* window;
	SDL_Renderer* renderer;
	int2 worldSize = {5, 5};
	float2 cameraPosition = {0, 0};
	bool cameraFollowCar = true;
	bool inputCaptureMouse = false;

	bool carPaused = false;
	float2 carPosition = {0.5f, 0.5f};
	float2 carForwardDirection = {1, 0};
	float2 carSteerDirection = {1, 0};
	bool carSkid = false;

	Session(const Settings& _settings, Content& _content, SDL_Window* _window, SDL_Renderer* _renderer)
		: settings(_settings), content(_content), window(_window), renderer(_renderer) {}

	float2 getRenderTileSize() const {
		int render_width = 0, render_height = 0;
		const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
		SDL_assert(output_size_ok);

		const float2 render_size(render_width, render_height);
		const float2 tile_size = float2(render_size.y * 0.5f * 2, render_size.y * 0.5f) * settings.camera.zoom;
		return tile_size;
	}

	float2 getRenderDirectionX() const {
		return getRenderTileSize() * 0.5f;
	}

	float2 getRenderDirectionY() const {
		return getRenderTileSize() * 0.5f * float2(-1, 1);
	}

	float2 worldToRender(const float2 world_coords) const {
		int render_width = 0, render_height = 0;
		const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
		SDL_assert(output_size_ok);

		const float2 x_dir = getRenderDirectionX();
		const float2 y_dir = getRenderDirectionY();
		const float2 render_pos = float2(render_width, render_height) * 0.5f + (world_coords - cameraPosition).x * x_dir + (world_coords - cameraPosition).y * y_dir;
		return render_pos;
	}

	void update(const std::chrono::microseconds delta_time) {
		bool debug_simulation = false;

		//---------- Debug Ui ----------//

		if (ImGui::Begin("Session")) {
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
					carPosition = {0.5f, 0.5f};
					carForwardDirection = {1, 0};
					carSteerDirection = {1, 0};
					carSkid = false;
				}
				ImGui::SameLine();
				ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_D);
				if (ImGui::Button("Debug Simulation")) {
					debug_simulation = true;
				}
				ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_P);
				ImGui::Checkbox("Pause", &carPaused);

				ImGui::BeginDisabled(!(carPaused));
				ImGui::DragFloat2("Position##Car", &carPosition.x, 0.01f);
				ImGui::DragFloat2("Forward", &carForwardDirection.x, 0.01f);
				ImGui::DragFloat2("Steer", &carSteerDirection.x, 0.01f);
				ImGui::Checkbox("Skid", &carSkid);
				ImGui::EndDisabled();
			}
		}
		ImGui::End();

		//---------- Car ----------//

		if (!carPaused) {
			if (debug_simulation) {
				__debugbreak();
			}

			float mouse_delta_x;
			SDL_GetRelativeMouseState(&mouse_delta_x, nullptr);

			const float steer_angle = mouse_delta_x * settings.car.mouseSensibility;
			carSteerDirection = glm::rotate(glm::normalize(carSteerDirection), steer_angle);

			const float delta_secs = float(delta_time.count()) / 1000000;
			const float wanted_angle = glm::orientedAngle(carForwardDirection, carSteerDirection);
			const float max_angle = glm::radians(settings.car.grip * 360) * delta_secs;
			carSkid = std::fabs(wanted_angle) >= max_angle;
			carForwardDirection = glm::rotate(carForwardDirection, std::clamp(wanted_angle, -max_angle, max_angle));
			carPosition += carForwardDirection * settings.car.speed * delta_secs;
		}

		if (cameraFollowCar) {
			cameraPosition = carPosition;
		}
	}

	void renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture, SDL_Renderer* renderer) {
		const float2 bottom_left = worldToRender(world_coords) + getRenderDirectionY() * world_size;
		const float2 bottom_right = worldToRender(world_coords) + getRenderDirectionX() * world_size;
		const float render_width = bottom_right.x - bottom_left.x;
		const float render_height = float(texture->h) / float(texture->w) * render_width;
		const SDL_FRect rect{.x = bottom_left.x, .y = bottom_left.y - render_height, .w = render_width, .h = render_height};
		const bool render_ok = SDL_RenderTexture(renderer, texture, nullptr, &rect);
		SDL_assert(render_ok);
	}

	void render() {
		//---------- Grid ----------//
		{
			const bool set_color_ok = SDL_SetRenderDrawColorFloat(renderer, 1, 1, 1, 1);
			SDL_assert(set_color_ok);

			for (int i = 0; i <= worldSize.y; ++i) {
				const float2 a = worldToRender(float2(0, i));
				const float2 b = worldToRender(float2(worldSize.x, i));
				const bool render_line_ok = SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
				SDL_assert(render_line_ok);
			}

			for (int j = 0; j <= worldSize.y; ++j) {
				const float2 a = worldToRender(float2(j, 0));
				const float2 b = worldToRender(float2(j, worldSize.y));
				const bool render_line_ok = SDL_RenderLine(renderer, a.x, a.y, b.x, b.y);
				SDL_assert(render_line_ok);
			}
		}

		//---------- Car ----------//
		{
			const float car_rotation = glm::orientedAngle(carSteerDirection, float2(1, 0));
			const float pi2 = std::numbers::pi * 2;
			const float rotation = car_rotation > 0 ? car_rotation : pi2 - std::fmod(-car_rotation, pi2);
			const int frame = int(rotation / pi2 * content.carFlipbook.textures.size()) % content.carFlipbook.textures.size();
			renderEntity(carPosition, settings.car.size, content.carFlipbook.textures.at(frame), renderer);
		}
	}
};

struct App {
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::unique_ptr<Storage> storage;
	Settings settings;
	std::unique_ptr<Content> content;
	std::unique_ptr<Session> session;
	bool showDebugUi = false;
	std::chrono::microseconds currentTime;
	std::chrono::microseconds deltaTime;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	bool init_ok = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
	SDL_assert(init_ok);

	App* app = new App;
	app->window = SDL_CreateWindow("Drunken Lobser Driving", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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

	app->content.reset(new Content(*app->storage, app->renderer));
	app->session.reset(new Session(app->settings, *app->content, app->window, app->renderer));
	app->currentTime = std::chrono::microseconds(SDL_GetTicksNS() / 1000);

	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	App* app = static_cast<App*>(appstate);

	if (app->showDebugUi) {
		ImGui_ImplSDL3_ProcessEvent(event);
	}

	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	} else if (event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
		const float main_scale = SDL_GetWindowDisplayScale(app->window);
		ImGui::GetStyle().FontScaleDpi = main_scale;
	} else if (event->type == SDL_EVENT_KEY_DOWN && event->key.scancode == SDL_SCANCODE_F1) {
		app->showDebugUi = !app->showDebugUi;
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
	app->session->update(app->deltaTime);

	const bool set_color_ok = SDL_SetRenderDrawColorFloat(app->renderer, 0, 0, 0, 1);
	SDL_assert(set_color_ok);

	const bool clear_ok = SDL_RenderClear(app->renderer);
	SDL_assert(clear_ok);

	app->session->render();

	ImGui::Render();
	if (app->showDebugUi) {
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), app->renderer);
	}

	const bool present_ok = SDL_RenderPresent(app->renderer);
	SDL_assert(present_ok);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	App* app = static_cast<App*>(appstate);

	app->session.reset();
	app->content.reset();
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
