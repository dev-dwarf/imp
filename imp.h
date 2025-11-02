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
#define imp_strl(literal) STRUCT(imp_str){literal, sizeof(literal"") - 1}

typedef union imp_v2 {
  struct { float x, y; }; // x, y point
  struct { float w, h; }; // width, height size
  struct { float l, _h; }; // low, high range
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
  imp_cf c;
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
  uintptr_t p = _imp_next_align((uintptr_t) (a->mem + a->used), align);
  uintptr_t new_used = p + size - ((uintptr_t) a->mem);
  if (new_used <= a->size) {
    // TODO virtual mem
    // if (a->commit_block != 0 && new_pos > a->commit) {
      // uintptr_t new_commit = next_align(p, a->commit_block);
      // a->commit_fun(a->mem, new_commit - ((uintptr_t) a->mem));
    // }
    a->used = new_used;
    return (void*) p;
  }
  return 0;
}
void *imp_arena_reset(imp_arena *a, uint32_t size) {
  // TODO virtual mem
  a->used = 0;
}


typedef struct imp_context imp_context;
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
  IMP_NOT_SERIES      = (1 << 1), // data is not a time series / x is not monotonic
  // IMP_REVERSED     = (1 << x), // data is in reverse sorted order TODO(lf)
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
  imp_context *context;
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

struct imp_context {
  imp_input input;
  imp_arena mem_string; // storing strings used in rendering ()
  imp_arena mem_cache; // storing caches of plot data (rec: proportional to n plots * plot w * plot h)

  uint32_t plots;
  imp_plot *first_plot;
  imp_plot *free_plot;
};

// user-defined functions
imp_v2 imp_measure_text(imp_text txt);
imp_v2 imp_render_text(imp_text txt);
void imp_assert(char *error);

// API
struct imp_draw {
  // TODO
};

void imp_context_update(imp_context *context, imp_input input);
int imp_next_draw(imp_context *context, imp_draw *cmd); // draw undrawn plots


imp_plot *imp_plot_start(imp_context *context, imp_plot_params p);
imp_data *imp_plot_x(imp_plot *p, imp_data x);
imp_data *imp_plot_y(imp_plot *p, imp_data y);

/* usage

// .. init context ..
imp_context *imp;

imp_plot *p = imp_plot_start(&imp, (imp_plot) { .screen.w = 640, .screen.h = 360, .title = "test" });
imp_plot_x(&p, (imp_data){ IMP_U64, &t, 1000, .name="time"} );
imp_plot_y(&p, (imp_data){ IMP_F32, &y1, .name = "y1" } );


// .. later ..
// draw everything in context

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

static imp_plot *_imp_get_plot(imp_context *context, uint32_t hash) {
  // search ll for new plot
  imp_plot *plot, *last_plot = 0;
  for (plot = context->first_plot; plot; plot = plot->_next) {
    if (plot->params.hash == hash) {
      return plot;
    }
    last_plot = plot;
  }
  // allocate new plot
  plot = (imp_plot*) imp_arena_take(&context->mem_cache, sizeof(imp_plot), 8);
  if (!last_plot) {
    context->first_plot = plot;
  } else {
    last_plot->_next = plot;
  }
  if (plot) {
    context->plots++;
  }
  return plot;
}

imp_plot *imp_plot_start(imp_context *context, imp_plot_params p) {
  // TODO check that user actually passed required params like title, size
  IMP_ASSERT(context != 0, "context should not be null!\n");
  IMP_ASSERT((p.title.s.len != 0 && p.title.s.str != 0) || (p.hash != 0),
    "plot title or hash must be set to uniquely identify plot!\n"
  );
  
  // fill in default params where possible
  if (!p.default_style) {
    p.default_style |= IMP_LINES;
  }

  p.hash = p.hash? p.hash : str_hash_fnv1a(p.title.s, 0);

  imp_plot *plot = _imp_get_plot(context, p.hash);
  if (plot != 0) {
    plot->params = p;
    plot->context = context;
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
  imp_data **first; int *count;
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
  
  data = (imp_data*) imp_arena_take(&p->context->mem_cache, sizeof(imp_data), 8);
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

