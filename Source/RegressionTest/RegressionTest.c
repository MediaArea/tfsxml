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
 * Regression test tool
 *
 * - unit tests of the API
 * - comparison of the traces of the XML files with the expected traces
 * - check that the traces do not depend on how the data is split in chunks
 * - speed test
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#elif !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
    #define _POSIX_C_SOURCE 200809L
#endif
#include "RegressionTest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

int test_api(void);
int test_speed(const char* file_name, double seconds, int value_depth, const char* write_dir, int whole_only);

/* Seeds of the random walks in the expected traces */
#define RANDOM_SEEDS 3

/* Files up to this size are tested with all split points and byte per byte */
#define SMALL_FILE_MAX 16384

static int option_update;
static int option_check_only;
static int option_streaming = 1;
static int option_verbose;

/** -------------------------------------------------------------------------
        Files
------------------------------------------------------------------------- **/

static unsigned char* file_read(const char* name, size_t* size) {
    FILE* f = fopen(name, "rb");
    unsigned char* data;
    long len;
    if (!f) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return NULL;
    }
    data = (unsigned char*)malloc(len ? (size_t)len : 1);
    if (!data || fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    *size = (size_t)len;
    return data;
}

static int file_write(const char* name, const char* data, size_t size) {
    FILE* f = fopen(name, "wb");
    int ok;
    if (!f) {
        return 0;
    }
    ok = fwrite(data, 1, size, f) == size;
    return fclose(f) == 0 && ok;
}

static int ends_with(const char* s, const char* end) {
    size_t s_len = strlen(s), end_len = strlen(end);
    return s_len >= end_len && !strcmp(s + s_len - end_len, end);
}

/* .xml files, or any file except expected traces and documentation with --check-only */
static int is_input(const char* name) {
    if (option_check_only) {
        return !ends_with(name, ".xml.txt") && strcmp(name, "ReadMe.txt");
    }
    return ends_with(name, ".xml");
}

static int compare_names(const void* a, const void* b) {
    return strcmp(*(const char* const*)a, *(const char* const*)b);
}

typedef struct file_list {
    char** names;
    size_t count;
    size_t cap;
} file_list;

static void list_add(file_list* list, const char* name) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 64;
        list->names = (char**)realloc(list->names, list->cap * sizeof(char*));
        if (!list->names) {
            fputs("Out of memory\n", stderr);
            exit(2);
        }
    }
    list->names[list->count] = (char*)malloc(strlen(name) + 1);
    if (!list->names[list->count]) {
        fputs("Out of memory\n", stderr);
        exit(2);
    }
    strcpy(list->names[list->count++], name);
}

static char* path_join(const char* dir, const char* name) {
    char* path = (char*)malloc(strlen(dir) + strlen(name) + 2);
    if (!path) {
        fputs("Out of memory\n", stderr);
        exit(2);
    }
    snprintf(path, strlen(dir) + strlen(name) + 2, "%s/%s", dir, name);
    return path;
}

/* All .xml files in a directory and its sub-directories */
static void list_dir(file_list* list, const char* dir) {
#ifdef _WIN32
    WIN32_FIND_DATAA find;
    HANDLE handle;
    char* pattern = path_join(dir, "*");
    handle = FindFirstFileA(pattern, &find);
    free(pattern);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }
    do {
        char* path;
        if (find.cFileName[0] == '.') {
            continue;
        }
        path = path_join(dir, find.cFileName);
        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            list_dir(list, path);
        }
        else if (is_input(find.cFileName)) {
            list_add(list, path);
        }
        free(path);
    } while (FindNextFileA(handle, &find));
    FindClose(handle);
#else
    DIR* d = opendir(dir);
    struct dirent* entry;
    if (!d) {
        return;
    }
    while ((entry = readdir(d))) {
        struct stat st;
        char* path;
        if (entry->d_name[0] == '.') {
            continue;
        }
        path = path_join(dir, entry->d_name);
        if (!stat(path, &st)) {
            if (S_ISDIR(st.st_mode)) {
                list_dir(list, path);
            }
            else if (is_input(entry->d_name)) {
                list_add(list, path);
            }
        }
        free(path);
    }
    closedir(d);
#endif
}

static int is_dir(const char* name) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(name);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
    struct stat st;
    return !stat(name, &st) && S_ISDIR(st.st_mode);
#endif
}

/** -------------------------------------------------------------------------
        Traces
------------------------------------------------------------------------- **/

static void config_set(walk_config* config, int mode, unsigned seed) {
    memset(config, 0, sizeof(*config));
    config->mode = mode;
    config->seed = seed;
}

/* Count of walk configurations in an expected trace */
#define CONFIG_COUNT (walk_random + RANDOM_SEEDS)

static void config_get(walk_config* config, int index) {
    if (index < walk_random) {
        config_set(config, index, 0);
    }
    else {
        config_set(config, walk_random, (unsigned)(index - walk_random + 1));
    }
}

static void config_name(char* out, size_t size, const walk_config* config) {
    if (config->mode == walk_random) {
        snprintf(out, size, "%s %u", walk_mode_names[config->mode], config->seed);
    }
    else {
        snprintf(out, size, "%s", walk_mode_names[config->mode]);
    }
}

/* Line number and content of the first difference */
static void print_difference(const trace* expected, const trace* got) {
    size_t i = 0, line = 1, line_begin = 0, e_end, g_end;
    while (i < expected->len && i < got->len && expected->buf[i] == got->buf[i]) {
        if (expected->buf[i] == '\n') {
            line++;
            line_begin = i + 1;
        }
        i++;
    }
    e_end = line_begin;
    while (e_end < expected->len && expected->buf[e_end] != '\n') {
        e_end++;
    }
    g_end = line_begin;
    while (g_end < got->len && got->buf[g_end] != '\n') {
        g_end++;
    }
    printf("    line %lu\n", (unsigned long)line);
    printf("    expected: %.*s\n", (int)(e_end - line_begin), expected->len > line_begin ? expected->buf + line_begin : "");
    printf("    got:      %.*s\n", (int)(g_end - line_begin), got->len > line_begin ? got->buf + line_begin : "");
}

static int same(const trace* a, const trace* b) {
    return a->len == b->len && (!a->len || !memcmp(a->buf, b->buf, a->len));
}

/* Trace with chunks must be same as the trace without chunks */
static int check_chunks(const char* name, const unsigned char* data, size_t size, const walk_config* base, const trace* reference) {
    walk_config config = *base;
    trace got;
    int is_small = size <= SMALL_FILE_MAX;
    size_t points = is_small ? size : 16;
    size_t randoms = is_small ? 8 : 2;
    size_t i;
    unsigned seed;
    int failed = 0;
    trace_init(&got);
    for (i = 0; i < points + 1 + randoms && !failed; i++) {
        if (i < points) {
            /* 2 chunks, all split points (or some split points for big files) */
            config.first = is_small ? i + 1 : 1 + (size - 1) / points * i;
            config.chunk = 0;
            config.chunk_seed = 0;
        }
        else if (i == points) {
            /* Byte per byte (small files only) */
            if (!is_small) {
                continue;
            }
            config.first = 1;
            config.chunk = 1;
            config.chunk_seed = 0;
        }
        else {
            /* Random sizes */
            seed = (unsigned)(i - points);
            config.first = seed;
            config.chunk = is_small ? 2 * seed : 65536;
            config.chunk_seed = seed;
        }
        got.len = 0;
        if (walk(data, size, &config, &got) || !same(reference, &got)) {
            char mode[32];
            config_name(mode, sizeof(mode), &config);
            printf("FAILED: %s, %s, chunks of %lu then %lu%s\n", name, mode, (unsigned long)config.first, (unsigned long)config.chunk, config.chunk_seed ? " max" : "");
            print_difference(reference, &got);
            failed = 1;
        }
    }
    trace_free(&got);
    return failed;
}

static int check_file(const char* name) {
    trace all, one;
    size_t size = 0;
    unsigned char* data = file_read(name, &size);
    char* expected_name;
    int failed = 0, i;
    if (!data) {
        printf("FAILED: %s, can not be read\n", name);
        return 1;
    }
    trace_init(&all);
    trace_init(&one);

    for (i = 0; i < CONFIG_COUNT; i++) {
        walk_config config;
        char mode[32];
        config_get(&config, i);
        config_name(mode, sizeof(mode), &config);
        one.len = 0;
        if (walk(data, size, &config, &one)) {
            printf("FAILED: %s, %s\n%s", name, mode, one.buf);
            failed = 1;
        }
        trace_puts(&all, "== ");
        trace_puts(&all, mode);
        trace_puts(&all, "\n");
        trace_append(&all, one.buf, one.len);
        if (option_streaming && !failed) {
            failed |= check_chunks(name, data, size, &config, &one);
        }
    }

    expected_name = (char*)malloc(strlen(name) + 5);
    if (!expected_name) {
        fputs("Out of memory\n", stderr);
        exit(2);
    }
    snprintf(expected_name, strlen(name) + 5, "%s.txt", name);
    if (option_update) {
        if (!file_write(expected_name, all.buf, all.len)) {
            printf("FAILED: %s, can not be written\n", expected_name);
            failed = 1;
        }
    }
    else if (!option_check_only) {
        size_t expected_size = 0;
        unsigned char* expected_data = file_read(expected_name, &expected_size);
        if (!expected_data) {
            printf("FAILED: %s, no expected trace (%s)\n", name, expected_name);
            failed = 1;
        }
        else {
            trace expected;
            expected.buf = (char*)expected_data;
            expected.len = expected_size;
            expected.cap = expected_size;
            if (!same(&expected, &all)) {
                printf("FAILED: %s, trace is not the expected one\n", name);
                print_difference(&expected, &all);
                failed = 1;
            }
            free(expected_data);
        }
    }
    if (option_verbose && !failed) {
        printf("OK: %s\n", name);
    }

    free(expected_name);
    trace_free(&one);
    trace_free(&all);
    free(data);
    return failed;
}

/* Trace of a file on the standard output */
static int dump_file(const char* name, const walk_config* only, int with_name) {
    trace out;
    size_t size = 0;
    unsigned char* data = file_read(name, &size);
    int failed = 0, i;
    if (!data) {
        fprintf(stderr, "%s can not be read\n", name);
        return 1;
    }
    trace_init(&out);
    if (with_name) {
        trace_puts(&out, "#### ");
        trace_puts(&out, name);
        trace_puts(&out, "\n");
    }
    for (i = 0; i < CONFIG_COUNT; i++) {
        walk_config config;
        char mode[32];
        if (only) {
            if (i) {
                break;
            }
            config = *only;
        }
        else {
            config_get(&config, i);
        }
        config_name(mode, sizeof(mode), &config);
        trace_puts(&out, "== ");
        trace_puts(&out, mode);
        trace_puts(&out, "\n");
        failed |= walk(data, size, &config, &out);
    }
    fwrite(out.buf, 1, out.len, stdout);
    trace_free(&out);
    free(data);
    return failed;
}

/** -------------------------------------------------------------------------
        Main
------------------------------------------------------------------------- **/

static void usage(void) {
    puts("Usage: RegressionTest [options] [directories or files]\n"
         "\n"
         "Without option, run the unit tests and check the traces of the .xml files\n"
         "(each expected trace is in a file with the same name plus .txt)\n"
         "\n"
         "Options:\n"
         "  --update         write the expected traces instead of checking them\n"
         "  --check-only     do not compare with expected traces (for files without\n"
         "                   expected traces, e.g. real files or fuzzing corpus)\n"
         "  --no-streaming   do not check the traces with data in chunks (faster)\n"
         "  --verbose        list also files without issue\n"
         "  --dump           print the traces of the files\n"
         "  --mode=MODE      with --dump, only one walk mode\n"
         "                   (tree, value, skip, enter, random, default seed is 1)\n"
         "  --seed=N         with --dump and random mode, seed of the walk\n"
         "  --chunks=F,N[,S] with --dump, data in chunks: F bytes, then N bytes\n"
         "                   (or random sizes up to N with seed S)\n"
         "  --speed[=FILE]   speed test with synthetic files (or FILE)\n"
         "  --duration=S     with --speed, duration of each test in seconds\n"
         "  --value-depth=N  with --speed=FILE, depth of the elements having a value\n"
         "                   (default 2)\n"
         "  --whole          with --speed, only with all data at once (no chunks)\n"
         "  --speed-files=D  write the synthetic files of the speed test in directory D\n");
}

int main(int argc, char* argv[]) {
    file_list list;
    walk_config only;
    int has_only = 0, dump = 0, speed = 0, failed = 0, i;
    const char* speed_file = NULL;
    double duration = 1;
    int value_depth = 2;
    const char* speed_write_dir = NULL;
    int speed_whole_only = 0;
    size_t f;

    memset(&list, 0, sizeof(list));
    config_set(&only, walk_tree, 1);
    for (i = 1; i < argc; i++) {
        const char* arg = argv[i];
        if (!strcmp(arg, "--update")) {
            option_update = 1;
        }
        else if (!strcmp(arg, "--check-only")) {
            option_check_only = 1;
        }
        else if (!strcmp(arg, "--no-streaming")) {
            option_streaming = 0;
        }
        else if (!strcmp(arg, "--verbose")) {
            option_verbose = 1;
        }
        else if (!strcmp(arg, "--dump")) {
            dump = 1;
        }
        else if (!strncmp(arg, "--mode=", 7)) {
            int m;
            for (m = 0; m < walk_mode_max && strcmp(arg + 7, walk_mode_names[m]); m++) {
            }
            if (m == walk_mode_max) {
                usage();
                return 2;
            }
            only.mode = m;
            has_only = 1;
        }
        else if (!strncmp(arg, "--seed=", 7)) {
            only.seed = (unsigned)strtoul(arg + 7, NULL, 10);
            has_only = 1;
        }
        else if (!strncmp(arg, "--chunks=", 9)) {
            char* end;
            only.first = (size_t)strtoul(arg + 9, &end, 10);
            only.chunk = *end == ',' ? (size_t)strtoul(end + 1, &end, 10) : 0;
            only.chunk_seed = *end == ',' ? (unsigned)strtoul(end + 1, &end, 10) : 0;
            has_only = 1;
        }
        else if (!strcmp(arg, "--speed")) {
            speed = 1;
        }
        else if (!strncmp(arg, "--speed=", 8)) {
            speed = 1;
            speed_file = arg + 8;
        }
        else if (!strncmp(arg, "--duration=", 11)) {
            duration = atof(arg + 11);
        }
        else if (!strncmp(arg, "--value-depth=", 14)) {
            value_depth = atoi(arg + 14);
        }
        else if (!strcmp(arg, "--whole")) {
            speed_whole_only = 1;
        }
        else if (!strncmp(arg, "--speed-files=", 14)) {
            speed = 1;
            speed_write_dir = arg + 14;
        }
        else if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            usage();
            return 0;
        }
        else if (arg[0] == '-' && arg[1] == '-') {
            usage();
            return 2;
        }
        else if (is_dir(arg)) {
            file_list sub;
            memset(&sub, 0, sizeof(sub));
            list_dir(&sub, arg);
            qsort(sub.names, sub.count, sizeof(char*), compare_names);
            for (f = 0; f < sub.count; f++) {
                list_add(&list, sub.names[f]);
                free(sub.names[f]);
            }
            free(sub.names);
        }
        else {
            list_add(&list, arg);
        }
    }

    if (speed) {
        return test_speed(speed_file, duration, value_depth, speed_write_dir, speed_whole_only);
    }

    if (dump) {
        for (f = 0; f < list.count; f++) {
            failed |= dump_file(list.names[f], has_only ? &only : NULL, list.count > 1);
        }
    }
    else {
        size_t failed_count = 0;
        if (!option_update) {
            int api = test_api();
            printf("API unit tests: %s\n", api ? "FAILED" : "OK");
            failed |= api != 0;
        }
        for (f = 0; f < list.count; f++) {
            if (check_file(list.names[f])) {
                failed_count++;
            }
        }
        if (list.count) {
            printf("Files: %lu checked, %lu failed%s\n", (unsigned long)list.count, (unsigned long)failed_count, option_streaming ? "" : " (without streaming checks)");
        }
        failed |= failed_count != 0;
    }

    for (f = 0; f < list.count; f++) {
        free(list.names[f]);
    }
    free(list.names);
    return failed;
}
