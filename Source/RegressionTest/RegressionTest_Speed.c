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
 * Speed test
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#elif !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif
#include "RegressionTest.h"
#include "tfsxml/tfsxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <time.h>
#endif

/** -------------------------------------------------------------------------
        Timer
------------------------------------------------------------------------- **/

static double now(void) {
#ifdef _WIN32
    LARGE_INTEGER frequency, counter;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)frequency.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

/** -------------------------------------------------------------------------
        Synthetic content
------------------------------------------------------------------------- **/

static unsigned rng_state = 1;

static unsigned rng(void) {
    rng_state = rng_state * 1103515245u + 12345u;
    return (rng_state >> 16) & 0x7FFF;
}

/* Audio Definition Model like: many small elements with attributes */
static void make_adm(trace* t, size_t target) {
    unsigned c = 0, b;
    char tmp[512];
    trace_puts(t, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                  "<ebuCoreMain xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns=\"urn:ebu:metadata-schema:ebucore\">\n"
                  "  <coreMetadata>\n"
                  "    <format>\n"
                  "      <audioFormatExtended version=\"ITU-R_BS.2076-2\">\n");
    while (t->len < target) {
        c++;
        snprintf(tmp, sizeof(tmp), "        <audioChannelFormat audioChannelFormatID=\"AC_0003%04X\" audioChannelFormatName=\"Object %u\" typeLabel=\"0003\" typeDefinition=\"Objects\">\n", c, c);
        trace_puts(t, tmp);
        for (b = 1; b <= 64; b++) {
            snprintf(tmp, sizeof(tmp), "          <audioBlockFormat audioBlockFormatID=\"AB_0003%04X_%08X\" rtime=\"00:00:%02u.%05u\" duration=\"00:00:00.02000\">\n"
                         "            <cartesian>1</cartesian>\n"
                         "            <position coordinate=\"X\">%d.%06u</position>\n"
                         "            <position coordinate=\"Y\">%d.%06u</position>\n"
                         "            <position coordinate=\"Z\">0.000000</position>\n"
                         "            <gain>1.000000</gain>\n"
                         "            <jumpPosition interpolationLength=\"0.005000\">1</jumpPosition>\n"
                         "          </audioBlockFormat>\n",
                    c, b, b / 50, (b % 50) * 2000, (int)(rng() % 3) - 1, rng() * 31, (int)(rng() % 3) - 1, rng() * 31);
            trace_puts(t, tmp);
        }
        trace_puts(t, "        </audioChannelFormat>\n");
    }
    trace_puts(t, "      </audioFormatExtended>\n"
                  "    </format>\n"
                  "  </coreMetadata>\n"
                  "</ebuCoreMain>\n");
}

/* XMP like: few elements with long values (embedded pictures) */
static void make_text(trace* t, size_t target) {
    static const char base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char line[77];
    unsigned i;
    trace_puts(t, "<?xpacket begin=\"\xEF\xBB\xBF\" id=\"W5M0MpCehiHzreSzNTczkc9d\"?>\n"
                  "<x:xmpmeta xmlns:x=\"adobe:ns:meta/\">\n"
                  " <rdf:RDF xmlns:rdf=\"http://www.w3.org/1999/02/22-rdf-syntax-ns#\">\n"
                  "  <rdf:Description rdf:about=\"\" xmlns:GImage=\"http://ns.google.com/photos/1.0/image/\">\n"
                  "   <GImage:Mime>image/jpeg</GImage:Mime>\n"
                  "   <GImage:Data>");
    line[76] = '\n';
    while (t->len < target) {
        for (i = 0; i < 76; i++) {
            line[i] = base64[rng() & 63];
        }
        trace_append(t, line, 77);
    }
    trace_puts(t, "</GImage:Data>\n"
                  "  </rdf:Description>\n"
                  " </rdf:RDF>\n"
                  "</x:xmpmeta>\n"
                  "<?xpacket end=\"w\"?>\n");
}

/* Entities, comments and CDATA */
static void make_markup(trace* t, size_t target) {
    char tmp[512];
    unsigned i = 0;
    trace_puts(t, "<?xml version=\"1.0\"?>\n<!DOCTYPE doc>\n<doc>\n");
    while (t->len < target) {
        i++;
        snprintf(tmp, sizeof(tmp), "  <!-- item %u -->\n"
                     "  <item id=\"%u\" name=\"A &amp; B &lt;%u&gt;\">Fish &amp; chips &#x20AC;%u.%02u<![CDATA[ <raw> & ]]>end</item>\n"
                     "  <note><![CDATA[x < y && y > z]]></note>\n",
                i, i, i, i % 100, i % 100);
        trace_puts(t, tmp);
    }
    trace_puts(t, "</doc>\n");
}

/* Deep nesting */
static void make_deep(trace* t, size_t target) {
    unsigned depth;
    trace_puts(t, "<root>\n");
    while (t->len < target) {
        for (depth = 0; depth < 200; depth++) {
            trace_puts(t, "<level a=\"1\">");
        }
        trace_puts(t, "x");
        for (depth = 0; depth < 200; depth++) {
            trace_puts(t, "</level>");
        }
        trace_puts(t, "\n");
    }
    trace_puts(t, "</root>\n");
}

/** -------------------------------------------------------------------------
        Lean walker
------------------------------------------------------------------------- **/

enum speed_mode {
    speed_tree,   /* next, attributes, enter all elements */
    speed_value,  /* next, attributes, enter elements until a depth, value of elements at this depth */
    speed_skip,   /* next, enter the root element only (its content is skipped by the parser) */
    speed_decode, /* same as value, attributes and values are decoded */
    speed_mode_max
};
static const char* const speed_mode_names[speed_mode_max] = {
    "tree",
    "value",
    "skip",
    "decode",
};

typedef struct lean {
    tfsxml_string        p;
    const unsigned char* data;
    size_t               size;
    size_t               exposed;
    size_t               chunk;
    unsigned char*       buffer;
    size_t               buffer_cap;
    unsigned long        count;
    trace                decoded;
} lean;

static void lean_decode_append(void* s, const char* buf, unsigned len) {
    trace_append((trace*)s, buf, len);
}

static int lean_refill(lean* l) {
    size_t keep, add;
    if (!l->chunk || l->exposed >= l->size) {
        return 0;
    }
    keep = l->p.len;
    add = l->size - l->exposed < l->chunk ? l->size - l->exposed : l->chunk;
    if (keep + add > l->buffer_cap) {
        unsigned char* buffer = (unsigned char*)malloc(keep + add);
        if (!buffer) {
            return 0;
        }
        memcpy(buffer, l->p.buf, keep);
        free(l->buffer);
        l->buffer = buffer;
        l->buffer_cap = keep + add;
    }
    else {
        memmove(l->buffer, l->p.buf, keep);
    }
    memcpy(l->buffer + keep, l->data + l->exposed, add);
    l->exposed += add;
    l->p.buf = (const char*)l->buffer;
    l->p.len = (unsigned)(keep + add);
    return 1;
}

#define LEAN_CALL(RESULT, CALL) \
    do { \
        RESULT = CALL; \
    } while (RESULT == 1 && lean_refill(l));

static void lean_count(lean* l, int mode, const tfsxml_string* v) {
    if (mode == speed_decode) {
        l->decoded.len = 0;
        tfsxml_decode(&l->decoded, v, lean_decode_append);
        l->count += (unsigned long)l->decoded.len;
    }
    else {
        l->count += v->len;
    }
}

/* value and decode modes: elements are entered until value_depth, then their value is parsed */
static int lean_level(lean* l, int mode, int depth, int value_depth) {
    for (;;) {
        tfsxml_string n, v;
        int result;
        LEAN_CALL(result, tfsxml_next(&l->p, &n));
        if (result) {
            return result;
        }
        l->count++;
        if (n.len && (n.buf[0] == '?' || n.buf[0] == '!')) {
            continue;
        }
        if (mode == speed_skip && depth) {
            continue; /* Content skipped by the parser */
        }
        if (mode != speed_skip) {
            for (;;) {
                LEAN_CALL(result, tfsxml_attr(&l->p, &n, &v));
                if (result) {
                    break;
                }
                lean_count(l, mode, &v);
            }
            if (result > 0) {
                return result;
            }
        }
        if ((mode == speed_value || mode == speed_decode) && depth >= value_depth) {
            LEAN_CALL(result, tfsxml_value(&l->p, &v));
            if (result > 0) {
                return result;
            }
            if (!result) {
                lean_count(l, mode, &v);
            }
            continue;
        }
        LEAN_CALL(result, tfsxml_enter(&l->p));
        if (result > 0) {
            return result;
        }
        if (!result) {
            result = lean_level(l, mode, depth + 1, value_depth);
            if (result > 0) {
                return result;
            }
        }
    }
}

static unsigned long lean_walk(const unsigned char* data, size_t size, int mode, size_t chunk, int value_depth) {
    lean l;
    memset(&l, 0, sizeof(l));
    trace_init(&l.decoded);
    l.data = data;
    l.size = size;
    l.chunk = chunk;
    if (chunk) {
        l.exposed = chunk < size ? chunk : size;
        l.buffer_cap = l.exposed;
        l.buffer = (unsigned char*)malloc(l.buffer_cap ? l.buffer_cap : 1);
        if (!l.buffer) {
            return 0;
        }
        memcpy(l.buffer, data, l.exposed);
        data = l.buffer;
        size = l.exposed;
    }
    if (!tfsxml_init(&l.p, data, (unsigned)size, 0)) {
        lean_level(&l, mode, 0, value_depth);
    }
    free(l.buffer);
    trace_free(&l.decoded);
    return l.count;
}

/** -------------------------------------------------------------------------
        Main
------------------------------------------------------------------------- **/

/* with_chunks: also with data in chunks (not for long values, the value is parsed again from its start after each new chunk)
 * value_depth: depth of elements having a value (value and decode modes) */
static void speed_run(const char* name, const unsigned char* data, size_t size, double duration, int with_chunks, int value_depth) {
    static const size_t chunks[] = { 0, 4096 };
    int mode;
    size_t c;
    for (c = 0; c < (with_chunks ? sizeof(chunks) / sizeof(chunks[0]) : 1); c++) {
        for (mode = 0; mode < speed_mode_max; mode++) {
            double start = now(), elapsed, best = 1e9;
            unsigned long runs = 0, count = 0;
            do {
                double run_start = now(), run;
                count = lean_walk(data, size, mode, chunks[c], value_depth);
                run = now() - run_start;
                if (run < best) {
                    best = run;
                }
                runs++;
                elapsed = now() - start;
            } while (elapsed < duration);
            printf("%-8s %-7s %-12s %8.1f MB/s  (%lu runs, count %lu)\n", name, speed_mode_names[mode], chunks[c] ? "chunks 4 KiB" : "whole",
                   (double)size / best / (1024 * 1024), runs, count);
        }
    }
}

int test_speed(const char* file_name, double duration, int value_depth, const char* write_dir, int whole_only) {
    static const char* const names[] = { "adm", "text", "markup", "deep" };
    static const int value_depths[] = { 6, 3, 1, 100 };
    size_t target = 32 * 1024 * 1024;
    int i;
    if (write_dir) {
        /* Synthetic files written instead of tested */
        for (i = 0; i < 4; i++) {
            trace t;
            char name[1024];
            FILE* f;
            trace_init(&t);
            rng_state = 1;
            switch (i) {
                case 0: make_adm(&t, target); break;
                case 1: make_text(&t, target); break;
                case 2: make_markup(&t, target); break;
                default: make_deep(&t, target);
            }
            snprintf(name, sizeof(name), "%s/%s.xml", write_dir, names[i]);
            f = fopen(name, "wb");
            if (!f || fwrite(t.buf, 1, t.len, f) != t.len) {
                fprintf(stderr, "%s can not be written\n", name);
                return 1;
            }
            fclose(f);
            printf("%s (value depth %d)\n", name, value_depths[i]);
            trace_free(&t);
        }
        return 0;
    }
    if (file_name) {
        FILE* f = fopen(file_name, "rb");
        trace t;
        char tmp[65536];
        size_t read;
        if (!f) {
            fprintf(stderr, "%s can not be read\n", file_name);
            return 1;
        }
        trace_init(&t);
        while ((read = fread(tmp, 1, sizeof(tmp), f))) {
            trace_append(&t, tmp, read);
        }
        fclose(f);
        speed_run("file", (const unsigned char*)t.buf, t.len, duration, !whole_only, value_depth);
        trace_free(&t);
        return 0;
    }
    for (i = 0; i < 4; i++) {
        trace t;
        trace_init(&t);
        rng_state = 1;
        switch (i) {
            case 0: make_adm(&t, target); break;
            case 1: make_text(&t, target); break;
            case 2: make_markup(&t, target); break;
            default: make_deep(&t, target);
        }
        speed_run(names[i], (const unsigned char*)t.buf, t.len, duration, i != 1 && !whole_only, value_depths[i]);
        trace_free(&t);
    }
    return 0;
}
