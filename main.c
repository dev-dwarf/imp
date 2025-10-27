#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>


#include "atlas.h"

static SDL_Window *window = 0;
static SDL_Renderer *renderer = 0;
static SDL_Surface *atlas_surface = 0; 
static SDL_Texture *atlas_texture = 0; 

#define ARRAY_LENGTH(A) (sizeof(A)/sizeof(*(A)))


#define WIDTH 640
#define HEIGHT 480

#define ASSERT_SDL(cond) do { if (!(cond)) { SDL_Log( "" __FILE__ ":%d: " #cond " gave SDL Error: %s", __LINE__, SDL_GetError()); return -1; } } while(0);

int main(int argc, char* argv[]) {
    ASSERT_SDL(SDL_Init(SDL_INIT_VIDEO) >= 0);
    ASSERT_SDL(window = SDL_CreateWindow("HelloWorld SDL3", WIDTH, HEIGHT, 0));
    ASSERT_SDL(renderer = SDL_CreateRenderer(window, NULL));
    
    SDL_Event event;
    bool running = true;

    SDL_FRect mouseRect;
    mouseRect.x = mouseRect.y = -1000;
    mouseRect.w = mouseRect.h = 50;

    // SDL_PixelFormat atlas_format = SDL_DEFINE_PIXELFORMAT(SDL_PIXELTYPE_ARRAYU8, SDL_ARRAYORDER_NONE, SDL_PACKEDLAYOUT_NONE, 8, 1);
    SDL_Palette *atlas_palette ;
    ASSERT_SDL(atlas_surface = SDL_CreateSurfaceFrom(ATLAS_WIDTH, ATLAS_HEIGHT, SDL_PIXELFORMAT_INDEX8, ATLAS_DATA, ATLAS_WIDTH));
    ASSERT_SDL(atlas_palette = SDL_CreatePalette(256));
    for (int i = 0; i < 256; i++) { atlas_palette->colors[i] = (SDL_Color) { i, i, i, 255}; }
    ASSERT_SDL(SDL_SetSurfacePalette(atlas_surface, atlas_palette));
    ASSERT_SDL(atlas_texture = SDL_CreateTextureFromSurface(renderer, atlas_surface));

    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_EVENT_QUIT: 
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.key == SDLK_ESCAPE) {
                        running = false;
                    }
                case SDL_EVENT_MOUSE_MOTION:
                    mouseRect.x = event.motion.x - (mouseRect.w / 2.0f);
                    mouseRect.y = event.motion.y - (mouseRect.h / 2.0f);
                    break;
            }
        }

        float height = HEIGHT;
        float width = WIDTH;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 128, 128, 128, 255);
        // SDL_RenderLine(renderer, 50, height / 2, width - 50, height / 2);
        // SDL_RenderLine(renderer, 50, 50, 50, height - 50);

        // for (int x = 50; x <= width - 50; x += 50) {
        //     SDL_RenderLine(renderer, x, height / 2 - 5, x, height / 2 + 5);
        // }
        // for (int y = 50; y <= height - 50; y += 50) {
        //     SDL_RenderLine(renderer, 45, y, 55, y);
        // }

        // SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        // SDL_FPoint points[360];
        
        // for (int x = 0; x < 360; x++) {
        //     float angle = 4 * x * M_PI / 180.0f;
        //     float y = sin(angle) * ((height - 100) / 4.0f);
        //     points[x].x = 50 + x * ((width - 100) / 360.0f);
        //     points[x].y = (height / 2.0f) - y;
        // }
        
        // SDL_RenderLines(renderer, points, 360);

        // SDL_RenderFillRect(renderer, &mouseRect);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        SDL_FRect r = {0, 0, ATLAS_WIDTH, ATLAS_HEIGHT};
        SDL_RenderTexture(renderer, atlas_texture, &r, &r);

        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);

        SDL_RenderRects(renderer, ATLAS_RECT, ARRAY_LENGTH(ATLAS_RECT));

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}


