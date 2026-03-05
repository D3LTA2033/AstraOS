/* ==========================================================================
 * AstraOS - Built-in GUI Applications
 * ==========================================================================
 * Text Editor (with Astracor support) and Terminal for the desktop.
 * ========================================================================== */

#include <kernel/gui.h>
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

    fb_fill_rect(x, y, w, h, COL_BG_DARK);

    /* Draw output lines */
    int visible_lines = (h - FONT_H - 4) / FONT_H;
    int start = t->line_count - visible_lines;
    if (start < 0) start = 0;

    for (int i = start; i <= t->line_count && (i - start) < visible_lines; i++) {
        fb_puts(x + 4, y + (i - start) * FONT_H + 2,
                t->lines[i], COL_TEXT, COL_BG_DARK);
    }

    /* Draw input line at bottom */
    int iy = y + h - FONT_H - 4;
    fb_fill_rect(x, iy, w, FONT_H + 4, COL_SURFACE);
    fb_puts(x + 4, iy + 2, ">> ", COL_ACCENT, COL_SURFACE);
    fb_puts(x + 28, iy + 2, t->input, COL_TEXT, COL_SURFACE);

    /* Cursor */
    int cx = x + 28 + t->cursor * FONT_W;
    fb_fill_rect(cx, iy + 2, 2, FONT_H, COL_ACCENT);
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
 * Code Editor Application
 * ======================================================================== */

#define EDITOR_MAX_LINES 512
#define EDITOR_LINE_LEN  256

typedef struct {
    char lines[EDITOR_MAX_LINES][EDITOR_LINE_LEN];
    int  total_lines;
    int  cursor_line;
    int  cursor_col;
    int  scroll_y;
    char filename[64];
    bool modified;
    /* Output panel */
    char output[2048];
    int  output_len;
    ac_interp_t *interp;
} editor_t;

static void editor_output(const char *text, void *data)
{
    editor_t *ed = (editor_t *)data;
    while (*text && ed->output_len < 2040) {
        ed->output[ed->output_len++] = *text++;
    }
    ed->output[ed->output_len] = '\0';
}

static bool is_keyword(const char *word, int len)
{
    static const char *kws[] = {"fn","let","if","elif","else","while","for","in","return","true","false","null","break","continue","import"};
    for (int i = 0; i < 15; i++) {
        int klen = (int)kstrlen(kws[i]);
        if (len == klen) {
            bool match = true;
            for (int j = 0; j < len; j++) if (word[j] != kws[i][j]) { match = false; break; }
            if (match) return true;
        }
    }
    return false;
}

static bool is_builtin(const char *word, int len)
{
    static const char *bfs[] = {"print","len","str","int","type","range","abs","substr","indexof","split","join","upper","lower","trim","replace","startswith","endswith","char_at","contains","min","max","clamp","pow","push","pop","get","set"};
    for (int i = 0; i < 27; i++) {
        int blen = (int)kstrlen(bfs[i]);
        if (len == blen) {
            bool match = true;
            for (int j = 0; j < len; j++) if (word[j] != bfs[i][j]) { match = false; break; }
            if (match) return true;
        }
    }
    return false;
}

static void editor_render_line(int x, int y, const char *line, int w)
{
    int col = 0;
    int px = x;
    int max_col = w / FONT_W;

    while (line[col] && col < max_col) {
        color_t fg = COL_TEXT;

        /* Syntax highlighting */
        if (line[col] == '#') {
            /* Comment — rest of line */
            fb_puts(px, y, line + col, COL_OVERLAY, COL_BG_DARK);
            return;
        }
        if (line[col] == '"') {
            /* String literal */
            fb_putchar(px, y, '"', COL_GREEN, COL_BG_DARK);
            px += FONT_W; col++;
            while (line[col] && line[col] != '"' && col < max_col) {
                fb_putchar(px, y, line[col], COL_GREEN, COL_BG_DARK);
                px += FONT_W; col++;
            }
            if (line[col] == '"') {
                fb_putchar(px, y, '"', COL_GREEN, COL_BG_DARK);
                px += FONT_W; col++;
            }
            continue;
        }
        if (line[col] >= '0' && line[col] <= '9') {
            fg = COL_ORANGE;
        } else if ((line[col] >= 'a' && line[col] <= 'z') || (line[col] >= 'A' && line[col] <= 'Z') || line[col] == '_') {
            /* Check for keyword */
            int start = col;
            int end = col;
            while ((line[end] >= 'a' && line[end] <= 'z') || (line[end] >= 'A' && line[end] <= 'Z') || line[end] == '_' || (line[end] >= '0' && line[end] <= '9'))
                end++;
            int wlen = end - start;
            if (is_keyword(line + start, wlen)) {
                for (int i = start; i < end && i < max_col; i++) {
                    fb_putchar(px, y, line[i], COL_PURPLE, COL_BG_DARK);
                    px += FONT_W;
                }
                col = end;
                continue;
            }
            if (is_builtin(line + start, wlen)) {
                for (int i = start; i < end && i < max_col; i++) {
                    fb_putchar(px, y, line[i], COL_BLUE, COL_BG_DARK);
                    px += FONT_W;
                }
                col = end;
                continue;
            }
            fg = COL_TEXT;
        } else if (line[col] == '(' || line[col] == ')' || line[col] == '{' || line[col] == '}' || line[col] == '[' || line[col] == ']') {
            fg = COL_YELLOW;
        } else if (line[col] == '+' || line[col] == '-' || line[col] == '*' || line[col] == '/' || line[col] == '=' || line[col] == '<' || line[col] == '>' || line[col] == '!') {
            fg = COL_ACCENT;
        }

        fb_putchar(px, y, line[col], fg, COL_BG_DARK);
        px += FONT_W;
        col++;
    }
}

static void editor_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;

    /* Split: code area (top 70%) and output (bottom 30%) */
    int code_h = (h * 70) / 100;
    int out_h = h - code_h;
    int out_y = y + code_h;

    /* Code area background */
    fb_fill_rect(x, y, w, code_h, COL_BG_DARK);

    /* Line numbers gutter */
    int gutter_w = 40;
    fb_fill_rect(x, y, gutter_w, code_h, RGB(35,35,55));

    /* Draw code lines */
    int visible = code_h / FONT_H;
    for (int i = 0; i < visible && (ed->scroll_y + i) < ed->total_lines; i++) {
        int line_num = ed->scroll_y + i;
        int ly = y + i * FONT_H;

        /* Line number */
        char lnum[8];
        int n = line_num + 1, li = 0;
        if (n == 0) lnum[li++] = '0';
        else { char tmp[8]; int ti=0; while(n>0){tmp[ti++]='0'+(char)(n%10);n/=10;} for(int j=ti-1;j>=0;j--)lnum[li++]=tmp[j]; }
        lnum[li] = '\0';
        fb_puts(x + gutter_w - li * FONT_W - 4, ly, lnum,
                line_num == ed->cursor_line ? COL_TEXT : COL_TEXT_DIM,
                RGB(35,35,55));

        /* Highlight current line */
        if (line_num == ed->cursor_line)
            fb_fill_rect(x + gutter_w, ly, w - gutter_w, FONT_H, RGB(40,42,64));

        /* Syntax-highlighted line */
        editor_render_line(x + gutter_w + 4, ly, ed->lines[line_num], w - gutter_w - 8);
    }

    /* Cursor */
    int cy = y + (ed->cursor_line - ed->scroll_y) * FONT_H;
    int cx = x + gutter_w + 4 + ed->cursor_col * FONT_W;
    if (cy >= y && cy < y + code_h)
        fb_fill_rect(cx, cy, 2, FONT_H, COL_ACCENT);

    /* Separator + run button */
    fb_fill_rect(x, out_y - 2, w, 2, COL_BORDER);
    fb_fill_rect(x, out_y - 22, 60, 20, COL_ACCENT2);
    fb_puts(x + 8, out_y - 20, "Run F5", COL_BG_DARK, COL_ACCENT2);

    /* Status bar with file info */
    fb_fill_rect(x + 68, out_y - 22, w - 68, 20, COL_SURFACE);
    char status[128];
    int si = 0;
    const char *fn = ed->filename;
    while (*fn && si < 60) status[si++] = *fn++;
    if (ed->modified) { status[si++] = ' '; status[si++] = '*'; }
    status[si++] = ' '; status[si++] = '|'; status[si++] = ' ';
    const char *lbl = "Ln "; while (*lbl) status[si++] = *lbl++;
    { char tmp[8]; int ti=0; int v=ed->cursor_line+1; if(v==0)tmp[ti++]='0'; else while(v>0){tmp[ti++]='0'+(char)(v%10);v/=10;} for(int j=ti-1;j>=0;j--)status[si++]=tmp[j]; }
    status[si] = '\0';
    fb_puts(x + 72, out_y - 20, status, COL_TEXT_DIM, COL_SURFACE);

    /* Output panel */
    fb_fill_rect(x, out_y, w, out_h, RGB(25,25,40));
    fb_puts(x + 4, out_y + 2, "Output:", COL_TEXT_DIM, RGB(25,25,40));

    /* Draw output text */
    int oy = out_y + FONT_H + 4;
    const char *p = ed->output;
    while (*p && oy < out_y + out_h - 4) {
        const char *nl = p;
        while (*nl && *nl != '\n') nl++;
        int llen = (int)(nl - p);
        /* Draw this line */
        for (int i = 0; i < llen && i < (w / FONT_W) - 1; i++)
            fb_putchar(x + 4 + i * FONT_W, oy, p[i], COL_GREEN, RGB(25,25,40));
        oy += FONT_H;
        p = *nl ? nl + 1 : nl;
    }
}

static void editor_run(editor_t *ed)
{
    /* Collect all lines into a single source string */
    ed->output_len = 0;
    ed->output[0] = '\0';

    int total_len = 0;
    for (int i = 0; i < ed->total_lines; i++)
        total_len += (int)kstrlen(ed->lines[i]) + 1;

    char *source = (char *)kmalloc((uint32_t)(total_len + 1));
    int pos = 0;
    for (int i = 0; i < ed->total_lines; i++) {
        int len = (int)kstrlen(ed->lines[i]);
        kmemcpy(source + pos, ed->lines[i], (uint32_t)len);
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

static void editor_key(int win_id, int key)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;

    if (key == '\n') {
        /* Insert new line */
        if (ed->total_lines < EDITOR_MAX_LINES - 1) {
            /* Shift lines down */
            for (int i = ed->total_lines; i > ed->cursor_line + 1; i--)
                kmemcpy(ed->lines[i], ed->lines[i - 1], EDITOR_LINE_LEN);
            ed->total_lines++;
            ed->cursor_line++;
            /* Split current line at cursor */
            char *cur = ed->lines[ed->cursor_line - 1];
            char *next = ed->lines[ed->cursor_line];
            kmemcpy(next, cur + ed->cursor_col, (uint32_t)(kstrlen(cur + ed->cursor_col) + 1));
            cur[ed->cursor_col] = '\0';
            ed->cursor_col = 0;
        }
    } else if (key == '\b') {
        if (ed->cursor_col > 0) {
            char *line = ed->lines[ed->cursor_line];
            int len = (int)kstrlen(line);
            for (int i = ed->cursor_col - 1; i < len; i++)
                line[i] = line[i + 1];
            ed->cursor_col--;
        } else if (ed->cursor_line > 0) {
            /* Join with previous line */
            char *prev = ed->lines[ed->cursor_line - 1];
            char *cur = ed->lines[ed->cursor_line];
            int plen = (int)kstrlen(prev);
            ed->cursor_col = plen;
            int clen = (int)kstrlen(cur);
            if (plen + clen < EDITOR_LINE_LEN - 1)
                kmemcpy(prev + plen, cur, (uint32_t)(clen + 1));
            /* Shift lines up */
            for (int i = ed->cursor_line; i < ed->total_lines - 1; i++)
                kmemcpy(ed->lines[i], ed->lines[i + 1], EDITOR_LINE_LEN);
            ed->total_lines--;
            ed->cursor_line--;
        }
    } else if (key == '\t') {
        /* Insert 4 spaces */
        char *line = ed->lines[ed->cursor_line];
        int len = (int)kstrlen(line);
        if (len + 4 < EDITOR_LINE_LEN - 1) {
            for (int i = len + 4; i >= ed->cursor_col + 4; i--)
                line[i] = line[i - 4];
            for (int i = 0; i < 4; i++)
                line[ed->cursor_col + i] = ' ';
            ed->cursor_col += 4;
        }
    } else if (key >= 32 && key < 127) {
        char *line = ed->lines[ed->cursor_line];
        int len = (int)kstrlen(line);
        if (len < EDITOR_LINE_LEN - 2) {
            for (int i = len + 1; i > ed->cursor_col; i--)
                line[i] = line[i - 1];
            line[ed->cursor_col] = (char)key;
            ed->cursor_col++;
        }
    }

    /* Keep cursor visible */
    int visible = 20;
    if (ed->cursor_line < ed->scroll_y)
        ed->scroll_y = ed->cursor_line;
    if (ed->cursor_line >= ed->scroll_y + visible)
        ed->scroll_y = ed->cursor_line - visible + 1;

    ed->modified = true;
    gui_invalidate(win_id);
}

static void editor_click(int win_id, int mx, int my, int button)
{
    (void)button;
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    editor_t *ed = (editor_t *)win->userdata;
    if (!ed) return;

    int cx, cy, cw, ch;
    gui_content_rect(win_id, &cx, &cy, &cw, &ch);
    int code_h = (ch * 70) / 100;

    /* Check if click is on Run button */
    int out_y = code_h;
    if (my >= out_y - 22 && my < out_y - 2 && mx < 60) {
        editor_run(ed);
        gui_invalidate(win_id);
        return;
    }

    /* Click in code area */
    if (my < code_h) {
        int gutter_w = 40;
        int line = ed->scroll_y + my / FONT_H;
        int col = (mx - gutter_w - 4) / FONT_W;
        if (col < 0) col = 0;
        if (line >= 0 && line < ed->total_lines) {
            ed->cursor_line = line;
            int len = (int)kstrlen(ed->lines[line]);
            ed->cursor_col = col > len ? len : col;
            gui_invalidate(win_id);
        }
    }
}

int app_editor_create(int x, int y, int w, int h, const char *filename)
{
    int wid = gui_create_window("Code Editor", x, y, w, h, WIN_DEFAULT);
    if (wid < 0) return -1;

    editor_t *ed = (editor_t *)kmalloc(sizeof(editor_t));
    kmemset(ed, 0, sizeof(editor_t));
    ed->total_lines = 1;
    kstrncpy(ed->filename, filename ? filename : "untitled.ac", 63);

    /* Default sample code */
    kstrncpy(ed->lines[0], "# Welcome to Astracor!", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[1], "# Press Run (or click Run button) to execute", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[2], "", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[3], "fn greet(name) {", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[4], "    print(\"Hello, \" + name + \"!\")", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[5], "}", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[6], "", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[7], "greet(\"World\")", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[8], "greet(\"AstraOS\")", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[9], "", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[10], "# Math", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[11], "let x = 10", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[12], "let y = 20", EDITOR_LINE_LEN);
    kstrncpy(ed->lines[13], "print(\"x + y = \" + str(x + y))", EDITOR_LINE_LEN);
    ed->total_lines = 14;

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
    /* Simple folder icon: yellow rectangle with a tab */
    fb_fill_rect(x, y + 4, 14, 10, COL_YELLOW);
    fb_fill_rect(x, y + 2, 8, 4, COL_YELLOW);
}

static void fm_draw_icon_file(int x, int y)
{
    /* Simple file icon: white/light rectangle */
    fb_fill_rect(x + 2, y + 2, 10, 13, COL_TEXT_DIM);
    fb_fill_rect(x + 3, y + 3, 8, 11, COL_SURFACE);
}

static void fm_render(int win_id, int x, int y, int w, int h)
{
    gui_window_t *win = gui_get_window(win_id);
    if (!win) return;
    filemgr_t *fm = (filemgr_t *)win->userdata;
    if (!fm || !fm->current_dir) return;

    /* Background */
    fb_fill_rect(x, y, w, h, COL_BG_DARK);

    /* Toolbar area */
    fb_fill_rect(x, y, w, FM_TOOLBAR_H, COL_SURFACE);
    fb_rect(x, y + FM_TOOLBAR_H - 1, w, 1, COL_BORDER);

    /* "New File" button */
    fb_fill_rect(x + 4, y + 4, 72, 22, COL_ACCENT);
    fb_puts(x + 10, y + 7, "New File", COL_BG_DARK, COL_ACCENT);

    /* "Open" button */
    fb_fill_rect(x + 82, y + 4, 52, 22, COL_BLUE);
    fb_puts(x + 92, y + 7, "Open", COL_BG_DARK, COL_BLUE);

    /* Path bar */
    int path_y = y + FM_TOOLBAR_H;
    fb_fill_rect(x, path_y, w, FM_PATHBAR_H, RGB(35, 35, 55));
    fb_rect(x, path_y + FM_PATHBAR_H - 1, w, 1, COL_BORDER);
    fb_puts(x + 8, path_y + 4, fm->path, COL_TEXT, RGB(35, 35, 55));

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
            fb_fill_rect(x, ey, w, FM_ENTRY_H, RGB(50, 52, 78));
        fm_draw_icon_folder(x + 8, ey + 2);
        fb_puts(x + 8 + FM_ICON_W + 8, ey + 4, "..", COL_TEXT, COL_BG_DARK);
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
                fb_fill_rect(x, ey, w, FM_ENTRY_H, RGB(50, 52, 78));

            /* Icon */
            if (child->type & VFS_DIRECTORY)
                fm_draw_icon_folder(x + 8, ey + 2);
            else
                fm_draw_icon_file(x + 8, ey + 2);

            /* Name */
            color_t name_col = (child->type & VFS_DIRECTORY) ? COL_YELLOW : COL_TEXT;
            fb_puts(x + 8 + FM_ICON_W + 8, ey + 4,
                    child->name, name_col, COL_BG_DARK);

            /* Show type indicator for directories */
            if (child->type & VFS_DIRECTORY) {
                int nlen = (int)kstrlen(child->name);
                fb_putchar(x + 8 + FM_ICON_W + 8 + nlen * FONT_W + 4,
                           ey + 4, '/', COL_TEXT_DIM, COL_BG_DARK);
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
