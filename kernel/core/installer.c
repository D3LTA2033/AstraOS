/* ==========================================================================
 * AstraOS - First-Boot Installation Wizard
 * ==========================================================================
 * Arch Linux-inspired graphical setup wizard. Runs before the desktop
 * launches on first boot. Collects hostname, username, and theme.
 * ========================================================================== */

#include <kernel/installer.h>
#include <drivers/framebuffer.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <kernel/kstring.h>
#include <drivers/pit.h>

static system_config_t config;

/* Theme palettes */
typedef struct {
    const char *name;
    color_t bg;
    color_t surface;
    color_t accent;
    color_t text;
    color_t dim;
    color_t highlight;
} theme_t;

static const theme_t themes[] = {
    { "Catppuccin Mocha", RGB(30,30,46),  RGB(49,50,68),  RGB(137,180,250), RGB(205,214,244), RGB(147,153,178), RGB(203,166,247) },
    { "Nord",            RGB(46,52,64),  RGB(59,66,82),  RGB(136,192,208), RGB(216,222,233), RGB(76,86,106),   RGB(163,190,140) },
    { "Dracula",         RGB(40,42,54),  RGB(68,71,90),  RGB(139,233,253), RGB(248,248,242), RGB(98,114,164),  RGB(255,121,198) },
    { "Gruvbox",         RGB(40,40,40),  RGB(60,56,54),  RGB(250,189,47),  RGB(235,219,178), RGB(146,131,116), RGB(184,187,38)  },
};
#define THEME_COUNT 4

/* Current step */
enum { STEP_WELCOME, STEP_HOSTNAME, STEP_USERNAME, STEP_THEME, STEP_SUMMARY, STEP_INSTALL, STEP_DONE };
static int step = STEP_WELCOME;
static int selected_theme = 0;

/* Input fields */
static char hostname_buf[32];
static int  hostname_len = 0;
static char username_buf[32];
static int  username_len = 0;

/* Animation state */
static int install_progress = 0;
static const char *install_steps[] = {
    "Configuring system hostname...",
    "Creating user account...",
    "Setting up desktop environment...",
    "Applying theme preferences...",
    "Initializing Astracor runtime...",
    "Building development workspace...",
    "Finalizing configuration...",
};
#define INSTALL_STEPS 7

/* --- Drawing helpers --- */

static void draw_bg(const theme_t *th)
{
    fb_clear(th->bg);
}

static void draw_centered(int y, const char *text, color_t fg, color_t bg)
{
    const fb_info_t *fb = fb_get_info();
    int w = fb_text_width(text);
    fb_puts(((int)fb->width - w) / 2, y, text, fg, bg);
}

static void draw_box(int x, int y, int w, int h, color_t bg, color_t border)
{
    fb_fill_rect(x, y, w, h, bg);
    fb_rect(x, y, w, h, border);
}

static void draw_input_field(int x, int y, int w, const char *label,
                             const char *value, bool active, const theme_t *th)
{
    fb_puts(x, y, label, th->dim, th->bg);
    draw_box(x, y + 20, w, 32, active ? th->surface : RGB(35,35,50), th->accent);
    fb_puts(x + 8, y + 26, value, th->text, active ? th->surface : RGB(35,35,50));
    if (active) {
        int cx = x + 8 + (int)kstrlen(value) * FONT_W;
        fb_fill_rect(cx, y + 24, 2, 20, th->accent);
    }
}

static void draw_button(int x, int y, int w, int h, const char *label,
                        bool selected, const theme_t *th)
{
    color_t bg = selected ? th->accent : th->surface;
    color_t fg = selected ? th->bg : th->text;
    fb_fill_rect(x, y, w, h, bg);
    fb_rect(x, y, w, h, selected ? th->highlight : th->dim);
    int tw = fb_text_width(label);
    fb_puts(x + (w - tw) / 2, y + (h - FONT_H) / 2, label, fg, bg);
}

static void draw_progress_bar(int x, int y, int w, int h, int pct, const theme_t *th)
{
    draw_box(x, y, w, h, th->surface, th->dim);
    int fill_w = (w - 4) * pct / 100;
    if (fill_w > 0)
        fb_fill_rect(x + 2, y + 2, fill_w, h - 4, th->accent);
}

/* Step indicator dots */
static void draw_step_indicator(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    int total = 5; /* Welcome, Hostname, Username, Theme, Summary */
    int dot_w = 12;
    int gap = 8;
    int total_w = total * dot_w + (total - 1) * gap;
    int sx = ((int)fb->width - total_w) / 2;
    int sy = (int)fb->height - 50;

    for (int i = 0; i < total; i++) {
        color_t c;
        if (i < step) c = th->accent;
        else if (i == step) c = th->highlight;
        else c = th->dim;
        fb_fill_rect(sx + i * (dot_w + gap), sy, dot_w, 6, c);
    }
}

/* --- ASCII art logo --- */

static void draw_logo(int y, const theme_t *th)
{
    const char *logo[] = {
        "     _        _              ___  ____  ",
        "    / \\   ___| |_ _ __ __ _ / _ \\/ ___| ",
        "   / _ \\ / __| __| '__/ _` | | | \\___ \\ ",
        "  / ___ \\\\__ \\ |_| | | (_| | |_| |___) |",
        " /_/   \\_\\___/\\__|_|  \\__,_|\\___/|____/ ",
    };
    for (int i = 0; i < 5; i++)
        draw_centered(y + i * FONT_H, logo[i], th->accent, th->bg);
}

/* --- Step renderers --- */

static void render_welcome(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    draw_bg(th);

    draw_logo(120, th);

    draw_centered(240, "Welcome to AstraOS", th->text, th->bg);
    draw_centered(270, "A Programming & Testing Environment", th->dim, th->bg);

    draw_centered(340, "This wizard will help you configure your system.", th->text, th->bg);
    draw_centered(365, "It only takes a moment.", th->dim, th->bg);

    int bw = 200, bh = 40;
    int bx = ((int)fb->width - bw) / 2;
    draw_button(bx, 440, bw, bh, "Get Started  ->", true, th);

    draw_centered(530, "v2.0.0  |  AstraCore Kernel  |  Astracor Language", th->dim, th->bg);
    draw_step_indicator(th);
}

static void render_hostname(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    draw_bg(th);

    draw_centered(100, "System Hostname", th->accent, th->bg);
    draw_centered(130, "Choose a name for your machine on the network.", th->dim, th->bg);

    int fw = 400;
    int fx = ((int)fb->width - fw) / 2;
    draw_input_field(fx, 200, fw, "Hostname:", hostname_buf, true, th);

    /* Suggestion */
    fb_puts(fx, 270, "Suggestion: astra, devbox, workstation", th->dim, th->bg);

    /* Navigation buttons */
    int bw = 140, bh = 36;
    draw_button(fx, 350, bw, bh, "<-  Back", false, th);
    draw_button(fx + fw - bw, 350, bw, bh, "Next  ->", true, th);

    draw_step_indicator(th);
}

static void render_username(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    draw_bg(th);

    draw_centered(100, "User Account", th->accent, th->bg);
    draw_centered(130, "Create your user account for the system.", th->dim, th->bg);

    int fw = 400;
    int fx = ((int)fb->width - fw) / 2;
    draw_input_field(fx, 200, fw, "Username:", username_buf, true, th);

    fb_puts(fx, 270, "This will be your login name.", th->dim, th->bg);

    int bw = 140, bh = 36;
    draw_button(fx, 350, bw, bh, "<-  Back", false, th);
    draw_button(fx + fw - bw, 350, bw, bh, "Next  ->", true, th);

    draw_step_indicator(th);
}

static void render_theme(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    draw_bg(th);

    draw_centered(80, "Desktop Theme", th->accent, th->bg);
    draw_centered(110, "Choose your preferred color scheme.", th->dim, th->bg);

    int card_w = 200, card_h = 140;
    int gap = 20;
    int total_w = THEME_COUNT * card_w + (THEME_COUNT - 1) * gap;
    int sx = ((int)fb->width - total_w) / 2;
    int sy = 170;

    for (int i = 0; i < THEME_COUNT; i++) {
        int cx = sx + i * (card_w + gap);
        bool sel = (i == selected_theme);

        /* Card */
        color_t border = sel ? themes[i].accent : themes[i].dim;
        draw_box(cx, sy, card_w, card_h, themes[i].bg, border);
        if (sel) {
            /* Selection indicator */
            fb_fill_rect(cx, sy, card_w, 4, themes[i].accent);
        }

        /* Theme name */
        int nw = fb_text_width(themes[i].name);
        fb_puts(cx + (card_w - nw) / 2, sy + 10, themes[i].name,
                themes[i].text, themes[i].bg);

        /* Color swatches */
        int sw_y = sy + 40;
        int sw_size = 24;
        int sw_gap = 6;
        int sw_total = 5 * sw_size + 4 * sw_gap;
        int sw_x = cx + (card_w - sw_total) / 2;

        fb_fill_rect(sw_x, sw_y, sw_size, sw_size, themes[i].accent);
        fb_fill_rect(sw_x + sw_size + sw_gap, sw_y, sw_size, sw_size, themes[i].highlight);
        fb_fill_rect(sw_x + 2*(sw_size + sw_gap), sw_y, sw_size, sw_size, themes[i].surface);
        fb_fill_rect(sw_x + 3*(sw_size + sw_gap), sw_y, sw_size, sw_size, themes[i].text);
        fb_fill_rect(sw_x + 4*(sw_size + sw_gap), sw_y, sw_size, sw_size, themes[i].dim);

        /* Preview bar */
        fb_fill_rect(cx + 10, sy + 80, card_w - 20, 30, themes[i].surface);
        fb_puts(cx + 16, sy + 87, "let x = 42", themes[i].accent, themes[i].surface);

        /* Selection radio */
        if (sel) {
            fb_fill_rect(cx + card_w/2 - 8, sy + card_h - 22, 16, 16, themes[i].accent);
            fb_puts(cx + card_w/2 - 4, sy + card_h - 22, "*", themes[i].bg, themes[i].accent);
        } else {
            fb_rect(cx + card_w/2 - 8, sy + card_h - 22, 16, 16, themes[i].dim);
        }
    }

    /* Navigation */
    int fw = 400;
    int fx = ((int)fb->width - fw) / 2;
    int bw = 140, bh = 36;
    draw_button(fx, 380, bw, bh, "<-  Back", false, th);
    draw_button(fx + fw - bw, 380, bw, bh, "Next  ->", true, th);

    /* Keyboard hint */
    draw_centered(440, "Use <- -> arrow keys or click to select theme", th->dim, th->bg);

    draw_step_indicator(th);
}

static void render_summary(const theme_t *th)
{
    const fb_info_t *fb = fb_get_info();
    draw_bg(th);

    draw_centered(100, "Configuration Summary", th->accent, th->bg);
    draw_centered(130, "Review your settings before installation.", th->dim, th->bg);

    int bx_w = 460, bx_h = 200;
    int bx_x = ((int)fb->width - bx_w) / 2;
    int bx_y = 180;
    draw_box(bx_x, bx_y, bx_w, bx_h, th->surface, th->dim);

    int lx = bx_x + 30;
    int ly = bx_y + 20;

    fb_puts(lx, ly, "Hostname:", th->dim, th->surface);
    fb_puts(lx + 120, ly, hostname_buf[0] ? hostname_buf : "astra", th->text, th->surface);
    ly += 30;

    fb_puts(lx, ly, "Username:", th->dim, th->surface);
    fb_puts(lx + 120, ly, username_buf[0] ? username_buf : "user", th->text, th->surface);
    ly += 30;

    fb_puts(lx, ly, "Theme:", th->dim, th->surface);
    fb_puts(lx + 120, ly, themes[selected_theme].name, themes[selected_theme].accent, th->surface);
    ly += 30;

    fb_puts(lx, ly, "Language:", th->dim, th->surface);
    fb_puts(lx + 120, ly, "Astracor v1.0", th->text, th->surface);
    ly += 30;

    fb_puts(lx, ly, "Desktop:", th->dim, th->surface);
    fb_puts(lx + 120, ly, "AstraOS Desktop Environment", th->text, th->surface);

    /* Buttons */
    int fw = 460;
    int fx = ((int)fb->width - fw) / 2;
    int bw = 140, bh = 40;
    draw_button(fx, 420, bw, bh, "<-  Back", false, th);
    draw_button(fx + fw - bw, 420, bw, bh, "Install", true, th);

    draw_step_indicator(th);
}

static void render_install(const theme_t *th)
{
    draw_bg(th);

    draw_logo(100, th);

    draw_centered(210, "Installing AstraOS...", th->text, th->bg);

    const fb_info_t *fb = fb_get_info();
    int pw = 500;
    int px = ((int)fb->width - pw) / 2;
    draw_progress_bar(px, 260, pw, 24, install_progress, th);

    /* Percentage text */
    char pct[8];
    int pi = 0;
    int v = install_progress;
    if (v == 0) pct[pi++] = '0';
    else { char tmp[4]; int ti=0; while(v>0){tmp[ti++]='0'+(char)(v%10);v/=10;} for(int j=ti-1;j>=0;j--)pct[pi++]=tmp[j]; }
    pct[pi++] = '%';
    pct[pi] = '\0';
    draw_centered(295, pct, th->dim, th->bg);

    /* Current step text */
    int step_idx = install_progress * INSTALL_STEPS / 101;
    if (step_idx >= INSTALL_STEPS) step_idx = INSTALL_STEPS - 1;
    draw_centered(330, install_steps[step_idx], th->dim, th->bg);
}

static void render_done(const theme_t *th)
{
    draw_bg(th);

    draw_logo(120, th);

    draw_centered(240, "Setup Complete!", th->accent, th->bg);

    char msg[128];
    int mi = 0;
    const char *p = "Welcome, ";
    while (*p) msg[mi++] = *p++;
    p = username_buf[0] ? username_buf : "user";
    while (*p) msg[mi++] = *p++;
    msg[mi++] = '!';
    msg[mi] = '\0';
    draw_centered(275, msg, th->text, th->bg);

    draw_centered(320, "Your development environment is ready.", th->dim, th->bg);
    draw_centered(345, "The desktop will load momentarily...", th->dim, th->bg);

    const fb_info_t *fb = fb_get_info();
    int bw = 220, bh = 40;
    draw_button(((int)fb->width - bw) / 2, 410, bw, bh, "Launch Desktop  ->", true, th);
}

static void render_step(void)
{
    const theme_t *th = &themes[selected_theme];

    switch (step) {
    case STEP_WELCOME:  render_welcome(th);  break;
    case STEP_HOSTNAME: render_hostname(th); break;
    case STEP_USERNAME: render_username(th); break;
    case STEP_THEME:    render_theme(th);    break;
    case STEP_SUMMARY:  render_summary(th);  break;
    case STEP_INSTALL:  render_install(th);  break;
    case STEP_DONE:     render_done(th);     break;
    }

    fb_swap();
}

/* --- Input handling --- */

static void handle_text_input(char *buf, int *len, int max, int key)
{
    if (key == '\b') {
        if (*len > 0) {
            (*len)--;
            buf[*len] = '\0';
        }
    } else if (key >= 32 && key < 127 && *len < max - 1) {
        /* Only allow valid hostname/username chars */
        bool valid = false;
        if (key >= 'a' && key <= 'z') valid = true;
        if (key >= 'A' && key <= 'Z') valid = true;
        if (key >= '0' && key <= '9') valid = true;
        if (key == '-' || key == '_' || key == '.') valid = true;
        if (valid) {
            buf[*len] = (char)key;
            (*len)++;
            buf[*len] = '\0';
        }
    }
}

static void next_step(void)
{
    if (step < STEP_DONE) step++;
}

static void prev_step(void)
{
    if (step > STEP_WELCOME) step--;
}

/* --- Main installer loop --- */

void installer_run(void)
{
    kmemset(&config, 0, sizeof(config));
    kmemset(hostname_buf, 0, sizeof(hostname_buf));
    kmemset(username_buf, 0, sizeof(username_buf));
    hostname_len = 0;
    username_len = 0;
    selected_theme = 0;
    step = STEP_WELCOME;

    bool running = true;
    bool need_render = true;

    while (running) {
        if (need_render) {
            render_step();
            need_render = false;
        }

        /* Handle keyboard */
        int key = keyboard_getchar();
        if (key >= 0) {
            need_render = true;

            switch (step) {
            case STEP_WELCOME:
                if (key == '\n') next_step();
                break;

            case STEP_HOSTNAME:
                if (key == '\n') next_step();
                else if (key == 27) prev_step(); /* Escape = back */
                else handle_text_input(hostname_buf, &hostname_len, 32, key);
                break;

            case STEP_USERNAME:
                if (key == '\n') next_step();
                else if (key == 27) prev_step();
                else handle_text_input(username_buf, &username_len, 32, key);
                break;

            case STEP_THEME:
                if (key == '\n') next_step();
                else if (key == 27) prev_step();
                /* Arrow keys come as escape sequences from keyboard driver.
                 * We'll also accept [ and ] as left/right for simplicity */
                else if (key == '[' || key == ',') {
                    if (selected_theme > 0) selected_theme--;
                }
                else if (key == ']' || key == '.') {
                    if (selected_theme < THEME_COUNT - 1) selected_theme++;
                }
                break;

            case STEP_SUMMARY:
                if (key == '\n') {
                    step = STEP_INSTALL;
                    need_render = true;

                    /* Run installation animation */
                    for (install_progress = 0; install_progress <= 100; install_progress += 2) {
                        render_step();
                        /* Delay ~50ms per tick */
                        uint32_t start = pit_get_ticks();
                        while (pit_get_ticks() - start < 5)
                            __asm__ volatile ("hlt");
                    }
                    step = STEP_DONE;
                }
                else if (key == 27) prev_step();
                break;

            case STEP_DONE:
                if (key == '\n') running = false;
                break;
            }
        }

        /* Handle mouse clicks */
        mouse_state_t ms = mouse_get_state();
        if (ms.clicked && (ms.click_btn & MOUSE_LEFT)) {
            const fb_info_t *fb = fb_get_info();
            need_render = true;

            switch (step) {
            case STEP_WELCOME: {
                /* Check "Get Started" button */
                int bw = 200, bh = 40;
                int bx = ((int)fb->width - bw) / 2;
                if (ms.x >= bx && ms.x < bx + bw && ms.y >= 440 && ms.y < 440 + bh)
                    next_step();
                break;
            }
            case STEP_HOSTNAME:
            case STEP_USERNAME: {
                int fw = 400;
                int fx = ((int)fb->width - fw) / 2;
                int bw_btn = 140, bh_btn = 36;
                /* Back button */
                if (ms.x >= fx && ms.x < fx + bw_btn && ms.y >= 350 && ms.y < 350 + bh_btn)
                    prev_step();
                /* Next button */
                if (ms.x >= fx + fw - bw_btn && ms.x < fx + fw && ms.y >= 350 && ms.y < 350 + bh_btn)
                    next_step();
                break;
            }
            case STEP_THEME: {
                /* Check theme card clicks */
                int card_w = 200, card_h = 140;
                int gap = 20;
                int total_w = THEME_COUNT * card_w + (THEME_COUNT - 1) * gap;
                int sx = ((int)fb->width - total_w) / 2;
                int sy = 170;
                for (int i = 0; i < THEME_COUNT; i++) {
                    int cx = sx + i * (card_w + gap);
                    if (ms.x >= cx && ms.x < cx + card_w && ms.y >= sy && ms.y < sy + card_h)
                        selected_theme = i;
                }
                /* Nav buttons */
                int fw = 400;
                int fx = ((int)fb->width - fw) / 2;
                int bw_btn = 140, bh_btn = 36;
                if (ms.x >= fx && ms.x < fx + bw_btn && ms.y >= 380 && ms.y < 380 + bh_btn)
                    prev_step();
                if (ms.x >= fx + fw - bw_btn && ms.x < fx + fw && ms.y >= 380 && ms.y < 380 + bh_btn)
                    next_step();
                break;
            }
            case STEP_SUMMARY: {
                int fw = 460;
                int fx = ((int)fb->width - fw) / 2;
                int bw_btn = 140, bh_btn = 40;
                if (ms.x >= fx && ms.x < fx + bw_btn && ms.y >= 420 && ms.y < 420 + bh_btn)
                    prev_step();
                if (ms.x >= fx + fw - bw_btn && ms.x < fx + fw && ms.y >= 420 && ms.y < 420 + bh_btn) {
                    step = STEP_INSTALL;

                    for (install_progress = 0; install_progress <= 100; install_progress += 2) {
                        render_step();
                        uint32_t start = pit_get_ticks();
                        while (pit_get_ticks() - start < 5)
                            __asm__ volatile ("hlt");
                    }
                    step = STEP_DONE;
                }
                break;
            }
            case STEP_DONE: {
                int bw = 220, bh_btn = 40;
                int bx = ((int)fb->width - bw) / 2;
                if (ms.x >= bx && ms.x < bx + bw && ms.y >= 410 && ms.y < 410 + bh_btn)
                    running = false;
                break;
            }
            default:
                break;
            }
        }

        if (!need_render)
            __asm__ volatile ("hlt");
    }

    /* Save config */
    kstrncpy(config.hostname, hostname_buf[0] ? hostname_buf : "astra", 31);
    kstrncpy(config.username, username_buf[0] ? username_buf : "user", 31);
    config.theme = selected_theme;
    config.setup_complete = true;
}

const system_config_t *installer_get_config(void)
{
    return &config;
}
