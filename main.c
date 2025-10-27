#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <math.h>

static SDL_Window *window = 0;
static SDL_Renderer *renderer = 0;
static SDL_Surface *atlas_surface = 0; 
static SDL_Texture *atlas_texture = 0; 

#define ARRAY_LENGTH(A) (sizeof(A)/sizeof(*(A)))
#define ASSERT_SDL(cond) do { if (!(cond)) { SDL_Log( "" __FILE__ ":%d: " #cond " gave SDL Error: %s", __LINE__, SDL_GetError()); return -1; } } while(0);

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#ifdef __cplusplus
#define STRUCT(type) type
#define STRUCT_ZERO(type) {}
#else
#define STRUCT(type) (type)
#define STRUCT_ZERO(type) (type){0}
#endif

#define WIDTH 640
#define HEIGHT 480

typedef struct str {
  char *str;
  int len;
} str;
#define strl(literal) STRUCT(str){literal, sizeof(literal"") - 1}

typedef union vec2 {
  struct { float x, y; };
  struct { float w, h; };
} vec2;    

typedef struct rect { 
  float x, y, w, h;
} rect;

typedef struct color {
  float r, g, b, a;
} color;

#include "atlas.h"


vec2 measure_text(str s) {
  vec2 out = {0};
  for (int i = 0; i < s.len; i++) {
    rect r = ATLAS_RECT[ATLAS_FONT + (unsigned char) s.str[i]];
    out.h = MAX(r.h, out.h);
    out.w += r.w + ATLAS_SPACING;
  }
}

void SDL_RenderText2D(vec2 p, str s, color c) {
  SDL_FRect r = { p.x, p.y, 0, 0};
  SDL_SetRenderDrawColorFloat(renderer, c.r*c.a, c.g*c.a, c.b*c.a, c.a);
  for (int i = 0; i < s.len; i++) {
    rect ag = ATLAS_RECT[ATLAS_FONT + (unsigned char) s.str[i]];
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

    SDL_SetRenderDrawColor(renderer, 10, 3, 13, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    SDL_FRect r = {0, 0, ATLAS_WIDTH, ATLAS_HEIGHT};
    SDL_RenderTexture(renderer, atlas_texture, &r, &r);

    SDL_RenderText2D((vec2){ WIDTH/2, HEIGHT/2 }, strl("Hello good IMP!"), (color) { 1.0, 0.5, 1.0, 1.0});

    SDL_RenderPresent(renderer);
  }

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}


