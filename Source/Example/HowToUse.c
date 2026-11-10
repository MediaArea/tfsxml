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
 * Example: outline of an XML file loaded in memory
 *
 * Usage: HowToUse file.xml [element names]
 * Elements with a name in the list are displayed with their value, other
 * elements are entered for displaying their sub-elements.
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "tfsxml/tfsxml.h"
#include <stdio.h>
#include <stdlib.h>

static int    names_count;
static char** names;

static void print_append(void* s, const char* buf, unsigned len) {
    fwrite(buf, 1, len, (FILE*)s);
}

/* Print the decoded content (entities and CDATA sections) */
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
static int parse_level(tfsxml_string* p, int depth) {
    tfsxml_string n, a, v;
    int result, i;

    /* Elements (and other content) of the current level */
    while (!(result = tfsxml_next(p, &n))) {
        for (i = 0; i < depth; i++) {
            fputs("  ", stdout);
        }
        if (n.len && (n.buf[0] == '?' || n.buf[0] == '!')) {
            /* Processing instruction, comment, declaration */
            printf("<%.*s>\n", (int)n.len, n.buf);
            continue;
        }
        printf("%.*s", (int)n.len, n.buf);

        /* Attributes */
        while (!(result = tfsxml_attr(p, &a, &v))) {
            printf(" %.*s=\"", (int)a.len, a.buf);
            print(&v);
            putchar('"');
        }
        if (result > 0) {
            return 1;
        }

        /* Value or sub-elements */
        if (is_in_list(n)) {
            result = tfsxml_value(p, &v);
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
            result = tfsxml_enter(p);
            if (result > 0 || (!result && parse_level(p, depth + 1))) {
                return 1;
            }
        }
    }

    /* -1 is the end of the level, 1 at the end of the content */
    return result > 0 && depth;
}

int main(int argc, char* argv[]) {
    FILE* f;
    char* buf;
    long size;
    tfsxml_string p;
    int truncated;

    if (argc < 2) {
        fputs("Usage: HowToUse file.xml [element names]\n", stderr);
        return 1;
    }
    names = argv + 2;
    names_count = argc - 2;

    /* Whole file in memory */
    f = fopen(argv[1], "rb");
    if (!f || fseek(f, 0, SEEK_END) || (size = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        fprintf(stderr, "%s can not be read\n", argv[1]);
        return 1;
    }
    buf = (char*)malloc(size ? (size_t)size : 1);
    if (!buf || fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "%s can not be read\n", argv[1]);
        return 1;
    }
    fclose(f);

    /* Parsing */
    if (tfsxml_init(&p, buf, (unsigned)size, 0)) {
        fprintf(stderr, "%s is not XML\n", argv[1]);
        free(buf);
        return 1;
    }
    truncated = parse_level(&p, 0);
    if (truncated) {
        fputs("Content is truncated\n", stderr);
    }

    free(buf);
    return truncated;
}
