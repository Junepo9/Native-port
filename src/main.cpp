#include <SDL.h>
int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) return 1;
    SDL_Window* w = SDL_CreateWindow("OoT3D Port", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 800, 480, SDL_WINDOW_SHOWN);
    bool run = true;
    while (run) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) if (e.type == SDL_QUIT) run = false;
        SDL_Delay(16);
    }
    SDL_DestroyWindow(w);
    SDL_Quit();
}
