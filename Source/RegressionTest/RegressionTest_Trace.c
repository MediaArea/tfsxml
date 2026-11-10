/* Copyright (c) MediaArea.net SARL

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

(MIT License)

*/

/*
 * Walk of an XML content with the tfsxml API, with a text trace of the results
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "RegressionTest.h"
#include "tfsxml/tfsxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef REGRESSIONTEST_LEGACY
    #define REGRESSIONTEST_HAS_TEXT 0
#else
    #define REGRESSIONTEST_HAS_TEXT 1
#endif

/* Deepest level entered by the walks (deeper content is skipped by the parser) */
#define WALK_DEPTH_MAX 32

/** -------------------------------------------------------------------------
        Growable text buffer
------------------------------------------------------------------------- **/

#ifndef REGRESSIONTEST_PREFIX /* Not duplicated when this file is built twice */

const char* const walk_mode_names[walk_mode_max] = {
    "tree",
    "value",
    "skip",
    "enter",
    "mixed",
    "random",
};

void trace_init(trace* t) {
    t->buf = NULL;
    t->len = 0;
    t->cap = 0;
}

void trace_free(trace* t) {
    free(t->buf);
    trace_init(t);
}

void trace_append(trace* t, const char* s, size_t len) {
    if (t->len + len + 1 > t->cap) {
        size_t cap = t->cap ? t->cap : 256;
        char* buf;
        while (t->len + len + 1 > cap) {
            cap *= 2;
        }
        buf = (char*)realloc(t->buf, cap);
        if (!buf) {
            fputs("Out of memory\n", stderr);
            abort();
        }
        t->buf = buf;
        t->cap = cap;
    }
    if (len) {
        memcpy(t->buf + t->len, s, len);
    }
    t->len += len;
    t->buf[t->len] = '\0';
}

void trace_puts(trace* t, const char* s) {
    trace_append(t, s, strlen(s));
}

#endif /* REGRESSIONTEST_PREFIX */

/** -------------------------------------------------------------------------
        Walker
------------------------------------------------------------------------- **/

typedef struct walker {
    /* Input */
    const unsigned char* data;
    size_t               size;
    const walk_config*   config;

    /* Current buffer, exact size of what the parser can read */
    unsigned char*       buffer;
    size_t               buffer_size;
    size_t               exposed;     /* count of bytes of data already provided */
    unsigned             chunk_rng;

    /* Parser */
    tfsxml_string        p;
    int                  is_init;

    /* Walk */
    unsigned             rng;
    int                  depth;
    unsigned long        calls;
    unsigned long        calls_max;
    int                  stop;        /* 1 = no more data, 2 = harness check failed */
    trace*               out;
} walker;

enum call_type {
    call_next,
    call_attr,
    call_value,
    call_enter,
    call_leave,
    call_text,
    call_max
};
static const char* const call_names[call_max] = {
    "next",
    "attr",
    "value",
    "enter",
    "leave",
    "text",
};

/* xorshift32 */
static unsigned rng_next(unsigned* state) {
    unsigned long x = *state;
    if (!x) {
        x = 0x9E3779B9UL;
    }
    x ^= (x << 13) & 0xFFFFFFFFUL;
    x ^= x >> 17;
    x ^= (x << 5) & 0xFFFFFFFFUL;
    *state = (unsigned)x;
    return (unsigned)x;
}

static void put(walker* w, const char* s) {
    trace_puts(w->out, s);
}

static void put_number(walker* w, long value) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%ld", value);
    put(w, tmp);
}

static void put_indent(walker* w) {
    int i;
    for (i = 0; i < w->depth; i++) {
        put(w, "  ");
    }
}

static void put_escaped(walker* w, const char* buf, size_t len) {
    static const char hex[] = "0123456789ABCDEF";
    size_t i;
    trace_append(w->out, "\"", 1);
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)buf[i];
        char tmp[4];
        switch (c) {
            case '"':  trace_append(w->out, "\\\"", 2); break;
            case '\\': trace_append(w->out, "\\\\", 2); break;
            case '\n': trace_append(w->out, "\\n", 2); break;
            case '\r': trace_append(w->out, "\\r", 2); break;
            case '\t': trace_append(w->out, "\\t", 2); break;
            default:
                if (c >= 0x20 && c < 0x7F) {
                    tmp[0] = (char)c;
                    trace_append(w->out, tmp, 1);
                }
                else {
                    tmp[0] = '\\';
                    tmp[1] = 'x';
                    tmp[2] = hex[c >> 4];
                    tmp[3] = hex[c & 0xF];
                    trace_append(w->out, tmp, 4);
                }
        }
    }
    trace_append(w->out, "\"", 1);
}

static void error(walker* w, const char* message) {
    put(w, "ERROR: ");
    put(w, message);
    put(w, "\n");
    w->stop = 2;
}

static void decode_append(void* s, const char* buf, unsigned len) {
    trace_append((trace*)s, buf, len);
}

static void put_string(walker* w, const tfsxml_string* s) {
    trace decoded;
    trace_init(&decoded);
    put_escaped(w, s->buf, s->len);
    tfsxml_decode(&decoded, s, decode_append);
    if (decoded.len > s->len) {
        error(w, "decoded content is longer than the encoded content");
    }
    else if (decoded.len != s->len || (decoded.len && memcmp(decoded.buf, s->buf, decoded.len))) {
        put(w, " -> ");
        put_escaped(w, decoded.buf, decoded.len);
    }
    trace_free(&decoded);
}

/* Count of bytes of data consumed by the parser */
static size_t consumed(const walker* w) {
    return w->is_init ? w->exposed - w->p.len : 0;
}

static void put_result(walker* w, int type, int result, const tfsxml_string* a, const tfsxml_string* b) {
    put_indent(w);
    put(w, call_names[type]);
    put(w, " ");
    put_number(w, result);
    if (a) {
        put(w, " ");
        put_string(w, a);
    }
    if (b) {
        put(w, " ");
        put_string(w, b);
    }
    put(w, " @");
    put_number(w, (long)consumed(w));
    put(w, "\n");
}

/* Check that the parser is still in the buffer, with the end of the buffer as end */
static int check(walker* w, const char* name) {
    size_t begin = (size_t)w->buffer;
    size_t pos = (size_t)w->p.buf;
    if (pos < begin || pos - begin > w->buffer_size || pos - begin + w->p.len != w->buffer_size) {
        char tmp[160];
        snprintf(tmp, sizeof(tmp), "parser position is out of the buffer after %s (offset %ld, length %lu, buffer size %lu)", name, (long)(pos - begin), (unsigned long)w->p.len, (unsigned long)w->buffer_size);
        error(w, tmp);
        return 1;
    }
    if (++w->calls > w->calls_max) {
        error(w, "too many calls, the parser does not progress");
        return 1;
    }
    return 0;
}

static size_t chunk_size(walker* w) {
    size_t remain = w->size - w->exposed;
    size_t size = w->config->chunk;
    if (!size) {
        return remain;
    }
    if (w->config->chunk_seed) {
        size = 1 + rng_next(&w->chunk_rng) % size;
    }
    return size < remain ? size : remain;
}

/* New buffer with the bytes not consumed by the parser and the next chunk, return 0 if no more data */
static int refill(walker* w) {
    size_t keep, add;
    unsigned char* buffer;
    if (w->exposed >= w->size) {
        return 0;
    }
    keep = w->is_init ? w->p.len : w->exposed;
    add = chunk_size(w);
    buffer = (unsigned char*)malloc(keep + add);
    if (!buffer) {
        fputs("Out of memory\n", stderr);
        abort();
    }
    if (w->is_init) {
        memcpy(buffer, w->p.buf, keep);
    }
    else {
        memcpy(buffer, w->data, keep);
    }
    memcpy(buffer + keep, w->data + w->exposed, add);
    free(w->buffer);
    w->buffer = buffer;
    w->buffer_size = keep + add;
    w->exposed += add;
    if (w->is_init) {
        w->p.buf = (const char*)buffer;
        w->p.len = (unsigned)w->buffer_size;
    }
    return 1;
}

/* Call with new buffers while the parser needs more data and data is available */
static int call(walker* w, int type, tfsxml_string* a, tfsxml_string* b) {
    for (;;) {
        int result;
        switch (type) {
            case call_next:  result = tfsxml_next(&w->p, a); break;
            case call_attr:  result = tfsxml_attr(&w->p, a, b); break;
            case call_value: result = tfsxml_value(&w->p, a); break;
            case call_enter: result = tfsxml_enter(&w->p); break;
#if REGRESSIONTEST_HAS_TEXT
            case call_text:  result = tfsxml_text(&w->p, a); break;
#else
            case call_text:  result = -1; break;
#endif
            default:         result = tfsxml_leave(&w->p); break;
        }
        if (check(w, call_names[type])) {
            return 2;
        }
        if (result != 1 || !refill(w)) {
            return result;
        }
    }
}

static int walk_level(walker* w, int max);

static void reset_strings(tfsxml_string* a, tfsxml_string* b) {
    memset(a, 0, sizeof(*a));
    memset(b, 0, sizeof(*b));
}

static void do_attributes(walker* w, int max) {
    int count;
    for (count = 0; max < 0 || count < max; count++) {
        tfsxml_string n, v;
        int result;
        reset_strings(&n, &v);
        result = call(w, call_attr, &n, &v);
        if (w->stop) {
            return;
        }
        put_result(w, call_attr, result, result ? NULL : &n, result ? NULL : &v);
        if (result) {
            if (result > 0) {
                w->stop = 1;
            }
            return;
        }
    }
}

static int do_simple(walker* w, int type) {
    tfsxml_string v, unused;
    int result;
    reset_strings(&v, &unused);
    result = call(w, type, &v, NULL);
    if (w->stop) {
        return 2;
    }
    put_result(w, type, result, ((type == call_value || type == call_text) && !result) ? &v : NULL, NULL);
    if (result > 0) {
        w->stop = 1;
    }
    return result;
}

static int do_enter(walker* w) {
    if (w->depth >= WALK_DEPTH_MAX) {
        return -2; /* Content is skipped by the next call */
    }
    return do_simple(w, call_enter);
}

/* Return 1 if the end of the level was reached */
static int do_children(walker* w, int max) {
    int ended;
    w->depth++;
    ended = walk_level(w, max);
    w->depth--;
    return ended;
}

static int is_element(const tfsxml_string* n) {
    return !n->len || (n->buf[0] != '?' && n->buf[0] != '!');
}

/* Return 1 if the current level was left */
static int handle_random(walker* w) {
    unsigned x = rng_next(&w->rng);
    switch (x % 4) {
        case 1: do_attributes(w, -1); break;
        case 2: do_attributes(w, 1); break;
        case 3: do_attributes(w, 2); break;
        default:;
    }
    if (w->stop) {
        return 0;
    }
    x = rng_next(&w->rng);
    switch (x % 8) {
        case 1: /* Value */
            do_simple(w, call_value);
            break;
        case 2: /* All children */
            if (!do_enter(w) && !w->stop) {
                do_children(w, -1);
            }
            break;
        case 3: /* Some children then leave */
            if (!do_enter(w) && !w->stop) {
                if (!do_children(w, (int)(rng_next(&w->rng) % 3)) && !w->stop) {
                    do_simple(w, call_leave);
                }
            }
            break;
        case 4: /* Leave the current level */
            do_simple(w, call_leave);
            return 1;
        case 5: /* Enter after value */
            do_simple(w, call_value);
            if (!w->stop) {
                do_enter(w);
            }
            break;
        case 6: /* Value after enter */
            if (!do_enter(w) && !w->stop) {
                do_simple(w, call_value);
                if (!w->stop) {
                    do_children(w, -1);
                }
            }
            break;
        case 7: /* Attributes after the end of attributes */
            do_attributes(w, -1);
            if (!w->stop) {
                do_attributes(w, -1);
            }
            break;
        default:;
    }
    return 0;
}

/* Return 1 if the current level was left */
static int handle_item(walker* w, const tfsxml_string* n) {
    int element = is_element(n);
    switch (w->config->mode) {
        case walk_tree:
            if (element) {
                do_attributes(w, -1);
                if (!w->stop && !do_enter(w) && !w->stop) {
                    do_children(w, -1);
                }
            }
            return 0;
        case walk_value:
            if (element) {
                do_attributes(w, -1);
                if (!w->stop) {
                    do_simple(w, call_value);
                }
            }
            return 0;
        case walk_skip:
            return 0;
        case walk_enter:
            if (element && !do_enter(w) && !w->stop) {
                do_children(w, -1);
            }
            return 0;
        case walk_mixed:
            if (element) {
                do_attributes(w, -1);
                if (!w->stop && !do_enter(w) && !w->stop) {
                    do_children(w, -1);
                }
            }
            return 0;
        default:
            return handle_random(w);
    }
}

/* Return 1 if the end of the level was reached */
static int walk_level(walker* w, int max) {
    int count;
    for (count = 0; max < 0 || count < max; count++) {
        tfsxml_string n, unused;
        int result;
        if (w->config->mode == walk_mixed || (w->config->mode == walk_random && !(rng_next(&w->rng) % 4))) {
            /* Text before the next element or the end of this level */
            if (REGRESSIONTEST_HAS_TEXT) {
                do_simple(w, call_text);
                if (w->stop) {
                    return 0;
                }
            }
        }
        reset_strings(&n, &unused);
        result = call(w, call_next, &n, NULL);
        if (w->stop) {
            return 0;
        }
        put_result(w, call_next, result, (result <= 0 && (!result || REGRESSIONTEST_HAS_TEXT)) ? &n : NULL, NULL);
        if (result) {
            if (result > 0) {
                w->stop = 1;
            }
            return result < 0;
        }
        if (handle_item(w, &n)) {
            return 1;
        }
        if (w->stop) {
            return 0;
        }
    }
    return 0;
}

/* First buffer and parser init, with more data if init fails */
static int walk_init(walker* w, size_t first) {
    int result;
    if (!first || first > w->size) {
        first = w->size;
    }
    w->buffer = (unsigned char*)malloc(first ? first : 1);
    if (!w->buffer) {
        fputs("Out of memory\n", stderr);
        abort();
    }
    memcpy(w->buffer, w->data, first);
    w->buffer_size = first;
    w->exposed = first;
    for (;;) {
        result = tfsxml_init(&w->p, w->buffer, (unsigned)w->buffer_size, 0);
        if (!result || !refill(w)) {
            break;
        }
    }
    if (!result) {
        w->is_init = 1;
        check(w, "init");
    }
    return result;
}

static void walker_start(walker* w, const unsigned char* data, size_t size, const walk_config* config, trace* out) {
    memset(w, 0, sizeof(*w));
    w->data = data;
    w->size = size;
    w->config = config;
    w->out = out;
    w->rng = config->seed;
    w->chunk_rng = config->chunk_seed;
    w->calls_max = 64 + 16 * (unsigned long)size;
}

int walk(const unsigned char* data, size_t size, const walk_config* config, trace* out) {
    walker w;
    int result;
    walker_start(&w, data, size, config, out);
    result = walk_init(&w, config->first);
    put(&w, "init ");
    put_number(&w, result);
    put(&w, "\n");
    if (!result && !w.stop) {
        walk_level(&w, -1);
    }
    if (w.stop != 2) {
        put(&w, "end ");
        put_number(&w, (long)(w.size - consumed(&w)));
        put(&w, "\n");
    }
    free(w.buffer);
    return w.stop == 2;
}

/** -------------------------------------------------------------------------
        Chaos walker
------------------------------------------------------------------------- **/

static unsigned touch(const tfsxml_string* s) {
    unsigned sum = 0, i;
    for (i = 0; i < s->len; i++) {
        sum += (unsigned char)s->buf[i];
    }
    return sum;
}

static void chaos_string(walker* w, const tfsxml_string* s) {
    trace decoded;
    tfsxml_string found;
    w->rng += touch(s);
    trace_init(&decoded);
    tfsxml_decode(&decoded, s, decode_append);
    if (decoded.len > s->len) {
        error(w, "decoded content is longer than the encoded content");
    }
    trace_free(&decoded);
    w->rng += (unsigned)tfsxml_strcmp_charp(*s, "a");
    w->rng += (unsigned)tfsxml_strncmp_charp(*s, "ab", 1);
    found = tfsxml_strstr_charp(*s, "a");
    w->rng += touch(&found);
}

int walk_chaos(const unsigned char* data, size_t size, unsigned seed, trace* out) {
    walk_config config;
    walker w;
    unsigned long i, count;
    memset(&config, 0, sizeof(config));
    config.seed = seed;
    config.first = 1 + seed % 16;
    config.chunk = 1 + (seed >> 4) % 64;
    config.chunk_seed = seed | 1;
    walker_start(&w, data, size, &config, out);
    if (!walk_init(&w, config.first) && !w.stop) {
        count = 64 + 4 * (unsigned long)size;
        for (i = 0; i < count && !w.stop; i++) {
            tfsxml_string a, b;
            unsigned x = rng_next(&w.rng);
            int result = 0, type = (int)((x >> 3) % call_max);
            if (!(x & 7)) {
                refill(&w); /* Even if the parser does not need more data */
                continue;
            }
            memset(&a, 0xA5, sizeof(a)); /* Garbage, the parser must not rely on it */
            memset(&b, 0xA5, sizeof(b));
            switch (type) {
                case call_next:  result = tfsxml_next(&w.p, &a); break;
                case call_attr:  result = tfsxml_attr(&w.p, &a, &b); break;
                case call_value: result = tfsxml_value(&w.p, &a); break;
                case call_enter: result = tfsxml_enter(&w.p); break;
#if REGRESSIONTEST_HAS_TEXT
                case call_text:  result = tfsxml_text(&w.p, &a); break;
#else
                case call_text:  result = -1; break;
#endif
                default:         result = tfsxml_leave(&w.p); break;
            }
            w.calls = 0; /* Calls without progress are expected */
            if (check(&w, call_names[type])) {
                break;
            }
            if (!result) {
                switch (type) {
                    case call_attr:
                        chaos_string(&w, &b);
                        /* fall through */
                    case call_next:
                    case call_value:
                    case call_text:
                        chaos_string(&w, &a);
                        break;
                    default:;
                }
            }
            else if (result > 0) {
                refill(&w);
            }
        }
    }
    free(w.buffer);
    return w.stop == 2;
}
