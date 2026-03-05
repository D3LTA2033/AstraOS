/* ==========================================================================
 * AstraOS - Astracor Programming Language
 * ==========================================================================
 * Astracor is AstraOS's built-in programming language. It features:
 *   - Dynamic typing (int, float, string, bool, array)
 *   - C-like syntax with modern conveniences
 *   - Functions (fn keyword)
 *   - Control flow: if/elif/else, while, for
 *   - Built-in print, input, len, str, int, type
 *   - Can run interpreted (immediate) or compiled to bytecode
 *
 * Example:
 *   fn greet(name) {
 *       print("Hello, " + name + "!")
 *   }
 *   greet("World")
 * ========================================================================== */

#ifndef _ASTRA_KERNEL_ASTRACOR_H
#define _ASTRA_KERNEL_ASTRACOR_H

#include <kernel/types.h>

/* Token types */
typedef enum {
    TOK_EOF, TOK_NEWLINE,
    /* Literals */
    TOK_INT, TOK_STRING, TOK_IDENT, TOK_FLOAT_LIT,
    /* Keywords */
    TOK_FN, TOK_LET, TOK_IF, TOK_ELIF, TOK_ELSE,
    TOK_WHILE, TOK_FOR, TOK_IN, TOK_RETURN,
    TOK_TRUE, TOK_FALSE, TOK_NULL,
    TOK_BREAK, TOK_CONTINUE, TOK_IMPORT,
    /* Operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LTE, TOK_GTE,
    TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
    TOK_AND, TOK_OR, TOK_NOT,
    /* Delimiters */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_COMMA, TOK_DOT, TOK_COLON, TOK_SEMICOLON,
    TOK_ARROW,  /* -> */
} token_type_t;

/* Token */
typedef struct {
    token_type_t type;
    const char  *start;
    int          length;
    int          line;
    int          int_val;   /* For TOK_INT */
} token_t;

/* Value types */
typedef enum {
    VAL_NULL,
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_ARRAY,
    VAL_FN,
} val_type_t;

/* Forward declarations */
struct ac_value;
struct ac_env;

/* Array */
typedef struct {
    struct ac_value *items;
    int count;
    int capacity;
} ac_array_t;

/* Function */
typedef struct {
    const char *name;
    const char *body_start;   /* Pointer into source */
    int         body_len;
    char      **params;
    int         param_count;
} ac_func_t;

/* Native function */
typedef struct ac_value (*ac_native_fn)(struct ac_env *env, struct ac_value *args, int argc);

/* Value */
typedef struct ac_value {
    val_type_t type;
    union {
        int          int_val;
        int          float_val;  /* Fixed-point: val * 1000 */
        bool         bool_val;
        char        *str_val;
        ac_array_t  *arr_val;
        ac_func_t   *fn_val;
        ac_native_fn native_fn;
    };
} ac_value_t;

/* Environment (variable scope) */
#define AC_ENV_MAX 64
typedef struct ac_env {
    char        *names[AC_ENV_MAX];
    ac_value_t   values[AC_ENV_MAX];
    int          count;
    struct ac_env *parent;
} ac_env_t;

/* Output callback — the interpreter calls this for print() */
typedef void (*ac_output_fn)(const char *text, void *userdata);

/* Interpreter context */
typedef struct {
    const char  *source;
    int          source_len;
    const char  *pos;
    int          line;
    token_t      current;
    ac_env_t    *global_env;
    ac_output_fn output;
    void        *output_data;
    char         error[256];
    bool         has_error;
    bool         should_break;
    bool         should_continue;
    bool         should_return;
    ac_value_t   return_val;
} ac_interp_t;

/* Create and destroy interpreter */
ac_interp_t *ac_create(void);
void ac_destroy(ac_interp_t *interp);

/* Set output callback */
void ac_set_output(ac_interp_t *interp, ac_output_fn fn, void *data);

/* Execute source code. Returns 0 on success, -1 on error. */
int ac_exec(ac_interp_t *interp, const char *source);

/* Get error message after failed exec */
const char *ac_get_error(ac_interp_t *interp);

/* Register a native function */
void ac_register_native(ac_interp_t *interp, const char *name, ac_native_fn fn);

#endif /* _ASTRA_KERNEL_ASTRACOR_H */
