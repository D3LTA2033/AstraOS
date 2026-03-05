/* ==========================================================================
 * AstraOS - Theme Engine Implementation
 * ==========================================================================
 * 12 built-in themes with procedural wallpaper generation.
 * ========================================================================== */

#include <kernel/theme.h>
#include <drivers/framebuffer.h>
#include <kernel/kstring.h>
#include <drivers/pit.h>

static int current_theme = 0;
static wallpaper_type_t current_wallpaper = WALLPAPER_GRADIENT;

/* ---- All 12 built-in themes ---- */

static const astra_theme_t themes[] = {
    /* 0: Catppuccin Mocha */
    {
        .name = "Catppuccin Mocha",
        .bg = RGB(30,30,46), .bg_dark = RGB(24,24,37), .surface = RGB(49,50,68),
        .overlay = RGB(108,112,134), .text = RGB(205,214,244),
        .text_dim = RGB(147,153,178), .text_bright = RGB(255,255,255),
        .accent = RGB(137,180,250), .accent2 = RGB(166,227,161),
        .highlight = RGB(203,166,247),
        .syn_keyword = RGB(203,166,247), .syn_string = RGB(166,227,161),
        .syn_number = RGB(250,179,135), .syn_comment = RGB(108,112,134),
        .syn_function = RGB(137,180,250), .syn_operator = RGB(137,180,250),
        .syn_bracket = RGB(249,226,175),
        .border = RGB(88,91,112), .titlebar = RGB(49,50,68),
        .taskbar = RGB(30,30,46),
        .red = RGB(243,139,168), .green = RGB(166,227,161), .yellow = RGB(249,226,175),
        .wall_top = RGB(15,15,35), .wall_bottom = RGB(30,20,50),
        .wall_glow = RGB(137,180,250),
        .window_alpha = 200, .titlebar_alpha = 220, .taskbar_alpha = 190,
    },
    /* 1: Nord */
    {
        .name = "Nord",
        .bg = RGB(46,52,64), .bg_dark = RGB(36,42,54), .surface = RGB(59,66,82),
        .overlay = RGB(76,86,106), .text = RGB(216,222,233),
        .text_dim = RGB(76,86,106), .text_bright = RGB(236,239,244),
        .accent = RGB(136,192,208), .accent2 = RGB(163,190,140),
        .highlight = RGB(129,161,193),
        .syn_keyword = RGB(129,161,193), .syn_string = RGB(163,190,140),
        .syn_number = RGB(180,142,173), .syn_comment = RGB(76,86,106),
        .syn_function = RGB(136,192,208), .syn_operator = RGB(129,161,193),
        .syn_bracket = RGB(235,203,139),
        .border = RGB(76,86,106), .titlebar = RGB(59,66,82),
        .taskbar = RGB(46,52,64),
        .red = RGB(191,97,106), .green = RGB(163,190,140), .yellow = RGB(235,203,139),
        .wall_top = RGB(20,25,35), .wall_bottom = RGB(46,52,64),
        .wall_glow = RGB(136,192,208),
        .window_alpha = 210, .titlebar_alpha = 230, .taskbar_alpha = 200,
    },
    /* 2: Dracula */
    {
        .name = "Dracula",
        .bg = RGB(40,42,54), .bg_dark = RGB(30,32,44), .surface = RGB(68,71,90),
        .overlay = RGB(98,114,164), .text = RGB(248,248,242),
        .text_dim = RGB(98,114,164), .text_bright = RGB(255,255,255),
        .accent = RGB(139,233,253), .accent2 = RGB(80,250,123),
        .highlight = RGB(255,121,198),
        .syn_keyword = RGB(255,121,198), .syn_string = RGB(241,250,140),
        .syn_number = RGB(189,147,249), .syn_comment = RGB(98,114,164),
        .syn_function = RGB(80,250,123), .syn_operator = RGB(255,121,198),
        .syn_bracket = RGB(248,248,242),
        .border = RGB(68,71,90), .titlebar = RGB(68,71,90),
        .taskbar = RGB(40,42,54),
        .red = RGB(255,85,85), .green = RGB(80,250,123), .yellow = RGB(241,250,140),
        .wall_top = RGB(20,20,35), .wall_bottom = RGB(40,30,55),
        .wall_glow = RGB(189,147,249),
        .window_alpha = 190, .titlebar_alpha = 210, .taskbar_alpha = 185,
    },
    /* 3: Gruvbox Dark */
    {
        .name = "Gruvbox Dark",
        .bg = RGB(40,40,40), .bg_dark = RGB(29,32,33), .surface = RGB(60,56,54),
        .overlay = RGB(80,73,69), .text = RGB(235,219,178),
        .text_dim = RGB(146,131,116), .text_bright = RGB(253,244,193),
        .accent = RGB(250,189,47), .accent2 = RGB(184,187,38),
        .highlight = RGB(254,128,25),
        .syn_keyword = RGB(204,36,29), .syn_string = RGB(184,187,38),
        .syn_number = RGB(211,134,155), .syn_comment = RGB(146,131,116),
        .syn_function = RGB(250,189,47), .syn_operator = RGB(254,128,25),
        .syn_bracket = RGB(235,219,178),
        .border = RGB(80,73,69), .titlebar = RGB(60,56,54),
        .taskbar = RGB(40,40,40),
        .red = RGB(204,36,29), .green = RGB(152,151,26), .yellow = RGB(215,153,33),
        .wall_top = RGB(25,23,20), .wall_bottom = RGB(50,45,38),
        .wall_glow = RGB(250,189,47),
        .window_alpha = 215, .titlebar_alpha = 235, .taskbar_alpha = 205,
    },
    /* 4: Tokyo Night */
    {
        .name = "Tokyo Night",
        .bg = RGB(26,27,38), .bg_dark = RGB(22,22,30), .surface = RGB(36,40,59),
        .overlay = RGB(65,72,104), .text = RGB(169,177,214),
        .text_dim = RGB(65,72,104), .text_bright = RGB(200,210,240),
        .accent = RGB(122,162,247), .accent2 = RGB(158,206,106),
        .highlight = RGB(187,154,247),
        .syn_keyword = RGB(187,154,247), .syn_string = RGB(158,206,106),
        .syn_number = RGB(255,158,100), .syn_comment = RGB(65,72,104),
        .syn_function = RGB(122,162,247), .syn_operator = RGB(137,221,255),
        .syn_bracket = RGB(169,177,214),
        .border = RGB(41,46,66), .titlebar = RGB(36,40,59),
        .taskbar = RGB(26,27,38),
        .red = RGB(247,118,142), .green = RGB(158,206,106), .yellow = RGB(224,175,104),
        .wall_top = RGB(12,12,22), .wall_bottom = RGB(26,20,45),
        .wall_glow = RGB(122,162,247),
        .window_alpha = 195, .titlebar_alpha = 215, .taskbar_alpha = 185,
    },
    /* 5: Rosé Pine */
    {
        .name = "Rose Pine",
        .bg = RGB(25,23,36), .bg_dark = RGB(21,19,30), .surface = RGB(38,35,53),
        .overlay = RGB(57,53,82), .text = RGB(224,222,244),
        .text_dim = RGB(110,106,134), .text_bright = RGB(240,240,255),
        .accent = RGB(196,167,231), .accent2 = RGB(49,116,143),
        .highlight = RGB(235,111,146),
        .syn_keyword = RGB(49,116,143), .syn_string = RGB(246,193,119),
        .syn_number = RGB(235,111,146), .syn_comment = RGB(110,106,134),
        .syn_function = RGB(196,167,231), .syn_operator = RGB(156,207,216),
        .syn_bracket = RGB(224,222,244),
        .border = RGB(57,53,82), .titlebar = RGB(38,35,53),
        .taskbar = RGB(25,23,36),
        .red = RGB(235,111,146), .green = RGB(49,116,143), .yellow = RGB(246,193,119),
        .wall_top = RGB(15,13,25), .wall_bottom = RGB(30,25,45),
        .wall_glow = RGB(196,167,231),
        .window_alpha = 195, .titlebar_alpha = 220, .taskbar_alpha = 185,
    },
    /* 6: Solarized Dark */
    {
        .name = "Solarized Dark",
        .bg = RGB(0,43,54), .bg_dark = RGB(0,34,43), .surface = RGB(7,54,66),
        .overlay = RGB(88,110,117), .text = RGB(147,161,161),
        .text_dim = RGB(88,110,117), .text_bright = RGB(253,246,227),
        .accent = RGB(38,139,210), .accent2 = RGB(133,153,0),
        .highlight = RGB(108,113,196),
        .syn_keyword = RGB(133,153,0), .syn_string = RGB(42,161,152),
        .syn_number = RGB(211,54,130), .syn_comment = RGB(88,110,117),
        .syn_function = RGB(38,139,210), .syn_operator = RGB(203,75,22),
        .syn_bracket = RGB(147,161,161),
        .border = RGB(88,110,117), .titlebar = RGB(7,54,66),
        .taskbar = RGB(0,43,54),
        .red = RGB(220,50,47), .green = RGB(133,153,0), .yellow = RGB(181,137,0),
        .wall_top = RGB(0,20,28), .wall_bottom = RGB(0,43,54),
        .wall_glow = RGB(38,139,210),
        .window_alpha = 210, .titlebar_alpha = 230, .taskbar_alpha = 200,
    },
    /* 7: One Dark */
    {
        .name = "One Dark",
        .bg = RGB(40,44,52), .bg_dark = RGB(33,37,43), .surface = RGB(50,55,65),
        .overlay = RGB(92,99,112), .text = RGB(171,178,191),
        .text_dim = RGB(92,99,112), .text_bright = RGB(220,223,228),
        .accent = RGB(97,175,239), .accent2 = RGB(152,195,121),
        .highlight = RGB(198,120,221),
        .syn_keyword = RGB(198,120,221), .syn_string = RGB(152,195,121),
        .syn_number = RGB(209,154,102), .syn_comment = RGB(92,99,112),
        .syn_function = RGB(97,175,239), .syn_operator = RGB(86,182,194),
        .syn_bracket = RGB(171,178,191),
        .border = RGB(62,68,81), .titlebar = RGB(50,55,65),
        .taskbar = RGB(40,44,52),
        .red = RGB(224,108,117), .green = RGB(152,195,121), .yellow = RGB(229,192,123),
        .wall_top = RGB(20,22,28), .wall_bottom = RGB(40,35,55),
        .wall_glow = RGB(97,175,239),
        .window_alpha = 205, .titlebar_alpha = 225, .taskbar_alpha = 195,
    },
    /* 8: Kanagawa */
    {
        .name = "Kanagawa",
        .bg = RGB(31,31,40), .bg_dark = RGB(22,22,30), .surface = RGB(42,42,54),
        .overlay = RGB(84,84,109), .text = RGB(220,215,186),
        .text_dim = RGB(84,84,109), .text_bright = RGB(240,235,210),
        .accent = RGB(127,180,202), .accent2 = RGB(152,196,124),
        .highlight = RGB(210,126,153),
        .syn_keyword = RGB(149,127,184), .syn_string = RGB(152,196,124),
        .syn_number = RGB(255,160,102), .syn_comment = RGB(84,84,109),
        .syn_function = RGB(127,180,202), .syn_operator = RGB(228,104,118),
        .syn_bracket = RGB(220,215,186),
        .border = RGB(54,54,70), .titlebar = RGB(42,42,54),
        .taskbar = RGB(31,31,40),
        .red = RGB(228,104,118), .green = RGB(152,196,124), .yellow = RGB(255,199,119),
        .wall_top = RGB(15,15,25), .wall_bottom = RGB(31,25,42),
        .wall_glow = RGB(127,180,202),
        .window_alpha = 195, .titlebar_alpha = 220, .taskbar_alpha = 185,
    },
    /* 9: Everforest */
    {
        .name = "Everforest",
        .bg = RGB(39,46,46), .bg_dark = RGB(32,38,38), .surface = RGB(52,60,57),
        .overlay = RGB(90,100,95), .text = RGB(211,198,170),
        .text_dim = RGB(135,144,138), .text_bright = RGB(230,220,200),
        .accent = RGB(131,165,152), .accent2 = RGB(167,192,128),
        .highlight = RGB(219,188,127),
        .syn_keyword = RGB(214,127,106), .syn_string = RGB(167,192,128),
        .syn_number = RGB(211,137,146), .syn_comment = RGB(135,144,138),
        .syn_function = RGB(131,165,152), .syn_operator = RGB(214,127,106),
        .syn_bracket = RGB(211,198,170),
        .border = RGB(80,90,85), .titlebar = RGB(52,60,57),
        .taskbar = RGB(39,46,46),
        .red = RGB(230,126,128), .green = RGB(167,192,128), .yellow = RGB(219,188,127),
        .wall_top = RGB(22,28,28), .wall_bottom = RGB(39,46,40),
        .wall_glow = RGB(131,165,152),
        .window_alpha = 210, .titlebar_alpha = 230, .taskbar_alpha = 200,
    },
    /* 10: Cyberpunk */
    {
        .name = "Cyberpunk",
        .bg = RGB(13,2,33), .bg_dark = RGB(8,0,22), .surface = RGB(25,10,55),
        .overlay = RGB(60,30,90), .text = RGB(230,230,255),
        .text_dim = RGB(120,100,160), .text_bright = RGB(255,255,255),
        .accent = RGB(255,0,200), .accent2 = RGB(0,255,200),
        .highlight = RGB(255,100,50),
        .syn_keyword = RGB(255,0,200), .syn_string = RGB(0,255,200),
        .syn_number = RGB(255,200,0), .syn_comment = RGB(80,60,120),
        .syn_function = RGB(100,200,255), .syn_operator = RGB(255,0,200),
        .syn_bracket = RGB(230,230,255),
        .border = RGB(255,0,200), .titlebar = RGB(25,10,55),
        .taskbar = RGB(13,2,33),
        .red = RGB(255,50,50), .green = RGB(0,255,150), .yellow = RGB(255,255,0),
        .wall_top = RGB(5,0,18), .wall_bottom = RGB(20,0,45),
        .wall_glow = RGB(255,0,200),
        .window_alpha = 170, .titlebar_alpha = 200, .taskbar_alpha = 165,
    },
    /* 11: Midnight Blue */
    {
        .name = "Midnight Blue",
        .bg = RGB(15,20,40), .bg_dark = RGB(10,14,30), .surface = RGB(25,35,65),
        .overlay = RGB(50,65,100), .text = RGB(200,210,230),
        .text_dim = RGB(90,110,150), .text_bright = RGB(230,240,255),
        .accent = RGB(100,180,255), .accent2 = RGB(120,230,180),
        .highlight = RGB(200,150,255),
        .syn_keyword = RGB(200,150,255), .syn_string = RGB(120,230,180),
        .syn_number = RGB(255,180,100), .syn_comment = RGB(70,90,130),
        .syn_function = RGB(100,180,255), .syn_operator = RGB(200,150,255),
        .syn_bracket = RGB(200,210,230),
        .border = RGB(50,70,110), .titlebar = RGB(25,35,65),
        .taskbar = RGB(15,20,40),
        .red = RGB(255,100,100), .green = RGB(100,220,150), .yellow = RGB(255,220,100),
        .wall_top = RGB(5,8,22), .wall_bottom = RGB(15,20,45),
        .wall_glow = RGB(100,180,255),
        .window_alpha = 185, .titlebar_alpha = 210, .taskbar_alpha = 180,
    },
};

#define THEME_TOTAL 12

/* ---- Simple pseudo-random for procedural wallpapers ---- */
static uint32_t rng_state = 12345;
static uint32_t rng_next(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

/* ---- Integer square root (for circles) ---- */
static uint32_t isqrt(uint32_t n)
{
    if (n == 0) return 0;
    uint32_t x = n;
    uint32_t y = (x + 1) / 2;
    while (y < x) { x = y; y = (x + n / x) / 2; }
    return x;
}

/* ---- Public API ---- */

void theme_init(void)
{
    current_theme = 0;
    current_wallpaper = WALLPAPER_GRADIENT;
}

const astra_theme_t *theme_current(void)
{
    return &themes[current_theme];
}

void theme_set(int index)
{
    if (index >= 0 && index < THEME_TOTAL)
        current_theme = index;
}

int theme_count(void)
{
    return THEME_TOTAL;
}

const astra_theme_t *theme_get(int index)
{
    if (index >= 0 && index < THEME_TOTAL)
        return &themes[index];
    return &themes[0];
}

const char *theme_name(int index)
{
    if (index >= 0 && index < THEME_TOTAL)
        return themes[index].name;
    return "Unknown";
}

void theme_set_wallpaper(wallpaper_type_t type)
{
    if (type < WALLPAPER_COUNT)
        current_wallpaper = type;
}

wallpaper_type_t theme_get_wallpaper(void)
{
    return current_wallpaper;
}

color_t theme_glass(color_t base)
{
    const astra_theme_t *t = theme_current();
    return RGBA(COL_R(base), COL_G(base), COL_B(base), t->window_alpha);
}

color_t theme_titlebar_glass(color_t base)
{
    const astra_theme_t *t = theme_current();
    return RGBA(COL_R(base), COL_G(base), COL_B(base), t->titlebar_alpha);
}

color_t theme_bg(void)       { return theme_current()->bg; }
color_t theme_surface(void)  { return theme_current()->surface; }
color_t theme_text(void)     { return theme_current()->text; }
color_t theme_accent(void)   { return theme_current()->accent; }
color_t theme_border(void)   { return theme_current()->border; }

/* ---- Wallpaper renderers ---- */

static void wallpaper_gradient(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Center glow */
    int cx = w / 2;
    int cy = h / 2;
    for (int r = 250; r > 0; r -= 25) {
        uint32_t alpha = (uint32_t)(20 * (250 - r) / 250);
        if (alpha > 25) alpha = 25;
        color_t glow = RGBA(COL_R(t->wall_glow), COL_G(t->wall_glow),
                           COL_B(t->wall_glow), alpha);
        fb_fill_rect_alpha(cx - r, cy - r, r * 2, r * 2, glow);
    }
}

static void wallpaper_mesh(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Procedural mesh gradient: overlapping alpha-blended blobs */
    rng_state = (uint32_t)(t->accent ^ t->highlight) | 1;

    for (int i = 0; i < 6; i++) {
        int bx = (int)(rng_next() % (uint32_t)w);
        int by = (int)(rng_next() % (uint32_t)h);
        int br = 150 + (int)(rng_next() % 200);
        color_t colors[] = { t->accent, t->accent2, t->highlight,
                            t->wall_glow, t->green, t->yellow };
        color_t c = colors[i % 6];
        for (int r = br; r > 0; r -= 20) {
            uint32_t a = (uint32_t)(12 * (br - r) / br);
            if (a > 15) a = 15;
            fb_fill_rect_alpha(bx - r, by - r, r * 2, r * 2,
                             RGBA(COL_R(c), COL_G(c), COL_B(c), a));
        }
    }
}

static void wallpaper_waves(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Horizontal wave bands with alpha */
    uint32_t tick = pit_get_ticks();
    for (int band = 0; band < 5; band++) {
        color_t c = (band & 1) ? t->accent : t->wall_glow;
        int base_y = h / 6 * (band + 1);
        for (int x = 0; x < w; x += 2) {
            /* Simple sine approximation using triangle wave */
            int phase = (int)((uint32_t)x + tick * 2 + (uint32_t)(band * 80));
            int period = 200 + band * 40;
            int pos = phase % period;
            int wave;
            if (pos < period / 2)
                wave = (pos * 40) / (period / 2) - 20;
            else
                wave = 20 - ((pos - period / 2) * 40) / (period / 2);
            int y = base_y + wave;
            fb_fill_rect_alpha(x, y, 3, 8, RGBA(COL_R(c), COL_G(c), COL_B(c), 18));
        }
    }
}

static void wallpaper_geometric(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Draw geometric shapes: triangles, rectangles, diamonds */
    rng_state = (uint32_t)(t->accent ^ t->bg) | 1;

    for (int i = 0; i < 20; i++) {
        int sx = (int)(rng_next() % (uint32_t)w);
        int sy = (int)(rng_next() % (uint32_t)h);
        int ss = 30 + (int)(rng_next() % 80);
        uint32_t a = 8 + rng_next() % 15;
        color_t colors[] = { t->accent, t->accent2, t->highlight, t->wall_glow };
        color_t c = colors[i % 4];
        color_t ac = RGBA(COL_R(c), COL_G(c), COL_B(c), a);

        if (i % 3 == 0) {
            /* Rotated rectangle (approximated as rectangle) */
            fb_fill_rect_alpha(sx, sy, ss, ss / 2, ac);
        } else if (i % 3 == 1) {
            /* Diamond (draw as two triangles via horizontal lines) */
            for (int dy = 0; dy < ss / 2; dy++) {
                int dw = dy * 2;
                fb_fill_rect_alpha(sx + ss / 2 - dy, sy + dy, dw, 1, ac);
                fb_fill_rect_alpha(sx + ss / 2 - dy, sy + ss - dy, dw, 1, ac);
            }
        } else {
            /* Circle */
            for (int ry = -ss / 2; ry <= ss / 2; ry++) {
                uint32_t rx = isqrt((uint32_t)(ss * ss / 4 - ry * ry));
                fb_fill_rect_alpha(sx - (int)rx, sy + ry, (int)(rx * 2), 1, ac);
            }
        }
    }
}

static void wallpaper_particles(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Starfield: random dots and small glows */
    rng_state = 42;  /* Fixed seed for consistent stars */
    for (int i = 0; i < 200; i++) {
        int sx = (int)(rng_next() % (uint32_t)w);
        int sy = (int)(rng_next() % (uint32_t)h);
        uint32_t brightness = 80 + rng_next() % 175;
        int sz = 1 + (int)(rng_next() % 3);

        if (sz > 1) {
            /* Bigger star with glow */
            fb_fill_rect_alpha(sx - 2, sy - 2, 5, 5,
                             RGBA(COL_R(t->wall_glow), COL_G(t->wall_glow),
                                  COL_B(t->wall_glow), 15));
        }
        fb_fill_rect(sx, sy, sz, sz, RGBA(brightness, brightness, brightness + 20, 255));
    }

    /* A few colored nebula blobs */
    for (int i = 0; i < 3; i++) {
        int bx = (int)(rng_next() % (uint32_t)w);
        int by = (int)(rng_next() % (uint32_t)h);
        color_t c = (i == 0) ? t->accent : (i == 1) ? t->highlight : t->accent2;
        for (int r = 100; r > 0; r -= 15) {
            fb_fill_rect_alpha(bx - r, by - r, r * 2, r * 2,
                             RGBA(COL_R(c), COL_G(c), COL_B(c), 5));
        }
    }
}

static void wallpaper_aurora(int w, int h)
{
    const astra_theme_t *t = theme_current();
    fb_gradient_v(0, 0, w, h, t->wall_top, t->wall_bottom);

    /* Aurora bands */
    uint32_t tick = pit_get_ticks();
    color_t aurora_colors[] = {
        t->accent, t->green, t->accent2, t->highlight
    };

    for (int band = 0; band < 4; band++) {
        color_t c = aurora_colors[band];
        int base_y = h / 4 + band * 40;
        for (int x = 0; x < w; x += 3) {
            int phase = (int)((uint32_t)x / 3 + tick + (uint32_t)(band * 50));
            int period = 120 + band * 30;
            int pos = ((phase % period) + period) % period;
            int wave;
            if (pos < period / 4)
                wave = pos * 60 / (period / 4);
            else if (pos < period * 3 / 4)
                wave = 60 - (pos - period / 4) * 120 / (period / 2);
            else
                wave = -60 + (pos - period * 3 / 4) * 60 / (period / 4);

            int vert_h = 30 + (wave > 0 ? wave : -wave) / 2;
            uint32_t a = (uint32_t)(10 + (wave > 0 ? wave : -wave) / 4);
            if (a > 30) a = 30;

            fb_fill_rect_alpha(x, base_y + wave, 4, vert_h,
                             RGBA(COL_R(c), COL_G(c), COL_B(c), a));
        }
    }
}

void theme_draw_wallpaper(int w, int h)
{
    switch (current_wallpaper) {
    case WALLPAPER_GRADIENT:   wallpaper_gradient(w, h);   break;
    case WALLPAPER_MESH:       wallpaper_mesh(w, h);       break;
    case WALLPAPER_WAVES:      wallpaper_waves(w, h);      break;
    case WALLPAPER_GEOMETRIC:  wallpaper_geometric(w, h);  break;
    case WALLPAPER_PARTICLES:  wallpaper_particles(w, h);  break;
    case WALLPAPER_AURORA:     wallpaper_aurora(w, h);     break;
    case WALLPAPER_SOLID:
        fb_clear(theme_current()->bg);
        break;
    default:
        wallpaper_gradient(w, h);
        break;
    }

    /* Subtle branding */
    const char *brand = "AstraOS 2.0";
    int bw = fb_text_width(brand);
    fb_puts_transparent((w - bw) / 2, h / 2 - 40, brand,
                       RGBA(COL_R(theme_current()->text), COL_G(theme_current()->text),
                            COL_B(theme_current()->text), 40));
}
