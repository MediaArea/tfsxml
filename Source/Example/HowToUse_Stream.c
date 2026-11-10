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
 * Example: outline of an XML file read by small chunks (streaming)
 *
 * Usage: HowToUse_Stream file.xml [chunk size [element names]]
 * Same output as HowToUse, but the file is read by chunks: when the parser
 * needs more data (return value 1), the bytes not yet consumed by the parser
 * are kept, the next chunk is appended, and the same function is called again.
 * Strings returned by the parser are valid only until the buffer is modified.
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "tfsxml/tfsxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct stream {
    FILE*         f;
    char*         buf;
    size_t        cap;
    size_t        chunk;
    tfsxml_string p;
} stream;

static int    names_count;
static char** names;

/* More data after the bytes not consumed by the parser, return 0 if no more data */
static int refill(stream* s) {
    size_t keep = s->p.len, read;
    memmove(s->buf, s->p.buf, keep);
    if (keep + s->chunk > s->cap) {
        char* buf = (char*)realloc(s->buf, keep + s->chunk);
        if (!buf) {
            return 0;
        }
        s->buf = buf;
        s->cap = keep + s->chunk;
    }
    read = fread(s->buf + keep, 1, s->chunk, s->f);
    s->p.buf = s->buf;
    s->p.len = (unsigned)(keep + read);
    return read != 0;
}

/* Call FUNCTION again with more data while the parser needs more data */
#define CALL(RESULT, FUNCTION) \
    do { \
        RESULT = FUNCTION; \
    } while (RESULT == 1 && refill(s))

static void print_append(void* f, const char* buf, unsigned len) {
    fwrite(buf, 1, len, (FILE*)f);
}

static void print(const tfsxml_string* s) {
    tfsxml_decode(stdout, s, print_append);
}

static int is_in_list(tfsxml_string n) {
    int i;
    for (i = 0; i < names_count; i++) {
        if (!tfsxml_strcmp_charp(n, names[i])) {
            return 1;
        }
    }
    return 0;
}

/* Return 1 if the content is truncated */
static int parse_level(stream* s, int depth) {
    tfsxml_string n, a, v;
    int result, i;

    for (;;) {
        int in_list;
        CALL(result, tfsxml_next(&s->p, &n));
        if (result) {
            break;
        }
        for (i = 0; i < depth; i++) {
            fputs("  ", stdout);
        }
        if (n.len && (n.buf[0] == '?' || n.buf[0] == '!')) {
            printf("<%.*s>\n", (int)n.len, n.buf);
            continue;
        }
        printf("%.*s", (int)n.len, n.buf);
        in_list = is_in_list(n); /* n is no more valid after the next call */

        /* Attributes */
        for (;;) {
            CALL(result, tfsxml_attr(&s->p, &a, &v));
            if (result) {
                break;
            }
            printf(" %.*s=\"", (int)a.len, a.buf);
            print(&v);
            putchar('"');
        }
        if (result > 0) {
            return 1;
        }

        /* Value or sub-elements */
        if (in_list) {
            CALL(result, tfsxml_value(&s->p, &v));
            if (result > 0) {
                return 1;
            }
            if (!result) {
                fputs(": ", stdout);
                print(&v);
            }
            putchar('\n');
        }
        else {
            putchar('\n');
            CALL(result, tfsxml_enter(&s->p));
            if (result > 0 || (!result && parse_level(s, depth + 1))) {
                return 1;
            }
        }
    }

    /* -1 is the end of the level, 1 at the end of the content */
    return result > 0 && depth;
}

static int only_spaces(const char* buf, size_t len) {
    size_t i = len >= 3 && !memcmp(buf, "\xEF\xBB\xBF", 3) ? 3 : 0;
    for (; i < len; i++) {
        if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char* argv[]) {
    stream str;
    stream* s = &str;
    size_t size = 0;
    int truncated;

    if (argc < 2) {
        fputs("Usage: HowToUse_Stream file.xml [chunk size [element names]]\n", stderr);
        return 1;
    }
    memset(s, 0, sizeof(*s));
    s->chunk = argc > 2 ? (size_t)strtoul(argv[2], NULL, 10) : 4096;
    if (!s->chunk) {
        s->chunk = 4096;
    }
    names = argv + 3;
    names_count = argc > 3 ? argc - 3 : 0;
    s->f = fopen(argv[1], "rb");
    s->buf = (char*)malloc(s->chunk);
    if (!s->f || !s->buf) {
        fprintf(stderr, "%s can not be read\n", argv[1]);
        return 1;
    }
    s->cap = s->chunk;

    /* First chunk, more if there are only whitespaces */
    for (;;) {
        s->p.buf = s->buf;
        s->p.len = (unsigned)size;
        if (!refill(s)) {
            fprintf(stderr, "%s is not XML\n", argv[1]);
            return 1;
        }
        size = s->p.len;
        if (!tfsxml_init(&s->p, s->buf, (unsigned)size, 0)) {
            break;
        }
        if (!only_spaces(s->buf, size)) {
            fprintf(stderr, "%s is not XML\n", argv[1]);
            return 1;
        }
    }

    /* Parsing */
    truncated = parse_level(s, 0);
    if (truncated) {
        fputs("Content is truncated\n", stderr);
    }

    fclose(s->f);
    free(s->buf);
    return truncated;
}
