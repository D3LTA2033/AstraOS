/* ==========================================================================
 * AstraOS - Built-in GUI Applications
 * ==========================================================================
 * Text Editor (with Astracor support) and Terminal for the desktop.
 * ========================================================================== */

#include <kernel/gui.h>
#include <kernel/theme.h>
#include <kernel/astracor.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
#include <kernel/vfs.h>
#include <drivers/framebuffer.h>

/* ========================================================================
 * Terminal Application
 * ======================================================================== */

#define TERM_LINES  256
#define TERM_COLS   120
#define TERM_INPUT  256

typedef struct {
    char lines[TERM_LINES][TERM_COLS];
    int  line_count;
    int  scroll;
    char input[TERM_INPUT];
    int  input_len;
    int  cursor;
    ac_interp_t *interp;
} terminal_t;

static void term_output(const char *text, void *data)
{
    terminal_t *t = (terminal_t *)data;
    while (*text) {
        if (*text == '\n' || t->lines[t->line_count][0] == '\0') {
            if (*text == '\n') {
                if (t->line_count < TERM_LINES - 1) t->line_count++;
                else {
                    /* Scroll buffer */
                    for (int i = 0; i < TERM_LINES - 1; i++)
                        kmemcpy(t->lines[i], t->lines[i + 1], TERM_COLS);
                    kmemset(t->lines[TERM_LINES - 1], 0, TERM_COLS);
                }
                text++;
                continue;
            }
        }
        int len = (int)kstrlen(t->lines[t->line_count]);
        if (len < TERM_COLS - 1) {
            t->lines[t->line_count][len] = *text;
            t->lines[t->line_count][len + 1] = '\0';
        }
        text++;
    }
}

static void term_add_line(terminal_t *t, const char *text)
{
    if (t->line_count < TERM_LINES - 1) t->line_count++;
    else {
        for (int i = 0; i < TERM_LINES - 1; i++)
            kmemcpy(t->lines[i], t->lines[i + 1], TERM_COLS);
        kmemset(t->lines[TERM_LINES - 1], 0, TERM_COLS);
    }
    kstrncpy(t->lines[t->line_count], text, TERM_COLS - 1);
}

static void term_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    terminal_t *t = (terminal_t *)win->userdata;
    if (!t) return;

    const astra_theme_t *th = theme_current();
    /* Transparent terminal background */
    fb_fill_rect_alpha(x, y, w, h,
                      RGBA(COL_R(th->bg_dark), COL_G(th->bg_dark), COL_B(th->bg_dark), 200));

    /* Draw output lines */
    int visible_lines = (h - FONT_H - 4) / FONT_H;
    int start = t->line_count - visible_lines;
    if (start < 0) start = 0;

    for (int i = start; i <= t->line_count && (i - start) < visible_lines; i++) {
        fb_puts_transparent(x + 4, y + (i - start) * FONT_H + 2,
                           t->lines[i], th->text);
    }

    /* Draw input line at bottom */
    int iy = y + h - FONT_H - 4;
    fb_fill_rect_alpha(x, iy, w, FONT_H + 4,
                      RGBA(COL_R(th->surface), COL_G(th->surface), COL_B(th->surface), 220));
    fb_puts_transparent(x + 4, iy + 2, ">> ", th->accent);
    fb_puts_transparent(x + 28, iy + 2, t->input, th->text);

    /* Cursor */
    int cx = x + 28 + t->cursor * FONT_W;
    fb_fill_rect(cx, iy + 2, 2, FONT_H, th->accent);
}

static void term_execute(terminal_t *t)
{
    if (t->input_len == 0) return;

    /* Show the command */
    char prompt[TERM_COLS];
    kstrncpy(prompt, ">> ", TERM_COLS);
    int plen = 3;
    for (int i = 0; i < t->input_len && plen < TERM_COLS - 1; i++)
        prompt[plen++] = t->input[i];
    prompt[plen] = '\0';
    term_add_line(t, prompt);

    /* Execute */
    int result = ac_exec(t->interp, t->input);
    if (result < 0) {
        const char *err = ac_get_error(t->interp);
        if (err) term_add_line(t, err);
    }

    /* Clear input */
    kmemset(t->input, 0, TERM_INPUT);
    t->input_len = 0;
    t->cursor = 0;
}

static void term_key(int win_id, int key)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    terminal_t *t = (terminal_t *)win->userdata;
    if (!t) return;

    if (key == '\n') {
        term_execute(t);
    } else if (key == '\b') {
        if (t->cursor > 0) {
            t->cursor--;
            t->input[t->cursor] = '\0';
            t->input_len--;
        }
    } else if (key >= 32 && key < 127 && t->input_len < TERM_INPUT - 1) {
        t->input[t->input_len++] = (char)key;
        t->input[t->input_len] = '\0';
        t->cursor++;
    }
    gui_invalidate(win_id);
}

int app_terminal_create(int x, int y, int w, int h)
{
    int wid = gui_create_window("Astracor Terminal", x, y, w, h, WIN_DEFAULT);
    if (wid < 0) return -1;

    terminal_t *t = (terminal_t *)kmalloc(sizeof(terminal_t));
    kmemset(t, 0, sizeof(terminal_t));
    t->interp = ac_create();
    ac_set_output(t->interp, term_output, t);

    /* Welcome message */
    kstrncpy(t->lines[0], "Astracor Language v1.0 - AstraOS", TERM_COLS);
    kstrncpy(t->lines[1], "Type expressions or programs. Examples:", TERM_COLS);
    kstrncpy(t->lines[2], "  print(\"Hello, World!\")", TERM_COLS);
    kstrncpy(t->lines[3], "  let x = 42", TERM_COLS);
    kstrncpy(t->lines[4], "  fn fib(n) { if n < 2 { return n } return fib(n-1) + fib(n-2) }", TERM_COLS);
    t->line_count = 5;

    gui_set_render(wid, term_render);
    gui_set_key_handler(wid, term_key);
    gui_set_userdata(wid, t);
    return wid;
}

/* ========================================================================
 * Code Editor Application — VS Code-like Editor
 * ======================================================================== */

#define EDITOR_MAX_LINES 512
#define EDITOR_LINE_LEN  256
#define EDITOR_MAX_TABS  8
#define TAB_BAR_H        28
#define STATUS_BAR_H     22
#define MINIMAP_W        60
#define SIDEBAR_W_DEFAULT 160
#define GUTTER_W         48
#define OUTPUT_RATIO     30  /* bottom 30% for output */

/* Key codes for special keys (mapped by GUI/keyboard layer) */
#define EKEY_UP    0x80
#define EKEY_DOWN  0x81
#define EKEY_LEFT  0x82
#define EKEY_RIGHT 0x83
#define EKEY_HOME  0x84
#define EKEY_END   0x85
#define EKEY_PGUP  0x86
#define EKEY_PGDN  0x87
#define EKEY_DEL   0x88
#define EKEY_F5    0x89

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
    int  total_lines;
    int  cursor_line;
    int  cursor_col;
    int  scroll_y;
    char filename[64];
    bool modified;
    bool active;
} editor_tab_t;

typedef struct {
    editor_tab_t tabs[EDITOR_MAX_TABS];
    int  tab_count;
    int  active_tab;
    /* Output panel */
    char output[2048];
    int  output_len;
    ac_interp_t *interp;
    /* Sidebar */
    bool sidebar_open;
    int  sidebar_width;
} editor_t;

/* ---------- helpers ---------- */

static void ed_int_to_str(int val, char *buf, int bufsz)
{
    if (val == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12];
    int ti = 0;
    int v = val < 0 ? -val : val;
    while (v > 0 && ti < 11) { tmp[ti++] = '0' + (char)(v % 10); v /= 10; }
    int bi = 0;
    if (val < 0 && bi < bufsz - 1) buf[bi++] = '-';
    for (int j = ti - 1; j >= 0 && bi < bufsz - 1; j--) buf[bi++] = tmp[j];
    buf[bi] = '\0';
}

static void editor_output(const char *text, void *data)
{
    editor_t *ed = (editor_t *)data;
    while (*text && ed->output_len < 2040) {
        ed->output[ed->output_len++] = *text++;
    }
    ed->output[ed->output_len] = '\0';
}

/* Get current active tab (convenience) */
static editor_tab_t *ed_tab(editor_t *ed)
{
    if (ed->active_tab < 0 || ed->active_tab >= ed->tab_count) return NULL;
    return &ed->tabs[ed->active_tab];
}

/* ---------- syntax helpers ---------- */

static bool is_keyword(const char *word, int len)
{
    static const char *kws[] = {"fn","let","if","elif","else","while","for","in",
        "return","true","false","null","break","continue","import"};
    for (int i = 0; i < 15; i++) {
        int klen = (int)kstrlen(kws[i]);
        if (len == klen) {
            bool match = true;
            for (int j = 0; j < len; j++)
                if (word[j] != kws[i][j]) { match = false; break; }
            if (match) return true;
        }
    }
    return false;
}

static bool is_builtin(const char *word, int len)
{
    static const char *bfs[] = {"print","len","str","int","type","range","abs",
        "substr","indexof","split","join","upper","lower","trim","replace",
        "startswith","endswith","char_at","contains","min","max","clamp",
        "pow","push","pop","get","set"};
    for (int i = 0; i < 27; i++) {
        int blen = (int)kstrlen(bfs[i]);
        if (len == blen) {
            bool match = true;
            for (int j = 0; j < len; j++)
                if (word[j] != bfs[i][j]) { match = false; break; }
            if (match) return true;
        }
    }
    return false;
}

static bool is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || (c >= '0' && c <= '9');
}

static bool is_alpha_or_under(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

/* Classify a character for minimap coloring (returns color) */
static color_t ed_syntax_color_at(const char *line, int col)
{
    /* Walk from start to determine context at col */
    int i = 0;
    while (i <= col && line[i]) {
        if (line[i] == '#' || (line[i] == '/' && line[i+1] == '/'))
            return COL_OVERLAY; /* comment */
        if (line[i] == '"') {
            i++;
            while (line[i] && line[i] != '"') {
                if (i == col) return COL_GREEN;
                i++;
            }
            if (i == col) return COL_GREEN;
            if (line[i]) i++;
            continue;
        }
        if (i == col) {
            if (is_alpha_or_under(line[i])) {
                int start = i, end = i;
                while (is_ident_char(line[end])) end++;
                if (is_keyword(line + start, end - start)) return COL_PURPLE;
                if (is_builtin(line + start, end - start)) return COL_BLUE;
                /* Function call: identifier followed by ( */
                if (line[end] == '(') return COL_YELLOW;
                return COL_TEXT;
            }
            if (line[i] >= '0' && line[i] <= '9') return COL_ORANGE;
            if (line[i] == '(' || line[i] == ')' || line[i] == '{' ||
                line[i] == '}' || line[i] == '[' || line[i] == ']')
                return COL_YELLOW;
            if (line[i] == '+' || line[i] == '-' || line[i] == '*' ||
                line[i] == '/' || line[i] == '=' || line[i] == '<' ||
                line[i] == '>' || line[i] == '!')
                return COL_ACCENT;
            return COL_TEXT_DIM;
        }
        if (is_alpha_or_under(line[i])) {
            while (is_ident_char(line[i])) i++;
            continue;
        }
        i++;
    }
    return COL_TEXT_DIM;
}

/* ---------- Render a syntax-highlighted line ---------- */

static void editor_render_line(int x, int y, const char *line, int w,
                               color_t bg)
{
    int col = 0;
    int px = x;
    int max_col = w / FONT_W;

    while (line[col] && col < max_col) {
        color_t fg = COL_TEXT;

        /* // comment */
        if (line[col] == '/' && line[col + 1] == '/') {
            while (line[col] && col < max_col) {
                fb_putchar(px, y, line[col], COL_OVERLAY, bg);
                px += FONT_W; col++;
            }
            return;
        }
        /* # comment */
        if (line[col] == '#') {
            while (line[col] && col < max_col) {
                fb_putchar(px, y, line[col], COL_OVERLAY, bg);
                px += FONT_W; col++;
            }
            return;
        }
        /* String literal */
        if (line[col] == '"') {
            fb_putchar(px, y, '"', COL_GREEN, bg);
            px += FONT_W; col++;
            while (line[col] && line[col] != '"' && col < max_col) {
                fb_putchar(px, y, line[col], COL_GREEN, bg);
                px += FONT_W; col++;
            }
            if (line[col] == '"') {
                fb_putchar(px, y, '"', COL_GREEN, bg);
                px += FONT_W; col++;
            }
            continue;
        }
        /* Numbers */
        if (line[col] >= '0' && line[col] <= '9') {
            fg = COL_ORANGE;
        }
        /* Identifiers, keywords, builtins, function calls */
        else if (is_alpha_or_under(line[col])) {
            int start = col;
            int end = col;
            while (is_ident_char(line[end])) end++;
            int wlen = end - start;

            color_t id_col = COL_TEXT;
            if (is_keyword(line + start, wlen))
                id_col = COL_PURPLE;
            else if (is_builtin(line + start, wlen))
                id_col = COL_BLUE;
            else if (line[end] == '(')
                id_col = COL_YELLOW; /* function call */

            for (int i = start; i < end && i < max_col; i++) {
                fb_putchar(px, y, line[i], id_col, bg);
                px += FONT_W;
            }
            col = end;
            continue;
        }
        /* Dot access */
        else if (line[col] == '.') {
            fg = COL_ACCENT;
        }
        /* Brackets */
        else if (line[col] == '(' || line[col] == ')' || line[col] == '{' ||
                 line[col] == '}' || line[col] == '[' || line[col] == ']') {
            fg = COL_YELLOW;
        }
        /* Operators */
        else if (line[col] == '+' || line[col] == '-' || line[col] == '*' ||
                 line[col] == '/' || line[col] == '=' || line[col] == '<' ||
                 line[col] == '>' || line[col] == '!') {
            fg = COL_ACCENT;
        }

        fb_putchar(px, y, line[col], fg, bg);
        px += FONT_W;
        col++;
    }
}

/* ---------- Sidebar: VFS file explorer ---------- */

static void editor_render_sidebar(editor_t *ed, int x, int y, int w, int h)
{
    fb_fill_rect(x, y, w, h, RGB(33, 33, 52));
    /* Header */
    fb_fill_rect(x, y, w, 24, RGB(38, 38, 58));
    fb_puts(x + 8, y + 4, "EXPLORER", COL_TEXT_DIM, RGB(38, 38, 58));
    fb_hline(x, y + 24, w, COL_BORDER);

    vfs_node_t *root = vfs_root();
    if (!root) return;

    int row_y = y + 28;
    int row_h = FONT_H + 2;

    /* Draw root children (one level deep for simplicity) */
    for (uint32_t i = 0; i < root->child_count && row_y < y + h - row_h; i++) {
        vfs_node_t *child = root->children[i];
        if (!child) continue;

        if (child->type & VFS_DIRECTORY) {
            /* Directory: show with > arrow */
            fb_putchar(x + 4, row_y, '>', COL_TEXT_DIM, RGB(33, 33, 52));
            fb_puts(x + 16, row_y, child->name, COL_YELLOW, RGB(33, 33, 52));
            row_y += row_h;

            /* Show children of directories */
            for (uint32_t j = 0; j < child->child_count &&
                 row_y < y + h - row_h; j++) {
                vfs_node_t *sub = child->children[j];
                if (!sub) continue;
                if (sub->type & VFS_DIRECTORY) {
                    fb_putchar(x + 20, row_y, '>', COL_TEXT_DIM,
                               RGB(33, 33, 52));
                    fb_puts(x + 32, row_y, sub->name, COL_YELLOW,
                            RGB(33, 33, 52));
                } else {
                    fb_putchar(x + 20, row_y, ' ', COL_TEXT_DIM,
                               RGB(33, 33, 52));
                    /* Color .ac files in accent */
                    int nlen = (int)kstrlen(sub->name);
                    bool is_ac = (nlen > 3 && sub->name[nlen-3] == '.' &&
                                  sub->name[nlen-2] == 'a' &&
                                  sub->name[nlen-1] == 'c');
                    fb_puts(x + 32, row_y, sub->name,
                            is_ac ? COL_ACCENT : COL_TEXT, RGB(33, 33, 52));
                }
                row_y += row_h;
            }
        } else {
            /* File at root level */
            int nlen = (int)kstrlen(child->name);
            bool is_ac = (nlen > 3 && child->name[nlen-3] == '.' &&
                          child->name[nlen-2] == 'a' &&
                          child->name[nlen-1] == 'c');
            fb_putchar(x + 4, row_y, ' ', COL_TEXT_DIM, RGB(33, 33, 52));
            fb_puts(x + 16, row_y, child->name,
                    is_ac ? COL_ACCENT : COL_TEXT, RGB(33, 33, 52));
            row_y += row_h;
        }
    }

    /* Right border */
    fb_vline(x + w - 1, y, h, COL_BORDER);
    (void)ed;
}

/* ---------- Tab bar ---------- */

static void editor_render_tabs(editor_t *ed, int x, int y, int w)
{
    fb_fill_rect(x, y, w, TAB_BAR_H, RGB(35, 35, 52));
    fb_hline(x, y + TAB_BAR_H - 1, w, COL_BORDER);

    int tx = x;
    for (int i = 0; i < ed->tab_count; i++) {
        editor_tab_t *tab = &ed->tabs[i];
        if (!tab->active) continue;

        int nlen = (int)kstrlen(tab->filename);
        int tab_w = (nlen + 3) * FONT_W + 8; /* name + modified dot + padding */
        if (tab_w < 80) tab_w = 80;
        if (tx + tab_w > x + w) break;

        color_t tab_bg = (i == ed->active_tab) ? COL_BG_DARK : RGB(38, 38, 56);
        fb_fill_rect(tx, y, tab_w, TAB_BAR_H - 1, tab_bg);

        /* Active tab accent underline */
        if (i == ed->active_tab)
            fb_fill_rect(tx, y + TAB_BAR_H - 3, tab_w, 2, COL_ACCENT);

        /* Filename */
        fb_puts(tx + 8, y + 6, tab->filename, COL_TEXT, tab_bg);

        /* Modified indicator (dot) */
        if (tab->modified) {
            int dot_x = tx + 8 + nlen * FONT_W + 4;
            fb_fill_rect(dot_x, y + 10, 6, 6, COL_ACCENT);
        }

        /* Tab separator */
        fb_vline(tx + tab_w - 1, y, TAB_BAR_H - 1, RGB(50, 50, 70));

        tx += tab_w;
    }
}

/* ---------- Minimap ---------- */

static void editor_render_minimap(editor_t *ed, editor_tab_t *tab,
                                  int x, int y, int w, int h,
                                  int visible_lines)
{
    fb_fill_rect(x, y, w, h, RGB(25, 25, 40));
    fb_vline(x, y, h, RGB(45, 45, 65));

    int line_h = 2; /* each line is 2px tall in minimap */
    int max_lines_shown = h / line_h;
    int total = tab->total_lines;
    if (total > max_lines_shown) total = max_lines_shown;

    /* Viewport rectangle */
    int vp_start = 0;
    int vp_h = visible_lines * line_h;
    if (tab->total_lines > 0) {
        vp_start = (tab->scroll_y * (max_lines_shown * line_h)) /
                   (tab->total_lines > 0 ? tab->total_lines : 1);
    }
    if (vp_start + vp_h > h) vp_h = h - vp_start;
    fb_fill_rect_alpha(x, y + vp_start, w, vp_h,
                       RGBA(137, 180, 250, 40));

    /* Draw code lines as colored dots */
    for (int i = 0; i < total; i++) {
        int ly = y + i * line_h;
        if (ly + line_h > y + h) break;
        const char *line = tab->lines[i];
        int llen = (int)kstrlen(line);
        int chars = llen;
        if (chars > w / 1) chars = w; /* 1px per char in minimap */
        for (int c = 0; c < chars && c < w - 2; c++) {
            if (line[c] == ' ' || line[c] == '\t') continue;
            color_t col = ed_syntax_color_at(line, c);
            /* Scale down: 1px per character */
            fb_putpixel(x + 2 + c, ly, col);
            fb_putpixel(x + 2 + c, ly + 1, col);
        }
    }
    (void)ed;
}

/* ---------- Status bar ---------- */

static void editor_render_status(editor_t *ed, editor_tab_t *tab,
                                 int x, int y, int w)
{
    fb_fill_rect(x, y, w, STATUS_BAR_H, COL_ACCENT);

    /* Left: filename + modified */
    char left[80];
    int li = 0;
    /* File icon placeholder: use a simple char */
    left[li++] = ' ';
    const char *fn = tab->filename;
    while (*fn && li < 60) left[li++] = *fn++;
    if (tab->modified) {
        left[li++] = ' ';
        left[li++] = '*';
    }
    left[li] = '\0';
    fb_puts(x + 4, y + 3, left, COL_BG_DARK, COL_ACCENT);

    /* Center: language */
    const char *lang = "Astracor";
    int lang_w = (int)kstrlen(lang) * FONT_W;
    fb_puts(x + (w - lang_w) / 2, y + 3, lang, COL_BG_DARK, COL_ACCENT);

    /* Right: Ln X, Col Y */
    char right[40];
    int ri = 0;
    const char *lnlbl = "Ln ";
    while (*lnlbl) right[ri++] = *lnlbl++;
    { char tmp[12]; ed_int_to_str(tab->cursor_line + 1, tmp, 12);
      const char *p = tmp; while (*p) right[ri++] = *p++; }
    right[ri++] = ',';
    right[ri++] = ' ';
    const char *collbl = "Col ";
    while (*collbl) right[ri++] = *collbl++;
    { char tmp[12]; ed_int_to_str(tab->cursor_col, tmp, 12);
      const char *p = tmp; while (*p) right[ri++] = *p++; }
    right[ri++] = ' ';
    right[ri] = '\0';
    int rw = ri * FONT_W;
    fb_puts(x + w - rw - 4, y + 3, right, COL_BG_DARK, COL_ACCENT);
    (void)ed;
}

/* ---------- Main render ---------- */

static void editor_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;
    editor_tab_t *tab = ed_tab(ed);
    if (!tab) return;

    /* Layout calculations */
    int sidebar_w = ed->sidebar_open ? ed->sidebar_width : 0;
    int status_h = STATUS_BAR_H;

    /* Total content area minus status bar */
    int content_h = h - status_h;

    /* Tab bar */
    int tab_y = y;
    int code_start_y = y + TAB_BAR_H;

    /* Code + output vertical split */
    int code_out_h = content_h - TAB_BAR_H;
    int out_h = (code_out_h * OUTPUT_RATIO) / 100;
    int code_h = code_out_h - out_h;

    /* Horizontal split */
    int code_x = x + sidebar_w;
    int code_w = w - sidebar_w - MINIMAP_W;
    if (code_w < 80) code_w = 80;

    int minimap_x = x + w - MINIMAP_W;

    /* 1) Background fill */
    fb_fill_rect(x, y, w, h, COL_BG_DARK);

    /* 2) Sidebar */
    if (ed->sidebar_open) {
        editor_render_sidebar(ed, x, y, sidebar_w, content_h);
    }

    /* 3) Tab bar */
    editor_render_tabs(ed, code_x, tab_y, code_w + MINIMAP_W);

    /* 4) Gutter + code area */
    int gutter_x = code_x;
    int text_x = code_x + GUTTER_W;
    int text_w = code_w - GUTTER_W;

    /* Gutter background */
    fb_fill_rect(gutter_x, code_start_y, GUTTER_W, code_h, RGB(35, 35, 55));

    /* Code area background */
    fb_fill_rect(text_x, code_start_y, text_w, code_h, COL_BG_DARK);

    /* Draw code lines */
    int visible = code_h / FONT_H;
    for (int i = 0; i < visible && (tab->scroll_y + i) < tab->total_lines;
         i++) {
        int line_num = tab->scroll_y + i;
        int ly = code_start_y + i * FONT_H;
        bool is_current = (line_num == tab->cursor_line);

        /* Line number (right-aligned in gutter) */
        char lnum[8];
        ed_int_to_str(line_num + 1, lnum, 8);
        int nlen = (int)kstrlen(lnum);
        color_t ln_col = is_current ? COL_TEXT : COL_TEXT_DIM;
        fb_puts(gutter_x + GUTTER_W - nlen * FONT_W - 8, ly, lnum,
                ln_col, RGB(35, 35, 55));

        /* Current line highlight */
        color_t line_bg = COL_BG_DARK;
        if (is_current) {
            line_bg = RGB(40, 42, 64);
            fb_fill_rect(text_x, ly, text_w, FONT_H, line_bg);
            /* Also highlight gutter for current line */
            fb_fill_rect(gutter_x, ly, GUTTER_W, FONT_H, RGB(40, 42, 58));
            /* Re-draw line number on highlighted gutter */
            fb_puts(gutter_x + GUTTER_W - nlen * FONT_W - 8, ly, lnum,
                    COL_ACCENT, RGB(40, 42, 58));
        }

        /* Syntax-highlighted line */
        editor_render_line(text_x + 4, ly, tab->lines[line_num],
                           text_w - 8, line_bg);
    }

    /* Cursor */
    {
        int cy = code_start_y + (tab->cursor_line - tab->scroll_y) * FONT_H;
        int cx = text_x + 4 + tab->cursor_col * FONT_W;
        if (cy >= code_start_y && cy < code_start_y + code_h)
            fb_fill_rect(cx, cy, 2, FONT_H, COL_ACCENT);
    }

    /* 5) Minimap */
    editor_render_minimap(ed, tab, minimap_x, code_start_y,
                          MINIMAP_W, code_h, visible);

    /* 6) Separator + Run button */
    int sep_y = code_start_y + code_h;
    fb_fill_rect(code_x, sep_y, code_w + MINIMAP_W, 2, COL_BORDER);

    /* Run button */
    int run_btn_w = 64;
    int run_btn_h = 20;
    int run_btn_y = sep_y + 2;
    fb_fill_rect(code_x + 4, run_btn_y, run_btn_w, run_btn_h, COL_ACCENT2);
    fb_puts(code_x + 12, run_btn_y + 2, "Run F5", COL_BG_DARK, COL_ACCENT2);

    /* Sidebar toggle button */
    int side_btn_x = code_x + run_btn_w + 12;
    fb_fill_rect(side_btn_x, run_btn_y, 24, run_btn_h, COL_SURFACE);
    fb_putchar(side_btn_x + 8, run_btn_y + 2,
               ed->sidebar_open ? '<' : '>', COL_TEXT, COL_SURFACE);

    /* Output header */
    fb_fill_rect(code_x + run_btn_w + 44, run_btn_y,
                 code_w + MINIMAP_W - run_btn_w - 48, run_btn_h, RGB(30, 30, 46));
    fb_puts(code_x + run_btn_w + 52, run_btn_y + 2,
            "OUTPUT", COL_TEXT_DIM, RGB(30, 30, 46));

    /* 7) Output panel */
    int out_y = run_btn_y + run_btn_h;
    int out_panel_h = out_h - run_btn_h - 2;
    fb_fill_rect(code_x, out_y, code_w + MINIMAP_W, out_panel_h,
                 RGB(25, 25, 40));

    /* Draw output text */
    int oy = out_y + 4;
    const char *p = ed->output;
    int out_end = out_y + out_panel_h - 4;
    while (*p && oy < out_end) {
        const char *nl = p;
        while (*nl && *nl != '\n') nl++;
        int llen = (int)(nl - p);
        int max_chars = (code_w + MINIMAP_W - 8) / FONT_W;
        if (llen > max_chars) llen = max_chars;
        for (int i = 0; i < llen; i++)
            fb_putchar(code_x + 4 + i * FONT_W, oy, p[i],
                       COL_GREEN, RGB(25, 25, 40));
        oy += FONT_H;
        p = *nl ? nl + 1 : nl;
    }

    /* 8) Status bar at very bottom */
    editor_render_status(ed, tab, x, y + h - status_h, w);
}

/* ---------- Run program ---------- */

static void editor_run(editor_t *ed)
{
    editor_tab_t *tab = ed_tab(ed);
    if (!tab) return;

    ed->output_len = 0;
    ed->output[0] = '\0';

    int total_len = 0;
    for (int i = 0; i < tab->total_lines; i++)
        total_len += (int)kstrlen(tab->lines[i]) + 1;

    char *source = (char *)kmalloc((uint32_t)(total_len + 1));
    int pos = 0;
    for (int i = 0; i < tab->total_lines; i++) {
        int len = (int)kstrlen(tab->lines[i]);
        kmemcpy(source + pos, tab->lines[i], (uint32_t)len);
        pos += len;
        source[pos++] = '\n';
    }
    source[pos] = '\0';

    if (ed->interp) ac_destroy(ed->interp);
    ed->interp = ac_create();
    ac_set_output(ed->interp, editor_output, ed);

    int result = ac_exec(ed->interp, source);
    if (result < 0) {
        const char *err = ac_get_error(ed->interp);
        if (err) editor_output(err, ed);
    }

    kfree(source);
}

/* ---------- Tab management ---------- */

static int editor_find_tab(editor_t *ed, const char *filename)
{
    for (int i = 0; i < ed->tab_count; i++) {
        if (!ed->tabs[i].active) continue;
        /* Compare filenames */
        const char *a = ed->tabs[i].filename;
        const char *b = filename;
        bool match = true;
        while (*a && *b) {
            if (*a != *b) { match = false; break; }
            a++; b++;
        }
        if (match && *a == '\0' && *b == '\0') return i;
    }
    return -1;
}

static int editor_open_tab(editor_t *ed, const char *filename)
{
    /* Check if already open */
    int existing = editor_find_tab(ed, filename);
    if (existing >= 0) {
        ed->active_tab = existing;
        return existing;
    }

    /* Find a free slot */
    if (ed->tab_count >= EDITOR_MAX_TABS) return -1;

    int idx = ed->tab_count;
    ed->tab_count++;
    editor_tab_t *tab = &ed->tabs[idx];
    kmemset(tab, 0, sizeof(editor_tab_t));
    tab->active = true;
    tab->total_lines = 1;
    kstrncpy(tab->filename, filename, 63);

    /* Try to load from VFS */
    vfs_node_t *node = vfs_lookup(filename);
    if (!node) {
        /* Try with leading / */
        char path[128];
        path[0] = '/';
        kstrncpy(path + 1, filename, 126);
        node = vfs_lookup(path);
    }
    if (node && node->size > 0 && node->read) {
        uint32_t sz = node->size;
        if (sz > (EDITOR_MAX_LINES * EDITOR_LINE_LEN) / 2)
            sz = (EDITOR_MAX_LINES * EDITOR_LINE_LEN) / 2;
        uint8_t *buf = (uint8_t *)kmalloc(sz + 1);
        uint32_t rd = vfs_read(node, 0, sz, buf);
        buf[rd] = '\0';

        /* Parse into lines */
        tab->total_lines = 0;
        uint32_t bi = 0;
        int col = 0;
        while (bi < rd && tab->total_lines < EDITOR_MAX_LINES) {
            if (buf[bi] == '\n') {
                tab->lines[tab->total_lines][col] = '\0';
                tab->total_lines++;
                col = 0;
            } else if (col < EDITOR_LINE_LEN - 1) {
                tab->lines[tab->total_lines][col++] = (char)buf[bi];
            }
            bi++;
        }
        if (col > 0 || tab->total_lines == 0) {
            tab->lines[tab->total_lines][col] = '\0';
            tab->total_lines++;
        }
        kfree(buf);
    }

    ed->active_tab = idx;
    return idx;
}

/* ---------- Insert char helper ---------- */

static void ed_insert_char(editor_tab_t *tab, char c)
{
    char *line = tab->lines[tab->cursor_line];
    int len = (int)kstrlen(line);
    if (len < EDITOR_LINE_LEN - 2) {
        for (int i = len + 1; i > tab->cursor_col; i--)
            line[i] = line[i - 1];
        line[tab->cursor_col] = c;
        tab->cursor_col++;
    }
}

/* Count leading spaces of a line */
static int ed_leading_spaces(const char *line)
{
    int n = 0;
    while (line[n] == ' ') n++;
    return n;
}

/* ---------- Key handler ---------- */

static void editor_key(int win_id, int key)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;
    editor_tab_t *tab = ed_tab(ed);
    if (!tab) return;

    if (key == '\n') {
        /* Insert new line with auto-indent */
        if (tab->total_lines < EDITOR_MAX_LINES - 1) {
            /* Get current line info before split */
            char *cur_line = tab->lines[tab->cursor_line];
            int indent = ed_leading_spaces(cur_line);
            bool after_brace = (tab->cursor_col > 0 &&
                                cur_line[tab->cursor_col - 1] == '{');

            /* Shift lines down */
            for (int i = tab->total_lines; i > tab->cursor_line + 1; i--)
                kmemcpy(tab->lines[i], tab->lines[i - 1], EDITOR_LINE_LEN);
            tab->total_lines++;
            tab->cursor_line++;

            /* Split current line at cursor */
            char *prev = tab->lines[tab->cursor_line - 1];
            char *next = tab->lines[tab->cursor_line];
            kmemcpy(next, prev + tab->cursor_col,
                    (uint32_t)(kstrlen(prev + tab->cursor_col) + 1));
            prev[tab->cursor_col] = '\0';

            /* Auto-indent: preserve indentation */
            if (after_brace) indent += 4;
            if (indent > 0 && indent < EDITOR_LINE_LEN - 2) {
                /* Shift existing content of new line right */
                int nlen = (int)kstrlen(next);
                for (int i = nlen; i >= 0; i--)
                    next[i + indent] = next[i];
                for (int i = 0; i < indent; i++)
                    next[i] = ' ';
            }
            tab->cursor_col = indent;
        }
    } else if (key == '\b') {
        if (tab->cursor_col > 0) {
            char *line = tab->lines[tab->cursor_line];
            int len = (int)kstrlen(line);
            for (int i = tab->cursor_col - 1; i < len; i++)
                line[i] = line[i + 1];
            tab->cursor_col--;
        } else if (tab->cursor_line > 0) {
            /* Join with previous line */
            char *prev = tab->lines[tab->cursor_line - 1];
            char *cur = tab->lines[tab->cursor_line];
            int plen = (int)kstrlen(prev);
            tab->cursor_col = plen;
            int clen = (int)kstrlen(cur);
            if (plen + clen < EDITOR_LINE_LEN - 1)
                kmemcpy(prev + plen, cur, (uint32_t)(clen + 1));
            /* Shift lines up */
            for (int i = tab->cursor_line; i < tab->total_lines - 1; i++)
                kmemcpy(tab->lines[i], tab->lines[i + 1], EDITOR_LINE_LEN);
            tab->total_lines--;
            tab->cursor_line--;
        }
    } else if (key == '\t') {
        /* Insert 4 spaces */
        char *line = tab->lines[tab->cursor_line];
        int len = (int)kstrlen(line);
        if (len + 4 < EDITOR_LINE_LEN - 1) {
            for (int i = len + 4; i >= tab->cursor_col + 4; i--)
                line[i] = line[i - 4];
            for (int i = 0; i < 4; i++)
                line[tab->cursor_col + i] = ' ';
            tab->cursor_col += 4;
        }
    } else if (key == EKEY_UP) {
        if (tab->cursor_line > 0) {
            tab->cursor_line--;
            int len = (int)kstrlen(tab->lines[tab->cursor_line]);
            if (tab->cursor_col > len) tab->cursor_col = len;
        }
    } else if (key == EKEY_DOWN) {
        if (tab->cursor_line < tab->total_lines - 1) {
            tab->cursor_line++;
            int len = (int)kstrlen(tab->lines[tab->cursor_line]);
            if (tab->cursor_col > len) tab->cursor_col = len;
        }
    } else if (key == EKEY_LEFT) {
        if (tab->cursor_col > 0) tab->cursor_col--;
        else if (tab->cursor_line > 0) {
            tab->cursor_line--;
            tab->cursor_col = (int)kstrlen(tab->lines[tab->cursor_line]);
        }
    } else if (key == EKEY_RIGHT) {
        int len = (int)kstrlen(tab->lines[tab->cursor_line]);
        if (tab->cursor_col < len) tab->cursor_col++;
        else if (tab->cursor_line < tab->total_lines - 1) {
            tab->cursor_line++;
            tab->cursor_col = 0;
        }
    } else if (key == EKEY_HOME) {
        tab->cursor_col = 0;
    } else if (key == EKEY_END) {
        tab->cursor_col = (int)kstrlen(tab->lines[tab->cursor_line]);
    } else if (key == EKEY_PGUP) {
        tab->cursor_line -= 20;
        if (tab->cursor_line < 0) tab->cursor_line = 0;
        int len = (int)kstrlen(tab->lines[tab->cursor_line]);
        if (tab->cursor_col > len) tab->cursor_col = len;
    } else if (key == EKEY_PGDN) {
        tab->cursor_line += 20;
        if (tab->cursor_line >= tab->total_lines)
            tab->cursor_line = tab->total_lines - 1;
        int len = (int)kstrlen(tab->lines[tab->cursor_line]);
        if (tab->cursor_col > len) tab->cursor_col = len;
    } else if (key == EKEY_DEL) {
        char *line = tab->lines[tab->cursor_line];
        int len = (int)kstrlen(line);
        if (tab->cursor_col < len) {
            for (int i = tab->cursor_col; i < len; i++)
                line[i] = line[i + 1];
        } else if (tab->cursor_line < tab->total_lines - 1) {
            /* Join with next line */
            char *next = tab->lines[tab->cursor_line + 1];
            int nlen = (int)kstrlen(next);
            if (len + nlen < EDITOR_LINE_LEN - 1)
                kmemcpy(line + len, next, (uint32_t)(nlen + 1));
            for (int i = tab->cursor_line + 1; i < tab->total_lines - 1; i++)
                kmemcpy(tab->lines[i], tab->lines[i + 1], EDITOR_LINE_LEN);
            tab->total_lines--;
        }
    } else if (key == EKEY_F5) {
        editor_run(ed);
    } else if (key >= 32 && key < 127) {
        /* Bracket auto-close */
        char close = 0;
        if (key == '{') close = '}';
        else if (key == '(') close = ')';
        else if (key == '[') close = ']';

        ed_insert_char(tab, (char)key);
        if (close) ed_insert_char(tab, close);
        /* Move cursor back before the closing bracket */
        if (close) tab->cursor_col--;
    }

    /* Keep cursor visible with scroll */
    {
        int cx_unused, cy_unused, cw_unused, ch;
        gui_content_rect(win_id, &cx_unused, &cy_unused, &cw_unused, &ch);
        (void)cx_unused; (void)cy_unused; (void)cw_unused;
        int code_h = ch - STATUS_BAR_H - TAB_BAR_H;
        int out_area = (code_h * OUTPUT_RATIO) / 100;
        int visible_lines = (code_h - out_area) / FONT_H;
        if (visible_lines < 5) visible_lines = 5;

        if (tab->cursor_line < tab->scroll_y)
            tab->scroll_y = tab->cursor_line;
        if (tab->cursor_line >= tab->scroll_y + visible_lines)
            tab->scroll_y = tab->cursor_line - visible_lines + 1;
    }

    tab->modified = true;
    gui_invalidate(win_id);
}

/* ---------- Click handler ---------- */

static void editor_click(int win_id, int mx, int my, int button)
{
    (void)button;
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;
    editor_tab_t *tab = ed_tab(ed);
    if (!tab) return;

    int cx_r, cy_r, cw, ch;
    gui_content_rect(win_id, &cx_r, &cy_r, &cw, &ch);
    (void)cx_r; (void)cy_r;

    int sidebar_w = ed->sidebar_open ? ed->sidebar_width : 0;
    int status_h = STATUS_BAR_H;
    int content_h = ch - status_h;
    int code_out_h = content_h - TAB_BAR_H;
    int out_h = (code_out_h * OUTPUT_RATIO) / 100;
    int code_h = code_out_h - out_h;
    int code_w = cw - sidebar_w - MINIMAP_W;
    if (code_w < 80) code_w = 80;

    /* Sidebar click: open file in new tab */
    if (ed->sidebar_open && mx < sidebar_w) {
        /* Calculate which row was clicked */
        int row_h = FONT_H + 2;
        int click_row = (my - 28) / row_h;
        if (click_row < 0) return;

        /* Walk VFS tree to find clicked entry */
        vfs_node_t *root = vfs_root();
        if (!root) return;
        int row = 0;
        for (uint32_t i = 0; i < root->child_count; i++) {
            vfs_node_t *child = root->children[i];
            if (!child) continue;
            if (child->type & VFS_DIRECTORY) {
                if (row == click_row) {
                    gui_invalidate(win_id);
                    return; /* Clicked directory name, ignore */
                }
                row++;
                for (uint32_t j = 0; j < child->child_count; j++) {
                    vfs_node_t *sub = child->children[j];
                    if (!sub) continue;
                    if (row == click_row && !(sub->type & VFS_DIRECTORY)) {
                        editor_open_tab(ed, sub->name);
                        gui_invalidate(win_id);
                        return;
                    }
                    row++;
                }
            } else {
                if (row == click_row) {
                    editor_open_tab(ed, child->name);
                    gui_invalidate(win_id);
                    return;
                }
                row++;
            }
        }
        gui_invalidate(win_id);
        return;
    }

    /* Tab bar click */
    if (my < TAB_BAR_H) {
        int tx = sidebar_w;
        for (int i = 0; i < ed->tab_count; i++) {
            if (!ed->tabs[i].active) continue;
            int nlen = (int)kstrlen(ed->tabs[i].filename);
            int tab_w = (nlen + 3) * FONT_W + 8;
            if (tab_w < 80) tab_w = 80;
            if (mx >= tx && mx < tx + tab_w) {
                ed->active_tab = i;
                gui_invalidate(win_id);
                return;
            }
            tx += tab_w;
        }
        return;
    }

    /* Minimap click: scroll to position */
    int minimap_x_start = sidebar_w + code_w;
    if (mx >= minimap_x_start) {
        int click_in_map = my - TAB_BAR_H;
        int map_h = code_h;
        if (map_h > 0 && tab->total_lines > 0) {
            int target_line = (click_in_map * tab->total_lines) / map_h;
            if (target_line < 0) target_line = 0;
            if (target_line >= tab->total_lines)
                target_line = tab->total_lines - 1;
            tab->scroll_y = target_line;
            tab->cursor_line = target_line;
            tab->cursor_col = 0;
        }
        gui_invalidate(win_id);
        return;
    }

    /* Run button area */
    int sep_y_rel = TAB_BAR_H + code_h;
    int run_btn_y = sep_y_rel + 2;
    if (my >= run_btn_y && my < run_btn_y + 20) {
        int btn_x = sidebar_w + 4;
        if (mx >= btn_x && mx < btn_x + 64) {
            editor_run(ed);
            gui_invalidate(win_id);
            return;
        }
        /* Sidebar toggle button */
        int side_btn_x = sidebar_w + 64 + 12;
        if (mx >= side_btn_x && mx < side_btn_x + 24) {
            ed->sidebar_open = !ed->sidebar_open;
            gui_invalidate(win_id);
            return;
        }
    }

    /* Click in code area */
    if (my >= TAB_BAR_H && my < TAB_BAR_H + code_h) {
        int rel_y = my - TAB_BAR_H;
        int rel_x = mx - sidebar_w;
        int line = tab->scroll_y + rel_y / FONT_H;
        int col = (rel_x - GUTTER_W - 4) / FONT_W;
        if (col < 0) col = 0;
        if (line >= 0 && line < tab->total_lines) {
            tab->cursor_line = line;
            int len = (int)kstrlen(tab->lines[line]);
            tab->cursor_col = col > len ? len : col;
            gui_invalidate(win_id);
        }
    }
}

/* ---------- Create editor ---------- */

int app_editor_create(int x, int y, int w, int h, const char *filename)
{
    int wid = gui_create_window("Code Editor", x, y, w, h, WIN_DEFAULT);
    if (wid < 0) return -1;

    editor_t *ed = (editor_t *)kmalloc(sizeof(editor_t));
    kmemset(ed, 0, sizeof(editor_t));

    /* Initialize sidebar */
    ed->sidebar_open = true;
    ed->sidebar_width = SIDEBAR_W_DEFAULT;

    /* Create initial tab */
    ed->tab_count = 1;
    ed->active_tab = 0;
    editor_tab_t *tab = &ed->tabs[0];
    tab->active = true;
    tab->total_lines = 1;
    kstrncpy(tab->filename, filename ? filename : "untitled.ac", 63);

    /* Default sample code */
    kstrncpy(tab->lines[0], "# Welcome to Astracor!", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[1], "# Press Run (or click Run button) to execute",
             EDITOR_LINE_LEN);
    kstrncpy(tab->lines[2], "", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[3], "fn greet(name) {", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[4], "    print(\"Hello, \" + name + \"!\")",
             EDITOR_LINE_LEN);
    kstrncpy(tab->lines[5], "}", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[6], "", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[7], "greet(\"World\")", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[8], "greet(\"AstraOS\")", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[9], "", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[10], "# Math", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[11], "let x = 10", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[12], "let y = 20", EDITOR_LINE_LEN);
    kstrncpy(tab->lines[13], "print(\"x + y = \" + str(x + y))",
             EDITOR_LINE_LEN);
    tab->total_lines = 14;

    gui_set_render(wid, editor_render);
    gui_set_key_handler(wid, editor_key);
    gui_set_click_handler(wid, editor_click);
    gui_set_userdata(wid, ed);
    return wid;
}

/* ========================================================================
 * File Manager Application
 * ======================================================================== */

#define FM_PATH_MAX     256
#define FM_ENTRY_H      24
#define FM_TOOLBAR_H    30
#define FM_PATHBAR_H    24
#define FM_ICON_W       16

typedef struct {
    vfs_node_t *current_dir;
    char        path[FM_PATH_MAX];
    int         scroll;
    int         selected;
} filemgr_t;

static bool fm_str_ends_with(const char *s, const char *suffix)
{
    int slen = (int)kstrlen(s);
    int xlen = (int)kstrlen(suffix);
    if (xlen > slen) return false;
    for (int i = 0; i < xlen; i++) {
        if (s[slen - xlen + i] != suffix[i]) return false;
    }
    return true;
}

static void fm_navigate_path(filemgr_t *fm, vfs_node_t *dir, const char *parent_path)
{
    if (!dir || !(dir->type & VFS_DIRECTORY)) return;
    fm->current_dir = dir;
    fm->scroll = 0;
    fm->selected = -1;

    kmemset(fm->path, 0, FM_PATH_MAX);
    int plen = (int)kstrlen(parent_path);

    /* Copy parent path */
    kstrncpy(fm->path, parent_path, FM_PATH_MAX - 1);

    /* Append / if needed */
    if (plen > 0 && fm->path[plen - 1] != '/') {
        if (plen < FM_PATH_MAX - 1) {
            fm->path[plen] = '/';
            plen++;
        }
    }

    /* Append dir name (unless root) */
    if (dir != vfs_root() && plen < FM_PATH_MAX - 1) {
        kstrncpy(fm->path + plen, dir->name, (size_t)(FM_PATH_MAX - plen - 1));
    }
}

static void fm_draw_icon_folder(int x, int y)
{
    const astra_theme_t *th = theme_current();
    fb_fill_rect(x, y + 4, 14, 10, th->yellow);
    fb_fill_rect(x, y + 2, 8, 4, th->yellow);
}

static void fm_draw_icon_file(int x, int y)
{
    const astra_theme_t *th = theme_current();
    fb_fill_rect(x + 2, y + 2, 10, 13, th->text_dim);
    fb_fill_rect(x + 3, y + 3, 8, 11, th->surface);
}

static void fm_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    filemgr_t *fm = (filemgr_t *)win->userdata;
    if (!fm || !fm->current_dir) return;

    const astra_theme_t *th = theme_current();

    /* Transparent background */
    fb_fill_rect_alpha(x, y, w, h,
                      RGBA(COL_R(th->bg_dark), COL_G(th->bg_dark), COL_B(th->bg_dark), 200));

    /* Toolbar area */
    fb_fill_rect_alpha(x, y, w, FM_TOOLBAR_H,
                      RGBA(COL_R(th->surface), COL_G(th->surface), COL_B(th->surface), 210));
    fb_fill_rect_alpha(x, y + FM_TOOLBAR_H - 1, w, 1,
                      RGBA(COL_R(th->border), COL_G(th->border), COL_B(th->border), 100));

    /* "New File" button */
    fb_fill_rounded_rect(x + 4, y + 4, 72, 22, 4, th->accent);
    fb_puts(x + 10, y + 7, "New File", th->bg_dark, th->accent);

    /* "Open" button */
    fb_fill_rounded_rect(x + 82, y + 4, 52, 22, 4, th->accent2);
    fb_puts(x + 92, y + 7, "Open", th->bg_dark, th->accent2);

    /* Path bar */
    int path_y = y + FM_TOOLBAR_H;
    fb_fill_rect_alpha(x, path_y, w, FM_PATHBAR_H,
                      RGBA(COL_R(th->surface), COL_G(th->surface), COL_B(th->surface), 180));
    fb_fill_rect_alpha(x, path_y + FM_PATHBAR_H - 1, w, 1,
                      RGBA(COL_R(th->border), COL_G(th->border), COL_B(th->border), 80));
    fb_puts_transparent(x + 8, path_y + 4, fm->path, th->text);

    /* File listing area */
    int list_y = path_y + FM_PATHBAR_H;
    int list_h = h - FM_TOOLBAR_H - FM_PATHBAR_H;
    int visible = list_h / FM_ENTRY_H;

    vfs_node_t *dir = fm->current_dir;

    /* ".." entry if not root */
    int row = 0;
    if (dir != vfs_root() && row >= fm->scroll && (row - fm->scroll) < visible) {
        int ey = list_y + (row - fm->scroll) * FM_ENTRY_H;
        if (fm->selected == row)
            fb_fill_rect_alpha(x, ey, w, FM_ENTRY_H,
                              RGBA(COL_R(th->accent), COL_G(th->accent), COL_B(th->accent), 30));
        fm_draw_icon_folder(x + 8, ey + 2);
        fb_puts_transparent(x + 8 + FM_ICON_W + 8, ey + 4, "..", th->text);
    }
    row++;

    /* Directory children */
    for (uint32_t i = 0; i < dir->child_count; i++) {
        vfs_node_t *child = dir->children[i];
        if (!child) continue;

        if (row >= fm->scroll && (row - fm->scroll) < visible) {
            int ey = list_y + (row - fm->scroll) * FM_ENTRY_H;

            /* Highlight selected */
            if (fm->selected == row)
                fb_fill_rect_alpha(x, ey, w, FM_ENTRY_H,
                                  RGBA(COL_R(th->accent), COL_G(th->accent), COL_B(th->accent), 30));

            /* Icon */
            if (child->type & VFS_DIRECTORY)
                fm_draw_icon_folder(x + 8, ey + 2);
            else
                fm_draw_icon_file(x + 8, ey + 2);

            /* Name */
            color_t name_col = (child->type & VFS_DIRECTORY) ? th->yellow : th->text;
            fb_puts_transparent(x + 8 + FM_ICON_W + 8, ey + 4, child->name, name_col);

            /* Show type indicator for directories */
            if (child->type & VFS_DIRECTORY) {
                int nlen = (int)kstrlen(child->name);
                fb_puts_transparent(x + 8 + FM_ICON_W + 8 + nlen * FONT_W + 4,
                                   ey + 4, "/", th->text_dim);
            }
        }
        row++;
    }
}

static void fm_click(int win_id, int mx, int my, int button)
{
    (void)button;
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    filemgr_t *fm = (filemgr_t *)win->userdata;
    if (!fm || !fm->current_dir) return;

    /* Toolbar clicks */
    if (my < FM_TOOLBAR_H) {
        if (mx >= 4 && mx < 76 && my >= 4 && my < 26) {
            /* "New File" button — create a new file in current dir */
            vfs_node_t *nf = vfs_create_node("new_file.ac", VFS_FILE);
            if (nf) vfs_add_child(fm->current_dir, nf);
            gui_invalidate(win_id);
            return;
        }
        if (mx >= 82 && mx < 134 && my >= 4 && my < 26) {
            /* "Open" button — open selected item */
            if (fm->selected >= 0) {
                int idx = fm->selected;
                vfs_node_t *dir = fm->current_dir;
                int entry = 0;

                /* Account for ".." entry */
                if (dir != vfs_root()) {
                    if (idx == 0) {
                        /* Navigate up: look up parent via path */
                        vfs_node_t *parent = vfs_root();
                        /* Find parent by re-resolving path minus last component */
                        char ppath[FM_PATH_MAX];
                        kstrncpy(ppath, fm->path, FM_PATH_MAX - 1);
                        int plen = (int)kstrlen(ppath);
                        /* Remove trailing slash */
                        if (plen > 1 && ppath[plen - 1] == '/')
                            ppath[--plen] = '\0';
                        /* Find last slash */
                        int last = 0;
                        for (int i = 0; i < plen; i++)
                            if (ppath[i] == '/') last = i;
                        if (last == 0)
                            ppath[1] = '\0';
                        else
                            ppath[last] = '\0';
                        vfs_node_t *resolved = vfs_lookup(ppath);
                        if (resolved && (resolved->type & VFS_DIRECTORY))
                            parent = resolved;
                        fm_navigate_path(fm, parent, ppath);
                        gui_invalidate(win_id);
                        return;
                    }
                    idx--; /* Adjust for ".." entry */
                    entry = 1;
                }
                (void)entry;

                if ((uint32_t)idx < dir->child_count) {
                    vfs_node_t *child = dir->children[idx];
                    if (child && (child->type & VFS_DIRECTORY)) {
                        fm_navigate_path(fm, child, fm->path);
                    } else if (child && fm_str_ends_with(child->name, ".ac")) {
                        /* Open .ac file in editor */
                        app_editor_create(150, 80, 640, 480, child->name);
                    }
                }
                gui_invalidate(win_id);
                return;
            }
        }
        return;
    }

    /* Path bar — ignore clicks */
    if (my < FM_TOOLBAR_H + FM_PATHBAR_H) return;

    /* File list area */
    int list_y_off = my - FM_TOOLBAR_H - FM_PATHBAR_H;
    int clicked_row = fm->scroll + list_y_off / FM_ENTRY_H;
    vfs_node_t *dir = fm->current_dir;

    /* Total entries: ".." (if not root) + child_count */
    int total = (int)dir->child_count;
    if (dir != vfs_root()) total++;

    if (clicked_row >= 0 && clicked_row < total) {
        if (fm->selected == clicked_row) {
            /* Double-click simulation: second click on same item opens it */
            int idx = clicked_row;
            if (dir != vfs_root()) {
                if (idx == 0) {
                    /* Go up */
                    vfs_node_t *parent = vfs_root();
                    char ppath[FM_PATH_MAX];
                    kstrncpy(ppath, fm->path, FM_PATH_MAX - 1);
                    int plen = (int)kstrlen(ppath);
                    if (plen > 1 && ppath[plen - 1] == '/')
                        ppath[--plen] = '\0';
                    int last = 0;
                    for (int i = 0; i < plen; i++)
                        if (ppath[i] == '/') last = i;
                    if (last == 0)
                        ppath[1] = '\0';
                    else
                        ppath[last] = '\0';
                    vfs_node_t *resolved = vfs_lookup(ppath);
                    if (resolved && (resolved->type & VFS_DIRECTORY))
                        parent = resolved;
                    fm_navigate_path(fm, parent, ppath);
                    gui_invalidate(win_id);
                    return;
                }
                idx--;
            }
            if ((uint32_t)idx < dir->child_count) {
                vfs_node_t *child = dir->children[idx];
                if (child && (child->type & VFS_DIRECTORY)) {
                    fm_navigate_path(fm, child, fm->path);
                } else if (child && fm_str_ends_with(child->name, ".ac")) {
                    app_editor_create(150, 80, 640, 480, child->name);
                }
            }
        } else {
            fm->selected = clicked_row;
        }
    }
    gui_invalidate(win_id);
}

static void fm_key(int win_id, int key)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    filemgr_t *fm = (filemgr_t *)win->userdata;
    if (!fm || !fm->current_dir) return;

    vfs_node_t *dir = fm->current_dir;
    int total = (int)dir->child_count;
    if (dir != vfs_root()) total++;

    /* Arrow key handling (scan codes mapped by GUI layer) */
    if (key == 0x80) { /* Up */
        if (fm->selected > 0) fm->selected--;
    } else if (key == 0x81) { /* Down */
        if (fm->selected < total - 1) fm->selected++;
    }
    gui_invalidate(win_id);
}

int app_filemgr_create(int x, int y, int w, int h)
{
    int wid = gui_create_window("File Manager", x, y, w, h, WIN_DEFAULT);
    if (wid < 0) return -1;

    filemgr_t *fm = (filemgr_t *)kmalloc(sizeof(filemgr_t));
    kmemset(fm, 0, sizeof(filemgr_t));
    fm->current_dir = vfs_root();
    fm->selected = -1;
    fm->path[0] = '/';
    fm->path[1] = '\0';

    gui_set_render(wid, fm_render);
    gui_set_key_handler(wid, fm_key);
    gui_set_click_handler(wid, fm_click);
    gui_set_userdata(wid, fm);
    return wid;
}

/* ========================================================================
 * Settings / Theme Chooser Application
 * ======================================================================== */

#define SETTINGS_SIDEBAR_W  140

typedef struct {
    int  tab;           /* 0=Themes, 1=Wallpaper, 2=About */
    int  theme_scroll;
    int  theme_hover;
    int  wall_hover;
} settings_t;

static void settings_draw_theme_card(int x, int y, int w, int idx, bool selected, bool hover)
{
    const astra_theme_t *t = theme_get(idx);

    color_t card_bg = hover ? RGBA(COL_R(t->bg), COL_G(t->bg), COL_B(t->bg), 240)
                            : RGBA(COL_R(t->bg), COL_G(t->bg), COL_B(t->bg), 200);
    fb_fill_rounded_rect(x, y, w, 70, 6, card_bg);

    if (selected) {
        const astra_theme_t *cur = theme_current();
        fb_rounded_rect(x, y, w, 70, 6, cur->accent);
        fb_rounded_rect(x + 1, y + 1, w - 2, 68, 5, cur->accent);
    }

    /* Theme name */
    fb_puts_transparent(x + 8, y + 4, t->name, t->text);

    /* Color swatches */
    int sx = x + 8;
    int sy = y + 24;
    fb_fill_rect(sx, sy, 20, 14, t->bg);
    fb_fill_rect(sx + 22, sy, 20, 14, t->surface);
    fb_fill_rect(sx + 44, sy, 20, 14, t->accent);
    fb_fill_rect(sx + 66, sy, 20, 14, t->accent2);
    fb_fill_rect(sx + 88, sy, 20, 14, t->highlight);

    /* Syntax preview line */
    fb_puts_transparent(x + 8, y + 44, "fn", t->syn_keyword);
    fb_puts_transparent(x + 24, y + 44, " hello", t->syn_function);
    fb_puts_transparent(x + 72, y + 44, "()", t->syn_bracket);
    fb_puts_transparent(x + 88, y + 44, " \"hi\"", t->syn_string);
    fb_puts_transparent(x + 128, y + 44, " #ok", t->syn_comment);

    /* Alpha indicator */
    char alpha_str[8];
    int a = (int)t->window_alpha * 100 / 255;
    alpha_str[0] = '0' + (char)(a / 10);
    alpha_str[1] = '0' + (char)(a % 10);
    alpha_str[2] = '%';
    alpha_str[3] = '\0';
    fb_puts_transparent(x + w - 40, y + 4, alpha_str, t->text_dim);
}

static const char *wallpaper_names[] = {
    "Gradient", "Mesh", "Waves", "Geometric",
    "Particles", "Aurora", "Solid"
};

static void settings_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    settings_t *s = (settings_t *)win->userdata;
    if (!s) return;

    const astra_theme_t *t = theme_current();

    /* Semi-transparent background */
    fb_fill_rect_alpha(x, y, w, h, RGBA(COL_R(t->bg_dark), COL_G(t->bg_dark),
                                         COL_B(t->bg_dark), 220));

    /* Sidebar */
    fb_fill_rect_alpha(x, y, SETTINGS_SIDEBAR_W, h,
                      RGBA(COL_R(t->surface), COL_G(t->surface), COL_B(t->surface), 180));
    fb_fill_rect_alpha(x + SETTINGS_SIDEBAR_W - 1, y, 1, h,
                      RGBA(COL_R(t->border), COL_G(t->border), COL_B(t->border), 100));

    fb_puts_transparent(x + 12, y + 8, "Settings", t->text);
    fb_fill_rect_alpha(x + 8, y + 26, SETTINGS_SIDEBAR_W - 16, 1,
                      RGBA(COL_R(t->border), COL_G(t->border), COL_B(t->border), 80));

    /* Sidebar tabs */
    const char *tabs[] = { "Themes", "Wallpaper", "About" };
    for (int i = 0; i < 3; i++) {
        int ty = y + 36 + i * 28;
        if (s->tab == i) {
            fb_fill_rect_alpha(x + 4, ty, SETTINGS_SIDEBAR_W - 8, 24,
                              RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 40));
            fb_fill_rect(x, ty, 3, 24, t->accent);
        }
        fb_puts_transparent(x + 16, ty + 4, tabs[i],
                          s->tab == i ? t->accent : t->text_dim);
    }

    /* Content area */
    int cx = x + SETTINGS_SIDEBAR_W + 8;
    int cw = w - SETTINGS_SIDEBAR_W - 16;
    int content_y = y + 8;

    if (s->tab == 0) {
        fb_puts_transparent(cx, content_y, "Choose Theme", t->text);
        content_y += 24;

        int card_y = content_y;
        int tc = theme_count();
        int current_idx = 0;
        for (int i = 0; i < tc; i++)
            if (theme_get(i) == theme_current()) current_idx = i;

        for (int i = s->theme_scroll; i < tc && card_y + 78 < y + h; i++) {
            settings_draw_theme_card(cx, card_y, cw, i, (i == current_idx),
                                    (i == s->theme_hover));
            card_y += 78;
        }
    } else if (s->tab == 1) {
        fb_puts_transparent(cx, content_y, "Wallpaper Style", t->text);
        content_y += 24;

        wallpaper_type_t cur_wp = theme_get_wallpaper();
        for (int i = 0; i < (int)WALLPAPER_COUNT; i++) {
            int ey = content_y + i * 36;
            if (ey + 32 > y + h) break;

            bool active = ((int)cur_wp == i);
            bool hover = (s->wall_hover == i);
            color_t card = active
                ? RGBA(COL_R(t->accent), COL_G(t->accent), COL_B(t->accent), 50)
                : hover ? RGBA(COL_R(t->surface), COL_G(t->surface), COL_B(t->surface), 150)
                        : RGBA(COL_R(t->surface), COL_G(t->surface), COL_B(t->surface), 80);
            fb_fill_rounded_rect(cx, ey, cw, 30, 4, card);
            if (active) fb_fill_rect(cx, ey, 3, 30, t->accent);
            fb_puts_transparent(cx + 16, ey + 7, wallpaper_names[i],
                              active ? t->accent : t->text);
            if (active) fb_puts_transparent(cx + cw - 24, ey + 7, "*", t->accent);
        }
    } else {
        fb_puts_transparent(cx, content_y, "AstraOS 2.0", t->text);
        content_y += 20;
        fb_puts_transparent(cx, content_y, "AstraCore Kernel", t->text_dim);
        content_y += 24;
        fb_puts_transparent(cx, content_y, "Hyprland-inspired desktop", t->text_dim);
        content_y += 16;
        fb_puts_transparent(cx, content_y, "with glassmorphism theme", t->text_dim);
        content_y += 32;
        fb_puts_transparent(cx, content_y, "Theme Engine:", t->text);
        content_y += 16;
        char info[32];
        int ci = 0;
        const char *lbl = "  12 themes, 7 wallpapers";
        while (*lbl) info[ci++] = *lbl++;
        info[ci] = '\0';
        fb_puts_transparent(cx, content_y, info, t->accent);
        content_y += 32;
        fb_puts_transparent(cx, content_y, "Language: Astracor v1.0", t->text);
    }
}

static void settings_click(int win_id, int mx, int my, int button)
{
    (void)button;
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    settings_t *s = (settings_t *)win->userdata;
    if (!s) return;

    if (mx < SETTINGS_SIDEBAR_W) {
        for (int i = 0; i < 3; i++) {
            int ty = 36 + i * 28;
            if (my >= ty && my < ty + 28) {
                s->tab = i;
                gui_invalidate(win_id);
                return;
            }
        }
        return;
    }

    int content_y = 32;
    if (s->tab == 0) {
        int card_y = content_y;
        int tc = theme_count();
        for (int i = s->theme_scroll; i < tc; i++) {
            if (my >= card_y && my < card_y + 70) {
                theme_set(i);
                gui_redraw_all();
                gui_invalidate(win_id);
                return;
            }
            card_y += 78;
        }
    } else if (s->tab == 1) {
        for (int i = 0; i < (int)WALLPAPER_COUNT; i++) {
            int ey = content_y + i * 36;
            if (my >= ey && my < ey + 30) {
                theme_set_wallpaper((wallpaper_type_t)i);
                gui_redraw_all();
                gui_invalidate(win_id);
                return;
            }
        }
    }
}

static void settings_key(int win_id, int key)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    settings_t *s = (settings_t *)win->userdata;
    if (!s) return;

    if (key == '\t') {
        s->tab = (s->tab + 1) % 3;
    } else if (key == '[') {
        int tc = theme_count();
        int cur = 0;
        for (int i = 0; i < tc; i++)
            if (theme_get(i) == theme_current()) cur = i;
        if (cur > 0) theme_set(cur - 1);
        gui_redraw_all();
    } else if (key == ']') {
        int tc = theme_count();
        int cur = 0;
        for (int i = 0; i < tc; i++)
            if (theme_get(i) == theme_current()) cur = i;
        if (cur < tc - 1) theme_set(cur + 1);
        gui_redraw_all();
    }
    gui_invalidate(win_id);
}

int app_settings_create(int x, int y, int w, int h)
{
    int wid = gui_create_window("Settings", x, y, w, h, WIN_DEFAULT);
    if (wid < 0) return -1;

    settings_t *s = (settings_t *)kmalloc(sizeof(settings_t));
    kmemset(s, 0, sizeof(settings_t));
    s->theme_hover = -1;
    s->wall_hover = -1;

    gui_set_render(wid, settings_render);
    gui_set_key_handler(wid, settings_key);
    gui_set_click_handler(wid, settings_click);
    gui_set_userdata(wid, s);
    return wid;
}
