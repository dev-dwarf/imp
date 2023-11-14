/* TODO(lcf, September 26, 2023)
   - major and minor grids
   -- should be able to set number of grid lines
   -- should also have good defaults that dont require config
   -- also just ticks but no grid option.

   - move configuration to plot_begin or plot_end?
   - so that user can override stuff more easily.

   - start making drawing more configurable.
   - switch to draw commands system.

   - translation
   - drag and move translation
   - simple in 2d, how should work in 3d?

   - fix 2d mode text alignment issue.

   - figure out number of grid lines automatically.
   - also let it be configured.

   - panning should maintain grid line positions to some extent.

   - make sure camera matrices are renderer agnostic.

   - support multiple Y (2D) or Z (3D) axis scales on same plotbox.

   - software clipping with linear projection.
   - 
 */

#include "third_party/raylib/raylib.h"
#include "third_party/raylib/rlgl.h"
#include "third_party/microui/microui.h"
#include "third_party/HandmadeMath.h"

extern void glPixelStorei(int, int);
extern void glGenTextures(unsigned, unsigned*);
extern void glBindTexture(int, unsigned);
extern void glTexImage2D(int, int, int, unsigned, unsigned, int, int, int, const void*);
extern void glTexParameteriv(int, int, const int*);
extern void glTexParameteri(int, int, int);

enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };
enum { ATLAS_WIDTH = 256, ATLAS_HEIGHT = 256 };

extern unsigned char atlas_data[ATLAS_WIDTH * ATLAS_HEIGHT];
extern Rectangle atlas_rect[256];
static Texture atlas;

#include "atlas.h"

#include "imp.h"
static Context *imp;

enum {
    IMP_MARKER_CIRCLE = 0,
    IMP_MARKER_X,
    IMP_MARKER_SQUARE,
    IMP_MARKER_TRIANGLE,
    IMP_MARKER_CROSS,
    IMP_MARKER_DIAMOND,
    IMP_MARKER_HEART,
    IMP_MARKER_ARROW,
    IMP_MARKER_COUNT,
    IMP_MARKER_CIRCLE_OUTLINE,
    IMP_MARKER_X_OUTLINE,
    IMP_MARKER_SQUARE_OUTLINE,
    IMP_MARKER_TRIANGLE_OUTLINE,
    IMP_MARKER_CROSS_OUTLINE,
    IMP_MARKER_DIAMOND_OUTLINE,
    IMP_MARKER_ARROW_OUTLINE,
    IMP_MARKER_HEART_OUTLINE,
    IMP_LINE_TEXTURE
};

typedef struct ImpDrawPlane ImpDrawPlane;
struct ImpDrawPlane {
    HMM_Vec3 bl;
    HMM_Vec3 r;
    HMM_Vec3 u;
};

enum {
    IMP_CAMERA_CUSTOM        = 1 << 0,
    IMP_CAMERA_PERSPECTIVE   = 1 << 1,
    IMP_CAMERA_Y_UP          = 1 << 2,
    IMP_AXIS_X               = 1 << 3,
    IMP_AXIS_Y               = 1 << 4,
    IMP_AXIS_Z               = 1 << 5,
    IMP_GRID_YZ              = 1 << 6,
    IMP_GRID_ZX              = 1 << 7,
    IMP_GRID_XY              = 1 << 8,
    IMP_AXIS_Z_TEXT_RIGHT    = 1 << 9,
    IMP_GRID_MARGIN          = 1 << 10,

    IMP_DRAW_MARKERS         = 1 << 16,
    IMP_DRAW_LINES           = 1 << 17,

    IMP_AXIS_ALL             = (IMP_AXIS_X | IMP_AXIS_Y | IMP_AXIS_Z),
    IMP_GRID_ALL             = (IMP_GRID_YZ | IMP_GRID_ZX | IMP_GRID_XY),
    IMP_PRESET_2D            = (IMP_CAMERA_Y_UP | IMP_AXIS_X | IMP_AXIS_Y | IMP_GRID_XY | IMP_GRID_MARGIN),
    IMP_PRESET_3D            = (IMP_AXIS_ALL | IMP_GRID_ALL),

};
typedef struct ImpPlot {
    u64 flags;

    Rect screen;
    HMM_Vec3 plot_min;
    HMM_Vec3 plot_max;
    HMM_Vec3 view_radius;
    HMM_Vec3 view_pos;
    HMM_Vec3 view_up;
    f32 angle;
    f32 zoom;
    f32 fov;
    f32 z_sensitivity;
    f32 angle_sensitivity;
    f32 zoom_sensitivity;
    HMM_Mat4 projection;
    HMM_Mat4 plot_rotation;
    HMM_Mat4 camera;
    ImpDrawPlane billboard;
    HMM_Vec3 billboard_z;
    HMM_Vec3 camera_pos;

    str xlabel;
    str ylabel;
    str zlabel;

    HMM_Vec2 mouse_drag;
    HMM_Vec3 drag_min;
    HMM_Vec3 drag_max;
    
    f32 text_size;
    f32 line_size;

    f32 line_size_f;
    f32 text_size_f;
    
    HMM_Vec3 text_percent_offset;
    f32 grid_margin;
} ImpPlot;

enum {
    IMP_TEXT_ALIGN_LEFT   = 0,
    IMP_TEXT_ALIGN_CENTER = 1,
    IMP_TEXT_ALIGN_RIGHT  = 2,
};
enum {
    IMP_TEXT_ALIGN_BOTTOM = 0,
    IMP_TEXT_ALIGN_MIDDLE = 1,
    IMP_TEXT_ALIGN_TOP    = 2,
};

static inline void ImpDrawTexQuadFromAtlas(ImpDrawPlane p, Rectangle tex, Color color) {
    rlSetTexture(atlas.id);
    HMM_Vec3 vbl = p.bl;
    HMM_Vec3 vbr = HMM_AddV3(p.bl, p.r);
    HMM_Vec3 vtl = HMM_AddV3(p.bl, p.u);
    HMM_Vec3 vtr = HMM_AddV3(vtl , p.r);

    HMM_Vec2 ttl = HMM_V2((f32)tex.x/ATLAS_WIDTH, (f32)tex.y/ATLAS_HEIGHT);
    HMM_Vec2 ttr = HMM_V2(ttl.X+(f32)tex.width/ATLAS_WIDTH, ttl.Y);
    HMM_Vec2 tbl = HMM_V2(ttl.X, ttl.Y+(f32)tex.height/ATLAS_HEIGHT);
    HMM_Vec2 tbr = HMM_V2(ttr.X, tbl.Y);

    rlBegin(RL_QUADS);
    rlColor4ub(color.r, color.g, color.b, color.a);
    rlTexCoord2f(ttl.X, ttl.Y); rlVertex3f(vtl.X, vtl.Y, vtl.Z);
    rlTexCoord2f(tbl.X, tbl.Y); rlVertex3f(vbl.X, vbl.Y, vbl.Z);
    rlTexCoord2f(tbr.X, tbr.Y); rlVertex3f(vbr.X, vbr.Y, vbr.Z);
    rlTexCoord2f(ttr.X, ttr.Y); rlVertex3f(vtr.X, vtr.Y, vtr.Z);
    rlEnd();
}

void ImpDrawLine(ImpPlot *plot, HMM_Vec3 start, HMM_Vec3 end, Color color, float thickness) {
    f32 size = plot->line_size_f*thickness;

    ImpDrawPlane p;
    HMM_Vec3 dir = HMM_SubV3(end, start);
    HMM_Vec3 udir = HMM_NormV3(dir);

    HMM_Vec3 to_camera;
    /* if (plot->flags & IMP_CAMERA_PERSPECTIVE) { */
        /* to_camera = (HMM_SubV3(start, plot->camera_pos)); */
    /* } else { */
        /* for orthographic mode the camera vector should be constant (no projection) */
        to_camera = plot->billboard_z;
    /* } */
    /* get direction orthogonal to line direction for maximum projected area. */
    to_camera = HMM_NormV3(HMM_SubV3(to_camera, HMM_MulV3F(udir, HMM_DotV3(udir, to_camera))));
    HMM_Vec3 width_dir = HMM_NormV3(HMM_Cross(udir, to_camera));
    /* color.r = 255.0*width_dir.X; color.g = 255.0*width_dir.Y; color.b = 255.0*width_dir.Z; */

    width_dir = HMM_MulV3F(width_dir, size);
    p.bl = HMM_SubV3(start, HMM_MulV3F(width_dir, 0.5));
    p.r = dir;
    p.u = width_dir;
    Rectangle r = atlas_rect[IMP_LINE_TEXTURE];
    ImpDrawTexQuadFromAtlas(p, r, color);
}

void ImpDrawGridLines(ImpPlot *plot, HMM_Vec3 start, HMM_Vec3 end, HMM_Vec3 sweep, Color color, s32 n, float thickness) {
    HMM_Vec3 step = HMM_DivV3F(sweep, n-1);
    ImpDrawPlane p;
    HMM_Vec3 dir = HMM_SubV3(end, start);


    Rectangle r = atlas_rect[IMP_LINE_TEXTURE];
    for (s32 i = 0; i < n; i++) {
        ImpDrawLine(plot, start, end, color, thickness);
        start = HMM_AddV3(start, step);
        end = HMM_AddV3(end, step);
    }
}

static HMM_Vec2 ImpMeasureText(str text) {
    HMM_Vec2 out = {0};
    for (s32 i = 0; i < text.len; i++) {
        /* Skip leading minus sign for numbers */
        if (i == 0 && text.str[i] == '-') {
            continue;
        }
        
        Rectangle r = atlas_rect[ATLAS_FONT + text.str[i]];
        out.Y = MAX(r.height, out.Y);
        out.X += r.width + 1.0;
    }
    return out;
}

static ImpDrawPlane ImpAlignText(ImpPlot *plot, ImpDrawPlane plane, HMM_Vec2 size, float scale, int align_h, int align_v) {
    scale *= plot->text_size_f;
    f32 f;
    switch (align_h) {
    case IMP_TEXT_ALIGN_LEFT  : {f = scale;         } break;
    case IMP_TEXT_ALIGN_CENTER: {f = scale*size.X/2;} break;
    case IMP_TEXT_ALIGN_RIGHT : {f = scale*size.X;  } break;
    }
    plane.bl = HMM_SubV3(plane.bl, HMM_MulV3F(plane.r, f));

    switch (align_v) {
    case IMP_TEXT_ALIGN_BOTTOM: {f = scale;         } break;
    case IMP_TEXT_ALIGN_MIDDLE: {f = scale*size.Y/2;} break;
    case IMP_TEXT_ALIGN_TOP   : {f = scale*size.Y;  } break;
    }
    plane.bl = HMM_SubV3(plane.bl, HMM_MulV3F(plane.u, f));

    return plane;
}

static void ImpDrawText3D(ImpPlot *plot, ImpDrawPlane plane, str text, Color color, f32 scale) {
    scale *= plot->text_size_f;
    plane.r = HMM_MulV3F(plane.r, scale);
    plane.u = HMM_MulV3F(plane.u, scale);
    for (s32 i = 0; i < text.len; i++) {
        Rectangle r = atlas_rect[ATLAS_FONT + text.str[i]];
        ImpDrawPlane p = plane;
        p.r = HMM_MulV3F(p.r, r.width);
        p.u = HMM_MulV3F(p.u, r.height);
        ImpDrawTexQuadFromAtlas(p, r, color);
        plane.bl = HMM_AddV3(plane.bl, p.r);
    }
}

void ImpDrawPlot(ImpPlot* plot, HMM_Vec3 camera_pos) {
    Color color = {.r = 0xdd, .g = 0xdd, .b = 0xdd, .a = 0xff};
    rlColor4ub(color.r, color.g, color.b, color.a);

    f32 _margin = (plot->flags & IMP_GRID_MARGIN)? 1 + plot->grid_margin : 1;
    HMM_Vec3 margin = HMM_MulV3F(plot->view_radius, -_margin);
    HMM_Vec3 margax = HMM_MulV3F(plot->view_radius, _margin);
    HMM_Vec3 center = {0,0,0};
    HMM_Vec3 min = HMM_MulV3F(plot->view_radius, -1);
    HMM_Vec3 max = HMM_MulV3F(plot->view_radius, +1);

    plot->text_size_f = plot->text_size*plot->zoom;
    plot->line_size_f = plot->text_size_f*plot->line_size;

    f32 num_text_size = 0.66;
    HMM_Vec3 num_percent_offset;
    num_percent_offset.X = 1 + 0.5*plot->text_percent_offset.X*plot->zoom;
    num_percent_offset.Y = 1 + 0.5*plot->text_percent_offset.Y*plot->zoom;
    num_percent_offset.Z = 1 + 0.5*plot->text_percent_offset.Z*plot->zoom;

    f32 label_text_size = 1;
    HMM_Vec3 label_percent_offset;
    label_percent_offset.X = 1 + plot->text_percent_offset.X*plot->zoom;
    label_percent_offset.Y = 1 + plot->text_percent_offset.Y*plot->zoom;
    label_percent_offset.Z = 1 + plot->text_percent_offset.Z*plot->zoom;

    color = BLACK;
#define COLOR_AXES BLACK
#define COLOR_GRID_LINE LIGHTGRAY
#define N_GRID_LINES 6

    if (plot->flags & IMP_GRID_XY) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=color.r, .g=color.g, .b=0xff, .a=color.a}; */
        
        if (camera_pos.Z > center.Z) {
            /* Bottom */
            ImpDrawGridLines(plot, HMM_V3(min.X, margin.Y, min.Z), HMM_V3(min.X, margax.Y, min.Z),
                             HMM_V3(max.X-min.X, 0, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(margin.X, min.Y, min.Z), HMM_V3(margax.X, min.Y, min.Z),
                             HMM_V3(0, max.Y-min.Y, 0), c, N_GRID_LINES, 1);
        } else {
            /* Top */
            ImpDrawGridLines(plot, HMM_V3(max.X, margax.Y, max.Z), HMM_V3(max.X, margin.Y, max.Z),
                             HMM_V3(min.X-max.X, 0, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(margax.X, max.Y, max.Z), HMM_V3(margin.X, max.Y, max.Z),
                             HMM_V3(0, min.Y-max.Y, 0), c, N_GRID_LINES, 1);
        }
    }

    if (plot->flags & IMP_GRID_ZX) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=color.r, .g=0xff, .b=color.b, .a=color.a}; */
                    
        if (camera_pos.Y > center.Y) {
            /* Right */
            ImpDrawGridLines(plot, HMM_V3(min.X, min.Y, margin.Z), HMM_V3(min.X, min.Y, margax.Z),
                             HMM_V3(max.X-min.X, 0, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(margin.X, min.Y, min.Z), HMM_V3(margax.X, min.Y, min.Z),
                             HMM_V3(0, 0, max.Z-min.Z), c, N_GRID_LINES, 1);

        } else {
            /* Left */
            ImpDrawGridLines(plot, HMM_V3(max.X, max.Y, margax.Z), HMM_V3(max.X, max.Y, margin.Z),
                             HMM_V3(min.X-max.X, 0, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(margax.X, max.Y, max.Z), HMM_V3(margin.X, max.Y, max.Z),
                             HMM_V3(0, 0, min.Z-max.Z), c, N_GRID_LINES, 1);
        }
    }

    if (plot->flags & IMP_GRID_YZ) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=0xff, .g=color.g, .b=color.b, .a=color.a}; */
        
        if (camera_pos.X > center.X) {
            /* Near */
            ImpDrawGridLines(plot, HMM_V3(min.X, min.Y, margin.Z), HMM_V3(min.X, min.Y, margax.Z),
                             HMM_V3(0, max.Y-min.Y, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(min.X, margin.Y, min.Z), HMM_V3(min.X, margax.Y, min.Z),
                             HMM_V3(0, 0, max.Z-min.Z), c, N_GRID_LINES, 1);
        } else {
            /* Far */
            ImpDrawGridLines(plot, HMM_V3(max.X, max.Y, margax.Z), HMM_V3(max.X, max.Y, margin.Z),
                             HMM_V3(0, min.Y-max.Y, 0), c, N_GRID_LINES, 1);
            ImpDrawGridLines(plot, HMM_V3(max.X, margax.Y, max.Z), HMM_V3(max.X, margin.Y, max.Z),
                             HMM_V3(0, 0, min.Z-max.Z), c, N_GRID_LINES, 1);
        }
    }

    if (plot->flags & IMP_AXIS_X) {
        Color c = COLOR_AXES;
        s32 align_h = IMP_TEXT_ALIGN_CENTER;
        s32 align_v = (camera_pos.Z > center.Z)? IMP_TEXT_ALIGN_BOTTOM : IMP_TEXT_ALIGN_TOP;

        if (plot->flags & IMP_CAMERA_Y_UP) {
            /* TODO: special case for 2d axes drawing? */
            align_v = IMP_TEXT_ALIGN_TOP;
        }
        
        HMM_Vec3 closest = min;
        closest.Y = (camera_pos.Y > center.Y)? max.Y : min.Y;
        closest.Z = (camera_pos.Z > center.Z)? min.Z : max.Z;
        HMM_Vec3 end = closest; end.X = max.X;
        HMM_Vec3 lclosest = closest; lclosest.X = margin.X;
        HMM_Vec3 lend = end; lend.X = margax.X;

        ImpDrawLine(plot, lclosest, lend, c, 1);
        
        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 x = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = plot->billboard;
            p.bl = HMM_MulV3F(closest, num_percent_offset.X);
            p.bl.X = HMM_Lerp(closest.X, x, end.X);
            str s = strf(imp, "%0.3g", HMM_Lerp(plot->plot_min.X, x, plot->plot_max.X));
            p = ImpAlignText(plot, p, ImpMeasureText(s), num_text_size, align_h, align_v);
            ImpDrawText3D(plot, p, s, c, num_text_size);
        }

        c = (Color){.r=0xdf, .g=color.g, .b=color.b, .a=color.a};
        ImpDrawPlane p = plot->billboard;
        p.bl = HMM_MulV3F(closest, label_percent_offset.X);
        p.bl.X = (camera_pos.X > center.X)? min.X - plot->view_radius.X*0.1*plot->zoom  : max.X + plot->view_radius.X*0.1*plot->zoom ;
        str s = plot->xlabel.str? plot->xlabel : imp_str("X");
        p = ImpAlignText(plot, p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(plot, p, s, c, 1);
    }
    
    if (plot->flags & IMP_AXIS_Y) {
        Color c = COLOR_AXES;
        s32 align_h = IMP_TEXT_ALIGN_CENTER;
        s32 align_v = (camera_pos.Z > center.Z)? IMP_TEXT_ALIGN_BOTTOM : IMP_TEXT_ALIGN_TOP;

        if (plot->flags & IMP_CAMERA_Y_UP) {
            /* TODO: special case for 2d axes drawing? */
            align_v = IMP_TEXT_ALIGN_MIDDLE;
        }
        
        HMM_Vec3 closest = min;
        closest.X = (camera_pos.X > center.X)? max.X : min.X;
        closest.Z = (camera_pos.Z > center.Z)? min.Z : max.Z;
        HMM_Vec3 end = closest; end.Y = max.Y;
        HMM_Vec3 lclosest = closest; lclosest.Y = margin.Y;
        HMM_Vec3 lend = end; lend.Y = margax.Y;

        ImpDrawLine(plot, lclosest, lend, c, 1);

        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 y = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = plot->billboard;
            p.bl = HMM_MulV3F(closest, num_percent_offset.Y);
            p.bl.Y = HMM_Lerp(closest.Y, y, end.Y);
            str s = strf(imp, "%0.3g", HMM_Lerp(plot->plot_min.Y, y, plot->plot_max.Y));
            p = ImpAlignText(plot, p, ImpMeasureText(s), num_text_size, align_h, align_v);
            ImpDrawText3D(plot, p, s, c, num_text_size);
        }

        c = (Color){.r=color.r, .g=0xdf, .b=color.b, .a=color.a};
        ImpDrawPlane p = plot->billboard;
        p.bl = HMM_MulV3F(closest, label_percent_offset.Y);
        p.bl.Y = (camera_pos.Y > center.Y)? min.Y - plot->view_radius.Y*0.1*plot->zoom  : max.Y + plot->view_radius.Y*0.1*plot->zoom ;
        str s = plot->ylabel.str? plot->ylabel : imp_str("Y");
        p = ImpAlignText(plot, p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(plot, p, s, c, 1);
    }

    if (plot->flags & IMP_AXIS_Z) {
        Color c = COLOR_AXES;
        s32 align_h = (plot->flags & IMP_AXIS_Z_TEXT_RIGHT)? IMP_TEXT_ALIGN_LEFT : IMP_TEXT_ALIGN_RIGHT;
        s32 align_v = (camera_pos.Z > center.Z)? IMP_TEXT_ALIGN_BOTTOM : IMP_TEXT_ALIGN_TOP;
        
        HMM_Vec3 closest = min;
        if (plot->flags & IMP_AXIS_Z_TEXT_RIGHT) {
            /* On Right */
            closest.X = (camera_pos.Y < center.Y)? max.X : min.X;
            closest.Y = (camera_pos.X < center.X)? min.Y : max.Y;
        } else {
            /* On Left */
            closest.X = (camera_pos.Y > center.Y)? max.X : min.X;
            closest.Y = (camera_pos.X > center.X)? min.Y : max.Y;
        }
        HMM_Vec3 end = closest; end.Z = max.Z;
        HMM_Vec3 lclosest = closest; lclosest.Z = margin.Z;
        HMM_Vec3 lend = end; lend.Z = margax.Z;
        
        ImpDrawLine(plot, lclosest, lend, c, 1);
        
        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 z = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = plot->billboard;
            p.bl = HMM_MulV3F(closest, num_percent_offset.Z);
            p.bl.Z = HMM_Lerp(closest.Z, z, end.Z);
            str s = strf(imp, "%0.3g", HMM_Lerp(plot->plot_min.Z, z, plot->plot_max.Z));
            p = ImpAlignText(plot, p, ImpMeasureText(s), num_text_size, align_h, align_v);
            ImpDrawText3D(plot, p, s, c, num_text_size);
        }

        c = (Color){.r=color.r, .g=color.g, .b=0xdf, .a=color.a};
        ImpDrawPlane p = plot->billboard;
        p.bl = HMM_MulV3F(closest, label_percent_offset.Z);
        p.bl.Z = (camera_pos.Z < center.Z)? min.Z - plot->view_radius.Z*0.1*plot->zoom : max.Z + plot->view_radius.Z*0.1*plot->zoom ;
        align_h = IMP_TEXT_ALIGN_CENTER;
        str s = plot->zlabel.str? plot->zlabel : imp_str("Z");
        p = ImpAlignText(plot, p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(plot, p, s, c, 1);
    }
}

void imp_update_plot_controls(Context *imp, ImpPlot *plot) {
    /* TODO move update code in here and get controls from imp context */
}

int main(void)
{
    const int screenWidth = 800;
    const int screenHeight = 800;

    imp = malloc(sizeof(Context));
    imp_init(imp, 0, 0, 0);

    ImpPlot Plot = {
        .flags = IMP_PRESET_3D,
        .view_pos = {-1, -1, +1},
        .view_radius = {+1, +1, +1},
        .plot_min = {0, 0, 0},
        .plot_max = {+5, +5, +5},
        /* .flags = IMP_PRESET_2D, */
        .zoom = 1,
        .fov = 75,
        .grid_margin = 0.05,

        .xlabel = imp_str("dev_dwarf!"),
    };

    f32 max_radius = HMM_MAX(Plot.view_radius.X, HMM_MAX(Plot.view_radius.Y, Plot.view_radius.Z));
    f32 sum_radius = Plot.view_radius.X + Plot.view_radius.Y + Plot.view_radius.Z;
    f32 len_radius = HMM_LenV3(Plot.view_radius);
    HMM_Vec3 bivec_radius = {
        len_radius/(3*3*Plot.view_radius.Y*Plot.view_radius.Z),
        len_radius/(3*3*Plot.view_radius.Z*Plot.view_radius.X),
        0.5*len_radius/(3*3*Plot.view_radius.X*Plot.view_radius.Y),
    };

    f32 text_scale = 5.0;
    Plot.text_size = HMM_SqrtF(text_scale*0.000001)*len_radius;
    Plot.text_percent_offset = bivec_radius;
    Plot.view_pos = HMM_MulV3F(Plot.view_pos, HMM_SqrtF(max_radius/2));

    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(screenWidth, screenHeight, "imp");
    SetTargetFPS(60);
    /* DisableCursor(); */

    { /* Load atlas - custom loading to get transparent background from 1 bytes per pixel */
        atlas = (Texture) {
            .width = ATLAS_WIDTH,
            .height = ATLAS_HEIGHT,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
        };

        Image img =  (Image) {
            .width = ATLAS_WIDTH,
            .height = ATLAS_HEIGHT,
            .mipmaps = 1,
            .format = PIXELFORMAT_UNCOMPRESSED_GRAYSCALE,
            .data = ATLAS_DATA
        };

        Image fileimg = LoadImage("src/atlas.png");
        ImageFormat(&fileimg, PIXELFORMAT_UNCOMPRESSED_GRAYSCALE);

        ExportImage(fileimg, "src/atlas.png");
        ExportImageAsCode(fileimg, "src/atlas.h");

        // https://javagl.github.io/GLConstantsTranslator/GLConstantsTranslator.html
        const s32 GL_ONE = 1;
        const s32 GL_UNPACK_ALIGNMENT = 4;
        const s32 GL_TEXTURE_2D = 3553;
        const s32 GL_RED = 6403;
        const s32 GL_TEXTURE_SWIZZLE_RGBA = 36422;
        const s32 GL_RGBA = 6408;
        const s32 GL_UNSIGNED_BYTE = 5121;
        const s32 GL_R8 = 33321;
        const s32 GL_TEXTURE_WRAP_S = 10242;
        const s32 GL_REPEAT = 10497;
        const s32 GL_CLAMP = 10496;
        const s32 GL_TEXTURE_WRAP_T = 10243;
        const s32 GL_NEAREST = 9728;
        const s32 GL_LINEAR = 9729;
        const s32 GL_TEXTURE_MAG_FILTER = 10240;
        const s32 GL_TEXTURE_MIN_FILTER = 10241;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        u32 id;
        glGenTextures(1, &id);            
        glBindTexture(GL_TEXTURE_2D, id);
        atlas.id = id;
        u32 glInternalFormat, glFormat, glType;
        rlGetGlTextureFormats(atlas.format, &glInternalFormat, &glFormat, &glType);
        glTexImage2D(GL_TEXTURE_2D, 0, glInternalFormat, atlas.width, atlas.height, 0, glFormat, glType, img.data);
        s32 swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
        /* s32 swizzleMask[] = { GL_RED, GL_RED, GL_RED, GL_ONE }; /* <- standard raylib */
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); */
        /* glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); */
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    f32 drag_angle = 0;
    f32 drag_start_x = 0;

    f32 drag_z = 0;
    f32 drag_start_y = 0;

    s32 points = 1 << 10;
    f32 t = 0;
    HMM_Vec3 *point = malloc(sizeof(HMM_Vec3)*(1 << 17));
    HMM_Vec3 *point2 = malloc(sizeof(HMM_Vec3)*(1 << 17));
    for (s32 i = 0; i < points; i++) {
        f32 r = 6.14 * (f32) i / (f32) points;
        point[i] = (HMM_Vec3){
            .X = 10*cos(r)*sin(r*60)*((f32)i/(f32)points),
            .Y = 10*sin(r)*cos(r*60)*((f32)i/(f32)points),
            .Z = 10*sin(30*r)*cos(r*5+t)
        };
        point2[i] = (HMM_Vec3){
            .X = r*cos(r*90)*sin(r*60),
            .Y = r*sin(r*90)*cos(r*60),
            .Z = 10*sin(30*r)*cos(r*5+t)
        };
    }

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) {
            Plot.flags = (Plot.flags & IMP_CAMERA_PERSPECTIVE)? Plot.flags & ~IMP_CAMERA_PERSPECTIVE
                : Plot.flags | IMP_CAMERA_PERSPECTIVE;
        }
        
        if (~Plot.flags & IMP_CAMERA_CUSTOM) { /* TODO seperate flag for controls? */
            // TODO move to init?   
            Plot.z_sensitivity = (Plot.z_sensitivity)? Plot.z_sensitivity : 0.005;
            Plot.angle_sensitivity = (Plot.angle_sensitivity)? Plot.angle_sensitivity : 0.01;
            Plot.zoom_sensitivity = (Plot.zoom_sensitivity)? Plot.zoom_sensitivity : 0.1;

            /* 3D Controls */
            if (~Plot.flags & IMP_CAMERA_Y_UP) {
                if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                    drag_angle = Plot.angle;
                    drag_start_x = GetMouseX();
                    drag_start_y = GetMouseY();
                    drag_z = Plot.view_pos.Z;
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                    Plot.angle = drag_angle - Plot.angle_sensitivity*(drag_start_x - GetMouseX());
                    Plot.view_pos.Z = drag_z - Plot.z_sensitivity*(drag_start_y - GetMouseY());
                }
            }

            /* Panning Controls */
            f32 pan_sensitivity = 0.5;
            HMM_Vec2 mouse = HMM_V2(GetMouseX(), GetMouseY());
            HMM_Vec2 size = HMM_V2(GetScreenWidth(), GetScreenHeight());
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                Plot.mouse_drag = mouse;
                Plot.drag_min = Plot.plot_min;
                Plot.drag_max = Plot.plot_max;
            }
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                HMM_Vec2 delta = HMM_SubV2(Plot.mouse_drag, mouse);

                HMM_Vec3 plot_size = HMM_SubV3(Plot.plot_max, Plot.plot_min);
                f32 plot_scale = HMM_DotV3(plot_size, plot_size)/3;
                
                HMM_Vec3 pan = HMM_AddV3(
                    HMM_MulV3F(Plot.billboard.r, (delta.X/size.X)*plot_scale*pan_sensitivity*Plot.zoom),
                    HMM_MulV3F(Plot.billboard.u,-(delta.Y/size.Y)*plot_scale*pan_sensitivity*Plot.zoom)
                    );
                Plot.plot_min = HMM_AddV3(Plot.drag_min, pan);
                Plot.plot_max = HMM_AddV3(Plot.drag_max, pan);
            }

            /* Zoom Controls */
            Vector2 scroll = GetMouseWheelMoveV();
            Plot.zoom = HMM_Clamp(0.5, Plot.zoom - Plot.zoom_sensitivity*scroll.y, 2);
        }

        // TODO should zoom be disabled with custom camera?
        HMM_Vec3 view_pos = HMM_MulV3F(Plot.view_pos, Plot.zoom);
        if (~Plot.flags & IMP_CAMERA_CUSTOM) {
            f64 aspect = (f64)screenWidth/(f64)screenHeight;

            // TODO move to init?
            if (Plot.flags & IMP_CAMERA_Y_UP) {
                view_pos.XY = HMM_V2(0, 0);
                Plot.view_up = HMM_V3(0, 1, 0);
            } else {
                Plot.view_up = HMM_V3(0, 0, 1);
            }

            f64 s;
            s = (Plot.flags & IMP_CAMERA_Y_UP)? Plot.view_pos.Z : HMM_LenV2(Plot.view_pos.XY);
            if (Plot.flags & IMP_CAMERA_PERSPECTIVE) {
#define CULL_NEAR 0.01
#define CULL_FAR 10000
                view_pos = HMM_MulV3F(view_pos, 3);
                Plot.projection = HMM_Perspective_RH_NO(HMM_AngleDeg(Plot.fov), aspect, CULL_NEAR, CULL_FAR);
            } else {
                f64 h = s*Plot.zoom*HMM_TanF(HMM_AngleDeg(Plot.fov))/2;
                f64 w = h*aspect;
                Plot.projection = HMM_Orthographic_RH_ZO(-w, w, -h,h, CULL_NEAR, CULL_FAR); 
            }
            
            Plot.plot_rotation = HMM_Rotate_RH(Plot.angle, Plot.view_up);
            Plot.camera = HMM_LookAt_RH(view_pos, HMM_V3(0,0,0), Plot.view_up);
        }
        
        HMM_Mat4 modelview = HMM_MulM4(Plot.camera, Plot.plot_rotation);
        HMM_Mat4 modelview_inv = HMM_InvGeneralM4(modelview);

        imp_begin(imp, (Inputs){0});

        BeginDrawing();

        ClearBackground(WHITE);
        /* rlDisableBackfaceCulling(); */
        
        BeginMode3D((Camera){0});
        rlSetMatrixProjection(PCAST(Matrix, Plot.projection));
        HMM_Mat4 trans = HMM_TransposeM4(modelview);
        rlSetMatrixModelview(PCAST(Matrix, trans));

        /* Get position of camera relative to plot rotation, as well as basis dirs for billboards */
        HMM_Vec3 rot_pos = HMM_MulM4V4(HMM_TransposeM4(Plot.plot_rotation), (HMM_Vec4){ .XYZ = view_pos, .W = 0 }).XYZ;
        HMM_Vec3 billboard_r = HMM_MulM4V4(modelview_inv, HMM_V4(1,0,0,0)).XYZ;
        HMM_Vec3 billboard_u = HMM_MulM4V4(modelview_inv, HMM_V4(0,1,0,0)).XYZ;
        Plot.billboard = (ImpDrawPlane){ .bl = rot_pos, .r = billboard_r, .u = billboard_u};
        Plot.billboard_z = HMM_NormV3(HMM_Cross(billboard_u, billboard_r));
        Plot.camera_pos = rot_pos;

        Plot.line_size = 1.5;// + cos(t);
        ImpDrawPlot(&Plot, rot_pos);

        HMM_Vec3 plot_center = HMM_MulV3F(HMM_LerpV3(Plot.plot_min, 0.5, Plot.plot_max), -1);
        HMM_Vec3 plot_scale = HMM_MulV3(HMM_SubV3(Plot.plot_max, Plot.plot_min), HMM_MulV3F(Plot.view_radius, 0.5));
        rlPushMatrix();
        rlScalef(1/plot_scale.X, 1/plot_scale.Y, 1/plot_scale.Z);
        rlTranslatef(plot_center.X, plot_center.Y, plot_center.Z);
        
        Color color = RED;

        ImpDrawPlane plane = Plot.billboard;
        Rectangle rect = atlas_rect[IMP_MARKER_CIRCLE];
        plane.r = HMM_MulV3(HMM_MulV3F(plane.r, 0.50*Plot.text_size*rect.width *(Plot.zoom)), plot_scale);
        plane.u = HMM_MulV3(HMM_MulV3F(plane.u, 0.50*Plot.text_size*rect.height*(Plot.zoom)), plot_scale);

        rlBegin(RL_QUADS);
        for (s32 i = 0; i < points; i++) {
            f32 r = 6.283185307 * (f32) i / (f32) (points-1);
            point2[i] = HMM_V3(
                /* 2.5*(1+cos(r*90)*sin(r*60+t)), */
                /* 2.5*(1+sin(r*90)*cos(r*60+t)), */
                /* 2.5*(1+sin(30*r)*cos(r*5.+t)) */
                2.5+cos(0.5*t+r),
                2.5+sin(t+r)+cos(2*r+t),
                2.5+sin(t+r)*cos(2*(r+t))
                );

            /* if ((i & 7) == 0) { */
                /* point2[i] = HMM_V3( */
                /* 2.5+cos(t+r), */
                /* 2.5+sin(2*t+r)+cos(2*r+t), */
                /* 2.5+sin(t+r)*cos(2*(r+t)) */
                /* ); */
            /* } */
        }
        for (s32 i = 0; i < points-1; i++) {
            ImpDrawLine(&Plot, point2[i], point2[i+1], color, 4.0);
            
            /* Rectangle rect = atlas_rect[IMP_MARKER_CIRCLE + (i % IMP_MARKER_COUNT)]; */
            /* ImpDrawPlane p = plane; */
            /* p.bl = point2[i]; */
            /* ImpDrawTexQuadFromAtlas(p, rect, color); */
        }
        rlEnd();
        rlDisableDepthTest();


        t += GetFrameTime();
        
        rlPopMatrix();

        EndMode3D();


        DrawFPS(8, 8);
        DrawTexture(atlas, 0, 0, RED);
        
        EndDrawing();
    }
    
    CloseWindow();        
    
    return 0;
}

Rectangle atlas_rect[256] = {
    [ MU_ICON_CLOSE ] = { 88, 68, 16, 16 },
    [ MU_ICON_CHECK ] = { 0, 0, 18, 18 },
    [ MU_ICON_EXPANDED ] = { 118, 68, 7, 5 },
    [ MU_ICON_COLLAPSED ] = { 113, 68, 5, 7 },

    [ IMP_MARKER_CIRCLE ] = { 0, 128-32, 16, 16},
    [ IMP_MARKER_X ] = { 16, 128-32, 16, 16},
    [ IMP_MARKER_SQUARE ] = { 32, 128-32, 16, 16},
    [ IMP_MARKER_TRIANGLE ] = { 48, 128-32, 16, 16},
    [ IMP_MARKER_CROSS ] = { 64, 128-32, 16, 16},
    [ IMP_MARKER_DIAMOND ] = { 80, 128-32, 16, 16},
    [ IMP_MARKER_HEART ] = { 96, 128-32, 16, 16},
    [ IMP_MARKER_ARROW ] = { 112, 128-32, 16, 16},
    [ IMP_MARKER_CIRCLE_OUTLINE ] = { 0, 128-16, 16, 16},
    [ IMP_MARKER_X_OUTLINE ] = { 16, 128-16, 16, 16},
    [ IMP_MARKER_SQUARE_OUTLINE ] = { 32, 128-16, 16, 16},
    [ IMP_MARKER_TRIANGLE_OUTLINE ] = { 48, 128-16, 16, 16},
    [ IMP_MARKER_CROSS_OUTLINE ] = { 64, 128-16, 16, 16},
    [ IMP_MARKER_DIAMOND_OUTLINE ] = { 80, 128-16, 16, 16},
    [ IMP_MARKER_HEART_OUTLINE ] = { 96, 128-16, 16, 16},
    [ IMP_MARKER_ARROW_OUTLINE ] = { 112, 128-16, 16, 16},
    [ IMP_LINE_TEXTURE ] = { 1, 225, 30, 30},
        
    [ ATLAS_WHITE ] = { 125, 68, 3, 3 },
    
    [ ATLAS_FONT + 0x0020 ] = { 2, 2, 16+1.5, 32},
    [ ATLAS_FONT + 0x0021 ] = { 50, 172, 4+1.5, 32,},
    [ ATLAS_FONT + 0x0022 ] = { 225, 138, 8+1.5, 32,},
    [ ATLAS_FONT + 0x0023 ] = { 56, 2, 16+1.5, 32,},
    [ ATLAS_FONT + 0x0024 ] = { 142, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0025 ] = { 74, 2, 16+1.5, 32,},
    [ ATLAS_FONT + 0x0026 ] = { 109, 2, 15+1.5, 32,},
    [ ATLAS_FONT + 0x0027 ] = { 44, 172, 4+1.5, 32,},
    [ ATLAS_FONT + 0x0028 ] = { 235, 138, 8+1.5, 32,},
    [ ATLAS_FONT + 0x0029 ] = { 215, 138, 8+1.5, 32,},
    [ ATLAS_FONT + 0x002A ] = { 100, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x002B ] = { 158, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x002C ] = { 28, 172, 6+1.5, 32,},
    [ ATLAS_FONT + 0x002D ] = { 138, 138, 10+1.5, 32,},
    [ ATLAS_FONT + 0x002E ] = { 36, 172, 6+1.5, 32,},
    [ ATLAS_FONT + 0x002F ] = { 44, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0030 ] = { 34, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0031 ] = { 173, 138, 9+1.5, 32,},
    [ ATLAS_FONT + 0x0032 ] = { 240, 36, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0033 ] = { 58, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0034 ] = { 225, 36, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0035 ] = { 16, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0036 ] = { 30, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0037 ] = { 72, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0038 ] = { 128, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0039 ] = { 122, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x003A ] = { 20, 172, 6+1.5, 32,},
    [ ATLAS_FONT + 0x003B ] = { 11, 172, 7+1.5, 32,},
    [ ATLAS_FONT + 0x003C ] = { 223, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x003D ] = { 178, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x003E ] = { 209, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x003F ] = { 195, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0040 ] = { 114, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0041 ] = { 162, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0042 ] = { 92, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0043 ] = { 77, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0044 ] = { 107, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0045 ] = { 113, 138, 11+1.5, 32,},
    [ ATLAS_FONT + 0x0046 ] = { 167, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0047 ] = { 194, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0048 ] = { 2, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0049 ] = { 126, 138, 10+1.5, 32,},
    [ ATLAS_FONT + 0x004A ] = { 100, 138, 11+1.5, 32,},
    [ ATLAS_FONT + 0x004B ] = { 47, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x004C ] = { 156, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x004D ] = { 222, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x004E ] = { 130, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x004F ] = { 18, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0050 ] = { 62, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0051 ] = { 190, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0052 ] = { 210, 36, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0053 ] = { 146, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0054 ] = { 174, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0055 ] = { 142, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0056 ] = { 92, 2, 15+1.5, 32,},
    [ ATLAS_FONT + 0x0057 ] = { 38, 2, 16+1.5, 32,},
    [ ATLAS_FONT + 0x0058 ] = { 50, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0059 ] = { 66, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x005A ] = { 184, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x005B ] = { 2, 172, 7+1.5, 32,},
    [ ATLAS_FONT + 0x005C ] = { 170, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x005D ] = { 245, 138, 7+1.5, 32,},
    [ ATLAS_FONT + 0x005E ] = { 198, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x005F ] = { 2, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0060 ] = { 195, 138, 8+1.5, 32,},
    [ ATLAS_FONT + 0x0061 ] = { 212, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0062 ] = { 17, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0063 ] = { 240, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0064 ] = { 137, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0065 ] = { 181, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0066 ] = { 58, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0067 ] = { 32, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0068 ] = { 72, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0069 ] = { 86, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x006A ] = { 205, 138, 8+1.5, 32,},
    [ ATLAS_FONT + 0x006B ] = { 44, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x006C ] = { 30, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x006D ] = { 126, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x006E ] = { 16, 138, 12+1.5, 32,},
    [ ATLAS_FONT + 0x006F ] = { 2, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0070 ] = { 2, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0071 ] = { 152, 70, 13+1.5, 32,},
    [ ATLAS_FONT + 0x0072 ] = { 150, 138, 10+1.5, 32,},
    [ ATLAS_FONT + 0x0073 ] = { 114, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0074 ] = { 237, 70, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0075 ] = { 86, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x0076 ] = { 98, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0077 ] = { 20, 2, 16+1.5, 32,},
    [ ATLAS_FONT + 0x0078 ] = { 206, 2, 14+1.5, 32,},
    [ ATLAS_FONT + 0x0079 ] = { 82, 36, 14+1.5, 32,},
    [ ATLAS_FONT + 0x007A ] = { 226, 104, 12+1.5, 32,},
    [ ATLAS_FONT + 0x007B ] = { 184, 138, 9+1.5, 32,},
    [ ATLAS_FONT + 0x007C ] = { 56, 172, 4+1.5, 32,},
    [ ATLAS_FONT + 0x007D ] = { 162, 138, 9+1.5, 32,},
    [ ATLAS_FONT + 0x007E ] = { 238, 2, 14+1.5, 32,},
};

