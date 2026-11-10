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
 * Fuzz target (libFuzzer interface)
 *
 * For each input, with sanitizers catching memory and undefined behavior issues:
 * - all walk modes, with all data at once and with data in chunks (random
 *   sizes, byte per byte for small inputs): traces must be the same
 * - walk with random calls and random buffer refills
 * - string helpers compared to naive implementations
 * - if built with TFSXML_FUZZ_REFERENCE, traces compared to the ones of a
 *   reference implementation (e.g. previous release), see Fuzz_Reference.c
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "RegressionTest/RegressionTest.h"
#include "tfsxml/tfsxml.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef TFSXML_FUZZ_REFERENCE
int ref_walk(const unsigned char* data, size_t size, const walk_config* config, trace* out);
#endif

static void fail(const char* what, const trace* expected, const trace* got) {
    fprintf(stderr, "\n==== tfsxml fuzz check failed: %s\n", what);
    if (expected) {
        fprintf(stderr, "==== expected:\n%s\n", expected->buf ? expected->buf : "");
    }
    if (got) {
        fprintf(stderr, "==== got:\n%s\n", got->buf ? got->buf : "");
    }
    abort();
}

static int same(const trace* a, const trace* b) {
    return a->len == b->len && (!a->len || !memcmp(a->buf, b->buf, a->len));
}

/* FNV-1a */
static unsigned hash(const unsigned char* data, size_t size) {
    unsigned long h = 2166136261UL;
    size_t i;
    for (i = 0; i < size; i++) {
        h ^= data[i];
        h = (h * 16777619UL) & 0xFFFFFFFFUL;
    }
    return (unsigned)h;
}

#ifdef TFSXML_FUZZ_REFERENCE
/* Position after "need more data" at the end of the walk is not compared (may differ between versions) */
static void normalize_end(trace* t) {
    size_t end_line, last_line;
    if (t->len < 2) {
        return;
    }
    end_line = t->len - 1;
    while (end_line && t->buf[end_line - 1] != '\n') {
        end_line--;
    }
    if (!end_line || strncmp(t->buf + end_line, "end ", 4)) {
        return;
    }
    last_line = end_line - 1;
    while (last_line && t->buf[last_line - 1] != '\n') {
        last_line--;
    }
    {
        const char* at = strstr(t->buf + last_line, " 1 @");
        if (at && at < t->buf + end_line) {
            size_t cut = (size_t)(at - t->buf) + 2;
            t->len = cut;
            t->buf[cut] = '\0';
            trace_puts(t, "\nend ?\n");
        }
    }
}
#endif

static void check_walk(const unsigned char* data, size_t size, int mode, unsigned seed) {
    walk_config config;
    trace whole, chunked;
    memset(&config, 0, sizeof(config));
    config.mode = mode;
    config.seed = seed;
    trace_init(&whole);
    trace_init(&chunked);

    if (walk(data, size, &config, &whole)) {
        fail("walk with all data at once", NULL, &whole);
    }

    /* Random chunk sizes */
    config.first = 1 + (seed >> 8) % 16;
    config.chunk = 1 + (seed >> 12) % 16;
    config.chunk_seed = seed | 1;
    if (walk(data, size, &config, &chunked) || !same(&whole, &chunked)) {
        fail("walk with data in chunks of random sizes", &whole, &chunked);
    }

    /* Byte per byte */
    if (size <= 256) {
        chunked.len = 0;
        config.first = 1;
        config.chunk = 1;
        config.chunk_seed = 0;
        if (walk(data, size, &config, &chunked) || !same(&whole, &chunked)) {
            fail("walk with data byte per byte", &whole, &chunked);
        }
    }

#ifdef TFSXML_FUZZ_REFERENCE
    {
        trace reference;
        trace_init(&reference);
        memset(&config, 0, sizeof(config));
        config.mode = mode;
        config.seed = seed;
        ref_walk(data, size, &config, &reference);
        normalize_end(&reference);
        normalize_end(&whole);
        if (!same(&reference, &whole)) {
            fail("walk compared to the reference implementation", &reference, &whole);
        }
        trace_free(&reference);
    }
#endif

    trace_free(&chunked);
    trace_free(&whole);
}

/* Naive implementations of the helpers */
static int sign(int value) {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

static int naive_strncmp(const unsigned char* a, size_t a_len, const unsigned char* b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = i < a_len ? a[i] : -1; /* End of a is lower than any byte */
        int cb = b[i] ? b[i] : -2;      /* End of b is lower than any byte and than end of a */
        if (ca == -1 && cb == -2) {
            return 0;
        }
        if (ca == -1) {
            return -1;
        }
        if (cb == -2) {
            return 1;
        }
        if (ca != cb) {
            return ca - cb;
        }
    }
    return 0;
}

static void check_helpers(const unsigned char* data, size_t size, unsigned seed) {
    size_t split, b_len, i;
    char* b;
    tfsxml_string a, found;
    unsigned n;
    int expected;

    if (size < 2) {
        return;
    }
    split = seed % size;
    b_len = size - split;
    if (b_len > 32) {
        b_len = 32;
    }
    b = (char*)malloc(b_len + 1);
    if (!b) {
        abort();
    }
    memcpy(b, data + split, b_len);
    b[b_len] = '\0';
    b_len = strlen(b);
    a.buf = (const char*)data;
    a.len = (unsigned)split;
    a.flags = 0;

    /* strcmp */
    expected = naive_strncmp(data, split, (const unsigned char*)b, split + b_len + 1);
    if (sign(tfsxml_strcmp_charp(a, b)) != sign(expected)) {
        fail("tfsxml_strcmp_charp", NULL, NULL);
    }

    /* strncmp */
    n = (seed >> 8) % 40;
    expected = naive_strncmp(data, split, (const unsigned char*)b, n);
    if (sign(tfsxml_strncmp_charp(a, b, n)) != sign(expected)) {
        fail("tfsxml_strncmp_charp", NULL, NULL);
    }

    /* strstr */
    found = tfsxml_strstr_charp(a, b);
    for (i = 0; i + b_len <= split; i++) {
        if (!memcmp(data + i, b, b_len)) {
            break;
        }
    }
    if (i + b_len <= split) {
        if (found.buf != (const char*)data + i || found.len != split - i) {
            fail("tfsxml_strstr_charp (found)", NULL, NULL);
        }
    }
    else if (found.buf || found.len) {
        fail("tfsxml_strstr_charp (not found)", NULL, NULL);
    }

    /* Namespaces in names: local name after the last ":", prefix before it */
    {
        size_t colon = split; /* Position after the last ":", 0 if none */
        tfsxml_string local, prefix;
        while (colon && data[colon - 1] != ':') {
            colon--;
        }
        local = tfsxml_local_name(a);
        if (local.buf != (const char*)data + colon || local.len != split - colon) {
            fail("tfsxml_local_name", NULL, NULL);
        }
        prefix = tfsxml_prefix(a);
        if (colon ? (prefix.buf != (const char*)data || prefix.len != colon - 1) : (prefix.buf || prefix.len)) {
            fail("tfsxml_prefix", NULL, NULL);
        }
        expected = naive_strncmp(data + colon, split - colon, (const unsigned char*)b, split + b_len + 1);
        if (sign(tfsxml_strcmp_local_charp(a, b)) != sign(expected)) {
            fail("tfsxml_strcmp_local_charp", NULL, NULL);
        }
        expected = naive_strncmp(data, colon ? colon - 1 : 0, (const unsigned char*)b, split + b_len + 1);
        if (sign(tfsxml_strcmp_prefix_charp(a, b)) != sign(expected)) {
            fail("tfsxml_strcmp_prefix_charp", NULL, NULL);
        }
    }

    free(b);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    unsigned seed = hash(data, size);
    int mode;
    trace chaos;

    for (mode = 0; mode < walk_mode_max; mode++) {
        check_walk(data, size, mode, seed + (unsigned)mode);
    }

    trace_init(&chaos);
    if (walk_chaos(data, size, seed, &chaos)) {
        fail("walk with random calls", NULL, &chaos);
    }
    trace_free(&chaos);

    check_helpers(data, size, seed);
    return 0;
}
