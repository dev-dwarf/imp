/* TODO(lcf, September 24, 2023):
   -start making actual render backend.
   -- use backend to stress test of many points, try 0xf0000
   -- options for rendering data series:
   --- draw lines
   --- draw fill (under or above lines)
   --- draw markers (circles for now, maybe rects and tris too since easy)
   --- colors
   --- gradients? x or y
   
   - 3d canvas?

   - data streams
   -- let data be passed in in different ways
   --- data types (f32, f64, s32, s16, etc)
   --- pattern (seperate arrays, interlaced, strided)
   --- 
   -- standard function backend can repeatedly call to decode data
   -- optionally a user callback gets the data instead for maximum flexibility

   - theme system
   --- have default colors plot can cycle through
   --- make it easy to swap out colors for backends
   
   - expand api
   -- sokol style? or builder style with more calls/params...

   - multiple data series in one plot
   - title
   - axes labels

   - let user customize grid/labels
   
   - legend based on all series
   -- vertical or horizontal stack options
   -- position (left/right +  up/down, inside/outside?)

   -- log scale for either axes individually
   
   - highlight individual points desmos style
   - return to original view after panning/zooming 
*/


#include "third_party/HandmadeMath.h"

/* TODO(lcf): prefix everything with an imp_ namespace */
/* TODO(lcf): we can remove these typedefs when the jam is over. just for my familiarity */
typedef unsigned long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;
typedef long s64;
typedef int s32;
typedef short s16;
typedef double f64;
typedef float f32;
typedef int b32;

typedef struct str str;
struct str {
    s64 len;
    char *str;
};

str imp_str(char *s) {
    str out = {.str = s};
    if (s) while (*s++ != 0) out.len++;
    return out;
}

#ifndef IMP_VSNSPRINTF
#define STB_SPRINTF_IMPLEMENTATION
#include "third_party/stb_sprintf.h"
#define IMP_VSNSPRINTF stbsp_vsnprintf
#endif

#define MAX(a,b) (((a) > (b))? (a) : (b))
#define ASSERT(c) do { if (!(c)) { (*(int*)0=0); }} while (0);
#define PCAST(type, p) (*((type*)&p))

typedef u32 ID;

typedef union imp_Color imp_Color;
union imp_Color {
    struct { u8 r, g, b, a; };
    u32 raw;
};

typedef struct Vec2 Vec2;
struct Vec2 { f32 x, y; };

typedef union Rect Rect;
union Rect {
    struct { f32 x, y, w, h; };
    struct { Vec2 pos;  Vec2 size; };
};

typedef struct Data Data;
struct Data {
    u32 flags;
    u16 marker_type;
    u8 fill_type;
    u8 data_size;
    s32 n;
    f32 *x;
    f32 *y;
    f32 *z;
    Rect view;
    imp_Color color;
    imp_Color line_color;
    imp_Color fill_color;
};

enum {
    IMP_DATA_LINES        = (1 << 0),
    IMP_DATA_MARKERS      = (1 << 1),
    IMP_DATA_FILL         = (1 << 2),
    IMP_DATA_CUSTOM_VIEW  = (1 << 8),
};

#define IMP_DATA_DEFAULT_FLAGS (IMP_DATA_LINES)

#define IMP_PLOT_DRAW_AXIS_X (1 << 2)
#define IMP_PLOT_DRAW_AXIS_Y (1 << 3)
#define IMP_PLOT_DRAW_AXIS_Z (1 << 4)
#define IMP_PLOT_DRAW_GRID_XY (1 << 5)
#define IMP_PLOT_DRAW_GRID_YZ (1 << 6)
#define IMP_PLOT_DRAW_GRID_ZX (1 << 7)
#define IMP_PLOT_DRAW_ALL_AXES (IMP_PLOT_DRAW_AXIS_X | IMP_PLOT_DRAW_AXIS_Y | IMP_PLOT_DRAW_AXIS_Z)
#define IMP_PLOT_DRAW_ALL_GRID (IMP_PLOT_DRAW_GRID_XY | IMP_PLOT_DRAW_GRID_YZ | IMP_PLOT_DRAW_GRID_ZX)
#define IMP_PLOT_DRAW_ALL_3D (IMP_PLOT_DRAW_ALL_AXES | IMP_PLOT_DRAW_ALL_GRID)
#define IMP_PLOT_DRAW_ALL_2D (IMP_PLOT_DRAW_AXIS_X | IMP_PLOT_DRAW_AXIS_Y | IMP_PLOT_DRAW_GRID_XY)

#define IMP_MAX_DATA 8
typedef struct Plot Plot;
struct Plot {
    Rect screen;
    Rect view;
    Rect target_view;
    Data data[IMP_MAX_DATA];

    HMM_Vec3 camera_pos;
    HMM_Vec3 camera_target;
    HMM_Vec3 camera_up;
    HMM_Mat4 camera;
    HMM_Mat4 view_to_screen;
    HMM_Mat4 screen_to_view;

    u32 flags;

    Vec2 mouse;
    Vec2 last_mouse;
    Vec2 drag;

    str title;
    ID id;
    s32 next;
    s32 first_command;
    s32 last_command;
    s32 next_command;
};

typedef struct BaseCommand BaseCommand;
struct BaseCommand {
    s16 type;
    s16 plot;
    imp_Color color;
};

typedef struct RectCommand RectCommand;
struct RectCommand {
    BaseCommand base;
    Rect screen;
};

typedef struct TextCommand TextCommand;
struct TextCommand {
    BaseCommand base;
    Rect screen;
    str text;
};

typedef struct DataCommand DataCommand;
struct DataCommand {
    BaseCommand base;
    Rect screen;
    Data data;
};

typedef struct CustomCommand CustomCommand;
struct CustomCommand {
    BaseCommand base;
    s32 size;
    void *data;
};

enum {
    IMP_COMMAND_RECT = 1,
    IMP_COMMAND_TEXT,
    IMP_COMMAND_DATA,
    IMP_COMMAND_CUSTOM, 
    IMP_COMMAND_MAX
};
typedef union Command Command;
union Command {
    s16 type;
    BaseCommand base;
    RectCommand rect;
    TextCommand text;
    DataCommand data;
};

typedef struct Inputs Inputs;
struct Inputs {
    Vec2 mouse;
    f32 mouse_scroll;
    s32 mouse_down;
    Vec2 last_mouse;
    s32 mouse_pressed;
};

/* NOTE(lcf): must be power of 2 */
/* #define IMP_MAX_PLOTS (2 << 5) */
#define IMP_MAX_PLOTS 4

#define IMP_COMMAND_BUFFER_SIZE 0x1000
#define IMP_CHAR_BUFFER_SIZE (0x4000)
typedef struct Context Context;
struct Context {
    Inputs input;
    
    s32 (*text_width_fun)(const void*, const char*, s32 len);
    const void *text_width_data;
    s32 text_height;

    s32 command_pos;
    s32 char_pos;

    u64 counter;
    ID next_id;
    ID current_plot;
    Plot *first_plot;
    Plot *prev_plot;

    Command command_buffer[IMP_COMMAND_BUFFER_SIZE];
    char char_buffer[IMP_CHAR_BUFFER_SIZE];
    ID plot_collision[IMP_MAX_PLOTS];
    Plot plot[IMP_MAX_PLOTS];
};

/* 32bit fnv-1a hash */
#define HASH_INITIAL 2166136261
static void hash(u32 *hash, const void *data, s32 size) {
    const u8 *p = data;
    while (size--) {
        *hash ^= *p++ & 0x8F;
        *hash *= 16777619;
    }
}

Plot *current_plot(Context *imp) {
    return imp->plot + (imp->current_plot & (IMP_MAX_PLOTS-1));
}

Plot *_get_plot_or_parent(Context *imp, ID plot_id) {
    /* Check default position */
    imp->current_plot = plot_id;
    Plot *plot = current_plot(imp);
    if (plot->id == 0 || plot->id == plot_id) {
        
    }  else {
        /* Check linked list */
        ID collision;
        while (collision = imp->plot_collision[imp->current_plot & (IMP_MAX_PLOTS-1)]) {
            imp->current_plot = collision;
            plot = current_plot(imp);

            if (plot->id == 0 || plot->id == plot_id) {
                break;
            }
        }
    }

    return plot;
}

Plot *alloc_plot(Context *imp) {
    ASSERT(imp->next_id != 0);
    ID plot_id = imp->next_id;
    imp->next_id  = HASH_INITIAL;

    Plot *plot = _get_plot_or_parent(imp, plot_id);
    if (plot->id != 0  && plot->id != plot_id) {
        /* Brute force find a slot, update linked list */
        for (s32 i = 0; i < IMP_MAX_PLOTS; i++) {
            imp->current_plot++;
            Plot *new_plot = current_plot(imp);

            if (new_plot->id == 0) {
                imp->plot_collision[plot - imp->plot] = imp->current_plot;
                plot = new_plot;
                break;
            }
        }

        if (plot->id != 0) {
            /* No room for new plots! */
            ASSERT(0);
        }

    }
    plot->id = plot_id;
    return plot;
}

Plot *plot_from_id(Context *imp, ID plot_id) {
    ASSERT(imp->current_plot == 0); /* WARN(lcf): Do not call this while doing a plot! */
    Plot *plot = _get_plot_or_parent(imp, plot_id);
    imp->current_plot = 0;
    return (plot->id == plot->id)? plot : 0;
}

/* NOTE(lcf): did this just for completeness. Its doubtful people will want to free plots. */
void free_plot(Context *imp, ID plot_id) {
    Plot *plot = plot_from_id(imp, plot_id);
    u32 plot_ind = plot - imp->plot;
    if (plot_ind < 0 || plot_ind > IMP_MAX_PLOTS) {
        return;
    }
    
    ID plot_link = imp->plot_collision[plot_ind];
    if (plot_link) {
        /* Replace self with child */
        u32 child_ind = plot_link & (IMP_MAX_PLOTS-1);
        imp->plot[plot_ind] = imp->plot[child_ind];
        imp->plot_collision[plot_ind] = imp->plot_collision[child_ind];
        imp->plot[child_ind] = (Plot){0};
        imp->plot_collision[child_ind] = 0;
        
        /* Parent link will now be valid if it exists */
    } else {
        /* Remove parent link */
        for (s32 i = 0; i < IMP_MAX_PLOTS; i++) {
            if (imp->plot_collision[i] & (IMP_MAX_PLOTS-1) == plot_ind) {
                imp->plot_collision[i] = 0;
            }
        }
    }
}

void push_command(Context *imp, Command cmd) {
    ASSERT(imp->command_pos+1 < IMP_COMMAND_BUFFER_SIZE);
    cmd.base.plot = imp->current_plot;
    imp->command_buffer[imp->command_pos++] = cmd;
}

#include <stdarg.h>
#include <stdio.h>
str strfv(Context *imp, char *fmt, va_list args) {
    str result = (str){0};
    result.str = imp->char_buffer + imp->char_pos;
    result.len = IMP_VSNSPRINTF(imp->char_buffer + imp->char_pos, IMP_CHAR_BUFFER_SIZE - imp->char_pos, fmt, args);
    imp->char_pos += result.len+1; /* include null terminator */
    return result;
}

str strf(Context *imp, char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    str result = strfv(imp, fmt, args);
    va_end(args);
    return result;
}

#define HEXCOLOR(hex) {                         \
        .a = (hex >> (0x0 * 0x8)) & 0xFF,       \
            .b = (hex >> (0x1 * 0x8)) & 0xFF,   \
            .g = (hex >> (0x2 * 0x8)) & 0xFF,   \
            .r = (hex >> (0x3 * 0x8)) & 0xFF,   \
            }
enum {
    PLOTBG,
    TEXT,
    GRIDAXES,
    GRIDMAJOR,
    GRIDMINOR,

    DATA0,
    DATA1,
    DATAMAX,

    COLOR_TABLE_SIZE = 0x100,
};

imp_Color imp_ColorTable[COLOR_TABLE_SIZE] = {
    HEXCOLOR(0x0a030dff),
    HEXCOLOR(0xfffff7ff),
    HEXCOLOR(0x6a636dff),
    HEXCOLOR(0x3a333dff),
    HEXCOLOR(0x2a232dff),

    HEXCOLOR(0xff0059ff),
    HEXCOLOR(0xffcf00ff),
};

imp_Color color(u32 ind) { return imp_ColorTable[ind]; };

Vec2 view_to_screen_raw(Rect view, Rect screen, Vec2 p) {
    return (Vec2){
        ((p.x - view.x)/view.w)*screen.w + screen.x,
        (1.0 - (p.y - view.y)/view.h)*screen.h + screen.y,
    };
}

Vec2 view_to_screen(Plot *plot, Vec2 p) {
    return view_to_screen_raw(plot->view, plot->screen, p);
}
Vec2 screen_to_view(Plot *plot, Vec2 p) {
    return view_to_screen_raw(plot->screen, plot->view, p);
}

/* TODO move to command buffer read by backend */
void draw_rect(Context *imp, Rect r, imp_Color c) {
    RectCommand cmd = {
        .base.type = IMP_COMMAND_RECT,
        .base.color = c,
        .screen = r
    };
    push_command(imp, (Command){.rect = cmd});
}

void draw_grid_line(Context *imp, Vec2 start, Vec2 end, imp_Color c) {
    draw_rect(imp, (Rect){ .x = start.x, .y = end.y, .w = MAX(1,end.x-start.x), .h=MAX(1,start.y-end.y)}, c);
}

void draw_text(Context *imp, Vec2 pos, str text, f32 w, imp_Color c) {
    if (!w) {
        w = imp->text_width_fun(imp->text_width_data, text.str, text.len);
    }
    TextCommand cmd = {
        .base.type = IMP_COMMAND_TEXT,
        .base.color = c,
        .screen = {.x = pos.x, .y = pos.y, .w = w, .h = imp->text_height},
        .text = text
    };
    push_command(imp, (Command){.text=cmd});
}

void draw_data(Context *imp, Plot *plot, Data data) {
    if (~data.flags & IMP_DATA_CUSTOM_VIEW) {
        data.view = plot->view;
    }
    
    DataCommand cmd = {
        .base.type = IMP_COMMAND_DATA,
        .screen = plot->screen,
        .data = data
    };
    push_command(imp, (Command){.data = cmd});
}

enum {
    TEXT_LEFT = 0,
    TEXT_CENTERED,
    TEXT_RIGHT
};
Vec2 position_text(Context *imp, Vec2 pos, str text, s32 align, f32 *out_w) {
    s32 w = imp->text_width_fun(imp->text_width_data, text.str, text.len);
    switch (align) {
    case TEXT_LEFT: { pos.x = pos.x; pos.y = pos.y; } break;
    case TEXT_CENTERED: { pos.x = pos.x - w/2; pos.y = pos.y; } break;
    case TEXT_RIGHT: { pos.x = pos.x - w; pos.y = pos.y; } break;
    }

    if (out_w) *out_w = w;
    
    return pos;
}
/* */

void imp_init(Context *imp, s32 (*text_width_fun)(const void*, const char*, s32), void* text_width_data,  s32 text_height) {
    *imp = (Context){0};
    imp->text_width_fun = text_width_fun;
    imp->text_width_data = text_width_data;
    imp->text_height = text_height;
    imp->next_id  = HASH_INITIAL;
}

void imp_begin(Context *imp, Inputs frame_input) {
    imp->input.last_mouse = (frame_input.last_mouse.x && frame_input.last_mouse.y)?
        frame_input.last_mouse : imp->input.mouse;
    imp->input.mouse_pressed = frame_input.mouse_down && !imp->input.mouse_down;
    imp->input.mouse = frame_input.mouse;
    imp->input.mouse_scroll = frame_input.mouse_scroll;
    imp->input.mouse_down = frame_input.mouse_down;

    imp->char_pos = 0;
    imp->command_pos = 1;

    imp->current_plot = -1;

    imp->first_plot = 0;
    imp->prev_plot = 0;

    imp->counter++;
}

Plot * begin_plot(Context *imp, Rect r, str title) {
    ASSERT(imp->current_plot == -1);

    /* User can override next_id to give their plot whatever id they want */
    if (imp->next_id == HASH_INITIAL) {
        hash(&imp->next_id, title.str, title.len);
    }

    Plot *plot = alloc_plot(imp);

    if (imp->first_plot == 0) {
        imp->first_plot = plot;
    }

    plot->screen = r;
    plot->last_mouse = plot->mouse;
    plot->mouse = screen_to_view(plot, imp->input.mouse);

    if (plot->view.w == 0) {
        plot->view = (Rect){ .x = -10, .y = -10, .w = 20};
        plot->target_view = plot->view;

        /* TODO(lcf): remove, just using to test pushing some data */
        s32 n = 0x2000;
        plot->data[0].n = n;
        plot->data[0].x = malloc(n*sizeof(f32));
        plot->data[0].y = malloc(n*sizeof(f32));
        plot->data[0].z = malloc(n*sizeof(f32));

        for (s32 i = 0; i < n; i++) {
            plot->data[0].x[i] = (i-n/2)*0.1;
            plot->data[0].y[i] = 5.0*sin(i);
            plot->data[0].z[i] = 0;
        }
        
        plot->data[0].color = (imp_Color)HEXCOLOR(0xff0059ff);
        plot->flags = IMP_PLOT_DRAW_ALL_3D;

        plot->camera_pos = HMM_V3(1, 1, 1);
        plot->camera_target = HMM_V3(0, 0, 0);
        plot->camera_up = HMM_V3(0, 1, 1);
        plot->camera = HMM_LookAt_RH(plot->camera_pos, plot->camera_target, plot->camera_up);
        plot->view_to_screen = HMM_Perspective_RH_NO(90, plot->screen.w/plot->screen.h, 0.0001, 1000);
        plot->screen_to_view = HMM_InvPerspective_RH(plot->view_to_screen);
        plot->view_to_screen = HMM_MulM4(plot->view_to_screen, plot->camera);
    }

    plot->camera = HMM_Rotate_RH(imp->counter*0.001, HMM_V3(0, 0, 1));

    plot->view.h = plot->view.w * (plot->screen.h / plot->screen.w);
    plot->target_view.h = plot->target_view.w * (plot->screen.h / plot->screen.w);

    plot->first_command = imp->command_pos;
    
    return plot;
}

b32 point_in_rect(Rect r, Vec2 p) {
    return !(
        (p.x < r.x) ||
        (p.y < r.y) ||
        (p.x > r.x+r.w) ||
        (p.y > r.y+r.h)
        );
}

Vec2 clamp_to_rect(Rect r, Vec2 p) {
    p.x = (p.x < r.x)? r.x : ((p.x > r.x + r.w)? r.x + r.w : p.x);
    p.y = (p.y < r.y)? r.y : ((p.y > r.y + r.h)? r.y + r.h : p.y);
    return p;
}

Vec2 fvec2(f32 x, f32 y) { return (Vec2) {.x = x, .y = y};};
Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 m) { return (Vec2){ m*a.x + (1-m)*b.x, m*a.y + (1-m)*b.y }; }

void end_plot(Context *imp) {
    Plot *plot = current_plot(imp);
    draw_rect(imp, plot->screen, color(PLOTBG));

    if (point_in_rect(plot->view, plot->mouse)) {
        /* Drag */
        Vec2 mouse_delta = {plot->mouse.x - plot->target_view.x, plot->mouse.y - plot->target_view.y};
        if (imp->input.mouse_pressed) {
            plot->drag.x = plot->target_view.x + mouse_delta.x;
            plot->drag.y = plot->target_view.y + mouse_delta.y;
        }
        if (imp->input.mouse_down) {
            plot->target_view.x = plot->drag.x - mouse_delta.x;
            plot->target_view.y = plot->drag.y - mouse_delta.y;
        }

        /* Zoom */
        if (abs(imp->input.mouse_scroll) > 0.01) {
            Vec2 center = {plot->target_view.x + plot->target_view.w/2, plot->target_view.y + plot->target_view.h/2};
            if (imp->input.mouse_scroll > 0) {
                plot->target_view.w = plot->target_view.w*1.05*imp->input.mouse_scroll;
                plot->target_view.h = plot->target_view.h*1.05*imp->input.mouse_scroll;
            } else {
                plot->target_view.w = plot->target_view.w/(1.05*-imp->input.mouse_scroll);
                plot->target_view.h = plot->target_view.h/(1.05*-imp->input.mouse_scroll);
            }
            plot->target_view.x = center.x - plot->target_view.w/2;
            plot->target_view.y = center.y - plot->target_view.h/2;
        }
    }

    plot->view.pos = vec2_lerp(plot->view.pos, plot->target_view.pos, 0.1);
    plot->view.size = vec2_lerp(plot->view.size, plot->target_view.size, 0.1);

    f64 maxdim = MAX(plot->view.w,plot->view.h);
    f64 logscale = log(maxdim)/log(10);
    f64 intpart, fracpart = modf(logscale, &intpart);
    f64 step = pow(10, intpart-1);
    f64 majstep = step;
    step /= 2;
    if (logscale < 0) {
        step /= 10;
        majstep /= 10;
        fracpart = 1.0 + fracpart;
    }
    if (fracpart > 0.75) {
        majstep *= 8;
        step *= 4;
    } else 
    if (fracpart > 0.5) {
        majstep *= 4;
        step *= 2;
    } else 
    if (fracpart > 0.25) {
        majstep *= 2;
    }


    /* Set precision for label formatting */
    /* TODO: Need a better way to do this. Should handle each axis seperately */
    char *labelfmt;
    s32 print_precision; {
        /* "Good" range where we dont have many rounding errors */
        if (logscale > -2 && logscale < 8) {
            labelfmt = "%.7g";
        } else {
            labelfmt = "%.1e";
        }
    }
        
    /* Draw Minor Grid */
    {
        f32 stepx = step;
        f32 stepy = step;

        f32 startx, starty;
        startx = copysign(floor(fabs(plot->view.x)/stepx)*stepx, plot->view.x);
        starty = copysign(floor(fabs(plot->view.y)/stepy)*stepy, plot->view.y);

        for (f32 x = startx; x < plot->view.x + plot->view.w; x += stepx ) {
            if (x < plot->view.x) continue;
            draw_grid_line(imp,
                      view_to_screen(plot, fvec2(x, plot->view.y)),
                      view_to_screen(plot, fvec2(x, plot->view.y + plot->view.h)),
                      color(GRIDMINOR)
                );
        }


        for (f32 y = starty; y < plot->view.y + plot->view.h; y += stepy ) {
            if (y < plot->view.y) continue;
            draw_grid_line(imp,
                      view_to_screen(plot, fvec2(plot->view.x, y)),
                      view_to_screen(plot, fvec2(plot->view.x + plot->view.w, y)),
                      color(GRIDMINOR)
                );
        }
    }
    
    /* Draw Major Grid and Labels */
    b32 xaxis_visible = point_in_rect(plot->view, (Vec2) {plot->view.x + plot->view.w/2, 0});
    b32 yaxis_visible = point_in_rect(plot->view, (Vec2) {0, plot->view.y + plot->view.h/2});
    {
        f32 stepx = majstep;
        f32 stepy = majstep;

        f32 startx, starty;
        startx = copysign(floor(fabs(plot->view.x)/stepx)*stepx, plot->view.x);
        starty = copysign(floor(fabs(plot->view.y)/stepy)*stepy, plot->view.y);

        for (f32 x = startx; x < plot->view.x + plot->view.w; x += stepx ) {
            if (x < plot->view.x) continue;
            draw_grid_line(imp,
                      view_to_screen(plot, fvec2(x, plot->view.y)),
                      view_to_screen(plot, fvec2(x, plot->view.y + plot->view.h)),
                      color(GRIDMAJOR)
                );
        }

        for (f32 y = starty; y < plot->view.y + plot->view.h; y += stepy ) {
            if (y < plot->view.y) continue;
            draw_grid_line(imp,
                      view_to_screen(plot, fvec2(plot->view.x, y)),
                      view_to_screen(plot, fvec2(plot->view.x + plot->view.w, y)),
                      color(GRIDMAJOR)
                );
        }

        if (yaxis_visible) {
            draw_grid_line(imp,
                      view_to_screen(plot, fvec2(0, plot->view.y)),
                      view_to_screen(plot, fvec2(0, plot->view.y+plot->view.h)),
                      color(GRIDAXES)
                );
        }

        if (xaxis_visible) {
        draw_grid_line(imp,
                  view_to_screen(plot, fvec2(plot->view.x, 0)),
                  view_to_screen(plot, fvec2(plot->view.x+plot->view.w, 0)),
                  color(GRIDAXES)
            );
        }
 
        Rect screen_margin = plot->screen;
        screen_margin.h -= imp->text_height;
        screen_margin.x += imp->text_height/2;
        screen_margin.w -= imp->text_height;

        for (f32 x = startx; x < plot->view.x + plot->view.w; x += stepx ) {
            if (x < plot->view.x) continue;
            /* Don't label origin  */
            if (fabs(x) < 0.01*step) {
                continue;
            }

            /* Draw Label */
            str label = strf(imp, labelfmt, x);
            Vec2 p = view_to_screen(plot, (Vec2){x, 0});
            p.y += imp->text_height*.05;
            f32 w;
            p = position_text(imp, p, label, TEXT_CENTERED, &w);
            p = clamp_to_rect(screen_margin, p);
            p.x += w;
            p = clamp_to_rect(screen_margin, p);
            p.x -= w;
            draw_text(imp, p, label, w, color(TEXT));
        }


        for (f32 y = starty; y < plot->view.y + plot->view.h; y += stepy ) {
            if (y < plot->view.y) continue;
            if (fabs(y) < 0.01*step) {
                continue;
            }

            /* Draw Label */
            str label = strf(imp, labelfmt, y);
            Vec2 p = view_to_screen(plot, (Vec2){0, y});
            f32 w;
            p = position_text(imp, p, label, plot->view.x > 0? TEXT_LEFT : TEXT_RIGHT, &w);
            if (plot->view.x > 0) {
                p = clamp_to_rect(screen_margin, p);
            } else {
                p.x += w;
                p = clamp_to_rect(screen_margin, p);
                p.x -= w;
            }
            
            draw_text(imp, p, label, w, color(TEXT));
        }

        draw_data(imp, plot, plot->data[0]);
    }

    plot->last_command = imp->command_pos;
    plot->next_command = plot->first_command;

    if (imp->prev_plot) {
        imp->prev_plot->next = plot - imp->plot;
    }
    imp->prev_plot = plot;
    plot->next = -1;

    imp->current_plot = -1;
}

void imp_end(Context *imp) {
    imp->current_plot = -1;
}

Plot * imp_next_plot(Context *imp) {
    Plot *plot = imp->first_plot;

    if (plot) {
        imp->first_plot = (plot->next > 0)? imp->plot + plot->next : 0;
    }

    return plot;
}

b32 imp_next_plot_command(Context *imp, Plot *plot, Command *cmd) {
    b32 out = 0;
    if (plot) {
        *cmd = imp->command_buffer[plot->next_command++];
        out = plot->next_command <= plot->last_command;
    }
    return out;
}

b32 imp_next_command(Context *imp, Command *cmd) {
    Plot *plot = imp->first_plot;

    if (plot && plot->next_command > plot->last_command) {
        plot = imp_next_plot(imp);
    }

    return imp_next_plot_command(imp, plot, cmd);
}

