/* ==========================================================================
 * AstraOS - Astracor Language Interpreter
 * ==========================================================================
 * Tree-walking interpreter for the Astracor language. Parses and executes
 * source code in a single pass (no separate AST construction phase).
 * ========================================================================== */

#include <kernel/astracor.h>
#include <kernel/kheap.h>
#include <kernel/kstring.h>

/* --- Lexer --- */

static bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }
static bool is_alnum(char c) { return is_alpha(c) || is_digit(c); }

static char peek(ac_interp_t *ctx)
{
    if (ctx->pos >= ctx->source + ctx->source_len) return '\0';
    return *ctx->pos;
}

static char advance(ac_interp_t *ctx)
{
    char c = peek(ctx);
    if (c == '\n') ctx->line++;
    ctx->pos++;
    return c;
}

static void skip_ws(ac_interp_t *ctx)
{
    while (1) {
        char c = peek(ctx);
        if (c == ' ' || c == '\t' || c == '\r') { advance(ctx); continue; }
        if (c == '#') { while (peek(ctx) != '\n' && peek(ctx) != '\0') advance(ctx); continue; }
        break;
    }
}

static token_type_t check_keyword(const char *s, int len)
{
    if (len == 2 && s[0] == 'f' && s[1] == 'n') return TOK_FN;
    if (len == 3 && s[0] == 'l' && s[1] == 'e' && s[2] == 't') return TOK_LET;
    if (len == 2 && s[0] == 'i' && s[1] == 'f') return TOK_IF;
    if (len == 4 && s[0] == 'e' && s[1] == 'l' && s[2] == 'i' && s[3] == 'f') return TOK_ELIF;
    if (len == 4 && s[0] == 'e' && s[1] == 'l' && s[2] == 's' && s[3] == 'e') return TOK_ELSE;
    if (len == 5 && s[0] == 'w' && s[1] == 'h' && s[2] == 'i' && s[3] == 'l' && s[4] == 'e') return TOK_WHILE;
    if (len == 3 && s[0] == 'f' && s[1] == 'o' && s[2] == 'r') return TOK_FOR;
    if (len == 2 && s[0] == 'i' && s[1] == 'n') return TOK_IN;
    if (len == 6 && s[0] == 'r') { if (kstrlen("return") == 6 && s[1]=='e'&&s[2]=='t'&&s[3]=='u'&&s[4]=='r'&&s[5]=='n') return TOK_RETURN; }
    if (len == 4 && s[0] == 't' && s[1] == 'r' && s[2] == 'u' && s[3] == 'e') return TOK_TRUE;
    if (len == 5 && s[0] == 'f' && s[1] == 'a' && s[2] == 'l' && s[3] == 's' && s[4] == 'e') return TOK_FALSE;
    if (len == 4 && s[0] == 'n' && s[1] == 'u' && s[2] == 'l' && s[3] == 'l') return TOK_NULL;
    if (len == 5 && s[0] == 'b' && s[1] == 'r' && s[2] == 'e' && s[3] == 'a' && s[4] == 'k') return TOK_BREAK;
    if (len == 8 && s[0] == 'c') {
        const char *kw = "continue";
        bool match = true;
        for (int i = 0; i < 8; i++) if (s[i] != kw[i]) { match = false; break; }
        if (match) return TOK_CONTINUE;
    }
    if (len == 6 && s[0] == 'i' && s[1] == 'm' && s[2] == 'p' && s[3] == 'o' && s[4] == 'r' && s[5] == 't') return TOK_IMPORT;
    return TOK_IDENT;
}

static void next_token(ac_interp_t *ctx)
{
    skip_ws(ctx);
    ctx->current.start = ctx->pos;
    ctx->current.line = ctx->line;

    char c = peek(ctx);
    if (c == '\0') { ctx->current.type = TOK_EOF; ctx->current.length = 0; return; }
    if (c == '\n') { advance(ctx); ctx->current.type = TOK_NEWLINE; ctx->current.length = 1; return; }

    /* String literal */
    if (c == '"') {
        advance(ctx);
        const char *start = ctx->pos;
        while (peek(ctx) != '"' && peek(ctx) != '\0') {
            if (peek(ctx) == '\\') advance(ctx);  /* skip escaped char */
            advance(ctx);
        }
        ctx->current.type = TOK_STRING;
        ctx->current.start = start;
        ctx->current.length = (int)(ctx->pos - start);
        if (peek(ctx) == '"') advance(ctx);
        return;
    }

    /* Number */
    if (is_digit(c)) {
        const char *start = ctx->pos;
        int val = 0;
        while (is_digit(peek(ctx))) val = val * 10 + (advance(ctx) - '0');
        ctx->current.type = TOK_INT;
        ctx->current.start = start;
        ctx->current.length = (int)(ctx->pos - start);
        ctx->current.int_val = val;
        return;
    }

    /* Identifier or keyword */
    if (is_alpha(c)) {
        const char *start = ctx->pos;
        while (is_alnum(peek(ctx))) advance(ctx);
        int len = (int)(ctx->pos - start);
        ctx->current.type = check_keyword(start, len);
        ctx->current.start = start;
        ctx->current.length = len;
        return;
    }

    /* Operators and punctuation */
    advance(ctx);
    switch (c) {
    case '+': if (peek(ctx)=='='){advance(ctx);ctx->current.type=TOK_PLUS_ASSIGN;}else ctx->current.type=TOK_PLUS; break;
    case '-': if (peek(ctx)=='>'){advance(ctx);ctx->current.type=TOK_ARROW;}else if(peek(ctx)=='='){advance(ctx);ctx->current.type=TOK_MINUS_ASSIGN;}else ctx->current.type=TOK_MINUS; break;
    case '*': ctx->current.type=TOK_STAR; break;
    case '/': ctx->current.type=TOK_SLASH; break;
    case '%': ctx->current.type=TOK_PERCENT; break;
    case '=': ctx->current.type=peek(ctx)=='='?(advance(ctx),TOK_EQ):TOK_ASSIGN; break;
    case '!': ctx->current.type=peek(ctx)=='='?(advance(ctx),TOK_NEQ):TOK_NOT; break;
    case '<': ctx->current.type=peek(ctx)=='='?(advance(ctx),TOK_LTE):TOK_LT; break;
    case '>': ctx->current.type=peek(ctx)=='='?(advance(ctx),TOK_GTE):TOK_GT; break;
    case '&': if(peek(ctx)=='&'){advance(ctx);ctx->current.type=TOK_AND;}else ctx->current.type=TOK_EOF; break;
    case '|': if(peek(ctx)=='|'){advance(ctx);ctx->current.type=TOK_OR;}else ctx->current.type=TOK_EOF; break;
    case '(': ctx->current.type=TOK_LPAREN; break;
    case ')': ctx->current.type=TOK_RPAREN; break;
    case '{': ctx->current.type=TOK_LBRACE; break;
    case '}': ctx->current.type=TOK_RBRACE; break;
    case '[': ctx->current.type=TOK_LBRACKET; break;
    case ']': ctx->current.type=TOK_RBRACKET; break;
    case ',': ctx->current.type=TOK_COMMA; break;
    case '.': ctx->current.type=TOK_DOT; break;
    case ':': ctx->current.type=TOK_COLON; break;
    case ';': ctx->current.type=TOK_SEMICOLON; break;
    default: ctx->current.type=TOK_EOF; break;
    }
    ctx->current.length = (int)(ctx->pos - ctx->current.start);
}

static void skip_newlines(ac_interp_t *ctx)
{
    while (ctx->current.type == TOK_NEWLINE || ctx->current.type == TOK_SEMICOLON)
        next_token(ctx);
}

static bool match(ac_interp_t *ctx, token_type_t type)
{
    skip_newlines(ctx);
    if (ctx->current.type == type) { next_token(ctx); return true; }
    return false;
}

static void error(ac_interp_t *ctx, const char *msg)
{
    if (ctx->has_error) return;
    ctx->has_error = true;
    int len = 0;
    const char *p = "Error line ";
    while (*p && len < 240) ctx->error[len++] = *p++;
    /* Line number */
    char nbuf[12]; int ni = 0; int ln = ctx->line;
    if (ln == 0) nbuf[ni++] = '0';
    else { while (ln > 0) { nbuf[ni++] = '0'+(char)(ln%10); ln/=10; } }
    for (int j = ni-1; j >= 0; j--) ctx->error[len++] = nbuf[j];
    ctx->error[len++] = ':'; ctx->error[len++] = ' ';
    while (*msg && len < 254) ctx->error[len++] = *msg++;
    ctx->error[len] = '\0';
}

/* --- Environment --- */

static ac_env_t *env_create(ac_env_t *parent)
{
    ac_env_t *e = (ac_env_t *)kmalloc(sizeof(ac_env_t));
    kmemset(e, 0, sizeof(ac_env_t));
    e->parent = parent;
    return e;
}

static void env_destroy(ac_env_t *e)
{
    for (int i = 0; i < e->count; i++) {
        if (e->names[i]) kfree(e->names[i]);
        if (e->values[i].type == VAL_STRING && e->values[i].str_val)
            kfree(e->values[i].str_val);
    }
    kfree(e);
}

static bool ident_eq(const char *a, int alen, const char *b)
{
    int blen = (int)kstrlen(b);
    if (alen != blen) return false;
    for (int i = 0; i < alen; i++) if (a[i] != b[i]) return false;
    return true;
}

static ac_value_t *env_lookup(ac_env_t *e, const char *name, int nlen)
{
    for (ac_env_t *cur = e; cur; cur = cur->parent)
        for (int i = 0; i < cur->count; i++)
            if (ident_eq(name, nlen, cur->names[i]))
                return &cur->values[i];
    return NULL;
}

static void env_set(ac_env_t *e, const char *name, int nlen, ac_value_t val)
{
    /* Update existing */
    for (ac_env_t *cur = e; cur; cur = cur->parent)
        for (int i = 0; i < cur->count; i++)
            if (ident_eq(name, nlen, cur->names[i])) {
                cur->values[i] = val;
                return;
            }
    /* Create new in current scope */
    if (e->count >= AC_ENV_MAX) return;
    char *n = (char *)kmalloc((uint32_t)(nlen + 1));
    kmemcpy(n, name, (uint32_t)nlen);
    n[nlen] = '\0';
    e->names[e->count] = n;
    e->values[e->count] = val;
    e->count++;
}

/* --- Value helpers --- */

static ac_value_t val_null(void) { ac_value_t v; v.type = VAL_NULL; v.int_val = 0; return v; }
static ac_value_t val_int(int i) { ac_value_t v; v.type = VAL_INT; v.int_val = i; return v; }
static ac_value_t val_bool(bool b) { ac_value_t v; v.type = VAL_BOOL; v.bool_val = b; return v; }

static ac_value_t val_str(const char *s, int len)
{
    ac_value_t v;
    v.type = VAL_STRING;
    v.str_val = (char *)kmalloc((uint32_t)(len + 1));
    kmemcpy(v.str_val, s, (uint32_t)len);
    v.str_val[len] = '\0';
    return v;
}

static bool is_truthy(ac_value_t v)
{
    switch (v.type) {
    case VAL_NULL: return false;
    case VAL_INT: return v.int_val != 0;
    case VAL_BOOL: return v.bool_val;
    case VAL_STRING: return v.str_val && v.str_val[0] != '\0';
    default: return true;
    }
}

static void val_to_str(ac_value_t v, char *buf, int max)
{
    int len = 0;
    switch (v.type) {
    case VAL_NULL:
        kstrncpy(buf, "null", (uint32_t)max); break;
    case VAL_BOOL:
        kstrncpy(buf, v.bool_val ? "true" : "false", (uint32_t)max); break;
    case VAL_INT: {
        int val = v.int_val;
        if (val < 0) { buf[len++] = '-'; val = -val; }
        if (val == 0) buf[len++] = '0';
        else {
            char tmp[12]; int ti = 0;
            while (val > 0) { tmp[ti++] = '0'+(char)(val%10); val/=10; }
            for (int j = ti-1; j >= 0 && len < max-1; j--) buf[len++] = tmp[j];
        }
        buf[len] = '\0';
        break;
    }
    case VAL_STRING:
        kstrncpy(buf, v.str_val ? v.str_val : "", (uint32_t)max);
        break;
    default:
        kstrncpy(buf, "<object>", (uint32_t)max); break;
    }
}

/* --- Output --- */

static void emit(ac_interp_t *ctx, const char *text)
{
    if (ctx->output)
        ctx->output(text, ctx->output_data);
}

/* --- Parser / Interpreter (recursive descent) --- */

static ac_value_t parse_expr(ac_interp_t *ctx, ac_env_t *env);
static void parse_block(ac_interp_t *ctx, ac_env_t *env);

static ac_value_t parse_primary(ac_interp_t *ctx, ac_env_t *env)
{
    skip_newlines(ctx);
    token_t tok = ctx->current;

    if (tok.type == TOK_INT) {
        next_token(ctx);
        return val_int(tok.int_val);
    }
    if (tok.type == TOK_STRING) {
        next_token(ctx);
        /* Process escape sequences */
        char *buf = (char *)kmalloc((uint32_t)(tok.length + 1));
        int j = 0;
        for (int i = 0; i < tok.length; i++) {
            if (tok.start[i] == '\\' && i + 1 < tok.length) {
                i++;
                switch (tok.start[i]) {
                case 'n': buf[j++] = '\n'; break;
                case 't': buf[j++] = '\t'; break;
                case '\\': buf[j++] = '\\'; break;
                case '"': buf[j++] = '"'; break;
                default: buf[j++] = tok.start[i]; break;
                }
            } else {
                buf[j++] = tok.start[i];
            }
        }
        buf[j] = '\0';
        ac_value_t v = val_str(buf, j);
        kfree(buf);
        return v;
    }
    if (tok.type == TOK_TRUE) { next_token(ctx); return val_bool(true); }
    if (tok.type == TOK_FALSE) { next_token(ctx); return val_bool(false); }
    if (tok.type == TOK_NULL) { next_token(ctx); return val_null(); }

    if (tok.type == TOK_IDENT) {
        next_token(ctx);
        /* Function call? */
        if (ctx->current.type == TOK_LPAREN) {
            next_token(ctx);  /* consume ( */
            ac_value_t args[16];
            int argc = 0;
            while (ctx->current.type != TOK_RPAREN && ctx->current.type != TOK_EOF && argc < 16) {
                args[argc++] = parse_expr(ctx, env);
                if (ctx->has_error) return val_null();
                if (ctx->current.type == TOK_COMMA) next_token(ctx);
            }
            match(ctx, TOK_RPAREN);

            /* Look up function */
            ac_value_t *fn = env_lookup(env, tok.start, tok.length);
            if (!fn) {
                error(ctx, "undefined function");
                return val_null();
            }
            if (fn->type == VAL_FN && fn->native_fn) {
                return fn->native_fn(env, args, argc);
            }
            if (fn->type == VAL_FN && fn->fn_val) {
                /* Call user-defined function */
                ac_func_t *f = fn->fn_val;
                ac_env_t *fenv = env_create(ctx->global_env);
                for (int i = 0; i < f->param_count && i < argc; i++)
                    env_set(fenv, f->params[i], (int)kstrlen(f->params[i]), args[i]);

                /* Save and restore parse state */
                const char *save_pos = ctx->pos;
                token_t save_tok = ctx->current;
                int save_line = ctx->line;

                ctx->pos = f->body_start;
                ctx->line = 1;
                next_token(ctx);
                parse_block(ctx, fenv);

                ac_value_t result = val_null();
                if (ctx->should_return) {
                    result = ctx->return_val;
                    ctx->should_return = false;
                }

                ctx->pos = save_pos;
                ctx->current = save_tok;
                ctx->line = save_line;
                env_destroy(fenv);
                return result;
            }
            error(ctx, "not callable");
            return val_null();
        }

        /* Variable lookup */
        ac_value_t *v = env_lookup(env, tok.start, tok.length);
        if (!v) {
            error(ctx, "undefined variable");
            return val_null();
        }
        return *v;
    }

    if (tok.type == TOK_LPAREN) {
        next_token(ctx);
        ac_value_t v = parse_expr(ctx, env);
        match(ctx, TOK_RPAREN);
        return v;
    }

    if (tok.type == TOK_MINUS) {
        next_token(ctx);
        ac_value_t v = parse_primary(ctx, env);
        if (v.type == VAL_INT) v.int_val = -v.int_val;
        return v;
    }

    if (tok.type == TOK_NOT) {
        next_token(ctx);
        ac_value_t v = parse_primary(ctx, env);
        return val_bool(!is_truthy(v));
    }

    if (tok.type == TOK_LBRACKET) {
        /* Array literal */
        next_token(ctx);
        ac_array_t *arr = (ac_array_t *)kmalloc(sizeof(ac_array_t));
        arr->count = 0; arr->capacity = 8;
        arr->items = (ac_value_t *)kmalloc(sizeof(ac_value_t) * 8);
        while (ctx->current.type != TOK_RBRACKET && ctx->current.type != TOK_EOF) {
            skip_newlines(ctx);
            if (arr->count >= arr->capacity) break;
            arr->items[arr->count++] = parse_expr(ctx, env);
            if (ctx->current.type == TOK_COMMA) next_token(ctx);
        }
        match(ctx, TOK_RBRACKET);
        ac_value_t v; v.type = VAL_ARRAY; v.arr_val = arr;
        return v;
    }

    error(ctx, "unexpected token");
    next_token(ctx);
    return val_null();
}

static ac_value_t parse_mul(ac_interp_t *ctx, ac_env_t *env)
{
    ac_value_t left = parse_primary(ctx, env);
    while (ctx->current.type == TOK_STAR || ctx->current.type == TOK_SLASH || ctx->current.type == TOK_PERCENT) {
        token_type_t op = ctx->current.type;
        next_token(ctx);
        ac_value_t right = parse_primary(ctx, env);
        if (left.type == VAL_INT && right.type == VAL_INT) {
            if (op == TOK_STAR) left.int_val *= right.int_val;
            else if (op == TOK_SLASH) { if (right.int_val) left.int_val /= right.int_val; else { error(ctx, "division by zero"); return val_null(); } }
            else if (op == TOK_PERCENT) { if (right.int_val) left.int_val %= right.int_val; else { error(ctx, "modulo by zero"); return val_null(); } }
        }
    }
    return left;
}

static ac_value_t parse_add(ac_interp_t *ctx, ac_env_t *env)
{
    ac_value_t left = parse_mul(ctx, env);
    while (ctx->current.type == TOK_PLUS || ctx->current.type == TOK_MINUS) {
        token_type_t op = ctx->current.type;
        next_token(ctx);
        ac_value_t right = parse_mul(ctx, env);
        if (op == TOK_PLUS && left.type == VAL_STRING) {
            /* String concatenation */
            char rbuf[256];
            val_to_str(right, rbuf, 256);
            int llen = (int)kstrlen(left.str_val);
            int rlen = (int)kstrlen(rbuf);
            char *cat = (char *)kmalloc((uint32_t)(llen + rlen + 1));
            kmemcpy(cat, left.str_val, (uint32_t)llen);
            kmemcpy(cat + llen, rbuf, (uint32_t)rlen);
            cat[llen + rlen] = '\0';
            left = val_str(cat, llen + rlen);
            kfree(cat);
        } else if (left.type == VAL_INT && right.type == VAL_INT) {
            if (op == TOK_PLUS) left.int_val += right.int_val;
            else left.int_val -= right.int_val;
        }
    }
    return left;
}

static ac_value_t parse_comparison(ac_interp_t *ctx, ac_env_t *env)
{
    ac_value_t left = parse_add(ctx, env);
    while (ctx->current.type == TOK_LT || ctx->current.type == TOK_GT ||
           ctx->current.type == TOK_LTE || ctx->current.type == TOK_GTE ||
           ctx->current.type == TOK_EQ || ctx->current.type == TOK_NEQ) {
        token_type_t op = ctx->current.type;
        next_token(ctx);
        ac_value_t right = parse_add(ctx, env);
        if (left.type == VAL_INT && right.type == VAL_INT) {
            bool r = false;
            switch (op) {
            case TOK_LT: r = left.int_val < right.int_val; break;
            case TOK_GT: r = left.int_val > right.int_val; break;
            case TOK_LTE: r = left.int_val <= right.int_val; break;
            case TOK_GTE: r = left.int_val >= right.int_val; break;
            case TOK_EQ: r = left.int_val == right.int_val; break;
            case TOK_NEQ: r = left.int_val != right.int_val; break;
            default: break;
            }
            left = val_bool(r);
        } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
            int cmp = kstrcmp(left.str_val, right.str_val);
            if (op == TOK_EQ) left = val_bool(cmp == 0);
            else if (op == TOK_NEQ) left = val_bool(cmp != 0);
        }
    }
    return left;
}

static ac_value_t parse_logic(ac_interp_t *ctx, ac_env_t *env)
{
    ac_value_t left = parse_comparison(ctx, env);
    while (ctx->current.type == TOK_AND || ctx->current.type == TOK_OR) {
        token_type_t op = ctx->current.type;
        next_token(ctx);
        ac_value_t right = parse_comparison(ctx, env);
        if (op == TOK_AND) left = val_bool(is_truthy(left) && is_truthy(right));
        else left = val_bool(is_truthy(left) || is_truthy(right));
    }
    return left;
}

static ac_value_t parse_expr(ac_interp_t *ctx, ac_env_t *env)
{
    return parse_logic(ctx, env);
}

/* --- Module system (must be before parse_stmt) --- */

static const char mod_math_src[] =
    "# math module\n"
    "let PI = 3141\n"
    "let E = 2718\n"
    "fn square(x) { return x * x }\n"
    "fn cube(x) { return x * x * x }\n"
    "fn factorial(n) { if n <= 1 { return 1 } return n * factorial(n - 1) }\n"
    "fn gcd(a, b) { while b != 0 { let t = b\n b = a % b\n a = t } return a }\n"
    "fn lcm(a, b) { return a * b / gcd(a, b) }\n"
    "fn is_even(n) { return n % 2 == 0 }\n"
    "fn is_odd(n) { return n % 2 != 0 }\n"
    "fn sum_range(n) { let s = 0\n for i in range(n) { s += i } return s }\n";

static const char mod_string_src[] =
    "# string module\n"
    "fn repeat(s, n) { let r = \"\"\n for i in range(n) { r = r + s } return r }\n"
    "fn reverse(s) { let r = \"\"\n let i = len(s) - 1\n while i >= 0 { r = r + char_at(s, i)\n i -= 1 } return r }\n"
    "fn is_empty(s) { return len(s) == 0 }\n"
    "fn pad_left(s, n, c) { while len(s) < n { s = c + s } return s }\n"
    "fn pad_right(s, n, c) { while len(s) < n { s = s + c } return s }\n";

static const char mod_array_src[] =
    "# array module\n"
    "fn map(arr, f) { let r = []\n for i in range(len(arr)) { push(r, f(get(arr, i))) } return r }\n"
    "fn filter(arr, f) { let r = []\n for i in range(len(arr)) { let v = get(arr, i)\n if f(v) { push(r, v) } } return r }\n"
    "fn reduce(arr, f, init) { let acc = init\n for i in range(len(arr)) { acc = f(acc, get(arr, i)) } return acc }\n"
    "fn foreach(arr, f) { for i in range(len(arr)) { f(get(arr, i)) } }\n"
    "fn find(arr, val) { for i in range(len(arr)) { if get(arr, i) == val { return i } } return -1 }\n"
    "fn sum(arr) { let s = 0\n for i in range(len(arr)) { s += get(arr, i) } return s }\n";

static const char mod_io_src[] =
    "# io module - basic I/O helpers\n"
    "fn println(s) { print(s) }\n"
    "fn debug(label, val) { print(label + \": \" + str(val)) }\n"
    "fn assert(cond, msg) { if !cond { print(\"ASSERT FAILED: \" + msg) } }\n"
    "fn assert_eq(a, b, msg) { if a != b { print(\"ASSERT_EQ FAILED: \" + msg + \" (\" + str(a) + \" != \" + str(b) + \")\") } }\n";

typedef struct {
    const char *name;
    const char *source;
} ac_module_t;

static const ac_module_t builtin_modules[] = {
    { "math",   mod_math_src },
    { "string", mod_string_src },
    { "array",  mod_array_src },
    { "io",     mod_io_src },
};
#define AC_MODULE_COUNT 4

/* --- Statements --- */

static void parse_stmt(ac_interp_t *ctx, ac_env_t *env);

static void parse_block(ac_interp_t *ctx, ac_env_t *env)
{
    if (!match(ctx, TOK_LBRACE)) { error(ctx, "expected '{'"); return; }
    while (ctx->current.type != TOK_RBRACE && ctx->current.type != TOK_EOF) {
        if (ctx->has_error || ctx->should_break || ctx->should_continue || ctx->should_return) break;
        parse_stmt(ctx, env);
    }
    match(ctx, TOK_RBRACE);
}

static void parse_stmt(ac_interp_t *ctx, ac_env_t *env)
{
    skip_newlines(ctx);
    if (ctx->current.type == TOK_EOF) return;
    if (ctx->has_error) return;

    /* let x = expr */
    if (ctx->current.type == TOK_LET) {
        next_token(ctx);
        if (ctx->current.type != TOK_IDENT) { error(ctx, "expected identifier"); return; }
        const char *name = ctx->current.start;
        int nlen = ctx->current.length;
        next_token(ctx);
        if (!match(ctx, TOK_ASSIGN)) { error(ctx, "expected '='"); return; }
        ac_value_t val = parse_expr(ctx, env);
        env_set(env, name, nlen, val);
        return;
    }

    /* fn name(params) { body } */
    if (ctx->current.type == TOK_FN) {
        next_token(ctx);
        if (ctx->current.type != TOK_IDENT) { error(ctx, "expected function name"); return; }
        const char *name = ctx->current.start;
        int nlen = ctx->current.length;
        next_token(ctx);
        if (!match(ctx, TOK_LPAREN)) { error(ctx, "expected '('"); return; }

        /* Parse parameters */
        char *params[16];
        int pcount = 0;
        while (ctx->current.type == TOK_IDENT && pcount < 16) {
            char *p = (char *)kmalloc((uint32_t)(ctx->current.length + 1));
            kmemcpy(p, ctx->current.start, (uint32_t)ctx->current.length);
            p[ctx->current.length] = '\0';
            params[pcount++] = p;
            next_token(ctx);
            if (ctx->current.type == TOK_COMMA) next_token(ctx);
        }
        match(ctx, TOK_RPAREN);

        skip_newlines(ctx);
        if (ctx->current.type != TOK_LBRACE) { error(ctx, "expected '{'"); return; }
        /* Record body position */
        const char *body_start = ctx->pos - 1;  /* include the '{' */
        /* Skip the body */
        int depth = 1;
        next_token(ctx);  /* consume '{' */
        while (depth > 0 && ctx->current.type != TOK_EOF) {
            if (ctx->current.type == TOK_LBRACE) depth++;
            else if (ctx->current.type == TOK_RBRACE) depth--;
            if (depth > 0) next_token(ctx);
        }
        const char *body_end = ctx->pos;
        next_token(ctx);  /* consume final '}' */

        ac_func_t *fn = (ac_func_t *)kmalloc(sizeof(ac_func_t));
        fn->name = name;
        fn->body_start = body_start;
        fn->body_len = (int)(body_end - body_start);
        fn->params = (char **)kmalloc(sizeof(char *) * (uint32_t)pcount);
        kmemcpy(fn->params, params, sizeof(char *) * (uint32_t)pcount);
        fn->param_count = pcount;

        ac_value_t fval;
        fval.type = VAL_FN;
        fval.fn_val = fn;
        env_set(env, name, nlen, fval);
        return;
    }

    /* if / elif / else */
    if (ctx->current.type == TOK_IF) {
        next_token(ctx);
        ac_value_t cond = parse_expr(ctx, env);
        ac_env_t *block_env = env_create(env);
        bool done = is_truthy(cond);
        if (done) {
            parse_block(ctx, block_env);
        } else {
            /* Skip the block */
            skip_newlines(ctx);
            if (ctx->current.type == TOK_LBRACE) {
                int depth = 0;
                do {
                    if (ctx->current.type == TOK_LBRACE) depth++;
                    if (ctx->current.type == TOK_RBRACE) depth--;
                    next_token(ctx);
                } while (depth > 0 && ctx->current.type != TOK_EOF);
            }
        }
        /* elif / else chains */
        while (1) {
            skip_newlines(ctx);
            if (ctx->current.type == TOK_ELIF && !done) {
                next_token(ctx);
                cond = parse_expr(ctx, env);
                if (is_truthy(cond)) {
                    done = true;
                    parse_block(ctx, block_env);
                } else {
                    skip_newlines(ctx);
                    if (ctx->current.type == TOK_LBRACE) {
                        int depth = 0;
                        do {
                            if (ctx->current.type == TOK_LBRACE) depth++;
                            if (ctx->current.type == TOK_RBRACE) depth--;
                            next_token(ctx);
                        } while (depth > 0 && ctx->current.type != TOK_EOF);
                    }
                }
            } else if (ctx->current.type == TOK_ELIF && done) {
                next_token(ctx);
                parse_expr(ctx, env);  /* consume condition */
                skip_newlines(ctx);
                if (ctx->current.type == TOK_LBRACE) {
                    int depth = 0;
                    do {
                        if (ctx->current.type == TOK_LBRACE) depth++;
                        if (ctx->current.type == TOK_RBRACE) depth--;
                        next_token(ctx);
                    } while (depth > 0 && ctx->current.type != TOK_EOF);
                }
            } else if (ctx->current.type == TOK_ELSE) {
                next_token(ctx);
                if (!done) {
                    done = true;
                    parse_block(ctx, block_env);
                } else {
                    skip_newlines(ctx);
                    if (ctx->current.type == TOK_LBRACE) {
                        int depth = 0;
                        do {
                            if (ctx->current.type == TOK_LBRACE) depth++;
                            if (ctx->current.type == TOK_RBRACE) depth--;
                            next_token(ctx);
                        } while (depth > 0 && ctx->current.type != TOK_EOF);
                    }
                }
            } else break;
        }
        env_destroy(block_env);
        return;
    }

    /* while */
    if (ctx->current.type == TOK_WHILE) {
        next_token(ctx);
        const char *cond_pos = ctx->pos;
        token_t cond_tok = ctx->current;
        int cond_line = ctx->line;

        for (int iter = 0; iter < 100000; iter++) {
            ctx->pos = cond_pos;
            ctx->current = cond_tok;
            ctx->line = cond_line;
            ac_value_t cond = parse_expr(ctx, env);
            if (!is_truthy(cond)) {
                /* Skip block */
                skip_newlines(ctx);
                if (ctx->current.type == TOK_LBRACE) {
                    int depth = 0;
                    do {
                        if (ctx->current.type == TOK_LBRACE) depth++;
                        if (ctx->current.type == TOK_RBRACE) depth--;
                        next_token(ctx);
                    } while (depth > 0 && ctx->current.type != TOK_EOF);
                }
                break;
            }
            parse_block(ctx, env);
            if (ctx->has_error || ctx->should_return) break;
            if (ctx->should_break) { ctx->should_break = false; break; }
            if (ctx->should_continue) { ctx->should_continue = false; continue; }
        }
        return;
    }

    /* for i in range(n) — simplified */
    if (ctx->current.type == TOK_FOR) {
        next_token(ctx);
        if (ctx->current.type != TOK_IDENT) { error(ctx, "expected identifier"); return; }
        const char *var = ctx->current.start;
        int vlen = ctx->current.length;
        next_token(ctx);
        if (!match(ctx, TOK_IN)) { error(ctx, "expected 'in'"); return; }
        ac_value_t range = parse_expr(ctx, env);
        if (range.type == VAL_INT) {
            const char *block_pos = ctx->pos;
            token_t block_tok = ctx->current;
            int block_line = ctx->line;
            for (int i = 0; i < range.int_val; i++) {
                env_set(env, var, vlen, val_int(i));
                ctx->pos = block_pos;
                ctx->current = block_tok;
                ctx->line = block_line;
                parse_block(ctx, env);
                if (ctx->has_error || ctx->should_return) break;
                if (ctx->should_break) { ctx->should_break = false; break; }
                if (ctx->should_continue) { ctx->should_continue = false; }
            }
            if (range.int_val == 0) {
                skip_newlines(ctx);
                if (ctx->current.type == TOK_LBRACE) {
                    int depth = 0;
                    do {
                        if (ctx->current.type == TOK_LBRACE) depth++;
                        if (ctx->current.type == TOK_RBRACE) depth--;
                        next_token(ctx);
                    } while (depth > 0 && ctx->current.type != TOK_EOF);
                }
            }
        }
        return;
    }

    /* return */
    if (ctx->current.type == TOK_RETURN) {
        next_token(ctx);
        skip_newlines(ctx);
        if (ctx->current.type != TOK_RBRACE && ctx->current.type != TOK_EOF &&
            ctx->current.type != TOK_NEWLINE) {
            ctx->return_val = parse_expr(ctx, env);
        } else {
            ctx->return_val = val_null();
        }
        ctx->should_return = true;
        return;
    }

    /* import module */
    if (ctx->current.type == TOK_IMPORT) {
        next_token(ctx);
        if (ctx->current.type != TOK_IDENT) { error(ctx, "expected module name"); return; }
        const char *mod_name = ctx->current.start;
        int mod_nlen = ctx->current.length;
        next_token(ctx);

        /* Find built-in module */
        const char *mod_src = NULL;
        for (int i = 0; i < AC_MODULE_COUNT; i++) {
            if (ident_eq(mod_name, mod_nlen, builtin_modules[i].name)) {
                mod_src = builtin_modules[i].source;
                break;
            }
        }
        if (!mod_src) { error(ctx, "unknown module"); return; }

        /* Save parse state and execute module source */
        const char *save_src = ctx->source;
        int save_srclen = ctx->source_len;
        const char *save_pos = ctx->pos;
        token_t save_tok = ctx->current;
        int save_line = ctx->line;

        ctx->source = mod_src;
        ctx->source_len = (int)kstrlen(mod_src);
        ctx->pos = mod_src;
        ctx->line = 1;
        next_token(ctx);
        while (ctx->current.type != TOK_EOF && !ctx->has_error)
            parse_stmt(ctx, ctx->global_env);

        /* Restore parse state */
        ctx->source = save_src;
        ctx->source_len = save_srclen;
        ctx->pos = save_pos;
        ctx->current = save_tok;
        ctx->line = save_line;
        return;
    }

    /* break / continue */
    if (ctx->current.type == TOK_BREAK) { next_token(ctx); ctx->should_break = true; return; }
    if (ctx->current.type == TOK_CONTINUE) { next_token(ctx); ctx->should_continue = true; return; }

    /* Assignment: ident = expr */
    if (ctx->current.type == TOK_IDENT) {
        const char *name = ctx->current.start;
        int nlen = ctx->current.length;
        const char *save = ctx->pos;
        token_t save_tok = ctx->current;
        int save_line = ctx->line;
        next_token(ctx);

        if (ctx->current.type == TOK_ASSIGN) {
            next_token(ctx);
            ac_value_t val = parse_expr(ctx, env);
            env_set(env, name, nlen, val);
            return;
        }
        if (ctx->current.type == TOK_PLUS_ASSIGN) {
            next_token(ctx);
            ac_value_t *cur = env_lookup(env, name, nlen);
            ac_value_t add = parse_expr(ctx, env);
            if (cur && cur->type == VAL_INT && add.type == VAL_INT) {
                cur->int_val += add.int_val;
            }
            return;
        }
        if (ctx->current.type == TOK_MINUS_ASSIGN) {
            next_token(ctx);
            ac_value_t *cur = env_lookup(env, name, nlen);
            ac_value_t sub = parse_expr(ctx, env);
            if (cur && cur->type == VAL_INT && sub.type == VAL_INT) {
                cur->int_val -= sub.int_val;
            }
            return;
        }

        /* Not an assignment — rewind and parse as expression */
        ctx->pos = save;
        ctx->current = save_tok;
        ctx->line = save_line;
    }

    /* Expression statement */
    parse_expr(ctx, env);
}

/* --- Built-in functions --- */

static ac_value_t builtin_print(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    /* Find the interpreter context — we stash it in parent's env */
    /* Use a global for simplicity */
    extern ac_interp_t *_ac_current;
    char buf[512];
    for (int i = 0; i < argc; i++) {
        val_to_str(args[i], buf, 512);
        if (_ac_current) emit(_ac_current, buf);
        if (i < argc - 1 && _ac_current) emit(_ac_current, " ");
    }
    if (_ac_current) emit(_ac_current, "\n");
    return val_null();
}

static ac_value_t builtin_len(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_int(0);
    if (args[0].type == VAL_STRING) return val_int((int)kstrlen(args[0].str_val));
    if (args[0].type == VAL_ARRAY) return val_int(args[0].arr_val->count);
    return val_int(0);
}

static ac_value_t builtin_str(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_str("", 0);
    char buf[256];
    val_to_str(args[0], buf, 256);
    return val_str(buf, (int)kstrlen(buf));
}

static ac_value_t builtin_int_cast(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_int(0);
    if (args[0].type == VAL_INT) return args[0];
    if (args[0].type == VAL_BOOL) return val_int(args[0].bool_val ? 1 : 0);
    if (args[0].type == VAL_STRING) {
        int v = 0;
        const char *s = args[0].str_val;
        bool neg = false;
        if (*s == '-') { neg = true; s++; }
        while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
        return val_int(neg ? -v : v);
    }
    return val_int(0);
}

static ac_value_t builtin_type(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_str("null", 4);
    switch (args[0].type) {
    case VAL_NULL: return val_str("null", 4);
    case VAL_INT: return val_str("int", 3);
    case VAL_BOOL: return val_str("bool", 4);
    case VAL_STRING: return val_str("string", 6);
    case VAL_ARRAY: return val_str("array", 5);
    case VAL_FN: return val_str("function", 8);
    default: return val_str("unknown", 7);
    }
}

static ac_value_t builtin_range(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_int(0);
    return args[0];  /* range(n) returns n for for-in iteration */
}

static ac_value_t builtin_abs(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1) return val_int(0);
    if (args[0].type == VAL_INT) return val_int(args[0].int_val < 0 ? -args[0].int_val : args[0].int_val);
    return val_int(0);
}

/* --- String methods --- */

static ac_value_t builtin_substr(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING) return val_str("", 0);
    int start = args[1].type == VAL_INT ? args[1].int_val : 0;
    int slen = (int)kstrlen(args[0].str_val);
    if (start < 0) start = 0;
    if (start >= slen) return val_str("", 0);
    int count = (argc >= 3 && args[2].type == VAL_INT) ? args[2].int_val : slen - start;
    if (start + count > slen) count = slen - start;
    if (count <= 0) return val_str("", 0);
    return val_str(args[0].str_val + start, count);
}

static ac_value_t builtin_indexof(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_int(-1);
    const char *hay = args[0].str_val;
    const char *needle = args[1].str_val;
    int hlen = (int)kstrlen(hay);
    int nlen = (int)kstrlen(needle);
    if (nlen == 0) return val_int(0);
    for (int i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (int j = 0; j < nlen; j++)
            if (hay[i+j] != needle[j]) { match = false; break; }
        if (match) return val_int(i);
    }
    return val_int(-1);
}

static ac_value_t builtin_split(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_null();
    const char *s = args[0].str_val;
    const char *delim = args[1].str_val;
    int dlen = (int)kstrlen(delim);
    if (dlen == 0) return val_null();

    ac_array_t *arr = (ac_array_t *)kmalloc(sizeof(ac_array_t));
    arr->count = 0; arr->capacity = 16;
    arr->items = (ac_value_t *)kmalloc(sizeof(ac_value_t) * 16);

    const char *start = s;
    while (*s) {
        bool found = true;
        for (int i = 0; i < dlen; i++)
            if (s[i] != delim[i]) { found = false; break; }
        if (found && arr->count < arr->capacity) {
            arr->items[arr->count++] = val_str(start, (int)(s - start));
            s += dlen;
            start = s;
        } else {
            s++;
        }
    }
    if (arr->count < arr->capacity)
        arr->items[arr->count++] = val_str(start, (int)(s - start));

    ac_value_t v; v.type = VAL_ARRAY; v.arr_val = arr;
    return v;
}

static ac_value_t builtin_join(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_STRING)
        return val_str("", 0);
    ac_array_t *arr = args[0].arr_val;
    const char *sep = args[1].str_val;
    int seplen = (int)kstrlen(sep);

    char buf[1024];
    int pos = 0;
    for (int i = 0; i < arr->count && pos < 1000; i++) {
        if (i > 0) { for (int j = 0; j < seplen && pos < 1000; j++) buf[pos++] = sep[j]; }
        char tmp[256];
        val_to_str(arr->items[i], tmp, 256);
        int tl = (int)kstrlen(tmp);
        for (int j = 0; j < tl && pos < 1000; j++) buf[pos++] = tmp[j];
    }
    buf[pos] = '\0';
    return val_str(buf, pos);
}

static ac_value_t builtin_upper(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1 || args[0].type != VAL_STRING) return val_str("", 0);
    int slen = (int)kstrlen(args[0].str_val);
    char *buf = (char *)kmalloc((uint32_t)(slen + 1));
    for (int i = 0; i < slen; i++) {
        buf[i] = (args[0].str_val[i] >= 'a' && args[0].str_val[i] <= 'z')
                 ? (char)(args[0].str_val[i] - 32) : args[0].str_val[i];
    }
    buf[slen] = '\0';
    ac_value_t v = val_str(buf, slen);
    kfree(buf);
    return v;
}

static ac_value_t builtin_lower(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1 || args[0].type != VAL_STRING) return val_str("", 0);
    int slen = (int)kstrlen(args[0].str_val);
    char *buf = (char *)kmalloc((uint32_t)(slen + 1));
    for (int i = 0; i < slen; i++) {
        buf[i] = (args[0].str_val[i] >= 'A' && args[0].str_val[i] <= 'Z')
                 ? (char)(args[0].str_val[i] + 32) : args[0].str_val[i];
    }
    buf[slen] = '\0';
    ac_value_t v = val_str(buf, slen);
    kfree(buf);
    return v;
}

static ac_value_t builtin_trim(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1 || args[0].type != VAL_STRING) return val_str("", 0);
    const char *s = args[0].str_val;
    int slen = (int)kstrlen(s);
    int start = 0, end = slen;
    while (start < end && (s[start] == ' ' || s[start] == '\t' || s[start] == '\n' || s[start] == '\r')) start++;
    while (end > start && (s[end-1] == ' ' || s[end-1] == '\t' || s[end-1] == '\n' || s[end-1] == '\r')) end--;
    return val_str(s + start, end - start);
}

static ac_value_t builtin_replace(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 3 || args[0].type != VAL_STRING || args[1].type != VAL_STRING || args[2].type != VAL_STRING)
        return argc >= 1 ? args[0] : val_str("", 0);
    const char *s = args[0].str_val;
    const char *from = args[1].str_val;
    const char *to = args[2].str_val;
    int slen = (int)kstrlen(s);
    int flen = (int)kstrlen(from);
    int tlen = (int)kstrlen(to);
    if (flen == 0) return args[0];

    char buf[1024];
    int pos = 0, i = 0;
    while (i < slen && pos < 1000) {
        bool found = true;
        if (i + flen <= slen) {
            for (int j = 0; j < flen; j++)
                if (s[i+j] != from[j]) { found = false; break; }
        } else found = false;
        if (found) {
            for (int j = 0; j < tlen && pos < 1000; j++) buf[pos++] = to[j];
            i += flen;
        } else {
            buf[pos++] = s[i++];
        }
    }
    buf[pos] = '\0';
    return val_str(buf, pos);
}

static ac_value_t builtin_startswith(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_bool(false);
    const char *s = args[0].str_val;
    const char *prefix = args[1].str_val;
    int plen = (int)kstrlen(prefix);
    for (int i = 0; i < plen; i++)
        if (s[i] != prefix[i]) return val_bool(false);
    return val_bool(true);
}

static ac_value_t builtin_endswith(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_STRING)
        return val_bool(false);
    int slen = (int)kstrlen(args[0].str_val);
    int plen = (int)kstrlen(args[1].str_val);
    if (plen > slen) return val_bool(false);
    for (int i = 0; i < plen; i++)
        if (args[0].str_val[slen - plen + i] != args[1].str_val[i]) return val_bool(false);
    return val_bool(true);
}

static ac_value_t builtin_char_at(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_STRING || args[1].type != VAL_INT)
        return val_str("", 0);
    int idx = args[1].int_val;
    int slen = (int)kstrlen(args[0].str_val);
    if (idx < 0 || idx >= slen) return val_str("", 0);
    return val_str(args[0].str_val + idx, 1);
}

/* --- Math builtins --- */

static ac_value_t builtin_min(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2) return val_int(0);
    if (args[0].type == VAL_INT && args[1].type == VAL_INT)
        return val_int(args[0].int_val < args[1].int_val ? args[0].int_val : args[1].int_val);
    return val_int(0);
}

static ac_value_t builtin_max(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2) return val_int(0);
    if (args[0].type == VAL_INT && args[1].type == VAL_INT)
        return val_int(args[0].int_val > args[1].int_val ? args[0].int_val : args[1].int_val);
    return val_int(0);
}

static ac_value_t builtin_clamp(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 3) return val_int(0);
    if (args[0].type == VAL_INT && args[1].type == VAL_INT && args[2].type == VAL_INT) {
        int v = args[0].int_val;
        if (v < args[1].int_val) v = args[1].int_val;
        if (v > args[2].int_val) v = args[2].int_val;
        return val_int(v);
    }
    return val_int(0);
}

static ac_value_t builtin_pow(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_INT || args[1].type != VAL_INT)
        return val_int(0);
    int base = args[0].int_val;
    int exp = args[1].int_val;
    if (exp < 0) return val_int(0);
    int result = 1;
    for (int i = 0; i < exp; i++) result *= base;
    return val_int(result);
}

/* --- Array builtins --- */

static ac_value_t builtin_push(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_ARRAY) return val_null();
    ac_array_t *arr = args[0].arr_val;
    if (arr->count >= arr->capacity) {
        int new_cap = arr->capacity * 2;
        ac_value_t *new_items = (ac_value_t *)kmalloc(sizeof(ac_value_t) * (uint32_t)new_cap);
        kmemcpy(new_items, arr->items, sizeof(ac_value_t) * (uint32_t)arr->count);
        kfree(arr->items);
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = args[1];
    return val_int(arr->count);
}

static ac_value_t builtin_pop(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 1 || args[0].type != VAL_ARRAY) return val_null();
    ac_array_t *arr = args[0].arr_val;
    if (arr->count <= 0) return val_null();
    return arr->items[--arr->count];
}

static ac_value_t builtin_get(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
        return val_null();
    int idx = args[1].int_val;
    ac_array_t *arr = args[0].arr_val;
    if (idx < 0 || idx >= arr->count) return val_null();
    return arr->items[idx];
}

static ac_value_t builtin_set(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 3 || args[0].type != VAL_ARRAY || args[1].type != VAL_INT)
        return val_null();
    int idx = args[1].int_val;
    ac_array_t *arr = args[0].arr_val;
    if (idx < 0 || idx >= arr->count) return val_null();
    arr->items[idx] = args[2];
    return args[2];
}

static ac_value_t builtin_contains(ac_env_t *env, ac_value_t *args, int argc)
{
    (void)env;
    if (argc < 2) return val_bool(false);
    if (args[0].type == VAL_STRING && args[1].type == VAL_STRING) {
        /* String contains */
        const char *hay = args[0].str_val;
        const char *needle = args[1].str_val;
        int hlen = (int)kstrlen(hay);
        int nlen = (int)kstrlen(needle);
        for (int i = 0; i <= hlen - nlen; i++) {
            bool m = true;
            for (int j = 0; j < nlen; j++)
                if (hay[i+j] != needle[j]) { m = false; break; }
            if (m) return val_bool(true);
        }
        return val_bool(false);
    }
    if (args[0].type == VAL_ARRAY) {
        ac_array_t *arr = args[0].arr_val;
        for (int i = 0; i < arr->count; i++) {
            if (arr->items[i].type == args[1].type) {
                if (args[1].type == VAL_INT && arr->items[i].int_val == args[1].int_val)
                    return val_bool(true);
                if (args[1].type == VAL_STRING && kstrcmp(arr->items[i].str_val, args[1].str_val) == 0)
                    return val_bool(true);
            }
        }
        return val_bool(false);
    }
    return val_bool(false);
}

/* Global interpreter pointer for builtins */
ac_interp_t *_ac_current = NULL;

/* --- Public API --- */

ac_interp_t *ac_create(void)
{
    ac_interp_t *ctx = (ac_interp_t *)kmalloc(sizeof(ac_interp_t));
    kmemset(ctx, 0, sizeof(ac_interp_t));
    ctx->global_env = env_create(NULL);

    /* Register builtins */
    ac_value_t fn;
    fn.type = VAL_FN; fn.fn_val = NULL;

    fn.native_fn = builtin_print;  env_set(ctx->global_env, "print", 5, fn);
    fn.native_fn = builtin_len;    env_set(ctx->global_env, "len", 3, fn);
    fn.native_fn = builtin_str;    env_set(ctx->global_env, "str", 3, fn);
    fn.native_fn = builtin_int_cast; env_set(ctx->global_env, "int", 3, fn);
    fn.native_fn = builtin_type;   env_set(ctx->global_env, "type", 4, fn);
    fn.native_fn = builtin_range;  env_set(ctx->global_env, "range", 5, fn);
    fn.native_fn = builtin_abs;    env_set(ctx->global_env, "abs", 3, fn);

    /* String builtins */
    fn.native_fn = builtin_substr;    env_set(ctx->global_env, "substr", 6, fn);
    fn.native_fn = builtin_indexof;   env_set(ctx->global_env, "indexof", 7, fn);
    fn.native_fn = builtin_split;     env_set(ctx->global_env, "split", 5, fn);
    fn.native_fn = builtin_join;      env_set(ctx->global_env, "join", 4, fn);
    fn.native_fn = builtin_upper;     env_set(ctx->global_env, "upper", 5, fn);
    fn.native_fn = builtin_lower;     env_set(ctx->global_env, "lower", 5, fn);
    fn.native_fn = builtin_trim;      env_set(ctx->global_env, "trim", 4, fn);
    fn.native_fn = builtin_replace;   env_set(ctx->global_env, "replace", 7, fn);
    fn.native_fn = builtin_startswith; env_set(ctx->global_env, "startswith", 10, fn);
    fn.native_fn = builtin_endswith;  env_set(ctx->global_env, "endswith", 8, fn);
    fn.native_fn = builtin_char_at;   env_set(ctx->global_env, "char_at", 7, fn);
    fn.native_fn = builtin_contains;  env_set(ctx->global_env, "contains", 8, fn);

    /* Math builtins */
    fn.native_fn = builtin_min;   env_set(ctx->global_env, "min", 3, fn);
    fn.native_fn = builtin_max;   env_set(ctx->global_env, "max", 3, fn);
    fn.native_fn = builtin_clamp; env_set(ctx->global_env, "clamp", 5, fn);
    fn.native_fn = builtin_pow;   env_set(ctx->global_env, "pow", 3, fn);

    /* Array builtins */
    fn.native_fn = builtin_push;  env_set(ctx->global_env, "push", 4, fn);
    fn.native_fn = builtin_pop;   env_set(ctx->global_env, "pop", 3, fn);
    fn.native_fn = builtin_get;   env_set(ctx->global_env, "get", 3, fn);
    fn.native_fn = builtin_set;   env_set(ctx->global_env, "set", 3, fn);

    return ctx;
}

void ac_destroy(ac_interp_t *ctx)
{
    if (ctx->global_env) env_destroy(ctx->global_env);
    kfree(ctx);
}

void ac_set_output(ac_interp_t *ctx, ac_output_fn fn, void *data)
{
    ctx->output = fn;
    ctx->output_data = data;
}

int ac_exec(ac_interp_t *ctx, const char *source)
{
    ctx->source = source;
    ctx->source_len = (int)kstrlen(source);
    ctx->pos = source;
    ctx->line = 1;
    ctx->has_error = false;
    ctx->should_break = false;
    ctx->should_continue = false;
    ctx->should_return = false;

    _ac_current = ctx;
    next_token(ctx);

    while (ctx->current.type != TOK_EOF && !ctx->has_error) {
        parse_stmt(ctx, ctx->global_env);
    }

    _ac_current = NULL;
    return ctx->has_error ? -1 : 0;
}

const char *ac_get_error(ac_interp_t *ctx)
{
    return ctx->has_error ? ctx->error : NULL;
}

void ac_register_native(ac_interp_t *ctx, const char *name, ac_native_fn fn)
{
    ac_value_t fval;
    fval.type = VAL_FN;
    fval.fn_val = NULL;
    fval.native_fn = fn;
    env_set(ctx->global_env, name, (int)kstrlen(name), fval);
}
