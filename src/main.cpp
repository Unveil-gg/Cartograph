// Enable SDL3 main callbacks - SDL owns the event loop
#define SDL_MAIN_USE_CALLBACKS 1

#include "App.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iostream>

// Global app instance (stored in appstate)
static Cartograph::App* g_app = nullptr;

/**
 * SDL3 callback: Application initialization.
 * Called once at startup before any other callbacks.
 */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    g_app = new Cartograph::App();
    
    if (!g_app->Init("Cartograph", 1280, 720)) {
        std::cerr << "Failed to initialize application" << std::endl;
        delete g_app;
        g_app = nullptr;
        return SDL_APP_FAILURE;
    }
    
    *appstate = g_app;
    return SDL_APP_CONTINUE;
}

/**
 * SDL3 callback: Per-frame iteration.
 * Called repeatedly by SDL, including during live window resize on macOS.
 */
SDL_AppResult SDL_AppIterate(void* appstate) {
    Cartograph::App* app = static_cast<Cartograph::App*>(appstate);
    return app->Iterate();
}

/**
 * SDL3 callback: Event handling.
 * Called for each SDL event before the next iteration.
 */
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    Cartograph::App* app = static_cast<Cartograph::App*>(appstate);
    return app->HandleEvent(event);
}

/**
 * SDL3 callback: Application shutdown.
 * Called once when the app is terminating.
 */
void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    (void)result;
    
    if (appstate) {
        Cartograph::App* app = static_cast<Cartograph::App*>(appstate);
        app->Shutdown();
        delete app;
    }
    
    g_app = nullptr;
}
