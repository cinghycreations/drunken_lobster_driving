#define SDL_MAIN_USE_CALLBACKS
#include "gamelib.hpp"
#include <SDL3/SDL_main.h>

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
