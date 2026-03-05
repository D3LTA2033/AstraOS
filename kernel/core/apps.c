/* ==========================================================================
 * AstraOS - Built-in GUI Applications
 * ==========================================================================
 * Text Editor (with Astracor support) and Terminal for the desktop.
 * ========================================================================== */

#include <kernel/gui.h>
#include <kernel/astracor.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>
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
    static const char *kws[] = {"fn","let","if","elif","else","while","for","in","return","true","false","null","break","continue"};
    for (int i = 0; i < 14; i++) {
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
    static const char *bfs[] = {"print","len","str","int","type","range","abs"};
    for (int i = 0; i < 7; i++) {
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
