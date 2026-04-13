/*
 *  This top comment is the only human-written thing in this
 *  entire file. please read the README.md file, thanks!
 *                                        - the Rbn. Std. Ct.
 */

/*
 * clorn.c - reborn to c99 transpiler
 * implements revision 26013 of the reborn standard (RS)
 *
 * pipeline: source -> lexer -> tokens -> parser -> ast -> codegen -> c99
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ============================================================
 * UTILITIES
 * ============================================================ */

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "reborn: fatal: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

static char *xstrdup(const char *s) {
    if (!s) return xstrdup("(null)");
    char *r = strdup(s);
    if (!r) die("out of memory");
    return r;
}

static void *xmalloc(size_t n) {
    void *p = malloc(n);
    if (!p) die("out of memory");
    return p;
}

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n, sz);
    if (!p) die("out of memory");
    return p;
}

static void *xrealloc(void *p, size_t n) {
    void *r = realloc(p, n);
    if (!r) die("out of memory");
    return r;
}

/* ============================================================
 * LEXER
 * ============================================================ */

typedef enum {
    /* literals */
    TK_INT_LIT,
    TK_FLOAT_LIT,
    TK_STRING_LIT,
    TK_CHAR_LIT,
    TK_BOOL_LIT,

    /* identifiers */
    TK_IDENT,

    /* keywords */
    TK_LET,
    TK_CONST,
    TK_UNSIGNED,
    TK_EXTERN,
    TK_STATIC,
    TK_INLINE,
    TK_RETURN,
    TK_IMPORT,
    TK_FROM,
    TK_IF,
    TK_ELIF,
    TK_ELSE,
    TK_FOR,
    TK_WHILE,
    TK_STRUCT,
    TK_ENUM,
    TK_UNION,
    TK_SIZEOF,
    TK_TYPEOF,

    /* types */
    TK_TYPE_INT,
    TK_TYPE_FLOAT,
    TK_TYPE_CHAR,
    TK_TYPE_BOOL,
    TK_TYPE_STRING,
    TK_TYPE_VOID,

    /* operators / punctuation */
    TK_COLONASSIGN,  /* := */
    TK_ARROW,        /* -> */
    TK_CAST,         /* --> */
    TK_DCOLON,       /* :: */
    TK_COLON,        /* : */
    TK_ASSIGN,       /* = */
    TK_EQ,           /* == or =: */
    TK_NEQ,          /* != */
    TK_LT,           /* < */
    TK_GT,           /* > */
    TK_LTE,          /* <= */
    TK_GTE,          /* >= */
    TK_NOT_GT,       /* !> resolves to < */
    TK_NOT_LT,       /* !< resolves to > */
    TK_AND,          /* && */
    TK_OR,           /* || */
    TK_BITAND,       /* & */
    TK_BITOR,        /* | */
    TK_PLUS,         /* + */
    TK_MINUS,        /* - */
    TK_STAR,         /* * */
    TK_SLASH,        /* / */
    TK_PERCENT,      /* % */
    TK_INC,          /* ++ */
    TK_DEC,          /* -- */
    TK_NOT,          /* ! */
    TK_AT,           /* @ (deref) */
    TK_DOT,          /* . */
    TK_SEMICOLON,    /* ; */
    TK_COMMA,        /* , */
    TK_LPAREN,       /* ( */
    TK_RPAREN,       /* ) */
    TK_LBRACE,       /* { */
    TK_RBRACE,       /* } */
    TK_LBRACKET,     /* [ */
    TK_RBRACKET,     /* ] */

    TK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    char     *text;   /* owned copy */
    int       line;
    int       col;
} Token;

typedef struct {
    const char *src;
    size_t      pos;
    int         line;
    int         col;
    /* token array (dynamic) */
    Token      *tokens;
    size_t      tok_len;
    size_t      tok_cap;
} Lexer;

static void lex_push(Lexer *l, TokenKind k, const char *text, int line, int col) {
    if (l->tok_len >= l->tok_cap) {
        l->tok_cap = l->tok_cap ? l->tok_cap * 2 : 256;
        l->tokens  = xrealloc(l->tokens, l->tok_cap * sizeof(Token));
    }
    l->tokens[l->tok_len++] = (Token){ k, xstrdup(text), line, col };
}

static TokenKind keyword_kind(const char *s) {
    if (!strcmp(s, "let"))      return TK_LET;
    if (!strcmp(s, "const"))    return TK_CONST;
    if (!strcmp(s, "unsigned")) return TK_UNSIGNED;
    if (!strcmp(s, "extern"))   return TK_EXTERN;
    if (!strcmp(s, "static"))   return TK_STATIC;
    if (!strcmp(s, "inline"))   return TK_INLINE;
    if (!strcmp(s, "return"))   return TK_RETURN;
    if (!strcmp(s, "import"))   return TK_IMPORT;
    if (!strcmp(s, "from"))     return TK_FROM;
    if (!strcmp(s, "if"))       return TK_IF;
    if (!strcmp(s, "elif"))     return TK_ELIF;
    if (!strcmp(s, "else"))     return TK_ELSE;
    if (!strcmp(s, "for"))      return TK_FOR;
    if (!strcmp(s, "while"))    return TK_WHILE;
    if (!strcmp(s, "struct"))   return TK_STRUCT;
    if (!strcmp(s, "enum"))     return TK_ENUM;
    if (!strcmp(s, "union"))    return TK_UNION;
    if (!strcmp(s, "sizeof"))   return TK_SIZEOF;
    if (!strcmp(s, "typeof"))   return TK_TYPEOF;
    if (!strcmp(s, "int"))      return TK_TYPE_INT;
    if (!strcmp(s, "float"))    return TK_TYPE_FLOAT;
    if (!strcmp(s, "char"))     return TK_TYPE_CHAR;
    if (!strcmp(s, "bool"))     return TK_TYPE_BOOL;
    if (!strcmp(s, "string"))   return TK_TYPE_STRING;
    if (!strcmp(s, "void"))     return TK_TYPE_VOID;
    if (!strcmp(s, "true"))     return TK_BOOL_LIT;
    if (!strcmp(s, "false"))    return TK_BOOL_LIT;
    return TK_IDENT;
}

static void lex(Lexer *l) {
    const char *s = l->src;
    size_t i = l->pos;
    int line = l->line, col = l->col;

#define ADVANCE() do { if (s[i]=='\n'){line++;col=1;}else{col++;} i++; } while(0)
#define PEEK()    (s[i])
#define PEEK1()   (s[i+1])
#define CURLINE   line
#define CURCOL    col

    while (s[i]) {
        /* skip whitespace */
        if (isspace((unsigned char)s[i])) { ADVANCE(); continue; }

        /* single line comment */
        if (s[i]=='/' && s[i+1]=='/') {
            while (s[i] && s[i]!='\n') ADVANCE();
            continue;
        }
        /* multi-line comment */
        if (s[i]=='/' && s[i+1]=='*') {
            ADVANCE(); ADVANCE();
            while (s[i] && !(s[i]=='*' && s[i+1]=='/')) ADVANCE();
            if (s[i]) { ADVANCE(); ADVANCE(); }
            continue;
        }

        int tl = CURLINE, tc = CURCOL;

        /* string literal */
        if (s[i]=='"') {
            ADVANCE();
            size_t start = i;
            while (s[i] && s[i]!='"') {
                if (s[i]=='\\') ADVANCE();
                ADVANCE();
            }
            size_t len = i - start;
            char *buf = xmalloc(len + 3);
            buf[0] = '"';
            memcpy(buf+1, s+start, len);
            buf[len+1] = '"';
            buf[len+2] = '\0';
            lex_push(l, TK_STRING_LIT, buf, tl, tc);
            free(buf);
            if (s[i]=='"') ADVANCE();
            continue;
        }

        /* char literal */
        if (s[i]=='\'') {
            ADVANCE();
            size_t start = i;
            while (s[i] && s[i]!='\'') {
                if (s[i]=='\\') ADVANCE();
                ADVANCE();
            }
            size_t len = i - start;
            char *buf = xmalloc(len + 3);
            buf[0] = '\'';
            memcpy(buf+1, s+start, len);
            buf[len+1] = '\'';
            buf[len+2] = '\0';
            lex_push(l, TK_CHAR_LIT, buf, tl, tc);
            free(buf);
            if (s[i]=='\'') ADVANCE();
            continue;
        }

        /* numeric literal */
        if (isdigit((unsigned char)s[i])) {
            size_t start = i;
            int is_float = 0;
            while (isdigit((unsigned char)s[i]) || s[i]=='.') {
                if (s[i]=='.') is_float = 1;
                ADVANCE();
            }
            /* optional f/F suffix */
            if (s[i]=='f' || s[i]=='F') { is_float = 1; ADVANCE(); }
            size_t len = i - start;
            char *buf = xmalloc(len + 1);
            memcpy(buf, s+start, len);
            buf[len] = '\0';
            lex_push(l, is_float ? TK_FLOAT_LIT : TK_INT_LIT, buf, tl, tc);
            free(buf);
            continue;
        }

        /* identifier or keyword */
        if (isalpha((unsigned char)s[i]) || s[i]=='_') {
            size_t start = i;
            while (isalnum((unsigned char)s[i]) || s[i]=='_') ADVANCE();
            size_t len = i - start;
            char *buf = xmalloc(len + 1);
            memcpy(buf, s+start, len);
            buf[len] = '\0';
            TokenKind k = keyword_kind(buf);
            lex_push(l, k, buf, tl, tc);
            free(buf);
            continue;
        }

        /* multi-char operators — order matters (longer first) */
        /* --> cast */
        if (s[i]=='-' && s[i+1]=='-' && s[i+2]=='>') {
            ADVANCE(); ADVANCE(); ADVANCE();
            lex_push(l, TK_CAST, "-->", tl, tc);
            continue;
        }
        /* -> arrow */
        if (s[i]=='-' && s[i+1]=='>') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_ARROW, "->", tl, tc);
            continue;
        }
        /* := */
        if (s[i]==':' && s[i+1]=='=') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_COLONASSIGN, ":=", tl, tc);
            continue;
        }
        /* :: */
        if (s[i]==':' && s[i+1]==':') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_DCOLON, "::", tl, tc);
            continue;
        }
        /* =: (equality) */
        if (s[i]=='=' && s[i+1]==':') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_EQ, "=:", tl, tc);
            continue;
        }
        /* == */
        if (s[i]=='=' && s[i+1]=='=') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_EQ, "==", tl, tc);
            continue;
        }
        /* != */
        if (s[i]=='!' && s[i+1]=='=') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_NEQ, "!=", tl, tc);
            continue;
        }
        /* !> */
        if (s[i]=='!' && s[i+1]=='>') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_NOT_GT, "!>", tl, tc);
            continue;
        }
        /* !< */
        if (s[i]=='!' && s[i+1]=='<') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_NOT_LT, "!<", tl, tc);
            continue;
        }
        /* <= */
        if (s[i]=='<' && s[i+1]=='=') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_LTE, "<=", tl, tc);
            continue;
        }
        /* >= */
        if (s[i]=='>' && s[i+1]=='=') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_GTE, ">=", tl, tc);
            continue;
        }
        /* && */
        if (s[i]=='&' && s[i+1]=='&') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_AND, "&&", tl, tc);
            continue;
        }
        /* || */
        if (s[i]=='|' && s[i+1]=='|') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_OR, "||", tl, tc);
            continue;
        }
        /* ++ */
        if (s[i]=='+' && s[i+1]=='+') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_INC, "++", tl, tc);
            continue;
        }
        /* -- */
        if (s[i]=='-' && s[i+1]=='-') {
            ADVANCE(); ADVANCE();
            lex_push(l, TK_DEC, "--", tl, tc);
            continue;
        }

        /* single char tokens */
        char buf2[2] = { s[i], '\0' };
        TokenKind k2;
        switch (s[i]) {
            case ':': k2 = TK_COLON;     break;
            case '=': k2 = TK_ASSIGN;    break;
            case '<': k2 = TK_LT;        break;
            case '>': k2 = TK_GT;        break;
            case '!': k2 = TK_NOT;       break;
            case '&': k2 = TK_BITAND;    break;
            case '|': k2 = TK_BITOR;     break;
            case '+': k2 = TK_PLUS;      break;
            case '-': k2 = TK_MINUS;     break;
            case '*': k2 = TK_STAR;      break;
            case '/': k2 = TK_SLASH;     break;
            case '%': k2 = TK_PERCENT;   break;
            case '@': k2 = TK_AT;        break;
            case '.': k2 = TK_DOT;       break;
            case ';': k2 = TK_SEMICOLON; break;
            case ',': k2 = TK_COMMA;     break;
            case '(': k2 = TK_LPAREN;    break;
            case ')': k2 = TK_RPAREN;    break;
            case '{': k2 = TK_LBRACE;    break;
            case '}': k2 = TK_RBRACE;    break;
            case '[': k2 = TK_LBRACKET;  break;
            case ']': k2 = TK_RBRACKET;  break;
            default:
                fprintf(stderr, "reborn: warning: unknown char '%c' at line %d col %d, skipping\n",
                        s[i], tl, tc);
                ADVANCE();
                continue;
        }
        lex_push(l, k2, buf2, tl, tc);
        ADVANCE();
    }

    lex_push(l, TK_EOF, "", line, col);

#undef ADVANCE
#undef PEEK
#undef PEEK1
#undef CURLINE
#undef CURCOL
}

/* ============================================================
 * AST NODES
 * ============================================================ */

typedef enum {
    /* declarations */
    ND_VAR_DECL,
    ND_FUNC_DECL,     /* forward declaration */
    ND_FUNC_DEF,
    ND_PARAM,
    ND_STRUCT_DEF,
    ND_ENUM_DEF,
    ND_UNION_DEF,
    ND_EXTERN_BLOCK,
    ND_IMPORT,

    /* statements */
    ND_BLOCK,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_FOR_RANGE,
    ND_EXPR_STMT,
    ND_ASSIGN,

    /* expressions */
    ND_BINOP,
    ND_UNOP,
    ND_CALL,
    ND_IDENT,
    ND_INT_LIT,
    ND_FLOAT_LIT,
    ND_STRING_LIT,
    ND_CHAR_LIT,
    ND_BOOL_LIT,
    ND_CAST,
    ND_INDEX,
    ND_MEMBER,
    ND_SIZEOF,
    ND_TYPEOF,

    ND_PROGRAM,
} NodeKind;

/* forward */
struct Node;
typedef struct Node Node;

/* dynamic node list */
typedef struct {
    Node  **items;
    size_t  len;
    size_t  cap;
} NodeList;

static void nodelist_push(NodeList *nl, Node *n) {
    if (nl->len >= nl->cap) {
        nl->cap = nl->cap ? nl->cap * 2 : 8;
        nl->items = xrealloc(nl->items, nl->cap * sizeof(Node*));
    }
    nl->items[nl->len++] = n;
}

struct Node {
    NodeKind kind;
    int      line;

    /* type string (owned), used where relevant */
    char    *type_str;   /* e.g. "int", "float", "*int", "void", ... */
    char    *name;       /* identifier name */

    /* attributes */
    int      is_const;
    int      is_unsigned;
    int      is_static;
    int      is_inline;
    int      is_array;
    int      is_pointer;
    int      is_inferred; /* := form */

    /* children */
    Node    *left;
    Node    *right;
    Node    *cond;
    Node    *init;
    Node    *post;
    Node    *body;
    Node    *else_branch;
    NodeList params;
    NodeList stmts;    /* block body / struct members / enum members */
    NodeList args;     /* call arguments */
    NodeList branches; /* elif/else chain for if */

    /* literal / operator value */
    char    *value;

    /* for extern block: raw C string */
    char    *raw_c;
};

static Node *node_new(NodeKind kind, int line) {
    Node *n = xcalloc(1, sizeof(Node));
    n->kind = kind;
    n->line = line;
    return n;
}

/* ============================================================
 * PARSER
 * ============================================================ */

typedef struct {
    Token  *tokens;
    size_t  pos;
    size_t  len;
    /* error reporting */
    int     had_error;
} Parser;

static Token *p_peek(Parser *p) { return &p->tokens[p->pos]; }
static Token *p_peek1(Parser *p) {
    if (p->pos + 1 < p->len) return &p->tokens[p->pos + 1];
    return &p->tokens[p->len - 1]; /* EOF */
}

static Token *p_advance(Parser *p) {
    Token *t = &p->tokens[p->pos];
    if (p->pos + 1 < p->len) p->pos++;
    return t;
}

static int p_check(Parser *p, TokenKind k) {
    return p_peek(p)->kind == k;
}

static int p_match(Parser *p, TokenKind k) {
    if (p_check(p, k)) { p_advance(p); return 1; }
    return 0;
}

static Token *p_expect(Parser *p, TokenKind k, const char *msg) {
    if (p_check(p, k)) return p_advance(p);
    Token *t = p_peek(p);
    fprintf(stderr, "reborn: error: line %d col %d: %s (got '%s')\n",
            t->line, t->col, msg, t->text);
    p->had_error = 1;
    return t; /* try to recover */
}

/* forward declarations */
static Node *parse_stmt(Parser *p);
static Node *parse_expr(Parser *p);
static Node *parse_expr_prec(Parser *p, int min_prec);
static Node *parse_let(Parser *p);
static Node *parse_block(Parser *p);
static char *parse_type(Parser *p);

/* parse a type string: [* | unsigned] base_type [ [] ] */
static char *parse_type(Parser *p) {
    /* collect into a buffer */
    char buf[256] = {0};
    int ptr = 0;
    int unsig = 0;

    if (p_match(p, TK_UNSIGNED)) { unsig = 1; }
    if (p_match(p, TK_STAR))     { ptr = 1; }

    Token *t = p_peek(p);
    const char *base = NULL;
    switch (t->kind) {
        case TK_TYPE_INT:    base = "int";    p_advance(p); break;
        case TK_TYPE_FLOAT:  base = "float";  p_advance(p); break;
        case TK_TYPE_CHAR:   base = "char";   p_advance(p); break;
        case TK_TYPE_BOOL:   base = "bool";   p_advance(p); break;
        case TK_TYPE_STRING: base = "string"; p_advance(p); break;
        case TK_TYPE_VOID:   base = "void";   p_advance(p); break;
        case TK_IDENT:       base = t->text;  p_advance(p); break;
        default:
            fprintf(stderr, "reborn: error: line %d: expected type, got '%s'\n",
                    t->line, t->text);
            p->had_error = 1;
            return xstrdup("int");
    }

    if (unsig) {
        snprintf(buf, sizeof(buf), "unsigned %s", base);
    } else {
        snprintf(buf, sizeof(buf), "%s", base);
    }
    if (ptr) {
        size_t l = strlen(buf);
        buf[l] = '*'; buf[l+1] = '\0';
    }
    /* array suffix */
    if (p_check(p, TK_LBRACKET) && p_peek1(p)->kind == TK_RBRACKET) {
        p_advance(p); p_advance(p);
        strncat(buf, "[]", sizeof(buf)-strlen(buf)-1);
    }
    return xstrdup(buf);
}

/* parse attribute list before identifier: static, inline, const, unsigned */
static void parse_attrs(Parser *p, Node *n) {
    for (;;) {
        if (p_match(p, TK_STATIC))   { n->is_static   = 1; continue; }
        if (p_match(p, TK_INLINE))   { n->is_inline   = 1; continue; }
        if (p_match(p, TK_CONST))    { n->is_const    = 1; continue; }
        if (p_match(p, TK_UNSIGNED)) { n->is_unsigned = 1; continue; }
        break;
    }
}

/* parse parameter list: (p1: type, p2: type) */
static void parse_params(Parser *p, Node *fn) {
    p_expect(p, TK_LPAREN, "expected '(' in function definition");
    while (!p_check(p, TK_RPAREN) && !p_check(p, TK_EOF)) {
        Node *param = node_new(ND_PARAM, p_peek(p)->line);

        /* optional * for pointer param */
        if (p_match(p, TK_STAR)) param->is_pointer = 1;

        Token *name = p_expect(p, TK_IDENT, "expected parameter name");
        param->name = xstrdup(name->text);
        p_expect(p, TK_COLON, "expected ':' after parameter name");
        param->type_str = parse_type(p);
        nodelist_push(&fn->params, param);
        if (!p_match(p, TK_COMMA)) break;
    }
    p_expect(p, TK_RPAREN, "expected ')' to close parameter list");
}

/* parse block { stmt* } */
static Node *parse_block(Parser *p) {
    int line = p_peek(p)->line;
    p_expect(p, TK_LBRACE, "expected '{'");
    Node *blk = node_new(ND_BLOCK, line);
    while (!p_check(p, TK_RBRACE) && !p_check(p, TK_EOF)) {
        Node *s = parse_stmt(p);
        if (s) nodelist_push(&blk->stmts, s);
    }
    p_expect(p, TK_RBRACE, "expected '}'");
    return blk;
}

/* parse a braced block OR a single braceless statement (R6.1)
 * always returns an ND_BLOCK node for uniformity in codegen */
static Node *parse_block_or_stmt(Parser *p) {
    if (p_check(p, TK_LBRACE)) {
        return parse_block(p);
    }
    /* braceless: wrap the single statement in a synthetic block */
    int line = p_peek(p)->line;
    Node *blk = node_new(ND_BLOCK, line);
    blk->stmts.items = NULL;
    blk->stmts.len = 0;
    blk->stmts.cap = 0;
    Node *s = parse_stmt(p);
    if (s) nodelist_push(&blk->stmts, s);
    return blk;
}

/* let statement parsing — handles vars, functions, structs, enums, unions */
static Node *parse_let(Parser *p) {
    int line = p_peek(p)->line;
    p_expect(p, TK_LET, "expected 'let'");

    /* check for struct/enum/union */
    if (p_check(p, TK_STRUCT) || p_check(p, TK_ENUM) || p_check(p, TK_UNION)) {
        TokenKind sk = p_peek(p)->kind;
        p_advance(p);
        NodeKind nk = sk == TK_STRUCT ? ND_STRUCT_DEF
                    : sk == TK_ENUM   ? ND_ENUM_DEF
                                      : ND_UNION_DEF;
        Node *sn = node_new(nk, line);

        /* optional name before block (let struct Name { }) */
        if (p_check(p, TK_IDENT)) {
            sn->name = xstrdup(p_peek(p)->text);
            p_advance(p);
        }

        p_expect(p, TK_LBRACE, "expected '{' in struct/enum/union definition");
        while (!p_check(p, TK_RBRACE) && !p_check(p, TK_EOF)) {
            /* members are also 'let' declared */
            int mline = p_peek(p)->line;
            if (p_match(p, TK_LET)) {
                Node *mem = node_new(ND_VAR_DECL, mline);
                Token *mn = p_expect(p, TK_IDENT, "expected member name");
                mem->name = xstrdup(mn->text);
                if (p_match(p, TK_COLON)) {
                    mem->type_str = parse_type(p);
                } else if (p_match(p, TK_COLONASSIGN)) {
                    /* enum explicit value */
                    mem->right = parse_expr(p);
                    mem->is_inferred = 1;
                }
                p_expect(p, TK_SEMICOLON, "expected ';' after member");
                nodelist_push(&sn->stmts, mem);
            } else {
                /* skip unexpected token */
                p_advance(p);
            }
        }
        p_expect(p, TK_RBRACE, "expected '}' closing struct/enum/union");

        /* name after block: } Name; */
        if (!sn->name && p_check(p, TK_IDENT)) {
            sn->name = xstrdup(p_peek(p)->text);
            p_advance(p);
        }
        p_expect(p, TK_SEMICOLON, "expected ';' after struct/enum/union definition");
        return sn;
    }

    Node *n = node_new(ND_VAR_DECL, line);
    parse_attrs(p, n);

    /* pointer marker */
    if (p_match(p, TK_STAR)) n->is_pointer = 1;

    /* identifier */
    Token *id = p_expect(p, TK_IDENT, "expected identifier after 'let'");
    n->name = xstrdup(id->text);

    /* array marker: identifier[] */
    if (p_check(p, TK_LBRACKET) && p_peek1(p)->kind == TK_RBRACKET) {
        p_advance(p); p_advance(p);
        n->is_array = 1;
    }

    /* := (inferred type) */
    if (p_match(p, TK_COLONASSIGN)) {
        n->is_inferred = 1;

        /* function definition/declaration: := (params) { body } or := (params); */
        if (p_check(p, TK_LPAREN)) {
            n->kind = ND_FUNC_DEF;
            parse_params(p, n);

            if (p_check(p, TK_LBRACE)) {
                n->body = parse_block(p);
                /* no semicolon after } per R5.1 */
            } else {
                /* forward declaration */
                n->kind = ND_FUNC_DECL;
                p_expect(p, TK_SEMICOLON, "expected ';' after function forward declaration");
            }
        } else {
            /* variable with inferred type */
            n->right = parse_expr(p);
            p_expect(p, TK_SEMICOLON, "expected ';' after variable declaration");
        }
        return n;
    }

    /* : type = ... (explicit type) or : type; (no initializer) */
    if (p_match(p, TK_COLON)) {
        n->type_str = parse_type(p);

        /* no initializer: let x: Type; */
        if (p_match(p, TK_SEMICOLON)) {
            return n;
        }

        p_expect(p, TK_ASSIGN, "expected '=' after type");

        /* function: : type = (params) {...} */
        if (p_check(p, TK_LPAREN)) {
            n->kind = ND_FUNC_DEF;
            parse_params(p, n);

            if (p_check(p, TK_LBRACE)) {
                n->body = parse_block(p);
            } else {
                n->kind = ND_FUNC_DECL;
                p_expect(p, TK_SEMICOLON, "expected ';' after function forward declaration");
            }
        } else {
            n->right = parse_expr(p);
            p_expect(p, TK_SEMICOLON, "expected ';' after variable declaration");
        }
        return n;
    }

    fprintf(stderr, "reborn: error: line %d: expected ':=' or ': type =' after identifier '%s'\n",
            line, n->name);
    p->had_error = 1;
    return n;
}


/* import statement */
static Node *parse_import(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p); /* consume 'import' */
    Node *n = node_new(ND_IMPORT, line);
    n->name = xstrdup("import");

    if (p_match(p, TK_LBRACE)) {
        /* import { a, b } */
        while (!p_check(p, TK_RBRACE) && !p_check(p, TK_EOF)) {
            /* just skip for now */
            p_advance(p);
            p_match(p, TK_COMMA);
        }
        p_expect(p, TK_RBRACE, "expected '}' after import list");
    } else {
        /* import name; or import "file"; */
        p_advance(p); /* skip name/string */
    }
    p_match(p, TK_SEMICOLON);
    return n;
}

/* from rsl import { printf(), ... } */
static Node *parse_from(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p); /* consume 'from' */
    Node *n = node_new(ND_IMPORT, line);
    n->name = xstrdup("from");
    /* skip everything until semicolon or closing brace */
    while (!p_check(p, TK_SEMICOLON) && !p_check(p, TK_EOF)) p_advance(p);
    p_match(p, TK_SEMICOLON);
    return n;
}

/* extern block / declaration */
static Node *parse_extern(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p); /* consume 'extern' */
    Node *n = node_new(ND_EXTERN_BLOCK, line);

    /* consume "C" */
    if (p_check(p, TK_STRING_LIT)) p_advance(p);

    if (p_check(p, TK_LBRACE)) {
        /* extern "C" { ... } — grab raw tokens until matching } */
        p_advance(p);
        /* we'll just skip for now and note it in output */
        int depth = 1;
        size_t start = p->pos;
        while (!p_check(p, TK_EOF) && depth > 0) {
            if (p_check(p, TK_LBRACE)) depth++;
            else if (p_check(p, TK_RBRACE)) depth--;
            if (depth > 0) p_advance(p);
        }
        (void)start;
        p_expect(p, TK_RBRACE, "expected '}' to close extern block");
        n->raw_c = xstrdup("/* extern C block — not yet emitted */");
    } else {
        /* single extern decl: type name(params); */
        /* just skip to semicolon */
        while (!p_check(p, TK_SEMICOLON) && !p_check(p, TK_EOF)) p_advance(p);
        p_match(p, TK_SEMICOLON);
        n->raw_c = xstrdup("/* extern declaration — not yet emitted */");
    }
    return n;
}

/* if / elif / else */
static Node *parse_if(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p); /* consume 'if' */
    Node *n = node_new(ND_IF, line);

    /* optional parens around condition */
    int has_paren = p_match(p, TK_LPAREN);
    n->cond = parse_expr(p);
    if (has_paren) p_expect(p, TK_RPAREN, "expected ')' after if condition");

    n->body = parse_block_or_stmt(p);

    /* elif chain */
    while (p_check(p, TK_ELIF)) {
        int el = p_peek(p)->line;
        p_advance(p);
        Node *elif = node_new(ND_IF, el);
        has_paren = p_match(p, TK_LPAREN);
        elif->cond = parse_expr(p);
        if (has_paren) p_expect(p, TK_RPAREN, "expected ')' after elif condition");
        elif->body = parse_block_or_stmt(p);
        nodelist_push(&n->branches, elif);
    }

    if (p_match(p, TK_ELSE)) {
        n->else_branch = parse_block_or_stmt(p);
    }
    return n;
}

/* while loop */
static Node *parse_while(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p);
    Node *n = node_new(ND_WHILE, line);
    int has_paren = p_match(p, TK_LPAREN);
    n->cond = parse_expr(p);
    if (has_paren) p_expect(p, TK_RPAREN, "expected ')' after while condition");
    n->body = parse_block_or_stmt(p);
    return n;
}

/* for loop and for::n */
static Node *parse_for(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p);

    /* for::n range syntax */
    if (p_match(p, TK_DCOLON)) {
        Node *n = node_new(ND_FOR_RANGE, line);
        n->value = xstrdup(p_peek(p)->text);
        p_advance(p);
        n->body = parse_block_or_stmt(p);
        return n;
    }

    Node *n = node_new(ND_FOR, line);
    int has_paren = p_match(p, TK_LPAREN);

    /* init */
    if (!p_check(p, TK_SEMICOLON)) {
        if (p_check(p, TK_LET)) {
            n->init = parse_let(p);
            /* parse_let already eats the semicolon */
        } else {
            n->init = parse_expr(p);
            p_expect(p, TK_SEMICOLON, "expected ';' after for init");
        }
    } else {
        p_advance(p);
    }

    /* condition */
    if (!p_check(p, TK_SEMICOLON)) n->cond = parse_expr(p);
    p_expect(p, TK_SEMICOLON, "expected ';' after for condition");

    /* post */
    if (has_paren ? !p_check(p, TK_RPAREN) : !p_check(p, TK_LBRACE))
        n->post = parse_expr(p);
    if (has_paren) p_expect(p, TK_RPAREN, "expected ')' after for clauses");

    n->body = parse_block_or_stmt(p);
    return n;
}

/* return statement */
static Node *parse_return(Parser *p) {
    int line = p_peek(p)->line;
    p_advance(p);
    Node *n = node_new(ND_RETURN, line);
    if (!p_check(p, TK_SEMICOLON)) n->left = parse_expr(p);
    p_expect(p, TK_SEMICOLON, "expected ';' after return");
    return n;
}

static Node *parse_stmt(Parser *p) {
    Token *t = p_peek(p);

    switch (t->kind) {
        case TK_LET:    return parse_let(p);
        case TK_RETURN: return parse_return(p);
        case TK_IF:     return parse_if(p);
        case TK_WHILE:  return parse_while(p);
        case TK_FOR:    return parse_for(p);
        case TK_EXTERN: return parse_extern(p);
        case TK_IMPORT: return parse_import(p);
        case TK_FROM:   return parse_from(p);
        case TK_LBRACE: return parse_block(p);
        default: {
            /* expression statement */
            Node *n = node_new(ND_EXPR_STMT, t->line);
            n->left = parse_expr(p);
            p_expect(p, TK_SEMICOLON, "expected ';' after expression");
            return n;
        }
    }
}

/* ============================================================
 * EXPRESSION PARSER (pratt-style precedence climbing)
 * ============================================================ */

static int get_prec(TokenKind k) {
    switch (k) {
        case TK_ASSIGN:   return 1;
        case TK_OR:       return 2;
        case TK_AND:      return 3;
        case TK_BITOR:    return 4;
        case TK_BITAND:   return 5;
        case TK_EQ:
        case TK_NEQ:      return 6;
        case TK_LT:
        case TK_GT:
        case TK_LTE:
        case TK_GTE:
        case TK_NOT_GT:
        case TK_NOT_LT:   return 7;
        case TK_PLUS:
        case TK_MINUS:    return 8;
        case TK_STAR:
        case TK_SLASH:
        case TK_PERCENT:  return 9;
        case TK_CAST:     return 10;
        case TK_DOT:
        case TK_ARROW:    return 11;
        default:          return 0;
    }
}

static const char *tok_op_str(TokenKind k) {
    switch (k) {
        case TK_PLUS:    return "+";
        case TK_MINUS:   return "-";
        case TK_STAR:    return "*";
        case TK_SLASH:   return "/";
        case TK_PERCENT: return "%";
        case TK_EQ:      return "==";
        case TK_NEQ:     return "!=";
        case TK_LT:      return "<";
        case TK_GT:      return ">";
        case TK_LTE:     return "<=";
        case TK_GTE:     return ">=";
        case TK_NOT_GT:  return "<";   /* !> resolves to < */
        case TK_NOT_LT:  return ">";   /* !< resolves to > */
        case TK_AND:     return "&&";
        case TK_OR:      return "||";
        case TK_BITAND:  return "&";
        case TK_BITOR:   return "|";
        case TK_ASSIGN:  return "=";
        case TK_DOT:     return ".";
        case TK_ARROW:   return "->";
        default:         return "?";
    }
}

/* parse a primary expression */
static Node *parse_primary(Parser *p) {
    Token *t = p_peek(p);
    int line = t->line;

    /* parenthesised expression */
    if (p_match(p, TK_LPAREN)) {
        Node *n = parse_expr(p);
        p_expect(p, TK_RPAREN, "expected ')'");
        return n;
    }

    /* unary operators */
    if (t->kind == TK_MINUS || t->kind == TK_NOT || t->kind == TK_STAR ||
        t->kind == TK_BITAND || t->kind == TK_AT ||
        t->kind == TK_INC || t->kind == TK_DEC) {
        p_advance(p);
        Node *n = node_new(ND_UNOP, line);
        n->value = xstrdup(t->text);
        n->left  = parse_primary(p);
        return n;
    }

    /* sizeof / typeof */
    if (t->kind == TK_SIZEOF || t->kind == TK_TYPEOF) {
        NodeKind nk = t->kind == TK_SIZEOF ? ND_SIZEOF : ND_TYPEOF;
        p_advance(p);
        Node *n = node_new(nk, line);
        p_expect(p, TK_LPAREN, "expected '(' after sizeof/typeof");
        /* argument may be a type keyword or an expression */
        Token *arg = p_peek(p);
        if (arg->kind == TK_TYPE_INT || arg->kind == TK_TYPE_FLOAT ||
            arg->kind == TK_TYPE_CHAR || arg->kind == TK_TYPE_BOOL ||
            arg->kind == TK_TYPE_STRING || arg->kind == TK_TYPE_VOID) {
            /* it's a type name — store it as an ident node */
            Node *tn = node_new(ND_IDENT, arg->line);
            tn->name = xstrdup(arg->text);
            n->left = tn;
            p_advance(p);
        } else {
            n->left = parse_expr(p);
        }
        p_expect(p, TK_RPAREN, "expected ')' after sizeof/typeof argument");
        return n;
    }

    /* literals */
    if (t->kind == TK_INT_LIT) {
        p_advance(p);
        Node *n = node_new(ND_INT_LIT, line);
        n->value = xstrdup(t->text);
        return n;
    }
    if (t->kind == TK_FLOAT_LIT) {
        p_advance(p);
        Node *n = node_new(ND_FLOAT_LIT, line);
        n->value = xstrdup(t->text);
        return n;
    }
    if (t->kind == TK_STRING_LIT) {
        p_advance(p);
        Node *n = node_new(ND_STRING_LIT, line);
        n->value = xstrdup(t->text);
        return n;
    }
    if (t->kind == TK_CHAR_LIT) {
        p_advance(p);
        Node *n = node_new(ND_CHAR_LIT, line);
        n->value = xstrdup(t->text);
        return n;
    }
    if (t->kind == TK_BOOL_LIT) {
        p_advance(p);
        Node *n = node_new(ND_BOOL_LIT, line);
        n->value = xstrdup(t->text);
        return n;
    }

    /* identifier or function call */
    if (t->kind == TK_IDENT) {
        p_advance(p);
        /* function call */
        if (p_check(p, TK_LPAREN)) {
            p_advance(p);
            Node *n = node_new(ND_CALL, line);
            n->name = xstrdup(t->text);
            while (!p_check(p, TK_RPAREN) && !p_check(p, TK_EOF)) {
                nodelist_push(&n->args, parse_expr(p));
                if (!p_match(p, TK_COMMA)) break;
            }
            p_expect(p, TK_RPAREN, "expected ')' to close function call");
            return n;
        }
        /* post-increment/decrement */
        Node *n = node_new(ND_IDENT, line);
        n->name = xstrdup(t->text);
        if (p_check(p, TK_INC) || p_check(p, TK_DEC)) {
            Token *op = p_advance(p);
            Node *post = node_new(ND_UNOP, line);
            post->value = xstrdup(op->kind == TK_INC ? "post++" : "post--");
            post->left = n;
            return post;
        }
        return n;
    }

    /* if nothing matched, skip and return dummy */
    fprintf(stderr, "reborn: error: line %d: unexpected token '%s' in expression\n",
            line, t->text);
    p->had_error = 1;
    p_advance(p);
    Node *dummy = node_new(ND_INT_LIT, line);
    dummy->value = xstrdup("0");
    return dummy;
}

static Node *parse_expr_prec(Parser *p, int min_prec) {
    Node *left = parse_primary(p);

    for (;;) {
        Token *t = p_peek(p);
        int prec = get_prec(t->kind);
        if (prec < min_prec || prec == 0) break;

        /* cast: expr --> type */
        if (t->kind == TK_CAST) {
            p_advance(p);
            Node *n = node_new(ND_CAST, t->line);
            n->left = left;
            n->type_str = parse_type(p);
            left = n;
            continue;
        }

        /* array index: expr[expr] */
        if (t->kind == TK_LBRACKET) {
            p_advance(p);
            Node *n = node_new(ND_INDEX, t->line);
            n->left  = left;
            n->right = parse_expr(p);
            p_expect(p, TK_RBRACKET, "expected ']'");
            left = n;
            continue;
        }

        p_advance(p);
        Node *n = node_new(ND_BINOP, t->line);
        n->value = xstrdup(tok_op_str(t->kind));
        n->left  = left;
        n->right = parse_expr_prec(p, prec + 1);
        left = n;
    }
    return left;
}

static Node *parse_expr(Parser *p) {
    return parse_expr_prec(p, 1);
}

/* top-level program parser */
static Node *parse_program(Parser *p) {
    Node *prog = node_new(ND_PROGRAM, 0);

    while (!p_check(p, TK_EOF)) {
        Token *t = p_peek(p);

        if (t->kind == TK_LET) {
            nodelist_push(&prog->stmts, parse_let(p));
        } else if (t->kind == TK_IDENT && !strcmp(t->text, "main")) {
            /* main := () { } or main: void = () { } */
            if (p_peek1(p)->kind == TK_COLONASSIGN || p_peek1(p)->kind == TK_COLON) {
                Node *mn = node_new(ND_FUNC_DEF, t->line);
                p_advance(p); /* consume 'main' */
                mn->name = xstrdup("main");
                mn->type_str = xstrdup("int");

                if (p_match(p, TK_COLON)) {
                    /* main: type = () */
                    free(mn->type_str);
                    mn->type_str = parse_type(p);
                    p_expect(p, TK_ASSIGN, "expected '=' in main definition");
                } else {
                    /* main := () */
                    p_expect(p, TK_COLONASSIGN, "expected ':=' in main definition");
                }
                parse_params(p, mn);
                mn->body = parse_block(p);
                nodelist_push(&prog->stmts, mn);
            } else {
                nodelist_push(&prog->stmts, parse_stmt(p));
            }
        } else if (t->kind == TK_IMPORT) {
            nodelist_push(&prog->stmts, parse_import(p));
        } else if (t->kind == TK_FROM) {
            nodelist_push(&prog->stmts, parse_from(p));
        } else if (t->kind == TK_EXTERN) {
            nodelist_push(&prog->stmts, parse_extern(p));
        } else {
            fprintf(stderr, "reborn: error: line %d: unexpected top-level token '%s'\n",
                    t->line, t->text);
            p->had_error = 1;
            p_advance(p);
        }
    }
    return prog;
}

/* ============================================================
 * CODE GENERATION (AST -> C99)
 * ============================================================ */

typedef struct {
    FILE *out;
    int   indent;
    int   in_main;   /* 1 if we're inside main() */
    int   in_void;   /* 1 if current function is void */
} CGen;

static void emit(CGen *g, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g->out, fmt, ap);
    va_end(ap);
}

static void emit_indent(CGen *g) {
    for (int i = 0; i < g->indent; i++) fprintf(g->out, "    ");
}

/* map reborn type string to C type string */
static const char *c_type(const char *t) {
    if (!t) return "int";
    if (!strcmp(t, "string"))  return "char*";
    if (!strcmp(t, "bool"))    return "int";
    /* everything else maps 1:1 */
    return t;
}

static void gen_node(CGen *g, Node *n);

static void gen_expr(CGen *g, Node *n) {
    if (!n) return;
    switch (n->kind) {
        case ND_INT_LIT:    emit(g, "%s", n->value); break;
        case ND_FLOAT_LIT:  emit(g, "%s", n->value); break;
        case ND_STRING_LIT: emit(g, "%s", n->value); break;
        case ND_CHAR_LIT:   emit(g, "%s", n->value); break;
        case ND_BOOL_LIT:
            emit(g, "%s", !strcmp(n->value, "true") ? "1" : "0");
            break;
        case ND_IDENT:
            emit(g, "%s", n->name);
            break;
        case ND_BINOP:
            /* member access: no spaces, no parens */
            if (!strcmp(n->value, ".") || !strcmp(n->value, "->")) {
                gen_expr(g, n->left);
                emit(g, "%s", n->value);
                gen_expr(g, n->right);
            } else if (!strcmp(n->value, "=")) {
                /* assignment: no outer parens needed */
                gen_expr(g, n->left);
                emit(g, " = ");
                gen_expr(g, n->right);
            } else {
                emit(g, "(");
                gen_expr(g, n->left);
                emit(g, " %s ", n->value);
                gen_expr(g, n->right);
                emit(g, ")");
            }
            break;
        case ND_UNOP:
            if (!strcmp(n->value, "post++")) {
                gen_expr(g, n->left); emit(g, "++");
            } else if (!strcmp(n->value, "post--")) {
                gen_expr(g, n->left); emit(g, "--");
            } else if (!strcmp(n->value, "@")) {
                /* @ is dereference in reborn */
                emit(g, "*("); gen_expr(g, n->left); emit(g, ")");
            } else {
                emit(g, "%s(", n->value);
                gen_expr(g, n->left);
                emit(g, ")");
            }
            break;
        case ND_CALL:
            emit(g, "%s(", n->name);
            for (size_t i = 0; i < n->args.len; i++) {
                if (i) emit(g, ", ");
                gen_expr(g, n->args.items[i]);
            }
            emit(g, ")");
            break;
        case ND_CAST:
            emit(g, "((%s)(", c_type(n->type_str));
            gen_expr(g, n->left);
            emit(g, "))");
            break;
        case ND_INDEX:
            gen_expr(g, n->left);
            emit(g, "[");
            gen_expr(g, n->right);
            emit(g, "]");
            break;
        case ND_MEMBER:
            gen_expr(g, n->left);
            emit(g, ".%s", n->name);
            break;
        case ND_SIZEOF:
            emit(g, "sizeof(");
            gen_expr(g, n->left);
            emit(g, ")");
            break;
        case ND_TYPEOF:
            /* typeof returns a string — not directly in C99, emit a comment */
            emit(g, "/* typeof */ \"?\"");
            break;
        default:
            gen_node(g, n);
            break;
    }
}

static void gen_type_prefix(CGen *g, Node *n) {
    if (n->is_static)   emit(g, "static ");
    if (n->is_inline)   emit(g, "inline ");
    if (n->is_const)    emit(g, "const ");
    if (n->is_unsigned) emit(g, "unsigned ");
}

/* emit a C type for a variable, handling arrays and pointers */
static void emit_var_type(CGen *g, Node *n, const char *name) {
    const char *ct = c_type(n->type_str ? n->type_str : "int");
    if (n->is_pointer) {
        emit(g, "%s *%s", ct, name);
    } else if (n->is_array) {
        emit(g, "%s %s[]", ct, name);
    } else {
        emit(g, "%s %s", ct, name);
    }
}

static void gen_node(CGen *g, Node *n) {
    if (!n) return;

    switch (n->kind) {

        case ND_IMPORT:
            /* stub: emit a comment */
            emit_indent(g);
            emit(g, "/* import (stubbed) */\n");
            break;

        case ND_EXTERN_BLOCK:
            emit_indent(g);
            emit(g, "%s\n", n->raw_c ? n->raw_c : "");
            break;

        case ND_VAR_DECL: {
            emit_indent(g);
            gen_type_prefix(g, n);
            /* inferred type: use C23 `auto` directly — this is exactly what := maps to */
            if (n->is_inferred && !n->type_str) {
                if (n->is_pointer)    emit(g, "auto *%s", n->name);
                else if (n->is_array) emit(g, "auto %s[]", n->name);
                else                  emit(g, "auto %s", n->name);
            } else {
                emit_var_type(g, n, n->name);
            }
            if (n->right) {
                emit(g, " = ");
                gen_expr(g, n->right);
            }
            emit(g, ";\n");
            break;
        }

        case ND_FUNC_DECL: {
            /* forward declaration */
            emit_indent(g);
            gen_type_prefix(g, n);
            emit(g, "%s %s(", c_type(n->type_str ? n->type_str : "void"), n->name);
            for (size_t i = 0; i < n->params.len; i++) {
                if (i) emit(g, ", ");
                Node *pm = n->params.items[i];
                emit(g, "%s %s%s", c_type(pm->type_str), pm->is_pointer ? "*" : "", pm->name);
            }
            if (n->params.len == 0) emit(g, "void");
            emit(g, ");\n");
            break;
        }

        case ND_FUNC_DEF: {
            emit_indent(g);
            gen_type_prefix(g, n);

            const char *ret = c_type(n->type_str ? n->type_str : "void");

            /* special case: main should return int */
            if (!strcmp(n->name, "main")) ret = "int";

            emit(g, "%s %s(", ret, n->name);
            for (size_t i = 0; i < n->params.len; i++) {
                if (i) emit(g, ", ");
                Node *pm = n->params.items[i];
                emit(g, "%s %s%s", c_type(pm->type_str), pm->is_pointer ? "*" : "", pm->name);
            }
            if (n->params.len == 0) emit(g, "void");
            emit(g, ")\n");
            /* track context for return codegen */
            int prev_main = g->in_main;
            int prev_void = g->in_void;
            g->in_main = !strcmp(n->name, "main");
            g->in_void = (strcmp(ret, "int") != 0 && strcmp(ret, "float") != 0 &&
                          strcmp(ret, "char") != 0 && strcmp(ret, "char*") != 0 &&
                          strcmp(ret, "bool") != 0);
            gen_node(g, n->body);
            g->in_main = prev_main;
            g->in_void = prev_void;
            emit(g, "\n");
            break;
        }

        case ND_BLOCK: {
            emit_indent(g);
            emit(g, "{\n");
            g->indent++;
            for (size_t i = 0; i < n->stmts.len; i++) {
                gen_node(g, n->stmts.items[i]);
            }
            g->indent--;
            emit_indent(g);
            emit(g, "}\n");
            break;
        }

        case ND_RETURN:
            emit_indent(g);
            if (n->left) {
                emit(g, "return ");
                gen_expr(g, n->left);
                emit(g, ";\n");
            } else if (g->in_main) {
                emit(g, "return 0;\n");
            } else {
                emit(g, "return;\n");
            }
            break;

        case ND_IF: {
            emit_indent(g);
            emit(g, "if (");
            gen_expr(g, n->cond);
            emit(g, ")\n");
            gen_node(g, n->body);
            for (size_t i = 0; i < n->branches.len; i++) {
                Node *elif = n->branches.items[i];
                emit_indent(g);
                emit(g, "else if (");
                gen_expr(g, elif->cond);
                emit(g, ")\n");
                gen_node(g, elif->body);
            }
            if (n->else_branch) {
                emit_indent(g);
                emit(g, "else\n");
                gen_node(g, n->else_branch);
            }
            break;
        }

        case ND_WHILE:
            emit_indent(g);
            emit(g, "while (");
            gen_expr(g, n->cond);
            emit(g, ")\n");
            gen_node(g, n->body);
            break;

        case ND_FOR:
            emit_indent(g);
            emit(g, "for (");
            /* init — suppress trailing ;\n from var decl by detecting it */
            if (n->init) {
                if (n->init->kind == ND_VAR_DECL) {
                    /* gen without newline: a bit hacky, we redirect to emit inline */
                    /* emit type + name + init inline */
                    Node *vd = n->init;
                    gen_type_prefix(g, vd);
                    if (vd->is_inferred && !vd->type_str) emit(g, "int");
                    else emit(g, "%s", c_type(vd->type_str ? vd->type_str : "int"));
                    emit(g, " %s", vd->name);
                    if (vd->right) { emit(g, " = "); gen_expr(g, vd->right); }
                } else {
                    gen_expr(g, n->init);
                }
            }
            emit(g, "; ");
            if (n->cond) gen_expr(g, n->cond);
            emit(g, "; ");
            if (n->post) gen_expr(g, n->post);
            emit(g, ")\n");
            gen_node(g, n->body);
            break;

        case ND_FOR_RANGE: {
            /* for::n  =>  for (int _i = 1; _i < n; _i++) */
            emit_indent(g);
            emit(g, "for (int _i = 1; _i < %s; _i++)\n", n->value);
            gen_node(g, n->body);
            break;
        }

        case ND_EXPR_STMT:
            emit_indent(g);
            gen_expr(g, n->left);
            emit(g, ";\n");
            break;

        case ND_STRUCT_DEF: {
            emit_indent(g);
            emit(g, "typedef struct {\n");
            g->indent++;
            for (size_t i = 0; i < n->stmts.len; i++) {
                Node *mem = n->stmts.items[i];
                emit_indent(g);
                emit(g, "%s %s;\n", c_type(mem->type_str ? mem->type_str : "int"), mem->name);
            }
            g->indent--;
            emit_indent(g);
            emit(g, "} %s;\n", n->name ? n->name : "_anon_struct");
            break;
        }

        case ND_ENUM_DEF: {
            emit_indent(g);
            emit(g, "typedef enum {\n");
            g->indent++;
            for (size_t i = 0; i < n->stmts.len; i++) {
                Node *mem = n->stmts.items[i];
                emit_indent(g);
                emit(g, "%s", mem->name);
                if (mem->right) {
                    emit(g, " = ");
                    gen_expr(g, mem->right);
                }
                if (i + 1 < n->stmts.len) emit(g, ",");
                emit(g, "\n");
            }
            g->indent--;
            emit_indent(g);
            emit(g, "} %s;\n", n->name ? n->name : "_anon_enum");
            break;
        }

        case ND_UNION_DEF: {
            emit_indent(g);
            emit(g, "typedef union {\n");
            g->indent++;
            for (size_t i = 0; i < n->stmts.len; i++) {
                Node *mem = n->stmts.items[i];
                emit_indent(g);
                emit(g, "%s %s;\n", c_type(mem->type_str ? mem->type_str : "int"), mem->name);
            }
            g->indent--;
            emit_indent(g);
            emit(g, "} %s;\n", n->name ? n->name : "_anon_union");
            break;
        }

        case ND_PROGRAM: {
            emit(g, "/* generated by clorn (reborn revision 26013) — target: POSIX/ISO C23 */\n");
            emit(g, "#include <stdio.h>\n");
            emit(g, "#include <stdlib.h>\n");
            emit(g, "#include <string.h>\n\n");
            for (size_t i = 0; i < n->stmts.len; i++) {
                gen_node(g, n->stmts.items[i]);
            }
            break;
        }

        default:
            emit_indent(g);
            emit(g, "/* unhandled node kind %d */\n", n->kind);
            break;
    }
}

/* ============================================================
 * ENTRY POINT
 * ============================================================ */

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) die("cannot open '%s'", path);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = xmalloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: reborn <source.rn> [output.c]\n");
        return 1;
    }

    char *src = read_file(argv[1]);

    /* lex */
    Lexer lexer = { .src = src, .pos = 0, .line = 1, .col = 1 };
    lex(&lexer);

    /* parse */
    Parser parser = { .tokens = lexer.tokens, .pos = 0, .len = lexer.tok_len };
    Node *ast = parse_program(&parser);

    if (parser.had_error) {
        fprintf(stderr, "reborn: compilation failed due to errors\n");
        return 1;
    }

    /* codegen */
    FILE *out = stdout;
    if (argc >= 3) {
        out = fopen(argv[2], "w");
        if (!out) die("cannot open output file '%s'", argv[2]);
    }

    CGen cgen = { .out = out, .indent = 0 };
    gen_node(&cgen, ast);

    if (out != stdout) fclose(out);
    free(src);
    return 0;
}
