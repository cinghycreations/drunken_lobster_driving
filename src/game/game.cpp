#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <fmt/format.h>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <memory>
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
	Content& content;
	SDL_Renderer* renderer;
	int2 worldSize = {5, 5};
	float2 cameraPosition = {0, 0};
	float renderScale = 1;
	float2 carPosition = {0.5f, 0.5f};
	float carSize = 0.2f;
	float carRotation = 0;

	Session(Content& _content, SDL_Renderer* _renderer) : content(_content), renderer(_renderer) {}

	float2 getIsoTileSize() const {
		int render_width = 0, render_height = 0;
		const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
		SDL_assert(output_size_ok);

		const float2 render_size(render_width, render_height);
		const float2 tile_size = float2(render_size.y * 0.5f * 2, render_size.y * 0.5f) * renderScale;
		return tile_size;
	}

	float2 getIsoDirectionX() const {
		return getIsoTileSize() * 0.5f;
	}

	float2 getIsoDirectionY() const {
		return getIsoTileSize() * 0.5f * float2(-1, 1);
	}

	float2 worldToRender(const float2 world_coords) const {
		int render_width = 0, render_height = 0;
		const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
		SDL_assert(output_size_ok);

		const float2 render_size(render_width, render_height);
		const float2 x_dir = getIsoDirectionX();
		const float2 y_dir = getIsoDirectionY();
		const float2 render_pos = render_size * 0.5f + (world_coords - cameraPosition).x * x_dir + (world_coords - cameraPosition).y * y_dir;
		return render_pos;
	}

	void update() {
		if (ImGui::Begin("Session")) {
			ImGui::DragFloat2("Camera Position", &cameraPosition.x, 0.01f);
			ImGui::DragFloat("Render Scale", &renderScale, 0.01f, 0.01f, 10);
			ImGui::DragFloat2("Car Position", &carPosition.x, 0.01f);
			ImGui::DragFloat("Car Size", &carSize, 0.01f, 0.01f, 5);
			ImGui::SliderAngle("Car Rotation", &carRotation);
		}
		ImGui::End();
	}

	void renderEntity(const float2& world_coords, const float world_size, SDL_Texture* texture, SDL_Renderer* renderer) {
		const float2 bottom_left = worldToRender(world_coords) + getIsoDirectionY() * world_size;
		const float2 bottom_right = worldToRender(world_coords) + getIsoDirectionX() * world_size;
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
			const float pi2 = std::numbers::pi * 2;
			const float rotation = carRotation > 0 ? carRotation : pi2 - std::fmod(-carRotation, pi2);
			const int frame = int(rotation / pi2 * content.carFlipbook.textures.size()) % content.carFlipbook.textures.size();
			renderEntity(carPosition, carSize, content.carFlipbook.textures.at(frame), renderer);
		}
	}
};

struct App {
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::unique_ptr<Storage> storage;
	std::unique_ptr<Content> content;
	std::unique_ptr<Session> session;
	bool showDebugUi = false;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	bool init_ok = SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO);
	SDL_assert(init_ok);

	App* app = new App;
	app->window = SDL_CreateWindow("Drunken Lobser Driving", 1920, 1080, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
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
	app->content.reset(new Content(*app->storage, app->renderer));
	app->session.reset(new Session(*app->content, app->renderer));

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

	ImGui_ImplSDLRenderer3_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);

	ImGui::ShowDemoWindow();
	app->session->update();

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

	SDL_Quit();

	delete app;
}
