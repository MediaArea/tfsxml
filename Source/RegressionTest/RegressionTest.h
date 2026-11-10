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
 * Test helpers shared by the regression test tool and the fuzzer
 */

#ifndef REGRESSIONTEST_H
#define REGRESSIONTEST_H

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/* The fuzzer may build this code twice (current and reference implementations)
 * REGRESSIONTEST_LEGACY: for a version without tfsxml_text and without end tag
 * name returned by tfsxml_next (e.g. the version from MediaInfoLib), to be
 * defined for both implementations */
#ifdef REGRESSIONTEST_PREFIX
    #define REGRESSIONTEST_CAT2(A, B) A##B
    #define REGRESSIONTEST_CAT(A, B) REGRESSIONTEST_CAT2(A, B)
    #define walk REGRESSIONTEST_CAT(REGRESSIONTEST_PREFIX, walk)
    #define walk_chaos REGRESSIONTEST_CAT(REGRESSIONTEST_PREFIX, walk_chaos)
#endif

/** -------------------------------------------------------------------------
        Growable text buffer
------------------------------------------------------------------------- **/

typedef struct trace {
    char*  buf;
    size_t len;
    size_t cap;
} trace;

void trace_init(trace* t);
void trace_free(trace* t);
void trace_append(trace* t, const char* s, size_t len);
void trace_puts(trace* t, const char* s);

/** -------------------------------------------------------------------------
        Walk of an XML content with the tfsxml API
------------------------------------------------------------------------- **/

/* Call sequences */
enum walk_mode {
    walk_tree,   /* next, all attributes, enter in all elements */
    walk_value,  /* next, all attributes, value of all elements */
    walk_skip,   /* next only, attributes and content are skipped by the parser */
    walk_enter,  /* next and enter only, attributes are skipped by the parser */
    walk_mixed,  /* text, next, all attributes, enter in all elements (as for building a tree) */
    walk_random, /* pseudo-random valid sequence of calls, depends on seed */
    walk_mode_max
};
extern const char* const walk_mode_names[walk_mode_max];

typedef struct walk_config {
    int      mode;       /* walk_mode value */
    unsigned seed;       /* pseudo-random decisions of walk_random */
    size_t   first;      /* size of the first chunk of data, 0 for all data at once */
    size_t   chunk;      /* size of the next chunks, 0 for all remaining data at once */
    unsigned chunk_seed; /* if not 0, chunk sizes are pseudo-random in [1, chunk] */
} walk_config;

/** Parse data with a call sequence and append the trace of the results
 *
 * Each call result is a line with the call name, the returned value, the
 * returned strings (and their decoded version if different) and the count of
 * consumed bytes.
 * If chunks are used, data is provided in several buffers, each new buffer
 * containing the bytes not yet consumed by the parser and the next chunk,
 * the same call is done again when the parser needs more data. The trace must
 * not depend on the chunk sizes.
 *
 * @return  0 if the walk is OK
 *          other values if a harness check failed (details in the trace)
 */
int walk(const unsigned char* data, size_t size, const walk_config* config, trace* out);

/** Parse data with a random call sequence, including calls not following the
 *  API rules, and random refills of the buffer
 *
 * Only memory safety and parser position are checked.
 *
 * @return  0 if the walk is OK
 *          other values if a harness check failed (details in the trace)
 */
int walk_chaos(const unsigned char* data, size_t size, unsigned seed, trace* out);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
