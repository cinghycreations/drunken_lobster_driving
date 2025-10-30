#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL_assert.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_render.h>
#include <glm/glm.hpp>
#include <memory>

using float2 = glm::vec2;
using int2 = glm::ivec2;

struct Session {
	SDL_Renderer* renderer;
	int2 worldSize = {5, 5};
	float2 cameraPosition = {0, 0};
	float renderScale = 1;

	Session(SDL_Renderer* _renderer) : renderer(_renderer) {}

	float2 worldToRender(const float2 world_coords) const {
		int render_width = 0, render_height = 0;
		const bool output_size_ok = SDL_GetRenderOutputSize(renderer, &render_width, &render_height);
		SDL_assert(output_size_ok);

		const float2 render_size(render_width, render_height);
		const float2 x_dir = float2(render_size.y * 0.5f * 0.5f * 2, render_size.y * 0.5f * 0.5f) * renderScale;
		const float2 y_dir = float2(-x_dir.x, x_dir.y);
		const float2 render_pos = render_size * 0.5f + (world_coords - cameraPosition).x * x_dir + (world_coords - cameraPosition).y * y_dir;
		return render_pos;
	}

	void render() {
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
};

struct App {
	SDL_Window* window = nullptr;
	SDL_Renderer* renderer = nullptr;
	std::unique_ptr<Session> session;
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

	app->session.reset(new Session(app->renderer));

	*appstate = app;
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	App* app = static_cast<App*>(appstate);

	if (event->type == SDL_EVENT_QUIT) {
		return SDL_APP_SUCCESS;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	App* app = static_cast<App*>(appstate);

	const bool set_color_ok = SDL_SetRenderDrawColorFloat(app->renderer, 0, 0, 0, 1);
	SDL_assert(set_color_ok);

	const bool clear_ok = SDL_RenderClear(app->renderer);
	SDL_assert(clear_ok);

	app->session->render();

	const bool present_ok = SDL_RenderPresent(app->renderer);
	SDL_assert(present_ok);

	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	App* app = static_cast<App*>(appstate);

	app->session.reset();

	SDL_DestroyWindow(app->window);
	app->window = nullptr;

	SDL_DestroyRenderer(app->renderer);
	app->renderer = nullptr;

	SDL_Quit();

	delete app;
}
