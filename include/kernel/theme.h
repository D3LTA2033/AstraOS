/* ==========================================================================
 * AstraOS - Theme Engine
 * ==========================================================================
 * Centralized theming with multiple palettes, procedural wallpapers,
 * and transparency settings. All GUI components use this.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_THEME_H
#define _ASTRA_KERNEL_THEME_H

#include <kernel/types.h>
#include <drivers/framebuffer.h>

/* ---- Theme palette ---- */
typedef struct {
    const char *name;
    /* Base colors */
    color_t bg;              /* Desktop/window body background */
    color_t bg_dark;         /* Darker variant (editor bg) */
    color_t surface;         /* Panels, toolbars */
    color_t overlay;         /* Popups, overlays */
    /* Text */
    color_t text;            /* Primary text */
    color_t text_dim;        /* Secondary/muted text */
    color_t text_bright;     /* Bright/highlight text */
    /* Accents */
    color_t accent;          /* Primary accent (links, active items) */
    color_t accent2;         /* Secondary accent */
    color_t highlight;       /* Selection, hover */
    /* Syntax colors */
    color_t syn_keyword;     /* Keywords (fn, let, if) */
    color_t syn_string;      /* String literals */
    color_t syn_number;      /* Numeric literals */
    color_t syn_comment;     /* Comments */
    color_t syn_function;    /* Function names/builtins */
    color_t syn_operator;    /* Operators */
    color_t syn_bracket;     /* Brackets/delimiters */
    /* UI chrome */
    color_t border;          /* Window borders */
    color_t titlebar;        /* Title bar bg */
    color_t taskbar;         /* Taskbar bg */
    color_t red;             /* Close button, errors */
    color_t green;           /* Success, maximize */
    color_t yellow;          /* Warning, minimize */
    /* Wallpaper gradient */
    color_t wall_top;        /* Gradient top */
    color_t wall_bottom;     /* Gradient bottom */
    color_t wall_glow;       /* Center glow color */
    /* Transparency (0=opaque, 255=fully transparent) */
    uint8_t  window_alpha;   /* Window body alpha (inverted: 255=opaque) */
    uint8_t  titlebar_alpha;
    uint8_t  taskbar_alpha;
} astra_theme_t;

/* Wallpaper type */
typedef enum {
    WALLPAPER_GRADIENT,       /* Default gradient + glow */
    WALLPAPER_MESH,           /* Mesh gradient (procedural) */
    WALLPAPER_WAVES,          /* Animated wave pattern */
    WALLPAPER_GEOMETRIC,      /* Geometric shapes */
    WALLPAPER_PARTICLES,      /* Starfield/particles */
    WALLPAPER_AURORA,         /* Aurora borealis effect */
    WALLPAPER_SOLID,          /* Solid color */
    WALLPAPER_COUNT
} wallpaper_type_t;

/* Initialize theme engine */
void theme_init(void);

/* Get current theme */
const astra_theme_t *theme_current(void);

/* Set theme by index */
void theme_set(int index);

/* Get theme count */
int theme_count(void);

/* Get theme by index (for preview) */
const astra_theme_t *theme_get(int index);

/* Get theme name by index */
const char *theme_name(int index);

/* Set wallpaper type */
void theme_set_wallpaper(wallpaper_type_t type);

/* Get current wallpaper type */
wallpaper_type_t theme_get_wallpaper(void);

/* Draw the desktop wallpaper (called by WM) */
void theme_draw_wallpaper(int w, int h);

/* Get a glass-effect color (applies window_alpha to a base color) */
color_t theme_glass(color_t base);

/* Get titlebar glass color */
color_t theme_titlebar_glass(color_t base);

/* Theme-aware helpers */
color_t theme_bg(void);
color_t theme_surface(void);
color_t theme_text(void);
color_t theme_accent(void);
color_t theme_border(void);

#endif /* _ASTRA_KERNEL_THEME_H */
