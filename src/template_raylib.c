/* TODO(lcf, September 26, 2023)
   - 2d camera mode (top down ortho)
   - 3d pan, zoom controls
   - render axes near 0
   - centered, left, right text

   - major and minor grids
   -- should be able to set number of grid lines
   -- should also have good defaults that dont require config

   - move configuration to plot_begin or plot_end? so that user can override stuff more easily.
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
enum { ATLAS_WIDTH = 128, ATLAS_HEIGHT = 128 };

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
    IMP_AXIS_ALL             = (IMP_AXIS_X | IMP_AXIS_Y | IMP_AXIS_Z),
    IMP_GRID_ALL             = (IMP_GRID_YZ | IMP_GRID_ZX | IMP_GRID_XY),
    IMP_PRESET_2D            = (IMP_CAMERA_Y_UP | IMP_AXIS_X | IMP_AXIS_Y | IMP_GRID_XY),
    IMP_PRESET_3D            = (IMP_AXIS_ALL | IMP_GRID_ALL),

    IMP_GRID_MARGIN          = 1 << 10,
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

    f32 text_size;
    HMM_Vec3 text_percent_offset;
    f32 grid_margin;
    
} ImpPlot;

enum {
    IMP_TEXT_ALIGN_LEFT   = 0,
    IMP_TEXT_ALIGN_CENTER = 1,
    IMP_TEXT_ALIGN_RIGHT  = 2
};
enum {
    IMP_TEXT_ALIGN_BOTTOM = 0,
    IMP_TEXT_ALIGN_MIDDLE = 1,
    IMP_TEXT_ALIGN_TOP    = 2,
};

void ImpDrawGridLines(HMM_Vec3 start, HMM_Vec3 end, HMM_Vec3 sweep, Color color, s32 n) {
    HMM_Vec3 step = HMM_DivV3F(sweep, n-1);
    for (s32 i = 0; i < n; i++) {
        DrawLine3D(PCAST(Vector3, start), PCAST(Vector3, end), color);
        start = HMM_AddV3(start, step);
        end = HMM_AddV3(end, step);
    }
}

typedef struct ImpDrawPlane ImpDrawPlane;
struct ImpDrawPlane {
    HMM_Vec3 bl;
    HMM_Vec3 r;
    HMM_Vec3 u;
};
static inline void ImpDrawVertex(HMM_Vec3 v, HMM_Vec2 t) {
    rlTexCoord2f(t.X, t.Y);
    rlVertex3f(v.X, v.Y, v.Z);
}

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

static HMM_Vec2 ImpMeasureText(str text) {
    HMM_Vec2 out = {0};
    for (s32 i = 0; i < text.len; i++) {
        Rectangle r = atlas_rect[ATLAS_FONT + text.str[i]];
        out.Y = MAX(r.height, out.Y);
        out.X += r.width;
    }
    return out;
}

static ImpDrawPlane ImpAlignText(ImpDrawPlane plane, HMM_Vec2 size, float text_size, int align_h, int align_v) {
    f32 f;
    switch (align_h) {
    case IMP_TEXT_ALIGN_LEFT  : {f = text_size;         } break;
    case IMP_TEXT_ALIGN_CENTER: {f = text_size*size.X/2;} break;
    case IMP_TEXT_ALIGN_RIGHT : {f = text_size*size.X;  } break;
    }
    plane.bl = HMM_SubV3(plane.bl, HMM_MulV3F(plane.r, f));

    switch (align_v) {
    case IMP_TEXT_ALIGN_BOTTOM: {f = text_size;         } break;
    case IMP_TEXT_ALIGN_MIDDLE: {f = text_size*size.Y/2;} break;
    case IMP_TEXT_ALIGN_TOP   : {f = text_size*size.Y;  } break;
    }
    plane.bl = HMM_SubV3(plane.bl, HMM_MulV3F(plane.u, f));

    return plane;
}

static void ImpDrawText3D(ImpDrawPlane plane, str text, Color color, f32 size) {
    plane.r = HMM_MulV3F(plane.r, size);
    plane.u = HMM_MulV3F(plane.u, size);
    for (s32 i = 0; i < text.len; i++) {
        Rectangle r = atlas_rect[ATLAS_FONT + text.str[i]];
        ImpDrawPlane p = plane;
        p.r = HMM_MulV3F(p.r, r.width);
        p.u = HMM_MulV3F(p.u, r.height);
        ImpDrawTexQuadFromAtlas(p, r, color);
        plane.bl = HMM_AddV3(plane.bl, p.r);
    }
}

void ImpDrawPlot(ImpPlot* plot, HMM_Vec3 camera_pos, ImpDrawPlane billboard) {
    Color color = {.r = 0xdd, .g = 0xdd, .b = 0xdd, .a = 0xff};
    rlColor4ub(color.r, color.g, color.b, color.a);

    f32 _margin = (plot->flags & IMP_GRID_MARGIN)? 1 + plot->grid_margin : 1;
    HMM_Vec3 margin = HMM_MulV3F(plot->view_radius, -_margin);
    HMM_Vec3 margax = HMM_MulV3F(plot->view_radius, _margin);
    HMM_Vec3 center = {0,0,0};
    HMM_Vec3 min = HMM_MulV3F(plot->view_radius, -1);
    HMM_Vec3 max = HMM_MulV3F(plot->view_radius, +1);

    color = BLACK;
#define COLOR_AXES BLACK
#define COLOR_GRID_LINE LIGHTGRAY
#define N_GRID_LINES 6

    if (plot->flags & IMP_GRID_XY) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=color.r, .g=color.g, .b=0xff, .a=color.a}; */
        
        if (camera_pos.Z > center.Z) {
            /* Bottom */
            ImpDrawGridLines(HMM_V3(min.X, margin.Y, min.Z), HMM_V3(min.X, margax.Y, min.Z), HMM_V3(max.X-min.X, 0, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(margin.X, min.Y, min.Z), HMM_V3(margax.X, min.Y, min.Z), HMM_V3(0, max.Y-min.Y, 0), c, N_GRID_LINES);
        } else {
            /* Top */
            ImpDrawGridLines(HMM_V3(max.X, margax.Y, max.Z), HMM_V3(max.X, margin.Y, max.Z), HMM_V3(min.X-max.X, 0, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(margax.X, max.Y, max.Z), HMM_V3(margin.X, max.Y, max.Z), HMM_V3(0, min.Y-max.Y, 0), c, N_GRID_LINES);
        }
    }


    if (plot->flags & IMP_GRID_ZX) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=color.r, .g=0xff, .b=color.b, .a=color.a}; */
                    
        if (camera_pos.Y > center.Y) {
            /* Right */
            ImpDrawGridLines(HMM_V3(min.X, min.Y, margin.Z), HMM_V3(min.X, min.Y, margax.Z), HMM_V3(max.X-min.X, 0, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(margin.X, min.Y, min.Z), HMM_V3(margax.X, min.Y, min.Z), HMM_V3(0, 0, max.Z-min.Z), c, N_GRID_LINES);

        } else {
            /* Left */
            ImpDrawGridLines(HMM_V3(max.X, max.Y, margax.Z), HMM_V3(max.X, max.Y, margin.Z), HMM_V3(min.X-max.X, 0, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(margax.X, max.Y, max.Z), HMM_V3(margin.X, max.Y, max.Z), HMM_V3(0, 0, min.Z-max.Z), c, N_GRID_LINES);
        }
    }

    if (plot->flags & IMP_GRID_YZ) {
        Color c = COLOR_GRID_LINE;
        /* c = (Color){.r=0xff, .g=color.g, .b=color.b, .a=color.a}; */
        
        if (camera_pos.X > center.X) {
            /* Near */
            ImpDrawGridLines(HMM_V3(min.X, min.Y, margin.Z), HMM_V3(min.X, min.Y, margax.Z), HMM_V3(0, max.Y-min.Y, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(min.X, margin.Y, min.Z), HMM_V3(min.X, margax.Y, min.Z), HMM_V3(0, 0, max.Z-min.Z), c, N_GRID_LINES);
        } else {
            /* Far */
            ImpDrawGridLines(HMM_V3(max.X, max.Y, margax.Z), HMM_V3(max.X, max.Y, margin.Z), HMM_V3(0, min.Y-max.Y, 0), c, N_GRID_LINES);
            ImpDrawGridLines(HMM_V3(max.X, margax.Y, max.Z), HMM_V3(max.X, margin.Y, max.Z), HMM_V3(0, 0, min.Z-max.Z), c, N_GRID_LINES);
        }
    }

    f32 text_size = plot->text_size;
    if (plot->flags & IMP_CAMERA_PERSPECTIVE) {
        text_size *= sqrt(plot->zoom);
    }
    f32 num_text_size = text_size*0.66;
  
    if (plot->flags & IMP_AXIS_X) {
        Color c = COLOR_AXES;
        s32 align_h = IMP_TEXT_ALIGN_CENTER;
        s32 align_v = (camera_pos.Z > center.Z)? IMP_TEXT_ALIGN_BOTTOM : IMP_TEXT_ALIGN_TOP;

        HMM_Vec3 closest = min;
        closest.Y = (camera_pos.Y > center.Y)? max.Y : min.Y;
        closest.Z = (camera_pos.Z > center.Z)? min.Z : max.Z;
        HMM_Vec3 end = closest; closest.X = margin.X; end.X = margax.X;

        DrawLine3D(PCAST(Vector3, closest), PCAST(Vector3, end), c);

        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 x = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = billboard;
            p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.X);
            p.bl.X = HMM_Lerp(closest.X, x, end.X);
            str s = strf(imp, "%.5g", HMM_Lerp(plot->plot_min.X, x, plot->plot_max.X));
            p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
            ImpDrawText3D(p, s, c, num_text_size);
        }

        ImpDrawPlane p = billboard;
        c = (Color){.r=0xdf, .g=color.g, .b=color.b, .a=color.a};
        p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.X);
        p.bl.X = HMM_Lerp(closest.X, (camera_pos.X > center.X)? 0.1 : 0.9, end.X);
         str s = imp_str("X");
        p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(p, s, c, text_size);
    }
    
    if (plot->flags & IMP_AXIS_Y) {
        Color c = COLOR_AXES;
        s32 align_h = IMP_TEXT_ALIGN_CENTER;
        s32 align_v = (camera_pos.Z > center.Z)? IMP_TEXT_ALIGN_BOTTOM : IMP_TEXT_ALIGN_TOP;
            
        HMM_Vec3 closest = min;
        closest.X = (camera_pos.X > center.X)? max.X : min.X;
        closest.Z = (camera_pos.Z > center.Z)? min.Z : max.Z;
        HMM_Vec3 end = closest; closest.Y = margin.Y; end.Y = margax.Y;

        DrawLine3D(PCAST(Vector3, closest), PCAST(Vector3, end), c);

        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 y = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = billboard;
            p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.Y);
            p.bl.Y = HMM_Lerp(closest.Y, y, end.Y);
            str s = strf(imp, "%.5g", HMM_Lerp(plot->plot_min.Y, y, plot->plot_max.Y));
            p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
            ImpDrawText3D(p, s, c, num_text_size);
        }

        c = (Color){.r=color.r, .g=0xdf, .b=color.b, .a=color.a};
        ImpDrawPlane p = billboard;
        p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.Y);
        p.bl.Y = HMM_Lerp(closest.Y, (camera_pos.Y > center.Y)? 0.1 : 0.9, end.Y);
        str s = imp_str("Y");
        p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(p, s, c, text_size);
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
        HMM_Vec3 end = closest; closest.Z = margin.Z; end.Z = margax.Z;

        DrawLine3D(PCAST(Vector3, closest), PCAST(Vector3, end), c);
        
        for (s32 i = 0; i < N_GRID_LINES; i++) {
            f32 z = (f64)i/(f64)(N_GRID_LINES-1);
            ImpDrawPlane p = billboard;
            p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.Z);
            p.bl.Z = HMM_Lerp(closest.Z, z, end.Z);
            str s = strf(imp, "%.5g", HMM_Lerp(plot->plot_min.Z, z, plot->plot_max.Z));
            p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
            ImpDrawText3D(p, s, c, num_text_size);
        }

        ImpDrawPlane p = billboard;
        p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.Z);
        c = (Color){.r=color.r, .g=color.g, .b=0xdf, .a=color.a};
        p.bl = HMM_MulV3F(closest, 1 + plot->text_percent_offset.Z);
        p.bl.Z = HMM_Lerp(closest.Z, (camera_pos.Z < center.Z)? 0.1 : 0.9, end.Z);
        str s = imp_str("Z");
        p = ImpAlignText(p, ImpMeasureText(s), plot->text_size, align_h, align_v);
        ImpDrawText3D(p, s, c, text_size);
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
        /* .flags = IMP_PRESET_3D,// | IMP_CAMERA_PERSPECTIVE, */
        /* .view_pos = {-1, -1, +1}, */
        .view_radius = {+1, +1, +1},
        .plot_min = {-5, -5, -5},
        .plot_max = {+5, +5, +5},
        .flags = IMP_PRESET_2D | IMP_GRID_MARGIN, // | IMP_CAMERA_PERSPECTIVE,
        .view_pos = {0, 0, 1},
        .zoom = 1,
        .fov = 75,
        .grid_margin = 0.05,
    };

    f32 max_radius = HMM_MAX(Plot.view_radius.X, HMM_MAX(Plot.view_radius.Y, Plot.view_radius.Z));
    f32 sum_radius = Plot.view_radius.X + Plot.view_radius.Y + Plot.view_radius.Z;
    f32 len_radius = HMM_LenV3(Plot.view_radius);
    HMM_Vec3 bivec_radius = {
        len_radius/(3*3*Plot.view_radius.Y*Plot.view_radius.Z),
        len_radius/(3*3*Plot.view_radius.Z*Plot.view_radius.X),
        0.5*len_radius/(3*3*Plot.view_radius.X*Plot.view_radius.Y),
    };
    Plot.text_size = 0.0033*len_radius;
    Plot.text_percent_offset = bivec_radius;
    Plot.view_pos = HMM_MulV3F(Plot.view_pos, HMM_SqrtF(max_radius)/3);

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
            /* TODO option, matches perspective view better */
            /* s = HMM_LenV3(Plot.view_pos); */
            /* s = HMM_SqrtF(HMM_DotV3(Plot.view_pos, HMM_MulV3(Plot.view_pos, Plot.view_radius))); */

            if (Plot.flags & IMP_CAMERA_PERSPECTIVE) {
#define CULL_NEAR 0.01
#define CULL_FAR 10000
                view_pos = HMM_MulV3F(view_pos, 3);
                Plot.projection = HMM_Perspective_RH_NO(HMM_AngleDeg(Plot.fov), aspect, CULL_NEAR, CULL_FAR);
            } else {
                f64 h = s*HMM_TanF(HMM_AngleDeg(Plot.fov));
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
        rlDisableBackfaceCulling();
        
        BeginMode3D((Camera){0});
        rlSetMatrixProjection(PCAST(Matrix, Plot.projection));
        HMM_Mat4 trans = HMM_TransposeM4(modelview);
        rlSetMatrixModelview(PCAST(Matrix, trans));

        /* Get position of camera relative to plot rotation, as well as basis dirs for billboards */
        HMM_Vec3 rot_pos = HMM_MulM4V4(HMM_TransposeM4(Plot.plot_rotation), (HMM_Vec4){ .XYZ = view_pos, .W = 0 }).XYZ;
        HMM_Vec3 billboard_r = HMM_MulM4V4(modelview_inv, HMM_V4(1,0,0,0)).XYZ;
        HMM_Vec3 billboard_u = HMM_MulM4V4(modelview_inv, HMM_V4(0,1,0,0)).XYZ;
        ImpDrawPlane billboard_plane = { .bl = HMM_V3(0,0,0), .r = billboard_r, .u = billboard_u};
        ImpDrawPlot(&Plot, rot_pos, billboard_plane);

        HMM_Vec3 plot_scale = HMM_MulV3(HMM_SubV3(Plot.plot_max, Plot.plot_min), HMM_MulV3F(Plot.view_radius, 0.5));
        rlPushMatrix();
        rlScalef(1/plot_scale.X, 1/plot_scale.Y, 1/plot_scale.Z);
        
        rlDisableDepthTest();
        /* rlSetLineWidth(0.5+3*(1+sin(t*0.4))); */
        rlSetLineWidth(2);
        Color color = RED;

        ImpDrawPlane plane = billboard_plane;
        Rectangle rect = atlas_rect[IMP_MARKER_CIRCLE];
        plane.r = HMM_MulV3(HMM_MulV3F(plane.r, 0.50*Plot.text_size*rect.width *(Plot.zoom)), plot_scale);
        plane.u = HMM_MulV3(HMM_MulV3F(plane.u, 0.50*Plot.text_size*rect.height*(Plot.zoom)), plot_scale);

        rlBegin(RL_LINES);
        for (s32 i = 0; i < points; i++) {
            f32 r = 6.14 * (f32) i / (f32) points;
            point2[i] = (HMM_Vec3){
                .X = 5*cos(r*90)*sin(r*60+t),
                .Y = 5*sin(r*90)*cos(r*60+t),
                .Z = 5*sin(30*r)*cos(r*5+t)
            };
        }
        for (s32 i = 0; i < points-1; i++) {
            rlColor4ub(color.r, color.g, color.b, color.a);
            rlVertex3f(point2[i].X, point2[i].Y, point2[i].Z);
            rlVertex3f(point2[i+1].X, point2[i+1].Y, point2[i+1].Z);
            /* Rectangle rect = atlas_rect[IMP_MARKER_CIRCLE + (i % IMP_MARKER_COUNT)]; */
            /* ImpDrawPlane p = plane; */
            /* p.bl = point2[i]; */
            /* ImpDrawTexQuadFromAtlas(p, rect, color); */
        }
        rlEnd();

        t += GetFrameTime();
        
        rlPopMatrix();
        /* rlEnableDepthTest(); */

       
        
        EndMode3D();

        /* DrawTexture(atlas, 0, 0, RED); */
        /* printf("%f, %f, %f\n", view_pos.X, view_pos.Y, view_pos.Z); */
        
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
        
    [ ATLAS_WHITE ] = { 125, 68, 3, 3 },
    [ ATLAS_FONT+32 ] = { 84, 68, 2, 17 },
    [ ATLAS_FONT+33 ] = { 39, 68, 3, 17 },
    [ ATLAS_FONT+34 ] = { 114, 51, 5, 17 },
    [ ATLAS_FONT+35 ] = { 34, 17, 7, 17 },
    [ ATLAS_FONT+36 ] = { 28, 34, 6, 17 },
    [ ATLAS_FONT+37 ] = { 58, 0, 9, 17 },
    [ ATLAS_FONT+38 ] = { 103, 0, 8, 17 },
    [ ATLAS_FONT+39 ] = { 86, 68, 2, 17 },
    [ ATLAS_FONT+40 ] = { 42, 68, 3, 17 },
    [ ATLAS_FONT+41 ] = { 45, 68, 3, 17 },
    [ ATLAS_FONT+42 ] = { 34, 34, 6, 17 },
    [ ATLAS_FONT+43 ] = { 40, 34, 6, 17 },
    [ ATLAS_FONT+44 ] = { 48, 68, 3, 17 },
    [ ATLAS_FONT+45 ] = { 51, 68, 3, 17 },
    [ ATLAS_FONT+46 ] = { 54, 68, 3, 17 },
    [ ATLAS_FONT+47 ] = { 124, 34, 4, 17 },
    [ ATLAS_FONT+48 ] = { 46, 34, 6, 17 },
    [ ATLAS_FONT+49 ] = { 52, 34, 6, 17 },
    [ ATLAS_FONT+50 ] = { 58, 34, 6, 17 },
    [ ATLAS_FONT+51 ] = { 64, 34, 6, 17 },
    [ ATLAS_FONT+52 ] = { 70, 34, 6, 17 },
    [ ATLAS_FONT+53 ] = { 76, 34, 6, 17 },
    [ ATLAS_FONT+54 ] = { 82, 34, 6, 17 },
    [ ATLAS_FONT+55 ] = { 88, 34, 6, 17 },
    [ ATLAS_FONT+56 ] = { 94, 34, 6, 17 },
    [ ATLAS_FONT+57 ] = { 100, 34, 6, 17 },
    [ ATLAS_FONT+58 ] = { 57, 68, 3, 17 },
    [ ATLAS_FONT+59 ] = { 60, 68, 3, 17 },
    [ ATLAS_FONT+60 ] = { 106, 34, 6, 17 },
    [ ATLAS_FONT+61 ] = { 112, 34, 6, 17 },
    [ ATLAS_FONT+62 ] = { 118, 34, 6, 17 },
    [ ATLAS_FONT+63 ] = { 119, 51, 5, 17 },
    [ ATLAS_FONT+64 ] = { 18, 0, 10, 17 },
    [ ATLAS_FONT+65 ] = { 41, 17, 7, 17 },
    [ ATLAS_FONT+66 ] = { 48, 17, 7, 17 },
    [ ATLAS_FONT+67 ] = { 55, 17, 7, 17 },
    [ ATLAS_FONT+68 ] = { 111, 0, 8, 17 },
    [ ATLAS_FONT+69 ] = { 0, 35, 6, 17 },
    [ ATLAS_FONT+70 ] = { 6, 35, 6, 17 },
    [ ATLAS_FONT+71 ] = { 119, 0, 8, 17 },
    [ ATLAS_FONT+72 ] = { 18, 17, 8, 17 },
    [ ATLAS_FONT+73 ] = { 63, 68, 3, 17 },
    [ ATLAS_FONT+74 ] = { 66, 68, 3, 17 },
    [ ATLAS_FONT+75 ] = { 62, 17, 7, 17 },
    [ ATLAS_FONT+76 ] = { 12, 51, 6, 17 },
    [ ATLAS_FONT+77 ] = { 28, 0, 10, 17 },
    [ ATLAS_FONT+78 ] = { 67, 0, 9, 17 },
    [ ATLAS_FONT+79 ] = { 76, 0, 9, 17 },
    [ ATLAS_FONT+80 ] = { 69, 17, 7, 17 },
    [ ATLAS_FONT+81 ] = { 85, 0, 9, 17 },
    [ ATLAS_FONT+82 ] = { 76, 17, 7, 17 },
    [ ATLAS_FONT+83 ] = { 18, 51, 6, 17 },
    [ ATLAS_FONT+84 ] = { 24, 51, 6, 17 },
    [ ATLAS_FONT+85 ] = { 26, 17, 8, 17 },
    [ ATLAS_FONT+86 ] = { 83, 17, 7, 17 },
    [ ATLAS_FONT+87 ] = { 38, 0, 10, 17 },
    [ ATLAS_FONT+88 ] = { 90, 17, 7, 17 },
    [ ATLAS_FONT+89 ] = { 30, 51, 6, 17 },
    [ ATLAS_FONT+90 ] = { 36, 51, 6, 17 },
    [ ATLAS_FONT+91 ] = { 69, 68, 3, 17 },
    [ ATLAS_FONT+92 ] = { 124, 51, 4, 17 },
    [ ATLAS_FONT+93 ] = { 72, 68, 3, 17 },
    [ ATLAS_FONT+94 ] = { 42, 51, 6, 17 },
    [ ATLAS_FONT+95 ] = { 15, 68, 4, 17 },
    [ ATLAS_FONT+96 ] = { 48, 51, 6, 17 },
    [ ATLAS_FONT+97 ] = { 54, 51, 6, 17 },
    [ ATLAS_FONT+98 ] = { 97, 17, 7, 17 },
    [ ATLAS_FONT+99 ] = { 0, 52, 5, 17 },
    [ ATLAS_FONT+100 ] = { 104, 17, 7, 17 },
    [ ATLAS_FONT+101 ] = { 60, 51, 6, 17 },
    [ ATLAS_FONT+102 ] = { 19, 68, 4, 17 },
    [ ATLAS_FONT+103 ] = { 66, 51, 6, 17 },
    [ ATLAS_FONT+104 ] = { 111, 17, 7, 17 },
    [ ATLAS_FONT+105 ] = { 75, 68, 3, 17 },
    [ ATLAS_FONT+106 ] = { 78, 68, 3, 17 },
    [ ATLAS_FONT+107 ] = { 72, 51, 6, 17 },
    [ ATLAS_FONT+108 ] = { 81, 68, 3, 17 },
    [ ATLAS_FONT+109 ] = { 48, 0, 10, 17 },
    [ ATLAS_FONT+110 ] = { 118, 17, 7, 17 },
    [ ATLAS_FONT+111 ] = { 0, 18, 7, 17 },
    [ ATLAS_FONT+112 ] = { 7, 18, 7, 17 },
    [ ATLAS_FONT+113 ] = { 14, 34, 7, 17 },
    [ ATLAS_FONT+114 ] = { 23, 68, 4, 17 },
    [ ATLAS_FONT+115 ] = { 5, 52, 5, 17 },
    [ ATLAS_FONT+116 ] = { 27, 68, 4, 17 },
    [ ATLAS_FONT+117 ] = { 21, 34, 7, 17 },
    [ ATLAS_FONT+118 ] = { 78, 51, 6, 17 },
    [ ATLAS_FONT+119 ] = { 94, 0, 9, 17 },
    [ ATLAS_FONT+120 ] = { 84, 51, 6, 17 },
    [ ATLAS_FONT+121 ] = { 90, 51, 6, 17 },
    [ ATLAS_FONT+122 ] = { 10, 68, 5, 17 },
    [ ATLAS_FONT+123 ] = { 31, 68, 4, 17 },
    [ ATLAS_FONT+124 ] = { 96, 51, 6, 17 },
    [ ATLAS_FONT+125 ] = { 35, 68, 4, 17 },
    [ ATLAS_FONT+126 ] = { 102, 51, 6, 17 },
    [ ATLAS_FONT+127 ] = { 108, 51, 6, 17 },
};

