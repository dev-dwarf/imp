#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stdlib.h>

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static SDL_Window *window = 0;
static SDL_Renderer *renderer = 0;
static SDL_Surface *atlas_surface = 0; 
static SDL_Texture *atlas_texture = 0; 

#define ASSERT_SDL(cond) do { if (!(cond)) { SDL_Log( "" __FILE__ ":%d: " #cond " gave SDL Error: %s", __LINE__, SDL_GetError()); return -1; } } while(0);

#define WIDTH 640
#define HEIGHT 480

#include <stdio.h>
void imp_assert(const char* err) {
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
    imp_r2 ag = ATLAS_RECT[(unsigned char) s.str[i]];
    SDL_FRect g = { ag.x, ag.y, ag.w, ag.h };
    r.w = g.w;
    r.h = g.h;
    SDL_RenderTexture(renderer, atlas_texture, &g, &r);
    r.x += g.w + ATLAS_SPACING;
  }
}

int main(int argc, char* argv[]) {
  (void) argc; (void) argv;
  
  ASSERT_SDL(SDL_Init(SDL_INIT_VIDEO));
  ASSERT_SDL(window = SDL_CreateWindow("IMP SDL3", WIDTH, HEIGHT, SDL_WINDOW_RESIZABLE));
  ASSERT_SDL(renderer = SDL_CreateRenderer(window, NULL));
  bool running = true;

  SDL_Palette *atlas_palette;
  ASSERT_SDL(atlas_surface = SDL_CreateSurfaceFrom(ATLAS_WIDTH, ATLAS_HEIGHT, SDL_PIXELFORMAT_INDEX8, ATLAS_DATA, ATLAS_WIDTH));
  ASSERT_SDL(atlas_palette = SDL_CreateSurfacePalette(atlas_surface));
  for (int i = 0; i < 256; i++) { atlas_palette->colors[i] = STRUCT(SDL_Color){(uint8_t) i, (uint8_t) i, (uint8_t) i, (uint8_t) i}; }
  ASSERT_SDL(SDL_SetSurfacePalette(atlas_surface, atlas_palette));
  ASSERT_SDL(atlas_texture = SDL_CreateTextureFromSurface(renderer, atlas_surface));
  ASSERT_SDL(SDL_SetTextureBlendMode(atlas_texture, SDL_BLENDMODE_BLEND_PREMULTIPLIED));

  // TODO better way to init plots that makes it more obvious 
  // what you have to start
  imp_ctx imp;
  memset(&imp, 0, sizeof(imp));
  imp.mem_string.size = 16*1024;
  imp.mem_string.mem = (uint8_t*) malloc(imp.mem_string.size);
  imp.mem_struct.size = 16*1024;
  imp.mem_struct.mem = (uint8_t*) malloc(imp.mem_struct.size);
  imp.mem_cache.size = 10*1024*1024;
  imp.mem_cache.mem = (uint8_t*) malloc(imp.mem_cache.size);
  
  

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
    {
      int w,h;
      if (SDL_GetWindowSizeInPixels(window, &w, &h)) {
        imp.input.screen.w = w;
        imp.input.screen.h = h;
      }
    }
    imp.input.mouse_flags = SDL_GetMouseState(&imp.input.mouse.x, &imp.input.mouse.y);

    SDL_SetRenderDrawColor(renderer, 10, 3, 13, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_FRect r = {0, 0, ATLAS_WIDTH, ATLAS_HEIGHT};
    SDL_RenderTexture(renderer, atlas_texture, &r, &r);
    
    imp_plot *p = imp_plot_start(&imp, STRUCT(imp_plot_params){ 
      STRUCT(imp_text){ imp_strl("Plot 1") }, 
      STRUCT(imp_r2){ (imp.input.screen.w-imp.input.screen.h)/2, 0, imp.input.screen.h, imp.input.screen.h },
    });

    #define N 1000
    double t[N];
    double y1[N];

    for (int i = 0; i < N; i++) {
      t[i] = 0.001 * i;
      y1[i] = sin ( 5 * (2 * M_PI * t[i]) );
    }
    
    imp_plot_x(p, STRUCT(imp_data){ imp_strl("time"), IMP_F64, &t, N });
    imp_plot_y(p, STRUCT(imp_data){ imp_strl("y1"), IMP_F64, &y1 });

    for (imp_draw *cmd; (cmd = imp_next_draw(&imp)); ) {
      ASSERT_SDL(SDL_SetRenderDrawColorFloat(renderer, cmd->color.r, cmd->color.g, cmd->color.b, cmd->color.a));
      
      switch (cmd->type) {
      case IMP_DRAW_RECTS: { 
        ASSERT_SDL(SDL_RenderFillRects(renderer, (SDL_FRect *) cmd->array.rect, cmd->count));
      } break;
      case IMP_DRAW_LINES: { 
        SDL_FPoint *points = (SDL_FPoint *) cmd->array.point;
        ASSERT_SDL(SDL_RenderLines(renderer, points, cmd->count));
      } break;
      case IMP_DRAW_STRIP: { 
      
      } break;
      case IMP_DRAW_TEXT: { 
      
      } break;
      default:
      case IMP_DRAW_NONE: { } break;
      }
    }

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}


