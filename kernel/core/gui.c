/* ==========================================================================
 * AstraOS - GUI Window Manager Implementation
 * ==========================================================================
 * Compositing WM with Catppuccin-inspired dark theme. Draws desktop,
 * taskbar, windows with title bars, and an animated mouse cursor.
 * ========================================================================== */

#include <kernel/gui.h>
#include <drivers/framebuffer.h>
#include <drivers/mouse.h>
#include <drivers/keyboard.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <drivers/rtc.h>

static gui_window_t windows[GUI_MAX_WINDOWS];
static int focus_order[GUI_MAX_WINDOWS]; /* Z-order, [0] = topmost */
static int focus_count = 0;
static bool gui_active = false;
static bool needs_redraw = true;

/* Dragging state */
static bool dragging = false;
static int  drag_win = -1;
static int  drag_ox, drag_oy;

static int next_win_id = 1;

/* --- Internal helpers --- */

static gui_window_t *find_window(int id)
{
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        if (windows[i].id == id && (windows[i].flags & WIN_VISIBLE))
            return &windows[i];
    return NULL;
}

static void draw_cursor(int x, int y)
{
    /* Simple arrow cursor */
    static const uint8_t cursor[12][8] = {
        {1,0,0,0,0,0,0,0},
        {1,1,0,0,0,0,0,0},
        {1,2,1,0,0,0,0,0},
        {1,2,2,1,0,0,0,0},
        {1,2,2,2,1,0,0,0},
        {1,2,2,2,2,1,0,0},
        {1,2,2,2,2,2,1,0},
        {1,2,2,2,1,1,0,0},
        {1,2,1,2,2,1,0,0},
        {1,1,0,1,2,1,0,0},
        {0,0,0,0,1,2,1,0},
        {0,0,0,0,1,1,0,0},
    };
    for (int r = 0; r < 12; r++)
        for (int c = 0; c < 8; c++) {
            if (cursor[r][c] == 1)
                fb_putpixel(x + c, y + r, COL_BLACK);
            else if (cursor[r][c] == 2)
                fb_putpixel(x + c, y + r, COL_WHITE);
        }
}

static void draw_taskbar(void)
{
    const fb_info_t *fb = fb_get_info();
    int ty = (int)fb->height - TASKBAR_H;

    /* Taskbar background */
    fb_fill_rect(0, ty, (int)fb->width, TASKBAR_H, COL_BG_MED);
    fb_hline(0, ty, (int)fb->width, COL_BORDER);

    /* "AstraOS" logo button */
    fb_fill_rect(4, ty + 4, 80, TASKBAR_H - 8, COL_ACCENT);
    fb_puts(12, ty + 10, "AstraOS", COL_BG_DARK, COL_ACCENT);

    /* Window buttons on taskbar */
    int bx = 92;
    for (int i = 0; i < focus_count && bx < (int)fb->width - 120; i++) {
        gui_window_t *w = find_window(focus_order[i]);
        if (!w) continue;

        color_t bg = (w->flags & WIN_FOCUSED) ? COL_SURFACE : COL_BG_DARK;
        color_t fg = (w->flags & WIN_FOCUSED) ? COL_ACCENT : COL_TEXT_DIM;
        int bw = fb_text_width(w->title) + 16;
        if (bw < 60) bw = 60;
        if (bw > 160) bw = 160;

        fb_fill_rect(bx, ty + 4, bw, TASKBAR_H - 8, bg);
        fb_puts(bx + 8, ty + 10, w->title, fg, bg);
        bx += bw + 4;
    }

    /* Clock on right */
    rtc_time_t t;
    rtc_read(&t);
    char clock[9];
    clock[0] = '0' + (char)(t.hour / 10);
    clock[1] = '0' + (char)(t.hour % 10);
    clock[2] = ':';
    clock[3] = '0' + (char)(t.minute / 10);
    clock[4] = '0' + (char)(t.minute % 10);
    clock[5] = ':';
    clock[6] = '0' + (char)(t.second / 10);
    clock[7] = '0' + (char)(t.second % 10);
    clock[8] = '\0';
    fb_puts((int)fb->width - 80, ty + 10, clock, COL_TEXT, COL_BG_MED);
}

static void draw_desktop(void)
{
    const fb_info_t *info = fb_get_info();

    /* Gradient-ish dark background */
    fb_clear(COL_BG_DARK);

    /* Subtle branding */
    const char *brand = "AstraOS 1.0";
    int bw = fb_text_width(brand);
    fb_puts_transparent((int)(info->width - (uint32_t)bw) / 2,
                       (int)(info->height - TASKBAR_H) / 2 - 40,
                       brand, COL_BG_LIGHT);

    const char *sub = "Programming Environment";
    int sw = fb_text_width(sub);
    fb_puts_transparent((int)(info->width - (uint32_t)sw) / 2,
                       (int)(info->height - TASKBAR_H) / 2 - 16,
                       sub, COL_BG_LIGHT);
}

static void draw_window(gui_window_t *w)
{
    bool focused = (w->flags & WIN_FOCUSED) != 0;
    color_t border = focused ? COL_ACCENT : COL_BORDER;

    /* Window shadow */
    fb_fill_rect(w->x + 3, w->y + 3, w->w, w->h, RGB(10,10,20));

    /* Window body */
    fb_fill_rect(w->x, w->y, w->w, w->h, COL_BG_DARK);
    fb_rect(w->x, w->y, w->w, w->h, border);

    if (w->flags & WIN_TITLEBAR) {
        /* Title bar */
        color_t tb_bg = focused ? COL_SURFACE : COL_BG_MED;
        fb_fill_rect(w->x + 1, w->y + 1, w->w - 2, TITLEBAR_H - 1, tb_bg);
        fb_hline(w->x, w->y + TITLEBAR_H, w->w, border);

        /* Title text */
        fb_puts(w->x + 10, w->y + 6, w->title,
                focused ? COL_TEXT : COL_TEXT_DIM, tb_bg);

        /* Close button */
        if (w->flags & WIN_CLOSABLE) {
            int cbx = w->x + w->w - 24;
            int cby = w->y + 4;
            fb_fill_rect(cbx, cby, 20, 20, COL_RED);
            fb_puts(cbx + 6, cby + 2, "x", COL_WHITE, COL_RED);
        }
    }

    /* Render content */
    if (w->on_render) {
        int cx, cy, cw, ch;
        cx = w->x + BORDER_W;
        cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
        cw = w->w - 2 * BORDER_W;
        ch = w->h - (w->flags & WIN_TITLEBAR ? TITLEBAR_H + BORDER_W + 1 : 2 * BORDER_W);
        w->on_render(w->id, cx, cy, cw, ch);
    }
}

static int window_at(int x, int y)
{
    /* Check in z-order (front to back) */
    for (int i = 0; i < focus_count; i++) {
        gui_window_t *w = find_window(focus_order[i]);
        if (!w) continue;
        if (x >= w->x && x < w->x + w->w &&
            y >= w->y && y < w->y + w->h)
            return w->id;
    }
    return -1;
}

static void set_focus(int id)
{
    /* Remove from current position in focus_order */
    int idx = -1;
    for (int i = 0; i < focus_count; i++) {
        if (focus_order[i] == id) { idx = i; break; }
    }
    if (idx < 0) return;

    /* Clear old focus */
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        windows[i].flags &= ~WIN_FOCUSED;

    /* Move to front */
    for (int i = idx; i > 0; i--)
        focus_order[i] = focus_order[i - 1];
    focus_order[0] = id;

    gui_window_t *w = find_window(id);
    if (w) w->flags |= WIN_FOCUSED;
    needs_redraw = true;
}

static void handle_mouse(void)
{
    mouse_state_t ms = mouse_get_state();

    if (ms.moved || ms.clicked || ms.released)
        needs_redraw = true;

    if (ms.clicked && (ms.click_btn & MOUSE_LEFT)) {
        int wid = window_at(ms.x, ms.y);
        if (wid >= 0) {
            gui_window_t *w = find_window(wid);
            if (w) {
                /* Check close button */
                if (w->flags & WIN_CLOSABLE) {
                    int cbx = w->x + w->w - 24;
                    int cby = w->y + 4;
                    if (ms.x >= cbx && ms.x < cbx + 20 &&
                        ms.y >= cby && ms.y < cby + 20) {
                        gui_close_window(wid);
                        return;
                    }
                }

                set_focus(wid);

                /* Check if click is in title bar → start drag */
                if ((w->flags & WIN_MOVABLE) && (w->flags & WIN_TITLEBAR) &&
                    ms.y >= w->y && ms.y < w->y + TITLEBAR_H) {
                    dragging = true;
                    drag_win = wid;
                    drag_ox = ms.x - w->x;
                    drag_oy = ms.y - w->y;
                } else {
                    /* Content click */
                    if (w->on_click) {
                        int cx = w->x + BORDER_W;
                        int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
                        w->on_click(wid, ms.x - cx, ms.y - cy, ms.click_btn);
                    }
                }
            }
        }
    }

    if (ms.released && (ms.release_btn & MOUSE_LEFT)) {
        dragging = false;
        drag_win = -1;
    }

    if (dragging && ms.moved) {
        gui_window_t *w = find_window(drag_win);
        if (w) {
            w->x = ms.x - drag_ox;
            w->y = ms.y - drag_oy;
            needs_redraw = true;
        }
    }
}

static void handle_keyboard(void)
{
    int c = keyboard_getchar();
    if (c < 0) return;

    /* Send to focused window */
    if (focus_count > 0) {
        gui_window_t *w = find_window(focus_order[0]);
        if (w && w->on_key) {
            w->on_key(w->id, c);
            needs_redraw = true;
        }
    }
}

/* --- Public API --- */

void gui_init(void)
{
    kmemset(windows, 0, sizeof(windows));
    focus_count = 0;
    gui_active = true;
    needs_redraw = true;
}

void gui_tick(void)
{
    if (!gui_active) return;

    handle_mouse();
    handle_keyboard();

    if (needs_redraw) {
        needs_redraw = false;
        draw_desktop();

        /* Draw windows back to front */
        for (int i = focus_count - 1; i >= 0; i--) {
            gui_window_t *w = find_window(focus_order[i]);
            if (w) draw_window(w);
        }

        draw_taskbar();

        mouse_state_t ms = mouse_peek_state();
        draw_cursor(ms.x, ms.y);

        fb_swap();
    }
}

int gui_create_window(const char *title, int x, int y, int w, int h, uint32_t flags)
{
    int slot = -1;
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (windows[i].id == 0) { slot = i; break; }
    }
    if (slot < 0) return -1;

    gui_window_t *win = &windows[slot];
    win->id = next_win_id++;
    kstrncpy(win->title, title, 63);
    win->title[63] = '\0';
    win->x = x;
    win->y = y;
    win->w = w;
    win->h = h;
    win->flags = flags;
    win->on_render = NULL;
    win->on_key = NULL;
    win->on_click = NULL;
    win->userdata = NULL;
    win->dirty = true;

    /* Add to focus order (front) */
    if (focus_count < GUI_MAX_WINDOWS) {
        for (int i = focus_count; i > 0; i--)
            focus_order[i] = focus_order[i - 1];
        focus_order[0] = win->id;
        focus_count++;
    }

    /* Set focus */
    for (int i = 0; i < GUI_MAX_WINDOWS; i++)
        windows[i].flags &= ~WIN_FOCUSED;
    win->flags |= WIN_FOCUSED;

    needs_redraw = true;
    return win->id;
}

void gui_close_window(int id)
{
    for (int i = 0; i < GUI_MAX_WINDOWS; i++) {
        if (windows[i].id == id) {
            windows[i].id = 0;
            windows[i].flags = 0;
            break;
        }
    }

    /* Remove from focus order */
    int idx = -1;
    for (int i = 0; i < focus_count; i++) {
        if (focus_order[i] == id) { idx = i; break; }
    }
    if (idx >= 0) {
        for (int i = idx; i < focus_count - 1; i++)
            focus_order[i] = focus_order[i + 1];
        focus_count--;
    }

    /* Focus next window if any */
    if (focus_count > 0) {
        gui_window_t *w = find_window(focus_order[0]);
        if (w) w->flags |= WIN_FOCUSED;
    }

    needs_redraw = true;
}

gui_window_t *gui_get_window(int id)
{
    return find_window(id);
}

void gui_set_render(int id, win_render_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_render = fn;
}

void gui_set_key_handler(int id, win_key_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_key = fn;
}

void gui_set_click_handler(int id, win_click_fn fn)
{
    gui_window_t *w = find_window(id);
    if (w) w->on_click = fn;
}

void gui_set_userdata(int id, void *data)
{
    gui_window_t *w = find_window(id);
    if (w) w->userdata = data;
}

void gui_invalidate(int id)
{
    gui_window_t *w = find_window(id);
    if (w) w->dirty = true;
    needs_redraw = true;
}

void gui_focus_window(int id)
{
    set_focus(id);
}

void gui_content_rect(int id, int *cx, int *cy, int *cw, int *ch)
{
    gui_window_t *w = find_window(id);
    if (!w) return;
    *cx = w->x + BORDER_W;
    *cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W);
    *cw = w->w - 2 * BORDER_W;
    *ch = w->h - (w->flags & WIN_TITLEBAR ? TITLEBAR_H + BORDER_W + 1 : 2 * BORDER_W);
}

void gui_draw_text(int win_id, int rx, int ry, const char *text, color_t fg)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_puts_transparent(cx, cy, text, fg);
}

void gui_draw_rect(int win_id, int rx, int ry, int rw, int rh, color_t color)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_rect(cx, cy, rw, rh, color);
}

void gui_fill_rect(int win_id, int rx, int ry, int rw, int rh, color_t color)
{
    gui_window_t *w = find_window(win_id);
    if (!w) return;
    int cx = w->x + BORDER_W + rx;
    int cy = w->y + (w->flags & WIN_TITLEBAR ? TITLEBAR_H + 1 : BORDER_W) + ry;
    fb_fill_rect(cx, cy, rw, rh, color);
}

void gui_redraw_all(void)
{
    needs_redraw = true;
}

bool gui_is_active(void)
{
    return gui_active;
}
