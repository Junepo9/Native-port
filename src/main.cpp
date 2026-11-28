#include <SDL.h>
#include <cstdio>

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) != 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("OoT3D Port", SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED, 800, 480, SDL_WINDOW_SHOWN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const SDL_Color colors[] = {
        {20, 20, 60, 255}, {10, 80, 40, 255}, {90, 30, 30, 255}, {60, 60, 10, 255}};
    const int numColors = 4;
    int colorIndex = 0;
    int touchX = 400;
    int touchY = 240;

    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            if (e.type == SDL_MOUSEBUTTONDOWN) {
                colorIndex = (colorIndex + 1) % numColors;
                touchX = e.button.x;
                touchY = e.button.y;
            }
            if (e.type == SDL_MOUSEMOTION) {
                touchX = e.motion.x;
                touchY = e.motion.y;
            }
        }

        const SDL_Color& c = colors[colorIndex];
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, 255);
        SDL_RenderClear(renderer);

        SDL_Rect box = {touchX - 40, touchY - 40, 80, 80};
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderFillRect(renderer, &box);

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
