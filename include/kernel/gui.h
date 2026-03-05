/* ==========================================================================
 * AstraOS - GUI Window Manager
 * ==========================================================================
 * Simple compositing window manager running in kernel mode.
 * Manages windows, taskbar, desktop, and mouse cursor.
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_GUI_H
#define _ASTRA_KERNEL_GUI_H

#include <kernel/types.h>
#include <drivers/framebuffer.h>

/* Window flags */
#define WIN_VISIBLE     0x01
#define WIN_FOCUSED     0x02
#define WIN_MOVABLE     0x04
#define WIN_CLOSABLE    0x08
#define WIN_RESIZABLE   0x10
#define WIN_TITLEBAR    0x20
#define WIN_DEFAULT     (WIN_VISIBLE | WIN_MOVABLE | WIN_CLOSABLE | WIN_TITLEBAR)

/* Maximum windows */
#define GUI_MAX_WINDOWS 16

/* Title bar height */
#define TITLEBAR_H      32
#define TASKBAR_H       36
#define BORDER_W        1

/* GUI event types */
typedef enum {
    GUI_EVENT_NONE,
    GUI_EVENT_CLOSE,
    GUI_EVENT_KEY,
    GUI_EVENT_CLICK,
    GUI_EVENT_FOCUS,
} gui_event_type_t;

typedef struct {
    gui_event_type_t type;
    int              window_id;
    int              key;       /* For KEY events */
    int              mx, my;    /* For CLICK events (relative to content) */
    int              button;    /* Mouse button for CLICK */
} gui_event_t;

/* Window render callback */
typedef void (*win_render_fn)(int win_id, int x, int y, int w, int h);
typedef void (*win_key_fn)(int win_id, int key);
typedef void (*win_click_fn)(int win_id, int mx, int my, int button);

typedef struct {
    int           id;
    char          title[64];
    int           x, y, w, h;  /* Including decorations */
    uint32_t      flags;
    win_render_fn on_render;
    win_key_fn    on_key;
    win_click_fn  on_click;
    void         *userdata;
    bool          dirty;
} gui_window_t;

/* Initialize the GUI subsystem */
void gui_init(void);

/* Main GUI event loop tick — call from kernel main loop */
void gui_tick(void);

/* Create a window. Returns window ID or -1. */
int gui_create_window(const char *title, int x, int y, int w, int h, uint32_t flags);

/* Close and destroy a window */
void gui_close_window(int id);

/* Get window struct by ID */
gui_window_t *gui_get_window(int id);

/* Set window callbacks */
void gui_set_render(int id, win_render_fn fn);
void gui_set_key_handler(int id, win_key_fn fn);
void gui_set_click_handler(int id, win_click_fn fn);
void gui_set_userdata(int id, void *data);

/* Mark window as needing redraw */
void gui_invalidate(int id);

/* Focus a window (bring to front) */
void gui_focus_window(int id);

/* Get content area position (below title bar) */
void gui_content_rect(int id, int *cx, int *cy, int *cw, int *ch);

/* Draw helpers for window content area */
void gui_draw_text(int win_id, int rx, int ry, const char *text, color_t fg);
void gui_draw_rect(int win_id, int rx, int ry, int rw, int rh, color_t color);
void gui_fill_rect(int win_id, int rx, int ry, int rw, int rh, color_t color);

/* Request full redraw */
void gui_redraw_all(void);

/* Check if GUI mode is active */
bool gui_is_active(void);

#endif /* _ASTRA_KERNEL_GUI_H */
