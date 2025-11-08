// TODO(lf) namespacing

/* TODO(lf)
how to deal with data cache memory?
can keep list of used plots for current frame and collect garbage
each series may need cache data
cache data is:
  0 if n points is < plot.w (just use points directly)
  else plot.w if time series
  else plot.w*plot.h 
  we can collect garbage and compact 


LONG-TERM:
- C11 _Generic Macros for plotting data in C
- 3D plots


*/

#include <stdint.h>
#include <string.h> // memset, memcmp

#define ARRAY_LENGTH(A) (sizeof(A)/sizeof(*(A)))

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#ifdef __cplusplus
#define STRUCT(type) type
#define STRUCT_ZERO(type) {}
#else
#define STRUCT(type) (type)
#define STRUCT_ZERO(type) (type){0}
#endif

#ifndef IMP_ASSERT
#define IMP_ASSERT(cond, ...) do { if (!(cond)) { imp_assert("" __FILE__ "%d: assert " #cond " failed!\n" #__VA_ARGS__); return 0; }} while(0)
#endif

typedef struct imp_str {
  char *str;
  int len;
} imp_str;
#define imp_strl(literal) STRUCT(imp_str){ (char*) literal, sizeof(literal"") - 1}

typedef struct imp_v2 {
  float x, y; 
} imp_v2;    

typedef struct imp_rect { 
  float x, y, w, h;
} imp_r2;

typedef struct imp_color {
  float r, g, b, a;
} imp_cf;

typedef struct imp_text {
  imp_str s;
  imp_v2 scale;
  imp_v2 position;
} imp_text;

typedef struct imp_input {
  imp_r2 screen; 
  imp_v2 mouse; // mouse coords in screen-ref
  uint32_t mouse_flags;
  imp_str text;
} imp_input;

// MEMORY
// the imp arena can be passed a fixed size block, or a virtual memory
// block with appropriate callbacks. imp never does any allocations,
// and failed allocations will be handled by all apis.
#define IMP_ARENA_COMMIT_FUN(name) void name(void* memory, uint32_t size)
#define IMP_ARENA_DECOMMIT_FUN(name) void name(void* memory, uint32_t size)
typedef IMP_ARENA_COMMIT_FUN(imp_arena_commit_fun);
typedef IMP_ARENA_DECOMMIT_FUN(imp_arena_decommit_fun);

typedef struct imp_arena {
  uint8_t *mem;
  uint32_t used; // 
  uint32_t size; // max size
  uint32_t commit; // commited size
  uint32_t commit_block; // block size to commit
  imp_arena_commit_fun *commit_fun;
  imp_arena_decommit_fun *decommit_fun;
} imp_arena;

void *imp_arena_take(imp_arena *a, uint32_t size, uint32_t align);
void *imp_arena_reset(imp_arena *a, uint32_t size);

static inline uintptr_t _imp_next_align(uintptr_t ptr, uint32_t align) {
  uintptr_t align1 = (align-1); // align must be power of 2
  return ptr + align1 - ((ptr-1) & align1);
}

void *imp_arena_take(imp_arena *a, uint32_t size, uint32_t align) { 
  align = align? align : sizeof(void*);
  uintptr_t p = _imp_next_align((uintptr_t) (a->mem + a->used), align);
  uintptr_t new_used = p + size - ((uintptr_t) a->mem);
  if (new_used <= a->size) {
    // TODO virtual mem
    // if (a->commit_block != 0 && new_pos > a->commit) {
      // uintptr_t new_commit = next_align(p, a->commit_block);
      // a->commit_fun(a->mem, new_commit - ((uintptr_t) a->mem));
    // }
    a->used = new_used;

    memset((void*) p, 0, size);
    return (void*) p;
  }
  return 0;
}

void *imp_arena_reset(imp_arena *a, uint32_t size) {
  // TODO virtual mem
  a->used = size;
  return 0;
}


typedef struct imp_ctx imp_ctx;
typedef struct imp_data imp_data;
typedef struct imp_plot_params imp_plot_params;
typedef struct imp_plot imp_plot;
typedef struct imp_draw imp_draw;

// this seems less straightforward then a normal enum
// but flags make it easier to make generic code for this.
enum imp_data_type {
  _IMP_NONE = 0, // err

  // building blocks
  // mutually exclusive size flags
  // this is designed so (type & _IMP_SIZE_MASK) == sizeof(data)
  // and is valid iff one bit in size mask is set
  _IMP_SIZE_8  = (1 << 0),
  _IMP_SIZE_16 = (1 << 1),
  _IMP_SIZE_32 = (1 << 2),
  _IMP_SIZE_64 = (1 << 3),
  _IMP_SIZE_MASK = ((1 << 4)-1),

  // type is signed
  _IMP_SIGNED = (1 << 4),

  // type is floating point
  _IMP_IEEE754 = (1 << 5),

  _IMP_DATA_TYPE_MASK = ((1 << 6)-1),

  // usable data types
  IMP_F32  = (_IMP_SIZE_32 | _IMP_IEEE754),
  IMP_F64  = (_IMP_SIZE_64 | _IMP_IEEE754),

  IMP_S8   = (_IMP_SIZE_8 | _IMP_SIGNED),
  IMP_S16  = (_IMP_SIZE_16 | _IMP_SIGNED),
  IMP_S32  = (_IMP_SIZE_32 | _IMP_SIGNED),
  IMP_S64  = (_IMP_SIZE_64 | _IMP_SIGNED),
  
  IMP_U8   = _IMP_SIZE_8,
  IMP_U16  = _IMP_SIZE_16,
  IMP_U32  = _IMP_SIZE_32,
  IMP_U64  = _IMP_SIZE_64,
};

enum imp_data_flags {
  IMP_NOT_SERIES        = (1 << 1), // data is not a time series / x is not monotonic
  // IMP_REVERSED       = (1 << x), // data is in reverse sorted order TODO(lf)

  // When there are many more input points than output pixels,
  // imp will aggregate the data based on the selected settings.
  IMP_AGG_MAX           = (1 << 28),
  IMP_AGG_MIN           = (1 << 29),
  IMP_AGG_MEAN          = (1 << 30),
  IMP_AGG_MM            = IMP_AGG_MAX | IMP_AGG_MIN, // per x pixel, draw y line between min/max
  IMP_AGG_MMM           = IMP_AGG_MM | IMP_AGG_MEAN, // draw color_alt between min/max, color at mean
  

};

enum imp_style_flags {
  IMP_LINES           = (1 << 0), // show lines for series
  IMP_LINES_FILL      = (1 << 1), // fill area under (or to reference) lines
  IMP_MARKERS         = (1 << 2), // show markers at each point
};


struct imp_data {
  // core
  imp_str name;
  uint32_t type;
  void *ptr;
  uint32_t n;
  uint32_t stride; // space in bytes between values in array at *data, 0 = sizeof(type)
  uint32_t flags;
  uint32_t hash;

  // decimation type TODO(lf)

  // style
  imp_cf color;
  imp_cf color_alt;
  uint32_t style;

  // internal
  struct imp_data *_x;
  struct imp_data *_next;
};

struct imp_plot_params {
  imp_text title;
  imp_r2 screen; // location in screen-ref
  imp_r2 view; // target view rectangle, data-ref

  uint32_t default_flags;
  uint32_t default_style;
  uint32_t hash;
};

struct imp_plot {
  imp_plot_params params;
  imp_ctx *ctx;
  imp_r2 _view; // current real view
  imp_v2 last_mouse;
  
  uint32_t x_series;
  uint32_t y_series;

  // internal
  imp_data *first_x; 
  imp_data *first_y;
  imp_data *last_x; // x to match y with
  imp_data *_free;
  
  imp_plot *_next;
};

struct imp_ctx {
  imp_input input;
  imp_arena mem_string; // storing strings used in rendering ()
  imp_arena mem_struct; // storing imp structs (rec: scale with to n data series * n plot)
  imp_arena mem_cache; // storing caches of plot data (rec: scale with n data series * n plot * plot w * plot h)

  uint32_t plots;
  imp_plot *first_plot;
  imp_plot *free_plot;

  uint32_t _cache_save;
  imp_draw *_next_cmd;
  imp_draw *_last_cmd;
};

// user-defined functions
imp_v2 imp_measure_text(imp_text txt);
imp_v2 imp_render_text(imp_text txt);
void imp_assert(const char *error);

// API
enum imp_draw_types {
  IMP_DRAW_NONE = 0,
  IMP_DRAW_RECTS, // render 1 or more filled rectangles
  IMP_DRAW_LINES, // render 1 or more connected line segments
  IMP_DRAW_STRIP, // render 1 or more connected triangles
  // IMP_DRAW_ICONS, // render 1 or more icons (textured quads)
  IMP_DRAW_TEXT, // render 1 or more strings

  
  IMP_DRAW_TYPES
};
struct imp_draw {
  struct imp_draw *next;
  imp_cf color;
  enum imp_draw_types type;
  int count;
  uint32_t _cap; // allocated count
  uint32_t _used; // cache mem used marker, for building commands
  union array {
    imp_r2 rect[1];
    imp_text text[1];
    imp_v2 point[1]; // used for verts and lines
  } array;
};

imp_draw * _imp_push_cmd(imp_ctx *ctx, imp_draw *cmd) {
  if (!ctx->_last_cmd) {
    ctx->_next_cmd = cmd;
  } else {
    ctx->_last_cmd->next = cmd;
  }
  ctx->_last_cmd = cmd; 
  return cmd;
}

imp_draw* _imp_push_rect(imp_ctx *ctx, imp_r2 rect, imp_cf color) {
  imp_draw *cmd = ctx->_last_cmd;
  if (cmd && (cmd->_used == ctx->mem_cache.used)
  && (cmd->type == IMP_DRAW_RECTS)
  && (memcmp(&cmd->color, &color, sizeof(color)) == 0)) {
    // append rec to previous cmd
    if (imp_arena_take(&ctx->mem_cache, sizeof(imp_r2), 1)) {
      cmd->array.rect[cmd->count++] = rect;
      cmd->_used = ctx->mem_cache.used;
      return cmd;
    } else {
      IMP_ASSERT(0, "mem_cache out of memory!");
    }
  } else {
    if ((cmd = (imp_draw *) imp_arena_take(&ctx->mem_cache, sizeof(imp_draw), 0))) {
      cmd->type = IMP_DRAW_RECTS;
      cmd->color = color;
      cmd->count = 1;
      cmd->array.rect[0] = rect;
      cmd->_used = ctx->mem_cache.used;
      return _imp_push_cmd(ctx, cmd);
    } else {
      IMP_ASSERT(0, "mem_cache out of memory!");
    }
  }
}

imp_draw* _imp_push_lines(imp_ctx *ctx, uint32_t cap, imp_cf color) {
  cap = MAX(cap, 1);
  imp_draw *cmd;
  if ((cmd = (imp_draw *) imp_arena_take(&ctx->mem_cache, sizeof(imp_draw) + (cap-1)*sizeof(imp_v2), 0))) {
    cmd->type = IMP_DRAW_LINES;
    cmd->color = color;
    cmd->_cap = cap;
    cmd->_used = ctx->mem_cache.used;
    return _imp_push_cmd(ctx, cmd);
  } else {
    IMP_ASSERT(0, "mem_cache out of memory!");
  }
}

void imp_ctx_update(imp_ctx *ctx, imp_input input);
imp_draw *imp_next_draw(imp_ctx *ctx); // draw undrawn plots


imp_plot *imp_plot_start(imp_ctx *ctx, imp_plot_params p);
imp_data *imp_plot_x(imp_plot *p, imp_data x);
imp_data *imp_plot_y(imp_plot *p, imp_data y);

/* usage

// .. init ctx ..
imp_ctx *imp;

imp_plot *p = imp_plot_start(&imp, (imp_plot) { .screen.w = 640, .screen.h = 360, .title = "test" });
imp_plot_x(&p, (imp_data){ IMP_U64, &t, 1000, .name="time"} );
imp_plot_y(&p, (imp_data){ IMP_F32, &y1, .name = "y1" } );


// .. later ..
// draw everything in ctx

for (imp_draw cmd; imp_next_draw(&imp, &cmd); ) {
  // handle drawing rotated, textured rects
}

// NOTE(lf) this api design basically means imp_next_draw has to do fucking everything
// this should maybe be an imp_ez thing, and the more core thing is draw commands for 
// ticks, axes, grid, lines, markers, labels, titles, legends
// really thats boxes, lines, markers (images), and text
*/
uint32_t str_hash_fnv1a(imp_str s, uint32_t current) {
    uint32_t hash = current? current : 0x811c9dc5;
    for (int i = 0; i < s.len; i++) {
        hash = (hash ^ (uint32_t)(s.str[i])) * 0x01000193;
    }
    return hash;
}

static imp_plot *_imp_get_plot(imp_ctx *ctx, uint32_t hash) {
  // search ll for new plot
  imp_plot *plot, *last_plot = 0;
  for (plot = ctx->first_plot; plot; plot = plot->_next) {
    if (plot->params.hash == hash) {
      return plot;
    }
    last_plot = plot;
  }
  // allocate new plot
  plot = (imp_plot*) imp_arena_take(&ctx->mem_struct, sizeof(imp_plot), 8);
  if (!last_plot) {
    ctx->first_plot = plot;
  } else {
    last_plot->_next = plot;
  }
  if (plot) {
    ctx->plots++;
  }
  return plot;
}

imp_plot *imp_plot_start(imp_ctx *ctx, imp_plot_params p) {
  // TODO check that user actually passed required params like title, size
  IMP_ASSERT(ctx != 0, "ctx should not be null!\n");
  IMP_ASSERT((p.title.s.len != 0 && p.title.s.str != 0) || (p.hash != 0),
    "plot title or hash must be set to uniquely identify plot!\n"
  );
  
  // fill in default params where possible
  if (!p.default_style) {
    p.default_style |= IMP_LINES;
  }

  p.hash = p.hash? p.hash : str_hash_fnv1a(p.title.s, 0);

  imp_plot *plot = _imp_get_plot(ctx, p.hash);
  if (plot != 0) {
    plot->params = p;
    plot->ctx = ctx;
  }
  return plot;
}

imp_data *_imp_get_data(imp_plot *p, imp_data *d) {
  // validation
  uint32_t size = d->type & _IMP_SIZE_MASK;
  IMP_ASSERT(size != 0 && (size & (size-1)) == 0, 
    "invalid data type!\n" 
    "set one of _IMP_SIZE_(8|16|32|64).\n"
  );
  IMP_ASSERT(!(((d->type & _IMP_IEEE754) > 0) && ((d->type & _IMP_SIGNED))),
    "invalid data type!\n"
    "set only one of _IMP_IEEE754 or _IMP_SIGNED"
  );
  IMP_ASSERT(d->ptr != 0 || d->n == 0, 
    "null pointer for data array with non-zero size!\n"
  );
  IMP_ASSERT((d->name.len != 0 && d->name.str != 0) || (d->hash != 0),
    "data name or hash must be set to uniquely identify data!\n"
  );
  
  // set defaults
  d->stride = d->stride? d->stride : size; // if stride not set, assume densely packed array

  // compute hash
  imp_data **first; uint32_t *count;
  if (d->_x) {
    first = &p->first_y;
    count = &p->y_series;
    IMP_ASSERT(d->n == d->_x->n, "data->n (number of elements) for y must equal x!");
  } else {
    first = &p->first_x;
    count = &p->x_series;
    IMP_ASSERT(d->n != 0, "data->n (number of elements) must be set for x!");
  }
  d->hash = d->hash? d->hash : str_hash_fnv1a(d->name, p->params.hash);
  
  imp_data *data, *last = 0;
  for (data = *first; data; data = data->_next) {
    if (data->hash == d->hash) {
      return data;
    }
    last = data;
  }
  
  data = (imp_data*) imp_arena_take(&p->ctx->mem_struct, sizeof(imp_data), 8);
  if (!last) {
    *first = data;
  } else {
    last->_next = data;
  }
  if (data) {
    (*count)++;
    *data = *d;
  }
  return data;
}

imp_data *imp_plot_x(imp_plot *p, imp_data x) {
  IMP_ASSERT(p != 0, "plot should not be null!\n");
  
  imp_data *d = _imp_get_data(p, &x);
  if (d) {
    p->last_x = d;
  }
  return d;
}

imp_data *imp_plot_y(imp_plot *p, imp_data y) {
  IMP_ASSERT(p != 0, "plot should not be null!\n");
  
  y.flags = y.flags? y.flags : p->params.default_flags;
  y.style = y.style? y.style : p->params.default_style;

  // TODO alloc decimation buffer for data, if n > p.width
  // nvm should happen when drawing I think
  
  IMP_ASSERT(p->last_x != 0, 
    "y data must have x data already set!\n"
    "call imp_plot_x before imp_plot_y.\n"
  );
  y._x = p->last_x;
  y.n = y.n ? y.n : y._x->n;

  return  _imp_get_data(p, &y);
}

imp_draw * imp_next_draw(imp_ctx *ctx) {
  // commands already generated, just keep outputting from array
  if (!ctx->_last_cmd) {
    imp_plot *p = ctx->first_plot;
  
    
    // for (imp_plot *p = ctx->first_plot; p; p = p->next) {
      
    // }

    // TODO(lf) caching
    // NOTE(lf) for now just dont cache, we need to develop and test
    // the full redraw case anyway, dont put cart before horse
    // allocate memory for aggregating data for each plot
      // mem does not need to be allocated again if size didnt change
      // if it did, reallocate
      // may need to garbage collect if cache is oom, which should be fine

    // save cache state, because draw commands will come next
    // ctx->_cache_save = mem_cache

    // what busts a cache:
      // mem:
      // - plot screen / output size change
      // - 
      // data / cmds:
      // - data updates
      //   - can be kept minimal for certain types of plots
      // - view changes (pan / zoom)
      
    // for each plot, do draw commands 


    // WARN for now just clear cache always
    imp_arena_reset(&ctx->mem_cache, 0);

    
    imp_cf white = STRUCT(imp_cf){ 1.0, 1.0, 1.0, 1.0 };
    imp_cf red = STRUCT(imp_cf){ 1.0, 0.0, 0.0, 1.0 };
    imp_cf blue = STRUCT(imp_cf){ 0.0, 0.0, 1.0, 1.0 };
    

    imp_r2 r = p->params.screen;

    _imp_push_rect(ctx, r, white);

    // float o = r.w * 0.1;
    // r.x += o;
    // r.w *= 0.6;

    // r.y += r.h * 0.1;
    // r.h *= 0.8;

    
    // r.w *= 0.5;

    // _imp_push_rect(ctx, r, red);
    
    // r.x += r.w + o*2;
    // _imp_push_rect(ctx, r, red);

    uint32_t agg_size = 2*p->params.screen.w;

    imp_draw *cmd = _imp_push_lines(ctx, agg_size, blue);

    { // aggregation for time series data 

      imp_data *datax = p->first_x;
      imp_data *datay = p->first_y;
      
      // TODO(lf) make this code generic across data types
      // This is going to be really tough. Honestly dont really know how to do it
      // without being balls slow or with tons of repeated code throughout this header
      // maybe macros can save the day?
      double *x = (double *) datax->ptr;
      double *y = (double *) datay->ptr;

      int padding = 15; // padding, in pixels

      float pw = (x[datax->n-1] - x[0]) / (p->params.screen.w - 2*padding);

      {
        int p = 0;
        int n = 0;
        float min = 0;
        float max = 0;
        for (int i = 0; i < (int) datax->n; ) {
          if (x[i] >= x[0]+p*pw) {
            // add points for current x
            if (n > 1) { // min + max, 2 points
              float px = x[0]+(p-0.5)*pw;
              cmd->array.point[cmd->count++] = STRUCT(imp_v2){ px, min };
              cmd->array.point[cmd->count++] = STRUCT(imp_v2){ px, max };
            } else if (n == 1) { // just 1 point
              cmd->array.point[cmd->count++] = STRUCT(imp_v2){ x[i-1], min };
            } // else no points
            n = 0;
            min = INFINITY;
            max = -INFINITY;
            p++;
          } else {
            n++;
            min = MIN(min, y[i]);
            max = MAX(max, y[i]);
            i++;
          }
        }
      }

      // TODO(lf) let user specifically set the yscale,
      // but otherwise scale it to fit the plot

      // TODO(lf): zoom + pan requires that this be stored across frames and modifiable
      // TODO(lf): limit aggregated points to what is actually in view

      /* REMAP -> FMA
        a + (b-a) * (x - d)/(c - d)
        x * ((b - a)/(c - d)) + (a + (-d)((b - a)/(c - d)))
        x * xs + xo
      */
      float xs = (p->params.screen.w - 2*padding) / (x[datax->n-1] - x[0]);
      float xo = p->params.screen.x + padding - x[0] * xs;
      float ys, yo;
      {
        float ymin = INFINITY;
        float ymax = -INFINITY;
        for (int i = 0; i < (int) cmd->count; i++) {
          ymin = MIN(ymin, cmd->array.point[i].y);
          ymax = MAX(ymax, cmd->array.point[i].y);
        }
        ys = (p->params.screen.h - 2*padding) / (ymax - ymin);
        yo = p->params.screen.y + padding - ymin * ys;
      }
        
      for (int i = 0; i < (int) cmd->count; i++) {
        cmd->array.point[i] = STRUCT(imp_v2) {
          cmd->array.point[i].x * xs + xo,
          cmd->array.point[i].y * ys + yo,
        };
      }
    }
  }

  


  // commands generated, output from array
  imp_draw *out = ctx->_next_cmd;
  if (out) {
    ctx->_next_cmd = out->next;
    IMP_ASSERT(ctx->_next_cmd || out == ctx->_last_cmd, "last command must match!");
  } else {
    ctx->_last_cmd = 0;
  }
  return out;

}

