#include "lcf/lcf.h"
#include "lcf/lcf.c"

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

#define PCAST(type, p) (*((type*)&p))

typedef union Color Color;
union Color {
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
    s32 nx, ny;
    Color color;
    f32 *x;
    f32 *y;
};

#define IMP_MAX_DATA 8
typedef struct Plot Plot;
struct Plot {
    Rect screen;
    Rect view;
    Rect target_view;
    Data data[IMP_MAX_DATA];

    s32 minorticksx;
    s32 minorticksy;
    s32 ticksx;
    s32 ticksy;

    Vec2 mouse;
    Vec2 last_mouse;
    Vec2 drag;
};

#define IMP_CHAR_BUFFER_SIZE (0x4000)
typedef struct Context Context;
struct Context {
    Plot plot[1];

    Vec2 screen_mouse;
    f32 mouse_scroll;
    s32 mouse_pressed;
    s32 mouse_down;

    mu_Context *mu;
    s32 font_height;

    s32 char_pos;
    char char_buffer[IMP_CHAR_BUFFER_SIZE];
};

#include <stdarg.h>
str strfv(Context *imp, char *fmt, va_list args) {
    str result = (str){0};
    result.str = imp->char_buffer + imp->char_pos;
    result.len = vsprintf_s(imp->char_buffer + imp->char_pos, IMP_CHAR_BUFFER_SIZE - imp->char_pos, fmt, args);
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

    COLOR_TABLE_SIZE = 0x100,
};

Color ColorTable[COLOR_TABLE_SIZE] = {
    HEXCOLOR(0x0a030dff),
    HEXCOLOR(0xfffff7ff),
    HEXCOLOR(0x6a636dff),
    HEXCOLOR(0x3a333dff),
    HEXCOLOR(0x2a232dff),
};

Color color(u32 ind) { return ColorTable[ind]; };

Vec2 view_to_screen(Plot *plot, Vec2 p) {
    return (Vec2){
        ((p.x - plot->view.x)/plot->view.w)*plot->screen.w + plot->screen.x,
        (1.0 - (p.y - plot->view.y)/plot->view.h)*plot->screen.h + plot->screen.y,
    };
}
Vec2 screen_to_view(Plot *plot, Vec2 p) {
    return (Vec2){
        ((p.x - plot->screen.x)/plot->screen.w)*plot->view.w + plot->view.x,
        (1.0 - (p.y - plot->screen.y)/plot->screen.h)*plot->view.h + plot->view.y,
    };
}

/* TODO move to command buffer read by backend */
void draw_rect(Context *imp, Rect r, Color c) {
    mu_Rect mr = {.x = r.x, .y = r.y, .w = r.w, .h=r.h};
    mu_draw_rect(imp->mu, mr, PCAST(mu_Color, c));
}

void draw_line(Context *imp, Vec2 start, Vec2 end, Color c) {
    draw_rect(imp, (Rect){ .x = start.x, .y = end.y, .w = MAX(1,end.x-start.x), .h=MAX(1,start.y-end.y)}, c);
}

void draw_data(Context *imp, Plot *plot, Data d) {
    for (s32 i = 0; i < d.nx; i++) {
        Vec2 p = {d.x[i], d.y[i]};
        if (point_in_rect(plot->view, p)) {
            Rect r; r.pos = view_to_screen(plot, p);
            r.w = 5; r.x -= r.w/2;
            r.h = 5; r.y -= r.w/2;
            draw_rect(imp, r, d.color);
        }
    }
}

enum {
    TEXT_LEFT = 0,
    TEXT_CENTERED,
    TEXT_RIGHT
};
Vec2 position_text(Context *imp, Vec2 pos, str text, s32 align, f32 *out_w) {
    s32 w = imp->mu->text_width(imp->mu->style->font, text.str, text.len);
    switch (align) {
    case TEXT_LEFT: { pos.x = pos.x; pos.y = pos.y; } break;
    case TEXT_CENTERED: { pos.x = pos.x - w/2; pos.y = pos.y; } break;
    case TEXT_RIGHT: { pos.x = pos.x - w; pos.y = pos.y; } break;
    }

    if (out_w) *out_w = w;
    
    return pos;
}
void draw_text(Context *imp, Vec2 pos, str text, Color c) {
    mu_Vec2 p = {pos.x, pos.y};
    mu_draw_text(imp->mu, imp->mu->style->font, text.str, text.len, p, PCAST(mu_Color, c));
}
/* */

void update_mouse(Context *imp, f32 mouse_x, f32 mouse_y, s32 mouse_down, f32 mouse_scroll) {
    imp->screen_mouse = (Vec2) {mouse_x, mouse_y};
    imp->mouse_pressed = !!mouse_down && !(imp->mouse_down);
    imp->mouse_down = !!mouse_down;
    imp->mouse_scroll = mouse_scroll;

    Plot *plot = imp->plot;
    plot->last_mouse = plot->mouse;
    plot->mouse = screen_to_view(plot, imp->screen_mouse);
}

void begin_plot(Context *imp, Rect r) {
    Plot *plot = imp->plot;
    plot->screen = r;

    /* TODO: move to imp_begin() */
    imp->char_pos = 0;

    if (plot->view.w == 0) {
        plot->view = (Rect){ .x = -10, .y = -10, .w = 20};
        plot->target_view = plot->view;

        plot->data[0].nx = 5;

        s32 n = 0xc000;
        plot->data[0].nx = n;
        plot->data[0].x = malloc(n*sizeof(f32));
        plot->data[0].y = malloc(n*sizeof(f32));

        for (s32 i = 0; i < n; i++) {
            plot->data[0].x[i] = (i-n/2)*0.1;
            plot->data[0].y[i] = 5.0*sin(i);
        }
        
        plot->data[0].color = (Color)HEXCOLOR(0xff0059ff);
    }

    plot->view.h = plot->view.w * (plot->screen.h / plot->screen.w);
    plot->target_view.h = plot->target_view.w * (plot->screen.h / plot->screen.w);
    plot->ticksx = 30;
    plot->ticksy = 30;
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
    Plot *plot = imp->plot;
    draw_rect(imp, plot->screen, color(PLOTBG));

    if (point_in_rect(plot->view, plot->mouse)) {
        /* Drag */
        Vec2 mouse_delta = {plot->mouse.x - plot->target_view.x, plot->mouse.y - plot->target_view.y};
        if (imp->mouse_pressed) {
            plot->drag.x = plot->target_view.x + mouse_delta.x;
            plot->drag.y = plot->target_view.y + mouse_delta.y;
        }
        if (imp->mouse_down) {
            plot->target_view.x = plot->drag.x - mouse_delta.x;
            plot->target_view.y = plot->drag.y - mouse_delta.y;
        }

        /* Zoom */
        if (abs(imp->mouse_scroll) > 0.01) {
            Vec2 center = {plot->target_view.x + plot->target_view.w/2, plot->target_view.y + plot->target_view.h/2};
            if (imp->mouse_scroll > 0) {
                plot->target_view.w = plot->target_view.w*1.05*imp->mouse_scroll;
                plot->target_view.h = plot->target_view.h*1.05*imp->mouse_scroll;
            } else {
                plot->target_view.w = plot->target_view.w/(1.05*-imp->mouse_scroll);
                plot->target_view.h = plot->target_view.h/(1.05*-imp->mouse_scroll);
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
            draw_line(imp,
                      view_to_screen(plot, fvec2(x, plot->view.y)),
                      view_to_screen(plot, fvec2(x, plot->view.y + plot->view.h)),
                      color(GRIDMINOR)
                );
        }


        for (f32 y = starty; y < plot->view.y + plot->view.h; y += stepy ) {
            if (y < plot->view.y) continue;
            draw_line(imp,
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
            draw_line(imp,
                      view_to_screen(plot, fvec2(x, plot->view.y)),
                      view_to_screen(plot, fvec2(x, plot->view.y + plot->view.h)),
                      color(GRIDMAJOR)
                );
        }

        for (f32 y = starty; y < plot->view.y + plot->view.h; y += stepy ) {
            if (y < plot->view.y) continue;
            draw_line(imp,
                      view_to_screen(plot, fvec2(plot->view.x, y)),
                      view_to_screen(plot, fvec2(plot->view.x + plot->view.w, y)),
                      color(GRIDMAJOR)
                );
        }

        if (yaxis_visible) {
            draw_line(imp,
                      view_to_screen(plot, fvec2(0, plot->view.y)),
                      view_to_screen(plot, fvec2(0, plot->view.y+plot->view.h)),
                      color(GRIDAXES)
                );
        }

        if (xaxis_visible) {
        draw_line(imp,
                  view_to_screen(plot, fvec2(plot->view.x, 0)),
                  view_to_screen(plot, fvec2(plot->view.x+plot->view.w, 0)),
                  color(GRIDAXES)
            );
        }
 
        Rect screen_margin = plot->screen;
        screen_margin.h -= imp->font_height;
        screen_margin.x += imp->font_height/2;
        screen_margin.w -= imp->font_height;

        for (f32 x = startx; x < plot->view.x + plot->view.w; x += stepx ) {
            if (x < plot->view.x) continue;
            /* Don't label origin  */
            if (fabs(x) < 0.01*step) {
                continue;
            }

            /* Draw Label */
            str label = strf(imp, labelfmt, x);
            Vec2 p = view_to_screen(plot, (Vec2){x, 0});
            p.y += imp->font_height*.05;
            f32 w;
            p = position_text(imp, p, label, TEXT_CENTERED, &w);
            p = clamp_to_rect(screen_margin, p);
            p.x += w;
            p = clamp_to_rect(screen_margin, p);
            p.x -= w;
            draw_text(imp, p, label, color(TEXT));
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
            
            draw_text(imp, p, label, color(TEXT));
        }

        draw_data(imp, plot, plot->data[0]);
    }
}
