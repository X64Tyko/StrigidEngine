#include <iostream>
#include <SDL3/SDL.h>

// If this runs, your Linker found SDL3.lib
// If the window opens, your DLL copy step worked.
int main([[maybe_unused]]int argc, [[maybe_unused]] char* argv[]) {
    // 1. Initialize
    if (SDL_Init(SDL_INIT_VIDEO) == false) {
        std::cerr << "SDL Failed to Init: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 2. Create Window
    SDL_Window* window = SDL_CreateWindow(
        "Strigid Engine | SDL3 Test", 
        800, 600, 
        SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "Window Failed: " << SDL_GetError() << std::endl;
        return -1;
    }

    // 3. Loop (Hack for testing)
    std::cout << "Engine is Running! (Check Task Manager to kill me or close window)" << std::endl;
    
    bool running = true;
    while(running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }
    }

    // 4. Cleanup
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}