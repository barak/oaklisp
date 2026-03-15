// This file is part of Oaklisp.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// The GNU GPL is available at http://www.gnu.org/licenses/gpl.html
// or from the Free Software Foundation, 59 Temple Place - Suite 330,
// Boston, MA 02111-1307, USA

/*
 * oak-cold-linker.c — Standalone C bootstrap linker for Oaklisp.
 *
 * Replaces tool.oak for cold world generation, breaking the circular
 * dependency that requires a running Oaklisp to build a cold world.
 *
 * Usage: oak-cold-linker [--64bit|--32bit] -o OUTBASE file1 file2 ...
 *
 * (C) Barak A. Pearlmutter, Kevin J. Lang, 1986-2025.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>

/* MSVC names strdup with a leading underscore. */
#if defined(_MSC_VER) && !defined(strdup)
#define strdup _strdup
#endif

/* ================================================================
 * Constants (from tool.oak)
 * ================================================================ */

#define CELL_SIZE           1
#define PAIR_SIZE           3
#define SYMBOL_SIZE         2
#define METHOD_SIZE         3
#define NULL_SIZE           1
#define T_SIZE              1
#define TYPE_SIZE           9
#define COERCABLE_TYPE_SIZE 10

#define INT_TAG  0
#define IMM_TAG  1
#define LOC_TAG  2
#define PTR_TAG  3

#define RETURN_OPCODE  (24 * 256)   /* 6144 = 0x1800 */
#define NOOP_OPCODE    0

#define VALUE_STACK_SIZE    0x1ABC
#define CONTEXT_STACK_SIZE  0x2ABC

#define REG_CODE_DELTA  4
#define TOP_CODE_DELTA  (-2)

/* ================================================================
 * S-expression types
 * ================================================================ */

enum sexp_type {
    S_NIL, S_INT, S_SYM, S_CHAR, S_STRING, S_PAIR, S_BOOL
};

typedef struct sexp {
    enum sexp_type type;
    union {
        long long ival;       /* S_INT */
        char *sval;           /* S_SYM, S_STRING */
        int cval;             /* S_CHAR */
        int bval;             /* S_BOOL: 1=#t */
        struct {
            struct sexp *car;
            struct sexp *cdr;
        } pair;               /* S_PAIR */
    } u;
} sexp_t;

/* ================================================================
 * S-expression pool allocator
 * ================================================================ */

#define SEXP_POOL_BLOCK 8192
static sexp_t **sexp_pools = NULL;
static int sexp_pool_count = 0;
static int sexp_pool_used = SEXP_POOL_BLOCK; /* force first alloc */

static sexp_t *sexp_alloc(void)
{
    if (sexp_pool_used >= SEXP_POOL_BLOCK) {
        sexp_pools = realloc(sexp_pools,
                             (sexp_pool_count + 1) * sizeof(sexp_t *));
        sexp_pools[sexp_pool_count] = malloc(SEXP_POOL_BLOCK * sizeof(sexp_t));
        sexp_pool_count++;
        sexp_pool_used = 0;
    }
    return &sexp_pools[sexp_pool_count - 1][sexp_pool_used++];
}

static sexp_t sexp_nil_val = { .type = S_NIL };
#define SEXP_NIL (&sexp_nil_val)

static sexp_t *make_int(long long v)
{
    sexp_t *s = sexp_alloc();
    s->type = S_INT;
    s->u.ival = v;
    return s;
}

static sexp_t *make_sym(const char *name)
{
    sexp_t *s = sexp_alloc();
    s->type = S_SYM;
    s->u.sval = strdup(name);
    return s;
}

static sexp_t *make_char(int c)
{
    sexp_t *s = sexp_alloc();
    s->type = S_CHAR;
    s->u.cval = c;
    return s;
}

static sexp_t *make_string(const char *str)
{
    sexp_t *s = sexp_alloc();
    s->type = S_STRING;
    s->u.sval = strdup(str);
    return s;
}

static sexp_t *make_bool(int val)
{
    sexp_t *s = sexp_alloc();
    s->type = S_BOOL;
    s->u.bval = val;
    return s;
}

static sexp_t *make_pair(sexp_t *car, sexp_t *cdr)
{
    sexp_t *s = sexp_alloc();
    s->type = S_PAIR;
    s->u.pair.car = car;
    s->u.pair.cdr = cdr;
    return s;
}

/* List length */
static int sexp_list_len(sexp_t *s)
{
    int n = 0;
    while (s && s->type == S_PAIR) {
        n++;
        s = s->u.pair.cdr;
    }
    return n;
}

/* nth element (0-based) */
static sexp_t *sexp_nth(sexp_t *s, int n)
{
    for (int i = 0; i < n && s && s->type == S_PAIR; i++)
        s = s->u.pair.cdr;
    return (s && s->type == S_PAIR) ? s->u.pair.car : NULL;
}

/* ================================================================
 * S-expression parser
 * ================================================================ */

typedef struct {
    FILE *fp;
    int ungot;
    int has_ungot;
} parser_t;

static void parser_init(parser_t *p, FILE *fp)
{
    p->fp = fp;
    p->ungot = 0;
    p->has_ungot = 0;
}

static int pgetc(parser_t *p)
{
    if (p->has_ungot) {
        p->has_ungot = 0;
        return p->ungot;
    }
    return fgetc(p->fp);
}

static void pungetc(parser_t *p, int c)
{
    p->ungot = c;
    p->has_ungot = 1;
}

static void skip_whitespace(parser_t *p)
{
    int c;
    for (;;) {
        c = pgetc(p);
        if (c == ';') {
            /* skip comment to end of line */
            while ((c = pgetc(p)) != '\n' && c != EOF)
                ;
        } else if (!isspace(c)) {
            pungetc(p, c);
            return;
        }
    }
}

static sexp_t *parse_sexp(parser_t *p);

/* Upcase a string in-place */
static void upcase_str(char *s)
{
    for (; *s; s++)
        *s = toupper((unsigned char)*s);
}

static sexp_t *parse_atom(parser_t *p, int first_char)
{
    char buf[4096];
    int len = 0;
    buf[len++] = first_char;

    for (;;) {
        int c = pgetc(p);
        if (c == EOF || isspace(c) || c == '(' || c == ')') {
            if (c != EOF) pungetc(p, c);
            break;
        }
        if (len < (int)sizeof(buf) - 1)
            buf[len++] = c;
    }
    buf[len] = '\0';

    /* Try to parse as integer */
    if ((buf[0] == '-' && len > 1 && isdigit((unsigned char)buf[1])) ||
        isdigit((unsigned char)buf[0])) {
        char *end;
        long long val = strtoll(buf, &end, 10);
        if (*end == '\0')
            return make_int(val);
    }

    /* It's a symbol — upcase it */
    upcase_str(buf);
    return make_sym(buf);
}

static sexp_t *parse_hash(parser_t *p)
{
    int c = pgetc(p);

    if (c == '[') {
        /* #[symbol "NAME"] syntax — Oaklisp printed representation */
        /* Read type name (e.g., "symbol") */
        char type_buf[64];
        int tlen = 0;
        skip_whitespace(p);
        for (;;) {
            c = pgetc(p);
            if (c == EOF || isspace(c)) break;
            if (tlen < (int)sizeof(type_buf) - 1)
                type_buf[tlen++] = c;
        }
        type_buf[tlen] = '\0';

        /* Read the string name */
        skip_whitespace(p);
        c = pgetc(p);
        char name_buf[4096];
        int nlen = 0;
        if (c == '"') {
            for (;;) {
                c = pgetc(p);
                if (c == '\\') {
                    c = pgetc(p);
                    if (c == '"') {
                        if (nlen < (int)sizeof(name_buf) - 1)
                            name_buf[nlen++] = '"';
                    } else {
                        if (nlen < (int)sizeof(name_buf) - 1)
                            name_buf[nlen++] = '\\';
                        if (c != EOF && nlen < (int)sizeof(name_buf) - 1)
                            name_buf[nlen++] = c;
                    }
                } else if (c == '"' || c == EOF) {
                    break;
                } else {
                    if (nlen < (int)sizeof(name_buf) - 1)
                        name_buf[nlen++] = c;
                }
            }
        }
        name_buf[nlen] = '\0';

        /* Skip to closing ] */
        while ((c = pgetc(p)) != ']' && c != EOF)
            ;

        /* For #[symbol "NAME"], return a symbol with that name */
        upcase_str(type_buf);
        if (!strcmp(type_buf, "SYMBOL"))
            return make_sym(name_buf);  /* Keep original case for name */

        fprintf(stderr, "Unknown #[%s ...] syntax\n", type_buf);
        exit(1);
    }

    if (c == '\\') {
        /* Character literal */
        char buf[32];
        int len = 0;
        c = pgetc(p);
        if (c == EOF)
            return make_char(0);
        buf[len++] = c;

        /* Check for multi-char name */
        int c2 = pgetc(p);
        if (c2 != EOF && !isspace(c2) && c2 != '(' && c2 != ')') {
            buf[len++] = c2;
            for (;;) {
                c2 = pgetc(p);
                if (c2 == EOF || isspace(c2) || c2 == '(' || c2 == ')') {
                    if (c2 != EOF) pungetc(p, c2);
                    break;
                }
                if (len < (int)sizeof(buf) - 1)
                    buf[len++] = c2;
            }
        } else {
            if (c2 != EOF) pungetc(p, c2);
        }
        buf[len] = '\0';

        if (len == 1)
            return make_char((unsigned char)buf[0]);

        upcase_str(buf);
        if (!strcmp(buf, "NEWLINE") || !strcmp(buf, "NL"))
            return make_char(10);
        if (!strcmp(buf, "SPACE") || !strcmp(buf, "SP"))
            return make_char(32);
        if (!strcmp(buf, "TAB"))
            return make_char(9);
        if (!strcmp(buf, "RETURN") || !strcmp(buf, "CR"))
            return make_char(13);
        if (!strcmp(buf, "BACKSPACE") || !strcmp(buf, "BS"))
            return make_char(8);
        if (!strcmp(buf, "DELETE") || !strcmp(buf, "DEL") || !strcmp(buf, "RUBOUT"))
            return make_char(127);
        if (!strcmp(buf, "ESCAPE") || !strcmp(buf, "ESC") || !strcmp(buf, "ALTMODE"))
            return make_char(27);
        if (!strcmp(buf, "NULL") || !strcmp(buf, "NUL"))
            return make_char(0);
        if (!strcmp(buf, "BELL") || !strcmp(buf, "BEL"))
            return make_char(7);
        if (!strcmp(buf, "PAGE") || !strcmp(buf, "FF"))
            return make_char(12);
        /* Oaklisp-specific character names */
        if (!strcmp(buf, "COERCER"))
            return make_char(25);   /* ASCII 25, used for #^ coercion syntax */
        if (!strcmp(buf, "FLUID"))
            return make_char(22);   /* ASCII 22, used for #* fluid syntax */

        fprintf(stderr, "Unknown character name: #\\%s\n", buf);
        exit(1);
    }

    if (c == 'T' || c == 't') {
        /* Check it's not part of a longer token */
        int c2 = pgetc(p);
        if (c2 == EOF || isspace(c2) || c2 == '(' || c2 == ')') {
            if (c2 != EOF) pungetc(p, c2);
            return make_bool(1);
        }
        pungetc(p, c2);
        /* #t followed by more chars... treat as symbol starting with #T */
        char buf[4096];
        buf[0] = '#';
        buf[1] = c;
        int len = 2;
        for (;;) {
            c2 = pgetc(p);
            if (c2 == EOF || isspace(c2) || c2 == '(' || c2 == ')') {
                if (c2 != EOF) pungetc(p, c2);
                break;
            }
            if (len < (int)sizeof(buf) - 1)
                buf[len++] = c2;
        }
        buf[len] = '\0';
        upcase_str(buf);
        return make_sym(buf);
    }

    if (c == 'F' || c == 'f') {
        int c2 = pgetc(p);
        if (c2 == EOF || isspace(c2) || c2 == '(' || c2 == ')') {
            if (c2 != EOF) pungetc(p, c2);
            return make_bool(0);
        }
        pungetc(p, c2);
    }

    fprintf(stderr, "Unknown hash syntax: #%c\n", c);
    exit(1);
}

static sexp_t *parse_string_lit(parser_t *p)
{
    char buf[65536];
    int len = 0;
    for (;;) {
        int c = pgetc(p);
        if (c == EOF) {
            fprintf(stderr, "Unterminated string\n");
            exit(1);
        }
        if (c == '"')
            break;
        if (c == '\\') {
            c = pgetc(p);
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == 'r') c = '\r';
            /* else literal backslash-escape */
        }
        if (len < (int)sizeof(buf) - 1)
            buf[len++] = c;
    }
    buf[len] = '\0';
    return make_string(buf);
}

static sexp_t *parse_list(parser_t *p)
{
    skip_whitespace(p);
    int c = pgetc(p);
    if (c == ')') return SEXP_NIL;
    pungetc(p, c);

    sexp_t *car = parse_sexp(p);
    skip_whitespace(p);

    c = pgetc(p);
    if (c == '.') {
        int c2 = pgetc(p);
        if (c2 != EOF && !isspace(c2) && c2 != '(' && c2 != ')') {
            /* Dot followed by non-delimiter — symbol like "..." or ".FOO" */
            pungetc(p, c2);
            sexp_t *sym = parse_atom(p, '.');
            sexp_t *rest = parse_list(p);
            return make_pair(car, make_pair(sym, rest));
        }
        /* Standalone dot: could be dotted-pair or a symbol named ".".
           Try dotted-pair: parse one sexp, check for closing ")".
           If ")" doesn't follow, it was actually the symbol ".". */
        if (c2 != EOF) pungetc(p, c2);
        sexp_t *next = parse_sexp(p);
        skip_whitespace(p);
        c = pgetc(p);
        if (c == ')') {
            /* (car . next) — genuine dotted pair */
            return make_pair(car, next);
        }
        /* Not a dotted pair — "." was a symbol.  We already parsed
           "next" as the element after it; continue with rest of list. */
        pungetc(p, c);
        sexp_t *rest = parse_list(p);
        return make_pair(car, make_pair(make_sym("."), make_pair(next, rest)));
    }
    pungetc(p, c);

    sexp_t *cdr = parse_list(p);
    return make_pair(car, cdr);
}

static sexp_t *parse_sexp(parser_t *p)
{
    skip_whitespace(p);
    int c = pgetc(p);
    if (c == EOF) return NULL;

    if (c == '(') return parse_list(p);
    if (c == '#') return parse_hash(p);
    if (c == '"') return parse_string_lit(p);
    if (c == '\'') {
        sexp_t *val = parse_sexp(p);
        return make_pair(make_sym("QUOTE"), make_pair(val, SEXP_NIL));
    }

    return parse_atom(p, c);
}

/* ================================================================
 * Hash table (string-keyed)
 * ================================================================ */

#define HT_INIT_SIZE 256

typedef struct ht_entry {
    char *key;
    long val;
    int has_val;          /* 0 = probed but no value set yet */
    struct ht_entry *next;
} ht_entry_t;

typedef struct {
    ht_entry_t **buckets;
    int size;
    int count;
} hashtable_t;

static void ht_init(hashtable_t *ht)
{
    ht->size = HT_INIT_SIZE;
    ht->count = 0;
    ht->buckets = calloc(ht->size, sizeof(ht_entry_t *));
}

static unsigned ht_hash(const char *key, int size)
{
    unsigned h = 5381;
    for (const char *p = key; *p; p++)
        h = h * 33 + (unsigned char)*p;
    return h % size;
}

/* Probe: return entry, creating if needed. Returns 1 if already existed. */
static int ht_probe(hashtable_t *ht, const char *key, ht_entry_t **out)
{
    unsigned h = ht_hash(key, ht->size);
    for (ht_entry_t *e = ht->buckets[h]; e; e = e->next) {
        if (!strcmp(e->key, key)) {
            *out = e;
            return 1;
        }
    }
    ht_entry_t *e = malloc(sizeof(ht_entry_t));
    e->key = strdup(key);
    e->val = 0;
    e->has_val = 0;
    e->next = ht->buckets[h];
    ht->buckets[h] = e;
    ht->count++;
    *out = e;
    return 0;
}

static ht_entry_t *ht_get(hashtable_t *ht, const char *key)
{
    unsigned h = ht_hash(key, ht->size);
    for (ht_entry_t *e = ht->buckets[h]; e; e = e->next) {
        if (!strcmp(e->key, key))
            return e;
    }
    return NULL;
}


/* ================================================================
 * Integer-keyed hash table (for blk-table and pair cache)
 * ================================================================ */

typedef struct iht_entry {
    long key;
    long val;
    int kind;             /* 0=toplevel, 1=lastoplevel, 2=regular */
    struct iht_entry *next;
} iht_entry_t;

typedef struct {
    iht_entry_t **buckets;
    int size;
} ihashtable_t;

static void iht_init(ihashtable_t *ht)
{
    ht->size = HT_INIT_SIZE;
    ht->buckets = calloc(ht->size, sizeof(iht_entry_t *));
}

static iht_entry_t *iht_get(ihashtable_t *ht, long key)
{
    unsigned h = (unsigned long)key % ht->size;
    for (iht_entry_t *e = ht->buckets[h]; e; e = e->next)
        if (e->key == key) return e;
    return NULL;
}

static iht_entry_t *iht_put(ihashtable_t *ht, long key, long val, int kind)
{
    unsigned h = (unsigned long)key % ht->size;
    iht_entry_t *e = malloc(sizeof(iht_entry_t));
    e->key = key;
    e->val = val;
    e->kind = kind;
    e->next = ht->buckets[h];
    ht->buckets[h] = e;
    return e;
}

/* ================================================================
 * Pair cache (equal-keyed) for caching-pair-alloc
 * ================================================================ */

typedef struct pc_entry {
    sexp_t *key;
    uint64_t val;
    struct pc_entry *next;
} pc_entry_t;

#define PC_SIZE 512
static pc_entry_t *pair_cache[PC_SIZE];

static int sexp_equal(sexp_t *a, sexp_t *b);

static unsigned sexp_hash(sexp_t *s)
{
    if (!s) return 0;
    switch (s->type) {
    case S_NIL:    return 1;
    case S_INT:    return (unsigned)(s->u.ival * 2654435761ULL);
    case S_SYM:    return ht_hash(s->u.sval, 1000000007);
    case S_CHAR:   return s->u.cval * 7919;
    case S_STRING: return ht_hash(s->u.sval, 1000000007) ^ 0xDEAD;
    case S_BOOL:   return s->u.bval ? 42 : 43;
    case S_PAIR:   return sexp_hash(s->u.pair.car) * 31 + sexp_hash(s->u.pair.cdr);
    }
    return 0;
}

static int sexp_equal(sexp_t *a, sexp_t *b)
{
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->type != b->type) return 0;
    switch (a->type) {
    case S_NIL:    return 1;
    case S_INT:    return a->u.ival == b->u.ival;
    case S_SYM:    return !strcmp(a->u.sval, b->u.sval);
    case S_CHAR:   return a->u.cval == b->u.cval;
    case S_STRING: return !strcmp(a->u.sval, b->u.sval);
    case S_BOOL:   return a->u.bval == b->u.bval;
    case S_PAIR:   return sexp_equal(a->u.pair.car, b->u.pair.car) &&
                          sexp_equal(a->u.pair.cdr, b->u.pair.cdr);
    }
    return 0;
}

static int pair_cache_lookup(sexp_t *key, uint64_t *val)
{
    unsigned h = sexp_hash(key) % PC_SIZE;
    for (pc_entry_t *e = pair_cache[h]; e; e = e->next)
        if (sexp_equal(e->key, key)) { *val = e->val; return 1; }
    return 0;
}

static void pair_cache_insert(sexp_t *key, uint64_t val)
{
    unsigned h = sexp_hash(key) % PC_SIZE;
    pc_entry_t *e = malloc(sizeof(pc_entry_t));
    e->key = key;
    e->val = val;
    e->next = pair_cache[h];
    pair_cache[h] = e;
}

/* ================================================================
 * Global state
 * ================================================================ */

static int bits64 = 1;           /* default 64-bit */
static int ref_shift;            /* 2 or 3 */
static int fixnum_bits;          /* 30 or 62 */

static int opc_count, var_count, sym_count, dat_count;
static int blk_count, max_blks;

static long start_of_opc_space, start_of_var_space;
static long start_of_sym_space, start_of_dat_space;
static long world_array_size, next_free_dat;

static long where_nil_lives, where_t_lives;
static long where_string_lives, where_cons_pair_lives;
static long where_code_vector_lives, where_boot_method_lives;
static long first_regular_blk_addr;

static hashtable_t var_table, sym_table;
static ihashtable_t blk_table;

/* var_list: ordered list of variable names */
#define MAX_VARS 8192
static char *var_list[MAX_VARS];
static int var_list_len = 0;

/* World array: each cell is either a number or an opcode pair */
typedef struct {
    int is_opc;            /* 1 = opcode pair, 0 = number */
    uint64_t val;          /* number value */
    uint16_t op1, op2;     /* opcode pair */
} cell_t;

static cell_t *world = NULL;

/* Parsed .oa files */
typedef struct {
    sexp_t *data;          /* list of blocks */
} oa_file_t;

static oa_file_t *files = NULL;
static int nfiles = 0;
static char **file_names = NULL;

/* Empty string cache */
static int64_t the_empty_string = -1;  /* -1 = not yet allocated */

/* vars-to-preload */
static const char *vars_to_preload[] = {
    "NIL", "T", "CONS-PAIR", "%CODE-VECTOR", "STRING",
    "%%SYMLOC", "%%NSYMS", "%%SYMSIZE", "%%VARLOC", "%%NVARS"
};
#define N_PRELOAD 10

/* ================================================================
 * Tag encoding (matching tool.oak)
 * ================================================================ */

static uint64_t tagize_int(long long x)
{
    uint64_t v;
    if (x < 0) {
        /* Two's complement in fixnum_bits range */
        uint64_t mod = (uint64_t)1 << fixnum_bits;
        v = (uint64_t)(x + (long long)mod);
    } else {
        v = (uint64_t)x;
    }
    return INT_TAG + v * 4;
}

static uint64_t tagize_imm(long long x)
{
    return IMM_TAG + ((uint64_t)x << 2);
}

static uint64_t tagize_ptr(long x)
{
    return PTR_TAG + ((uint64_t)x << ref_shift);
}

static uint64_t tagize_loc(long x)
{
    return LOC_TAG + ((uint64_t)x << ref_shift);
}

/* ================================================================
 * World array operations
 * ================================================================ */

static int zero_enough(cell_t *c)
{
    if (c->is_opc)
        return (c->op1 == 0 && c->op2 == 0);
    return (c->val == 0);
}

static void store_world_word(uint64_t word, long addr)
{
    if (addr < 0 || addr >= world_array_size) {
        fprintf(stderr, "store_world_word: addr %ld out of bounds (size %ld)\n",
                addr, world_array_size);
        exit(1);
    }
    cell_t *c = &world[addr];
    if (!zero_enough(c)) {
        fprintf(stderr, "Attempt to overwrite at addr %ld (is_opc=%d val=%llu op1=%d op2=%d)\n",
                addr, c->is_opc, (unsigned long long)c->val, c->op1, c->op2);
        exit(1);
    }
    c->is_opc = 0;
    c->op1 = 0;
    c->op2 = 0;
    c->val = word;
}

static void store_world_opcodes(uint16_t o1, uint16_t o2, long addr)
{
    if (addr < 0 || addr >= world_array_size) {
        fprintf(stderr, "store_world_opcodes: addr %ld out of bounds\n", addr);
        exit(1);
    }
    cell_t *c = &world[addr];
    if (!zero_enough(c)) {
        fprintf(stderr, "Attempt to overwrite opcodes at addr %ld\n", addr);
        exit(1);
    }
    c->is_opc = 1;
    c->op1 = o1;
    c->op2 = o2;
    c->val = 0;
}

static void overwrite_world_opcodes(uint16_t o1, uint16_t o2, long addr)
{
    cell_t *c = &world[addr];
    c->is_opc = 1;
    c->op1 = o1;
    c->op2 = o2;
    c->val = 0;
}

static void store_world_int(long long x, long addr)
{
    store_world_word(tagize_int(x), addr);
}

static void store_world_ptr(long x, long addr)
{
    store_world_word(tagize_ptr(x), addr);
}

static void store_world_loc(long x, long addr)
{
    store_world_word(tagize_loc(x), addr);
}

/* ================================================================
 * Data space allocator
 * ================================================================ */

static long alloc_dat(int n)
{
    long old = next_free_dat;
    next_free_dat += n;
    if (next_free_dat > world_array_size) {
        fprintf(stderr, "Out of data space\n");
        exit(1);
    }
    return old;
}

/* ================================================================
 * String size and allocation
 * ================================================================ */

static int string_size(const char *s)
{
    int slen = strlen(s);
    return 3 + (slen + 2) / 3;
}

static uint64_t real_string_alloc(const char *s, int slen)
{
    int strwordlen = 3 + (slen + 2) / 3;
    long newstring = alloc_dat(strwordlen);
    store_world_ptr(where_string_lives, newstring);
    store_world_int(strwordlen, newstring + 1);
    store_world_int(slen, newstring + 2);

    /* Pack characters 3 per word */
    const unsigned char *p = (const unsigned char *)s;
    int todo = slen;
    long addr = newstring + 3;
    while (todo > 0) {
        long long w = 0;
        if (todo >= 1) w = p[0];
        if (todo >= 2) w |= (long long)p[1] << 8;
        if (todo >= 3) w |= (long long)p[2] << 16;
        store_world_int(w, addr);
        addr++;
        p += 3;
        todo -= 3;
    }
    return tagize_ptr(newstring);
}

static uint64_t string_alloc(const char *s)
{
    int slen = strlen(s);
    if (slen > 0)
        return real_string_alloc(s, slen);
    /* Cache empty strings */
    if (the_empty_string >= 0)
        return (uint64_t)the_empty_string;
    the_empty_string = (int64_t)real_string_alloc(s, 0);
    return (uint64_t)the_empty_string;
}

/* ================================================================
 * Forward declarations
 * ================================================================ */

static uint64_t constant_refgen(sexp_t *c);

/* ================================================================
 * Pair allocation
 * ================================================================ */

static uint64_t pair_alloc(sexp_t *c)
{
    long newpair = alloc_dat(PAIR_SIZE);
    store_world_ptr(where_cons_pair_lives, newpair);
    store_world_word(constant_refgen(c->u.pair.car), newpair + 1);
    store_world_word(constant_refgen(c->u.pair.cdr), newpair + 2);
    return tagize_ptr(newpair);
}

static uint64_t caching_pair_alloc(sexp_t *c)
{
    uint64_t cached;
    if (pair_cache_lookup(c, &cached))
        return cached;
    uint64_t newp = pair_alloc(c);
    pair_cache_insert(c, newp);
    return newp;
}

/* ================================================================
 * constant-refgen
 * ================================================================ */

static uint64_t constant_refgen(sexp_t *c)
{
    if (!c) {
        fprintf(stderr, "constant_refgen: NULL sexp\n");
        exit(1);
    }

    switch (c->type) {
    case S_SYM: {
        ht_entry_t *e = ht_get(&sym_table, c->u.sval);
        if (!e || !e->has_val) {
            fprintf(stderr, "constant_refgen: symbol '%s' not in sym_table\n",
                    c->u.sval);
            exit(1);
        }
        return tagize_ptr(e->val);
    }
    case S_NIL:
        return tagize_ptr(where_nil_lives);
    case S_BOOL:
        if (c->u.bval)
            return tagize_ptr(where_t_lives);
        return tagize_ptr(where_nil_lives);
    case S_INT:
        return tagize_int(c->u.ival);
    case S_CHAR:
        return tagize_imm((long long)c->u.cval << 6);
    case S_PAIR:
        return caching_pair_alloc(c);
    case S_STRING:
        return string_alloc(c->u.sval);
    }
    fprintf(stderr, "constant_refgen: unknown sexp type %d\n", c->type);
    exit(1);
}

/* ================================================================
 * .oa file reader
 * ================================================================ */

/* Triplify a flat list: (a b c d e f ...) -> ((a b c) (d e f) ...) reversed */
static sexp_t *triplify(sexp_t *lst)
{
    sexp_t *out = SEXP_NIL;
    while (lst && lst->type == S_PAIR) {
        sexp_t *a = lst->u.pair.car;
        lst = lst->u.pair.cdr;
        if (!lst || lst->type != S_PAIR) {
            fprintf(stderr, "triplify: list length not multiple of 3\n");
            exit(1);
        }
        sexp_t *b = lst->u.pair.car;
        lst = lst->u.pair.cdr;
        if (!lst || lst->type != S_PAIR) {
            fprintf(stderr, "triplify: list length not multiple of 3\n");
            exit(1);
        }
        sexp_t *c = lst->u.pair.car;
        lst = lst->u.pair.cdr;
        sexp_t *triple = make_pair(a, make_pair(b, make_pair(c, SEXP_NIL)));
        out = make_pair(triple, out);
    }
    return out;
}

static sexp_t *read_oa_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Cannot open %s: %s\n", filename, strerror(errno));
        exit(1);
    }

    parser_t p;
    parser_init(&p, fp);
    sexp_t *form = parse_sexp(&p);
    fclose(fp);

    if (!form || form->type != S_PAIR) {
        fprintf(stderr, "Bad .oa file: %s\n", filename);
        exit(1);
    }

    /* Determine format: old = (sym-list (blocks...)) */
    sexp_t *first_elem = form->u.pair.car;

    /* old format: first element is a list (possibly empty) of symbols */
    int old_format = 1;
    if (first_elem->type == S_PAIR) {
        sexp_t *ff = first_elem->u.pair.car;
        if (ff && ff->type == S_PAIR) {
            /* first->car->car is a pair: new format */
            old_format = 0;
        }
    }

    if (!old_format) {
        /* Already in new format: ((patches opcodes) ...) */
        return form;
    }

    /* Old format: (sym-list (block1 block2 ...)) */
    sexp_t *sym_list = first_elem;
    sexp_t *blocks = sexp_nth(form, 1);

    /* Build sym_vec array from sym_list */
    int nsyms = sexp_list_len(sym_list);
    sexp_t **sym_vec = NULL;
    if (nsyms > 0) {
        sym_vec = malloc(nsyms * sizeof(sexp_t *));
        sexp_t *sl = sym_list;
        for (int i = 0; i < nsyms; i++) {
            sym_vec[i] = sl->u.pair.car;
            sl = sl->u.pair.cdr;
        }
    }

    /* Process each block */
    sexp_t *result = SEXP_NIL;
    sexp_t **tail = &result;

    sexp_t *blist = blocks;
    while (blist && blist->type == S_PAIR) {
        sexp_t *blk = blist->u.pair.car;
        sexp_t *flat_patches = sexp_nth(blk, 0);  /* flat patch list */
        sexp_t *opcodes = sexp_nth(blk, 1);       /* opcode list */

        /* Triplify patches (reverses order, matching tool.oak) */
        sexp_t *triples = triplify(flat_patches);

        /* Resolve symbol references (type >= 5 → index into sym_vec) */
        sexp_t *resolved = SEXP_NIL;
        sexp_t **rtail = &resolved;
        sexp_t *tl = triples;
        while (tl && tl->type == S_PAIR) {
            sexp_t *triple = tl->u.pair.car;
            sexp_t *type_s = sexp_nth(triple, 0);
            sexp_t *offset_s = sexp_nth(triple, 1);
            sexp_t *value_s = sexp_nth(triple, 2);

            long type_val = type_s->u.ival;

            if (type_val >= 5) {
                /* Symbol reference */
                long actual_type = type_val - 5;
                long sym_idx = value_s->u.ival;
                if (sym_idx < 0 || sym_idx >= nsyms) {
                    fprintf(stderr, "Symbol index %ld out of range\n", sym_idx);
                    exit(1);
                }
                sexp_t *new_triple = make_pair(
                    make_int(actual_type),
                    make_pair(offset_s,
                              make_pair(sym_vec[sym_idx], SEXP_NIL)));
                *rtail = make_pair(new_triple, SEXP_NIL);
            } else {
                *rtail = make_pair(triple, SEXP_NIL);
            }
            rtail = &((*rtail)->u.pair.cdr);
            tl = tl->u.pair.cdr;
        }

        sexp_t *new_blk = make_pair(resolved, make_pair(opcodes, SEXP_NIL));
        *tail = make_pair(new_blk, SEXP_NIL);
        tail = &((*tail)->u.pair.cdr);

        blist = blist->u.pair.cdr;
    }

    if (sym_vec) free(sym_vec);
    return result;
}

/* ================================================================
 * Phase 1: count-things
 * ================================================================ */

static void count_symbol(const char *name)
{
    ht_entry_t *e;
    dat_count += string_size(name);
    ht_probe(&sym_table, name, &e);
}

static void count_variable(const char *name)
{
    ht_entry_t *e;
    if (!ht_probe(&var_table, name, &e)) {
        /* New variable */
        if (var_list_len >= MAX_VARS) {
            fprintf(stderr, "Too many variables\n");
            exit(1);
        }
        var_list[var_list_len++] = strdup(name);
    }
    count_symbol(name);
}

static void count_data(sexp_t *d);

static void count_data(sexp_t *d)
{
    if (!d) {
        fprintf(stderr, "count_data: NULL\n");
        exit(1);
    }
    switch (d->type) {
    case S_SYM:
        count_symbol(d->u.sval);
        break;
    case S_INT:
    case S_CHAR:
        break;
    case S_NIL:
        break;
    case S_BOOL:
        break;
    case S_PAIR:
        dat_count += PAIR_SIZE;
        count_data(d->u.pair.car);
        count_data(d->u.pair.cdr);
        break;
    case S_STRING:
        dat_count += string_size(d->u.sval);
        break;
    }
}

static int handbuilt_data_size(void)
{
    return NULL_SIZE + T_SIZE + METHOD_SIZE + 2 * TYPE_SIZE + COERCABLE_TYPE_SIZE;
}

static void count_things(void)
{
    dat_count = 0;
    blk_count = 0;
    opc_count = REG_CODE_DELTA - TOP_CODE_DELTA;  /* = 6 */

    /* Pre-count vars-to-preload */
    for (int i = 0; i < N_PRELOAD; i++)
        count_variable(vars_to_preload[i]);

    /* Process each file */
    for (int fi = 0; fi < nfiles; fi++) {
        sexp_t *fil = files[fi].data;
        int nblks = sexp_list_len(fil);
        opc_count += TOP_CODE_DELTA + REG_CODE_DELTA * (nblks - 1);
        if (nblks > max_blks)
            max_blks = nblks;
        blk_count += nblks;

        sexp_t *blist = fil;
        while (blist && blist->type == S_PAIR) {
            sexp_t *blk = blist->u.pair.car;
            sexp_t *patches = sexp_nth(blk, 0);
            sexp_t *opcodes = sexp_nth(blk, 1);

            /* Count opcodes */
            int op_count = sexp_list_len(opcodes);
            if (op_count & 1) {
                fprintf(stderr, "Odd number of opcodes: %d\n", op_count);
                exit(1);
            }
            opc_count += op_count;

            /* Count patches */
            sexp_t *pl = patches;
            while (pl && pl->type == S_PAIR) {
                sexp_t *pat = pl->u.pair.car;
                int keyword = (int)sexp_nth(pat, 0)->u.ival;
                sexp_t *patval = sexp_nth(pat, 2);

                if (keyword == 2) {
                    /* constant */
                    count_data(patval);
                } else if (keyword == 0) {
                    /* variable */
                    count_variable(patval->u.sval);
                } else if (keyword == 1) {
                    /* code - noop */
                }
                pl = pl->u.pair.cdr;
            }

            blist = blist->u.pair.cdr;
        }
    }

    var_count = var_table.count;
    sym_count = sym_table.count;
    dat_count += handbuilt_data_size();

    fprintf(stderr, "ops:%d  vars:%d  syms:%d cells:%d\n",
            opc_count, var_count, sym_count, dat_count);
}

/* ================================================================
 * Phase 2: compute-base-addresses
 * ================================================================ */

static void compute_base_addresses(void)
{
    start_of_opc_space = 0;
    start_of_var_space = start_of_opc_space + opc_count / 2;
    start_of_sym_space = start_of_var_space + var_count * CELL_SIZE;
    start_of_dat_space = start_of_sym_space + sym_count * SYMBOL_SIZE;
    world_array_size = start_of_dat_space + dat_count;
    next_free_dat = start_of_dat_space;
}

/* ================================================================
 * Phase 3: init-world
 * ================================================================ */

static void init_world(void)
{
    world = calloc(world_array_size, sizeof(cell_t));
    if (!world) {
        fprintf(stderr, "Cannot allocate world of %ld cells\n", world_array_size);
        exit(1);
    }
}

/* ================================================================
 * Phase 4: layout-symbols-and-variables
 * ================================================================ */

static void layout_symbols_and_variables(void)
{
    long nextvar = start_of_var_space;
    long nextsym = start_of_sym_space;

    /* Pass 1: variables (in order from var_list) */
    for (int i = 0; i < var_list_len; i++) {
        const char *name = var_list[i];

        /* Set symbol address */
        ht_entry_t *se = ht_get(&sym_table, name);
        se->val = nextsym;
        se->has_val = 1;
        nextsym += SYMBOL_SIZE;

        /* Set variable address */
        ht_entry_t *ve = ht_get(&var_table, name);
        ve->val = nextvar;
        ve->has_val = 1;
        nextvar += CELL_SIZE;
    }

    /* Pass 2: non-variable symbols */
    for (int i = 0; i < sym_table.size; i++) {
        for (ht_entry_t *e = sym_table.buckets[i]; e; e = e->next) {
            if (!e->has_val) {
                e->val = nextsym;
                e->has_val = 1;
                nextsym += SYMBOL_SIZE;
            }
        }
    }
}

/* ================================================================
 * Phase 5: layout-handbuilt-data
 * ================================================================ */

static void layout_boot_method(void)
{
    where_boot_method_lives = alloc_dat(METHOD_SIZE);
    store_world_ptr(where_nil_lives, where_boot_method_lives);
    store_world_ptr(start_of_opc_space, where_boot_method_lives + 1);
    store_world_ptr(where_nil_lives, where_boot_method_lives + 2);
}

static void layout_handbuilt_data(void)
{
    where_nil_lives = alloc_dat(NULL_SIZE);
    where_t_lives = alloc_dat(T_SIZE);
    where_string_lives = alloc_dat(COERCABLE_TYPE_SIZE);
    where_cons_pair_lives = alloc_dat(TYPE_SIZE);
    where_code_vector_lives = alloc_dat(TYPE_SIZE);

    /* Store variable bindings */
    ht_entry_t *e;

    e = ht_get(&var_table, "NIL");
    store_world_ptr(where_nil_lives, e->val);

    e = ht_get(&var_table, "T");
    store_world_ptr(where_t_lives, e->val);

    e = ht_get(&var_table, "STRING");
    store_world_ptr(where_string_lives, e->val);

    e = ht_get(&var_table, "CONS-PAIR");
    store_world_ptr(where_cons_pair_lives, e->val);

    e = ht_get(&var_table, "%CODE-VECTOR");
    store_world_ptr(where_code_vector_lives, e->val);

    e = ht_get(&var_table, "%%SYMLOC");
    store_world_loc(start_of_sym_space, e->val);

    e = ht_get(&var_table, "%%VARLOC");
    store_world_loc(start_of_var_space, e->val);

    e = ht_get(&var_table, "%%NSYMS");
    store_world_int(sym_count, e->val);

    e = ht_get(&var_table, "%%NVARS");
    store_world_int(var_count, e->val);

    e = ht_get(&var_table, "%%SYMSIZE");
    store_world_int(SYMBOL_SIZE, e->val);

    layout_boot_method();
}

/* ================================================================
 * Phase 6: patch-symbols
 * ================================================================ */

static void patch_symbols(void)
{
    for (int i = 0; i < sym_table.size; i++) {
        for (ht_entry_t *e = sym_table.buckets[i]; e; e = e->next) {
            long addr = e->val;
            store_world_ptr(where_nil_lives, addr);
            store_world_word(string_alloc(e->key), addr + 1);
        }
    }
}

/* ================================================================
 * Phase 7: build-blk-table
 * ================================================================ */

static long uniq_blkno(int filno, int blkno)
{
    return blkno + (long)max_blks * filno;
}

static void build_blk_table(void)
{
    long next_blk_addr = start_of_opc_space +
                         (REG_CODE_DELTA - TOP_CODE_DELTA) / 2;

    /* Phase 1: top-level blocks (first block of each file) */
    for (int fi = 0; fi < nfiles; fi++) {
        sexp_t *blk = sexp_nth(files[fi].data, 0);
        sexp_t *opcodes = sexp_nth(blk, 1);
        int nops = sexp_list_len(opcodes);
        int nwords = (nops + TOP_CODE_DELTA) / 2;

        long old_addr = next_blk_addr;
        next_blk_addr += nwords;
        if (next_blk_addr > start_of_var_space) {
            fprintf(stderr, "Out of code space\n");
            exit(1);
        }

        int kind = (fi == nfiles - 1) ? 1 : 0; /* 1=lastoplevel, 0=toplevel */
        iht_put(&blk_table, uniq_blkno(fi, 0), old_addr, kind);
    }

    first_regular_blk_addr = next_blk_addr;

    /* Phase 2: regular blocks (non-first blocks of each file) */
    for (int fi = 0; fi < nfiles; fi++) {
        sexp_t *blist = files[fi].data;
        if (!blist || blist->type != S_PAIR)
            continue;
        blist = blist->u.pair.cdr;  /* skip first block */

        int blkno = 1;
        while (blist && blist->type == S_PAIR) {
            sexp_t *blk = blist->u.pair.car;
            sexp_t *opcodes = sexp_nth(blk, 1);
            int nops = sexp_list_len(opcodes);
            int nwords = (nops + REG_CODE_DELTA) / 2;

            long old_addr = next_blk_addr;
            next_blk_addr += nwords;
            if (next_blk_addr > start_of_var_space) {
                fprintf(stderr, "Out of code space\n");
                exit(1);
            }

            iht_put(&blk_table, uniq_blkno(fi, blkno), old_addr, 2);  /* 2=regular */
            blkno++;
            blist = blist->u.pair.cdr;
        }
    }
}

/* ================================================================
 * Phase 8: spew-opcodes
 * ================================================================ */

static void changereturntonoop(long addr)
{
    cell_t *c = &world[addr];
    if (!c->is_opc) {
        fprintf(stderr, "changereturntonoop: not opcodes at addr %ld\n", addr);
        exit(1);
    }
    uint16_t op1 = c->op1, op2 = c->op2;

    if (op2 == RETURN_OPCODE) {
        overwrite_world_opcodes(op1, NOOP_OPCODE, addr);
    } else if (op1 == RETURN_OPCODE && op2 == NOOP_OPCODE) {
        overwrite_world_opcodes(NOOP_OPCODE, NOOP_OPCODE, addr);
    } else {
        fprintf(stderr, "bad ops in toplvl blk end <%d %d>\n", op1, op2);
        exit(1);
    }
}

static void spew_opcodes(void)
{
    /* Code vector header: first 3 words */
    store_world_ptr(where_code_vector_lives, start_of_opc_space);
    store_world_int(first_regular_blk_addr, start_of_opc_space + 1);
    store_world_ptr(where_nil_lives, start_of_opc_space + 2);

    for (int fi = 0; fi < nfiles; fi++) {
        fprintf(stderr, " %s", file_names[fi]);
        sexp_t *blist = files[fi].data;
        int blkno = 0;

        while (blist && blist->type == S_PAIR) {
            sexp_t *blk = blist->u.pair.car;
            sexp_t *patches = sexp_nth(blk, 0);
            sexp_t *opcodes = sexp_nth(blk, 1);

            iht_entry_t *info = iht_get(&blk_table, uniq_blkno(fi, blkno));
            long base_addr = info->val;
            int blk_kind = info->kind;  /* 0=toplevel, 1=lastoplevel, 2=regular */
            int regp = (blk_kind == 2);
            int delta = (regp ? REG_CODE_DELTA : TOP_CODE_DELTA) / 2;
            long delbase_addr = delta + base_addr;

            /* Regular block header */
            if (regp) {
                store_world_ptr(where_code_vector_lives, base_addr);
                store_world_int(2 + sexp_list_len(opcodes) / 2, base_addr + 1);
            }

            /* Write opcodes */
            sexp_t *ops = opcodes;
            long addr = delbase_addr;
            while (ops && ops->type == S_PAIR) {
                uint16_t o1 = (uint16_t)ops->u.pair.car->u.ival;
                ops = ops->u.pair.cdr;
                uint16_t o2 = (ops && ops->type == S_PAIR) ?
                    (uint16_t)ops->u.pair.car->u.ival : 0;
                if (ops && ops->type == S_PAIR)
                    ops = ops->u.pair.cdr;

                if (addr >= base_addr)
                    store_world_opcodes(o1, o2, addr);
                addr++;
            }

            /* Chain top-level blocks */
            if (blk_kind == 0)  /* toplevel (not last) */
                changereturntonoop(addr - 1);

            /* Apply patches */
            sexp_t *pl = patches;
            while (pl && pl->type == S_PAIR) {
                sexp_t *pat = pl->u.pair.car;
                int patkind = (int)sexp_nth(pat, 0)->u.ival;
                int patoffset = (int)sexp_nth(pat, 1)->u.ival;
                sexp_t *patval = sexp_nth(pat, 2);
                long pataddr = delbase_addr + patoffset / 2;

                uint64_t patref;
                if (patkind == 2) {
                    /* constant */
                    patref = constant_refgen(patval);
                } else if (patkind == 0) {
                    /* variable */
                    ht_entry_t *ve = ht_get(&var_table, patval->u.sval);
                    if (!ve || !ve->has_val) {
                        fprintf(stderr, "Variable '%s' not found\n",
                                patval->u.sval);
                        exit(1);
                    }
                    patref = tagize_loc(ve->val);
                } else if (patkind == 1) {
                    /* code */
                    iht_entry_t *be = iht_get(&blk_table,
                                              uniq_blkno(fi, (int)patval->u.ival));
                    if (!be) {
                        fprintf(stderr, "Block %lld not found\n", patval->u.ival);
                        exit(1);
                    }
                    patref = tagize_ptr(be->val);
                } else {
                    fprintf(stderr, "Unknown patkind %d\n", patkind);
                    exit(1);
                }

                if (pataddr >= base_addr)
                    store_world_word(patref, pataddr);

                pl = pl->u.pair.cdr;
            }

            blkno++;
            blist = blist->u.pair.cdr;
        }
    }
    fprintf(stderr, "\n");
}

/* ================================================================
 * Output: dump world
 * ================================================================ */

static void print_hex(FILE *fp, uint64_t val)
{
    fprintf(fp, "%llX", (unsigned long long)val);
}

static void print_hex_padded4(FILE *fp, uint64_t val)
{
    fprintf(fp, "%04llX", (unsigned long long)val);
}

static void dump_world(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Cannot open %s for writing: %s\n",
                filename, strerror(errno));
        exit(1);
    }

    long actual_size = next_free_dat;

    /* Header */
    print_hex(fp, VALUE_STACK_SIZE);
    fprintf(fp, " ");
    print_hex(fp, CONTEXT_STACK_SIZE);
    fprintf(fp, " ");
    print_hex(fp, tagize_ptr(where_boot_method_lives));
    fprintf(fp, " ");
    print_hex(fp, actual_size);
    fprintf(fp, "\n");

    /* World data */
    for (long i = 0; i < actual_size; i++) {
        if ((i % 8) == 0)
            fprintf(fp, "\n");

        cell_t *c = &world[i];
        if (c->is_opc) {
            fprintf(fp, "^");
            print_hex(fp, c->op1);
            print_hex_padded4(fp, c->op2);
        } else {
            fprintf(fp, " ");
            print_hex(fp, c->val);
        }
    }

    /* Weak pointer table size */
    fprintf(fp, "\n0\n");
    fclose(fp);

    fprintf(stderr, "Total words:%ld\n", actual_size);
}

/* ================================================================
 * Output: dump symbol tables (.sym file)
 * ================================================================ */

static void dump_tables(const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Cannot open %s for writing: %s\n",
                filename, strerror(errno));
        exit(1);
    }

    fprintf(fp, "((variables");
    for (int i = 0; i < var_list_len; i++) {
        ht_entry_t *e = ht_get(&var_table, var_list[i]);
        fprintf(fp, " (%s . %ld)", var_list[i], e->val);
    }
    fprintf(fp, ")\n (symbols");

    /* Walk sym-table (reverse order like tool.oak) */
    /* Build list first, then reverse */
    typedef struct sym_pair { char *name; long addr; } sym_pair_t;
    sym_pair_t *spairs = malloc(sym_table.count * sizeof(sym_pair_t));
    int sp_count = 0;
    for (int i = 0; i < sym_table.size; i++)
        for (ht_entry_t *e = sym_table.buckets[i]; e; e = e->next)
            spairs[sp_count++] = (sym_pair_t){ e->key, e->val };

    /* Reverse them */
    for (int i = sp_count - 1; i >= 0; i--)
        fprintf(fp, " (%s . %ld)", spairs[i].name, spairs[i].addr);

    fprintf(fp, "))\n");
    free(spairs);
    fclose(fp);
}

/* ================================================================
 * Main
 * ================================================================ */

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [--64bit|--32bit] -o OUTBASE file1 file2 ...\n"
            "  Files are basenames; .oa extension appended automatically.\n"
            "  Default: 64-bit mode.\n", prog);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *outbase = NULL;
    char **infiles = NULL;
    int ninfiles = 0;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--64bit")) {
            bits64 = 1;
        } else if (!strcmp(argv[i], "--32bit")) {
            bits64 = 0;
        } else if (!strcmp(argv[i], "-o")) {
            if (++i >= argc) usage(argv[0]);
            outbase = argv[i];
        } else if (argv[i][0] == '-') {
            usage(argv[0]);
        } else {
            /* Remaining args are input files */
            infiles = &argv[i];
            ninfiles = argc - i;
            break;
        }
    }

    if (!outbase || ninfiles == 0)
        usage(argv[0]);

    /* Set architecture parameters */
    ref_shift = bits64 ? 3 : 2;
    fixnum_bits = bits64 ? 62 : 30;

    fprintf(stderr, "oak-cold-linker: %d-bit mode, ref-shift=%d, fixnum-bits=%d\n",
            bits64 ? 64 : 32, ref_shift, fixnum_bits);

    /* Initialize tables */
    ht_init(&var_table);
    ht_init(&sym_table);
    iht_init(&blk_table);
    memset(pair_cache, 0, sizeof(pair_cache));

    /* Read input files */
    fprintf(stderr, "reading ...");
    nfiles = ninfiles;
    files = malloc(nfiles * sizeof(oa_file_t));
    file_names = malloc(nfiles * sizeof(char *));

    for (int i = 0; i < nfiles; i++) {
        char fname[4096];
        snprintf(fname, sizeof(fname), "%s.oa", infiles[i]);
        fprintf(stderr, " %s", fname);
        files[i].data = read_oa_file(fname);
        file_names[i] = infiles[i];
    }
    fprintf(stderr, "\n");

    /* Phase 1: count things */
    count_things();
    fprintf(stderr, "counts\n");

    /* Phase 2: compute base addresses */
    compute_base_addresses();
    fprintf(stderr, "base-addrs\n");

    /* Phase 3: init world */
    init_world();
    fprintf(stderr, "world-init\n");

    /* Phase 4: layout symbols and variables */
    layout_symbols_and_variables();
    fprintf(stderr, "syms-and-vars\n");

    /* Phase 5: layout handbuilt data */
    layout_handbuilt_data();
    fprintf(stderr, "handbuilt\n");

    /* Phase 6: patch symbols */
    patch_symbols();
    fprintf(stderr, "symbol-patches\n");

    /* Phase 7: build block table */
    build_blk_table();
    fprintf(stderr, "blk-table\n");

    /* Phase 8: spew opcodes */
    fprintf(stderr, " spew");
    spew_opcodes();
    fprintf(stderr, "opcodes\n");

    /* Output */
    char outpath[4096];
    snprintf(outpath, sizeof(outpath), "%s.sym", outbase);
    dump_tables(outpath);
    fprintf(stderr, "symbol-table\n");

    snprintf(outpath, sizeof(outpath), "%s.cold", outbase);
    dump_world(outpath);

    return 0;
}
