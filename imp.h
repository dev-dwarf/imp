// TODO(lf) namespacing

// TODO(lf)
// how to deal with data cache memory?
// can keep list of used plots for current frame and collect garbage
// each series may need cache data
// cache data is:
// 0 if n points is < plot.w (just use points directly)
// else plot.w if time series
// else plot.w*plot.h 
// we can collect garbage and compact 

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

typedef struct imp_str {
  char *str;
  int len;
} imp_str;
#define imp_strl(literal) STRUCT(imp_str){literal, sizeof(literal"") - 1}

typedef union imp_v2 {
  struct { float x, y; }; // x, y point
  struct { float w, h; }; // width, height size
  struct { float l, h; }; // low, high range
} imp_v2;    

typedef struct imp_rect { 
  float x, y, w, h;
} imp_rect;

typedef struct imp_color {
  float r, g, b, a;
} imp_color;

typedef struct imp_text {
  imp_str s;
  imp_v2 scale;
  imp_v2 position;
  imp_color c;
} imp_text;

typedef struct imp_input {
  imp_rect screen; 
  imp_v2 mouse; // mouse coords in screen-ref
  imp_str text;
} imp_input;

#define IMP_ARENA_COMMIT_FUN(name) int name(void* memory, unsigned int size)
#define IMP_ARENA_DECOMMIT_FUN(name) void name(void* memory, unsigned int size)
typedef IMP_ARENA_COMMIT_FUN(imp_arena_commit_fun);
typedef IMP_ARENA_DECOMMIT_FUN(imp_arena_decommit_fun);

typedef struct imp_arena {
  unsigned char *mem;
  unsigned int size;
  unsigned int max_size;
  imp_arena_commit_fun *commit;
  imp_arena_decommit_fun *decommit;
} imp_arena;

void *imp_arena_take(imp_arena *a, unsigned int size);
void *imp_arena_reset(imp_arena *a, unsigned int size);


typedef struct imp_context {
  imp_input input;
  imp_mem mem_string; // storing strings used in rendering ()
  imp_mem mem_cache; // storing caches of plot data (rec: proportional to n plots * plot w * plot h)
} imp_context;

enum imp_data_type {
  IMP_NONE, // err
  IMP_F32,
  // TODO(lf) more types, all standard float and s/u int, + fn ptr
  // IMP_F64,
  IMP_U64,

  IMP_DATA_TYPES
};

enum imp_data_flags {
  IMP_NOT_SERIES      = (1 << 0), // data is not a time series / x is not monotonic
  // IMP_REVERSED     = (1 << x), // data is in reverse sorted order TODO(lf)
};

enum imp_style_flags {
  IMP_LINES           = (1 << 0), // show lines for series
  IMP_LINES_FILL      = (1 << 1), // fill area under (or to reference) lines
  IMP_MARKERS         = (1 << 2), // show markers at each point
};

typedef struct imp_data {
  // core
  unsigned int type;
  void *data; // data
  int n;
  int stride; // space in bytes between values in array at *data, 0 = sizeof(type)

  // decimation type TODO(lf)

  // style
  imp_str name;
  imp_color color;
  unsigned int style;

  // internal
  struct imp_data *_x;
  struct imp_data *_next;
} imp_data;

typedef struct imp_plot_params {
  imp_text title;
  imp_rect screen; // location in screen-ref
  imp_rect view; // target view rectangle, data-ref

  unsigned int default_series_flags;
} imp_plot_params;

typedef struct imp_plot {
  imp_plot_params params;
  imp_context *context;
  imp_rect _view; // current real view
  imp_v2 last_mouse;
  
  int x_series;
  int y_series;
  imp_data *first_x; 
  imp_data *first_y;
  imp_data *last_x; 
} imp_plot;

// user-defined functions
imp_v2 imp_measure_text(imp_text txt);
imp_v2 imp_render_text(imp_text txt);
void imp_assert(imp_str error);

// API
typedef struct imp_draw {
  // TODO
} imp_draw;

void imp_context_update(imp_context *context, imp_input input);
int imp_next_draw(imp_context *context, imp_draw *cmd); // draw undrawn plots


imp_plot *imp_plot_start(imp_context *context, imp_plot_params p);
void imp_plot_x(imp_plot *p, imp_data *x);
void imp_plot_y(imp_plot *p, imp_data *y);

/* usage

// .. init context ..
imp_context *imp;

imp_plot *p = imp_plot_start(&imp, (imp_plot) { .screen.w = 640, .screen.h = 360, .title = "test" });
imp_plot_x(&p, &(imp_data){ IMP_U64, &t, 1000, .name="time"} );
imp_plot_y(&p, &(imp_data){ IMP_F32, &y1, .name = "y1" } );


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

imp_plot *imp_plot_start(imp_context context, imp_plot_params p) {
  
  // TODO fill in default params

  // TODO check for cached plot obj
  imp_plot *plot = imp_arena_take(context->mem_cache, sizeof(imp_plot));
  // assert plot != 0


  
  plot->params = p;

}

void imp_plot_x(imp_plot *p, imp_data *x) {
    // TODO fill in defaults for x

    // calc id 

    // check for id in p

    // TODO alloc decimation buffer for data, if n > p.width
    // nvm should happen when drawing I think

    // if not in p already TODO
    if (p->x_series == 0) {
      p->first_x = x;
    }
    if (p->last_x) {
      p->last_x->_next = x;
    }
    p->last_x = x;
    p->x_series++;
}

void imp_plot_y(imp_plot *p, imp_data *y) {
    // TODO fill in defaults for y

    // calc id

    // check for id in p

    // TODO alloc decimation buffer for data, if n > p.width
    // nvm should happen when drawing I think
    
    // ASSERT that there is an x and that this vaguely matches it
    // y->_x

    // if not in p already TODO
    if (p->y_series == 0) {
      p->first_y = y;
    }
    p->y_series++;
}

