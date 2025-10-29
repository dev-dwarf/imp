#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>
#include <stdlib.h>

static SDL_Window *window = 0;
static SDL_Renderer *renderer = 0;
static SDL_Surface *atlas_surface = 0; 
static SDL_Texture *atlas_texture = 0; 

#define ASSERT_SDL(cond) do { if (!(cond)) { SDL_Log( "" __FILE__ ":%d: " #cond " gave SDL Error: %s", __LINE__, SDL_GetError()); return -1; } } while(0);

#define WIDTH 640
#define HEIGHT 480

#include <stdio.h>
void imp_assert(char* err) {
  printf("%s", err);
}

#include "imp.h"

#include "atlas.h"

// vec2 measure_text(str s) {
//   vec2 out = {0};
//   for (int i = 0; i < s.len; i++) {
//     rect r = ATLAS_RECT[ATLAS_FONT + (unsigned char) s.str[i]];
//     out.h = MAX(r.h, out.h);
//     out.w += r.w + ATLAS_SPACING;
//   }
// }

void SDL_RenderText2D(imp_v2 p, imp_str s, imp_cf c) {
  SDL_FRect r = { p.x, p.y, 0, 0};
  SDL_SetRenderDrawColorFloat(renderer, c.r*c.a, c.g*c.a, c.b*c.a, c.a);
  for (int i = 0; i < s.len; i++) {
    imp_r2 ag = ATLAS_RECT[ATLAS_FONT + (unsigned char) s.str[i]];
    SDL_FRect g = { ag.x, ag.y, ag.w, ag.h };
    r.w = g.w;
    r.h = g.h;
    SDL_RenderTexture(renderer, atlas_texture, &g, &r);
    r.x += g.w + ATLAS_SPACING;
  }
}

int main(int argc, char* argv[]) {
  ASSERT_SDL(SDL_Init(SDL_INIT_VIDEO) >= 0);
  ASSERT_SDL(window = SDL_CreateWindow("IMP SDL3", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE));
  ASSERT_SDL(renderer = SDL_CreateRenderer(window, NULL));
  bool running = true;

  SDL_Palette *atlas_palette;
  ASSERT_SDL(atlas_surface = SDL_CreateSurfaceFrom(ATLAS_WIDTH, ATLAS_HEIGHT, SDL_PIXELFORMAT_INDEX8, ATLAS_DATA, ATLAS_WIDTH));
  ASSERT_SDL(atlas_palette = SDL_CreateSurfacePalette(atlas_surface));
  for (int i = 0; i < 256; i++) { atlas_palette->colors[i] = (SDL_Color) { i, i, i, i }; }
  ASSERT_SDL(SDL_SetSurfacePalette(atlas_surface, atlas_palette));
  ASSERT_SDL(atlas_texture = SDL_CreateTextureFromSurface(renderer, atlas_surface));
  ASSERT_SDL(SDL_SetTextureBlendMode(atlas_texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED));

  // TODO better way to init plots that makes it more obvious 
  // what you have to start
  imp_context imp = {};
  imp.mem_cache.size = 10*1024*1024;
  imp.mem_cache.mem = malloc(imp.mem_cache.size);
  imp.mem_string.size = 16*1024;
  imp.mem_string.mem = malloc(imp.mem_string.size);
  imp.input.screen.w = WIDTH;
  imp.input.screen.h = HEIGHT; 

  while (running) {
    for (SDL_Event event; SDL_PollEvent(&event); ) {
      switch (event.type) {
      case SDL_EVENT_QUIT: 
        running = false;
      break;
      case SDL_EVENT_KEY_DOWN:
        if (event.key.key == SDLK_ESCAPE) {
          running = false;
        }
      case SDL_EVENT_MOUSE_MOTION:
        break;
      }
    }
    imp.input.mouse_flags = SDL_GetMouseState(&imp.input.mouse.x, &imp.input.mouse.y);

    SDL_SetRenderDrawColor(renderer, 10, 3, 13, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect r = {0, 0, ATLAS_WIDTH, ATLAS_HEIGHT};
    SDL_RenderTexture(renderer, atlas_texture, &r, &r);
    
    imp_plot *p = imp_plot_start(&imp, (imp_plot_params){ (imp_text){imp_strl("Plot 1")}, (imp_r2){ 200, 200, 200, 200 }} );

    #define N 1000
    uint64_t t[N];
    double y1[N];
    
    imp_plot_x(p, (imp_data){ imp_strl("time"), IMP_U64, &t, N });
    imp_plot_y(p, (imp_data){ imp_strl("y1"), IMP_F64, &y1 });

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}


