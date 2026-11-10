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
 * Standalone driver of the fuzz target, for compilers without libFuzzer
 *
 * Usage: Fuzz [options] [corpus directories or files]
 * - without TFSXML_FUZZ_COVERAGE: run each file once (corpus replay)
 * - with TFSXML_FUZZ_COVERAGE (POSIX, GCC or Clang with
 *   -fsanitize-coverage=trace-pc on tfsxml.c): coverage-guided fuzzing, new
 *   interesting inputs are saved in the first directory (as libFuzzer does,
 *   so the test files directory must not be the first one), crashes and
 *   timeouts in -artifact_prefix. Options are similar to libFuzzer ones:
 *   -runs=N -max_total_time=S -max_len=N -seed=N -timeout=S -dict=FILE
 *   -artifact_prefix=PATH -print_final_stats=1
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#elif !defined(_WIN32) && !defined(_XOPEN_SOURCE)
    #define _XOPEN_SOURCE 700
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif
#ifdef TFSXML_FUZZ_COVERAGE
    #include <signal.h>
    #include <time.h>
    #include <unistd.h>
    #include <sys/time.h>
#endif

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

/** -------------------------------------------------------------------------
        Files
------------------------------------------------------------------------- **/

typedef struct input {
    unsigned char* data;
    size_t         size;
} input;

typedef struct input_list {
    input* items;
    size_t count;
    size_t cap;
} input_list;

static void list_add(input_list* list, const unsigned char* data, size_t size) {
    if (list->count == list->cap) {
        list->cap = list->cap ? list->cap * 2 : 256;
        list->items = (input*)realloc(list->items, list->cap * sizeof(input));
        if (!list->items) {
            abort();
        }
    }
    list->items[list->count].data = (unsigned char*)malloc(size ? size : 1);
    if (!list->items[list->count].data) {
        abort();
    }
    if (size) {
        memcpy(list->items[list->count].data, data, size);
    }
    list->items[list->count].size = size;
    list->count++;
}

static int file_load(input_list* list, const char* name, size_t max_len) {
    FILE* f = fopen(name, "rb");
    unsigned char* data;
    long len;
    if (!f) {
        return 0;
    }
    if (fseek(f, 0, SEEK_END) || (len = ftell(f)) < 0 || fseek(f, 0, SEEK_SET)) {
        fclose(f);
        return 0;
    }
    data = (unsigned char*)malloc(len ? (size_t)len : 1);
    if (!data || fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);
    list_add(list, data, max_len && (size_t)len > max_len ? max_len : (size_t)len);
    free(data);
    return 1;
}

static void path_load(input_list* list, const char* path, size_t max_len) {
#ifdef _WIN32
    DWORD attributes = GetFileAttributesA(path);
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        WIN32_FIND_DATAA find;
        char* pattern = (char*)malloc(strlen(path) + 3);
        HANDLE handle;
        if (!pattern) {
            abort();
        }
        snprintf(pattern, strlen(path) + 3, "%s/*", path);
        handle = FindFirstFileA(pattern, &find);
        free(pattern);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }
        do {
            char* sub;
            if (find.cFileName[0] == '.') {
                continue;
            }
            sub = (char*)malloc(strlen(path) + strlen(find.cFileName) + 2);
            if (!sub) {
                abort();
            }
            snprintf(sub, strlen(path) + strlen(find.cFileName) + 2, "%s/%s", path, find.cFileName);
            path_load(list, sub, max_len);
            free(sub);
        } while (FindNextFileA(handle, &find));
        FindClose(handle);
        return;
    }
#else
    struct stat st;
    if (!stat(path, &st) && S_ISDIR(st.st_mode)) {
        DIR* d = opendir(path);
        struct dirent* entry;
        if (!d) {
            return;
        }
        while ((entry = readdir(d))) {
            char* sub;
            if (entry->d_name[0] == '.') {
                continue;
            }
            sub = (char*)malloc(strlen(path) + strlen(entry->d_name) + 2);
            if (!sub) {
                abort();
            }
            snprintf(sub, strlen(path) + strlen(entry->d_name) + 2, "%s/%s", path, entry->d_name);
            path_load(list, sub, max_len);
            free(sub);
        }
        closedir(d);
        return;
    }
#endif
    if (!file_load(list, path, max_len)) {
        fprintf(stderr, "%s can not be read\n", path);
    }
}

#ifndef TFSXML_FUZZ_COVERAGE

/** -------------------------------------------------------------------------
        Corpus replay
------------------------------------------------------------------------- **/

int main(int argc, char* argv[]) {
    input_list list;
    size_t i;
    int a;
    memset(&list, 0, sizeof(list));
    for (a = 1; a < argc; a++) {
        if (argv[a][0] == '-') {
            continue; /* libFuzzer options are ignored */
        }
        path_load(&list, argv[a], 0);
    }
    for (i = 0; i < list.count; i++) {
        LLVMFuzzerTestOneInput(list.items[i].data, list.items[i].size);
        free(list.items[i].data);
    }
    free(list.items);
    printf("%lu inputs OK\n", (unsigned long)list.count);
    return 0;
}

#else

/** -------------------------------------------------------------------------
        Coverage
------------------------------------------------------------------------- **/

#define MAP_SIZE (1 << 16)

static unsigned char coverage[MAP_SIZE];
static unsigned char virgin[MAP_SIZE];
static uintptr_t coverage_previous;

/* Called by the compiler at each edge of the instrumented code (-fsanitize-coverage=trace-pc) */
void __sanitizer_cov_trace_pc(void) {
    uintptr_t pc = (uintptr_t)__builtin_return_address(0);
    uintptr_t h = (pc ^ (pc >> 15) ^ (pc >> 31)) * 2654435761u;
    unsigned index = (unsigned)(((h >> 8) ^ coverage_previous) & (MAP_SIZE - 1));
    coverage[index]++;
    coverage_previous = (h >> 9) & (MAP_SIZE - 1);
}

static unsigned char bucket(unsigned char count) {
    if (count <= 3) {
        return (unsigned char)(1 << (count - 1));
    }
    if (count < 8) {
        return 8;
    }
    if (count < 16) {
        return 16;
    }
    if (count < 32) {
        return 32;
    }
    if (count < 128) {
        return 64;
    }
    return 128;
}

/* Return count of new map entries (or new hit counts) */
static unsigned coverage_new(unsigned* edges) {
    unsigned i, found = 0;
    for (i = 0; i < MAP_SIZE; i++) {
        if (coverage[i]) {
            unsigned char b = bucket(coverage[i]);
            if (b & ~virgin[i]) {
                if (!virgin[i]) {
                    (*edges)++;
                }
                virgin[i] |= b;
                found++;
            }
        }
    }
    return found;
}

/** -------------------------------------------------------------------------
        Crashes and timeouts
------------------------------------------------------------------------- **/

static const unsigned char* current_data;
static size_t current_size;
static char artifact_prefix[1024] = "./";
static volatile unsigned long runs_done;
static unsigned timeout = 10;

static void write_artifact(const char* kind) {
    char name[1200];
    unsigned long h = 2166136261UL;
    size_t i;
    FILE* f;
    if (!current_data) {
        return; /* Not in the fuzz target */
    }
    for (i = 0; i < current_size; i++) {
        h = ((h ^ current_data[i]) * 16777619UL) & 0xFFFFFFFFUL;
    }
    snprintf(name, sizeof(name), "%s%s-%08lx", artifact_prefix, kind, h);
    f = fopen(name, "wb");
    if (f) {
        fwrite(current_data, 1, current_size, f);
        fclose(f);
        fprintf(stderr, "\n==== %s, input written to %s\n", kind, name);
    }
}

/* Weak, so it works without sanitizers too */
extern void __sanitizer_set_death_callback(void (*callback)(void)) __attribute__((weak));

static void on_death(void) {
    write_artifact("crash");
}

static void on_signal(int sig) {
    signal(sig, SIG_DFL);
    write_artifact("crash");
    raise(sig);
}

static void on_alarm(int sig) {
    static unsigned long last_runs = (unsigned long)-1;
    static unsigned seconds;
    (void)sig;
    if (runs_done != last_runs) {
        last_runs = runs_done;
        seconds = 0;
        return;
    }
    if (++seconds >= timeout) {
        write_artifact("timeout");
        _exit(70);
    }
}

/** -------------------------------------------------------------------------
        Mutations
------------------------------------------------------------------------- **/

static uint64_t rng_state = 88172645463325252ULL;

static unsigned rng(void) {
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 7;
    rng_state ^= rng_state << 17;
    return (unsigned)(rng_state >> 16);
}

static const char* const builtin_tokens[] = {
    "<", ">", "/", "=", "\"", "'", "&", ";", "!", "?", "[", "]", "-", " ", "\t", "\n", "\r",
    "</", "/>", "<?", "?>", "<!--", "-->", "<![CDATA[", "]]>", "<!DOCTYPE", "<?xml version=\"1.0\"?>",
    "&amp;", "&lt;", "&gt;", "&quot;", "&apos;", "&#", "&#x", "&#65;", "&#x20AC;", "&#x10FFFF;", "&#xD800;",
    "\xEF\xBB\xBF", "\xC3\xA9", "a", "b", "x=\"1\"", " a='2'", "<a>", "</a>", "<b/>", "<c d=\"e\">",
};

typedef struct dictionary {
    char**  tokens;
    size_t* sizes;
    size_t  count;
} dictionary;

static dictionary dict;

static void dict_add(const char* token, size_t size) {
    dict.tokens = (char**)realloc(dict.tokens, (dict.count + 1) * sizeof(char*));
    dict.sizes = (size_t*)realloc(dict.sizes, (dict.count + 1) * sizeof(size_t));
    if (!dict.tokens || !dict.sizes) {
        abort();
    }
    dict.tokens[dict.count] = (char*)malloc(size + 1);
    if (!dict.tokens[dict.count]) {
        abort();
    }
    memcpy(dict.tokens[dict.count], token, size);
    dict.sizes[dict.count] = size;
    dict.count++;
}

/* libFuzzer/AFL dictionary format: name="value" with \xHH and \\ \" escapes */
static void dict_load(const char* name) {
    FILE* f = fopen(name, "rb");
    char line[4096];
    if (!f) {
        fprintf(stderr, "%s can not be read\n", name);
        exit(1);
    }
    while (fgets(line, sizeof(line), f)) {
        char token[4096];
        size_t size = 0;
        char* p = strchr(line, '"');
        if (line[0] == '#' || !p) {
            continue;
        }
        for (p++; *p && *p != '"'; p++) {
            if (*p == '\\' && p[1] == 'x' && p[2] && p[3]) {
                char hex[3];
                hex[0] = p[2];
                hex[1] = p[3];
                hex[2] = '\0';
                token[size++] = (char)strtoul(hex, NULL, 16);
                p += 3;
            }
            else if (*p == '\\' && p[1]) {
                token[size++] = *++p;
            }
            else {
                token[size++] = *p;
            }
        }
        if (size) {
            dict_add(token, size);
        }
    }
    fclose(f);
}

static size_t mutate(unsigned char* data, size_t size, size_t max_len, const input_list* corpus) {
    unsigned count = 1 + rng() % 8, i;
    for (i = 0; i < count; i++) {
        size_t pos = size ? rng() % size : 0;
        switch (rng() % 10) {
            case 0: /* Bit flip */
                if (size) {
                    data[pos] ^= (unsigned char)(1 << (rng() % 8));
                }
                break;
            case 1: /* Random byte */
                if (size) {
                    data[pos] = (unsigned char)rng();
                }
                break;
            case 2: /* Delete bytes */
                if (size) {
                    size_t len = 1 + rng() % (size - pos < 16 ? size - pos : 16);
                    memmove(data + pos, data + pos + len, size - pos - len);
                    size -= len;
                }
                break;
            case 3: /* Duplicate bytes */
                if (size && size < max_len) {
                    size_t from = rng() % size;
                    size_t len = 1 + rng() % 32;
                    if (len > size - from) {
                        len = size - from;
                    }
                    if (len > max_len - size) {
                        len = max_len - size;
                    }
                    memmove(data + pos + len, data + pos, size - pos);
                    memmove(data + pos, data + (from < pos ? from : from + len), len);
                    size += len;
                }
                break;
            case 4: /* Insert byte */
                if (size < max_len) {
                    memmove(data + pos + 1, data + pos, size - pos);
                    data[pos] = (unsigned char)rng();
                    size++;
                }
                break;
            case 5:
            case 6: /* Insert token */
            case 7: { /* Overwrite with token */
                const char* token;
                size_t len, k = rng() % (dict.count + sizeof(builtin_tokens) / sizeof(builtin_tokens[0]));
                if (k < dict.count) {
                    token = dict.tokens[k];
                    len = dict.sizes[k];
                }
                else {
                    token = builtin_tokens[k - dict.count];
                    len = strlen(token);
                }
                if (len > max_len - size) {
                    break;
                }
                if ((rng() % 3) || pos + len > size) {
                    memmove(data + pos + len, data + pos, size - pos);
                    size += len;
                }
                memcpy(data + pos, token, len);
                break;
            }
            case 8: /* Insert part of another input */
                if (corpus->count) {
                    const input* other = &corpus->items[rng() % corpus->count];
                    if (other->size) {
                        size_t from = rng() % other->size;
                        size_t len = 1 + rng() % 64;
                        if (len > other->size - from) {
                            len = other->size - from;
                        }
                        if (len > max_len - size) {
                            len = max_len - size;
                        }
                        memmove(data + pos + len, data + pos, size - pos);
                        memcpy(data + pos, other->data + from, len);
                        size += len;
                    }
                }
                break;
            default: /* Crossover: start of this input, end of another */
                if (corpus->count) {
                    const input* other = &corpus->items[rng() % corpus->count];
                    size_t from = other->size ? rng() % other->size : 0;
                    size_t len = other->size - from;
                    if (len > max_len - pos) {
                        len = max_len - pos;
                    }
                    memcpy(data + pos, other->data + from, len);
                    size = pos + len;
                }
        }
    }
    return size;
}

/** -------------------------------------------------------------------------
        Main
------------------------------------------------------------------------- **/

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static void save(const char* dir, const unsigned char* data, size_t size) {
    char name[1200];
    unsigned long h = 2166136261UL;
    size_t i;
    FILE* f;
    if (!dir) {
        return;
    }
    for (i = 0; i < size; i++) {
        h = ((h ^ data[i]) * 16777619UL) & 0xFFFFFFFFUL;
    }
    snprintf(name, sizeof(name), "%s/%08lx-%lu", dir, h, (unsigned long)size);
    f = fopen(name, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
    }
}

static unsigned run(const unsigned char* data, size_t size, unsigned* edges) {
    current_data = data;
    current_size = size;
    memset(coverage, 0, sizeof(coverage));
    coverage_previous = 0;
    LLVMFuzzerTestOneInput(data, size);
    current_data = NULL;
    runs_done++;
    return coverage_new(edges);
}

int main(int argc, char* argv[]) {
    input_list corpus;
    const char* output_dir = NULL;
    unsigned long runs = (unsigned long)-1, i;
    double max_total_time = 0, start, last_print;
    size_t max_len = 4096;
    unsigned seed = (unsigned)time(NULL), edges = 0;
    int print_final_stats = 0, a;
    unsigned char* data;
    struct itimerval timer;

    memset(&corpus, 0, sizeof(corpus));
    for (a = 1; a < argc; a++) {
        const char* arg = argv[a];
        if (!strncmp(arg, "-runs=", 6)) {
            runs = strtoul(arg + 6, NULL, 10);
        }
        else if (!strncmp(arg, "-max_total_time=", 16)) {
            max_total_time = atof(arg + 16);
        }
        else if (!strncmp(arg, "-max_len=", 9)) {
            max_len = (size_t)strtoul(arg + 9, NULL, 10);
        }
        else if (!strncmp(arg, "-seed=", 6)) {
            seed = (unsigned)strtoul(arg + 6, NULL, 10);
        }
        else if (!strncmp(arg, "-timeout=", 9)) {
            timeout = (unsigned)strtoul(arg + 9, NULL, 10);
        }
        else if (!strncmp(arg, "-dict=", 6)) {
            dict_load(arg + 6);
        }
        else if (!strncmp(arg, "-artifact_prefix=", 17)) {
            strncpy(artifact_prefix, arg + 17, sizeof(artifact_prefix) - 1);
        }
        else if (!strncmp(arg, "-print_final_stats=", 19)) {
            print_final_stats = atoi(arg + 19);
        }
        else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option %s ignored\n", arg);
        }
        else {
            if (!output_dir) {
                struct stat st;
                if (!stat(arg, &st) && S_ISDIR(st.st_mode)) {
                    output_dir = arg;
                    if (strstr(arg, "RegressionTestingFiles")) {
                        fprintf(stderr, "%s would receive new inputs, put a corpus directory first\n", arg);
                        return 1;
                    }
                }
            }
            path_load(&corpus, arg, max_len);
        }
    }
    if (!max_len) {
        max_len = 4096;
    }
    rng_state ^= (uint64_t)seed * 0x9E3779B97F4A7C15ULL;
    if (!rng_state) {
        rng_state = 1;
    }

    if (__sanitizer_set_death_callback) {
        __sanitizer_set_death_callback(on_death); /* Sanitizers handle the crashes */
    }
    else {
        signal(SIGSEGV, on_signal);
        signal(SIGBUS, on_signal);
        signal(SIGILL, on_signal);
        signal(SIGFPE, on_signal);
    }
    signal(SIGABRT, on_signal); /* Failed checks of the fuzz target */
    {
        struct sigaction action;
        memset(&action, 0, sizeof(action));
        action.sa_handler = on_alarm;
        action.sa_flags = SA_RESTART;
        sigemptyset(&action.sa_mask);
        sigaction(SIGALRM, &action, NULL);
    }
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;
    timer.it_value = timer.it_interval;
    setitimer(ITIMER_REAL, &timer, NULL);

    /* Initial corpus */
    {
        input_list initial = corpus;
        memset(&corpus, 0, sizeof(corpus));
        if (!initial.count) {
            list_add(&initial, (const unsigned char*)"<a/>", 4);
        }
        for (i = 0; i < initial.count; i++) {
            if (run(initial.items[i].data, initial.items[i].size, &edges) || !corpus.count) {
                list_add(&corpus, initial.items[i].data, initial.items[i].size);
            }
            free(initial.items[i].data);
        }
        free(initial.items);
    }
    fprintf(stderr, "#%lu INITED cov: %u corp: %lu seed: %u\n", runs_done, edges, (unsigned long)corpus.count, seed);

    /* Fuzzing loop */
    data = (unsigned char*)malloc(max_len);
    if (!data) {
        abort();
    }
    start = last_print = now();
    for (i = 0; i < runs; i++) {
        const input* base = &corpus.items[rng() % corpus.count];
        size_t size = base->size < max_len ? base->size : max_len;
        unsigned char* exact;
        double t;
        memcpy(data, base->data, size);
        size = mutate(data, size, max_len, &corpus);
        exact = (unsigned char*)malloc(size ? size : 1); /* Exact size, for overflow detection */
        if (!exact) {
            abort();
        }
        memcpy(exact, data, size);
        if (run(exact, size, &edges)) {
            list_add(&corpus, exact, size);
            save(output_dir, exact, size);
        }
        free(exact);
        if (!(i & 255)) {
            t = now();
            if (t - last_print >= 10) {
                fprintf(stderr, "#%lu cov: %u corp: %lu exec/s: %.0f\n", runs_done, edges, (unsigned long)corpus.count, (double)runs_done / (t - start));
                last_print = t;
            }
            if (max_total_time && t - start >= max_total_time) {
                break;
            }
        }
    }
    if (print_final_stats) {
        double t = now();
        fprintf(stderr, "stat::number_of_executed_units: %lu\nstat::average_exec_per_sec: %.0f\nstat::new_units_added: %lu\nstat::edges: %u\n",
                runs_done, (double)runs_done / (t - start > 0 ? t - start : 1), (unsigned long)corpus.count, edges);
    }
    fprintf(stderr, "Done %lu runs, cov: %u corp: %lu\n", runs_done, edges, (unsigned long)corpus.count);
    free(data);
    for (i = 0; i < corpus.count; i++) {
        free(corpus.items[i].data);
    }
    free(corpus.items);
    for (i = 0; i < dict.count; i++) {
        free(dict.tokens[i]);
    }
    free(dict.tokens);
    free(dict.sizes);
    return 0;
}

#endif /* TFSXML_FUZZ_COVERAGE */
