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
 * Regression test of the tinyxml2 compatibility layer
 *
 * Usage: RegressionTest_TinyXml2 [options] files or directories
 * Each file (except .txt files) is parsed with the API of the compatibility
 * layer with several parsing options, the dump of the results (see
 * RegressionTest_TinyXml2.h) is compared with the content of FILE.txt.
 * --update     FILE.txt is written
 * --reference  the dump is compared with the one from tinyxml2 instead of
 *              FILE.txt (build with TFSXML_TINYXML2_REFERENCE)
 * --dump       the dump is printed
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "RegressionTest/RegressionTest_TinyXml2.h"
#include <algorithm>
#include <vector>
#ifdef _WIN32
    #include <windows.h>
#else
    #include <dirent.h>
    #include <sys/stat.h>
#endif

using namespace regressiontest_tinyxml2;

namespace {

bool read_file(const std::string& name, std::string& data)
{
    FILE* f = fopen(name.c_str(), "rb");
    if (!f) {
        return false;
    }
    char buf[65536];
    size_t size;
    data.clear();
    while ((size = fread(buf, 1, sizeof(buf), f))) {
        data.append(buf, size);
    }
    fclose(f);
    return true;
}

bool write_file(const std::string& name, const std::string& data)
{
    FILE* f = fopen(name.c_str(), "wb");
    if (!f) {
        return false;
    }
    const bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
    return fclose(f) == 0 && ok;
}

bool ends_with(const std::string& s, const char* suffix)
{
    const size_t len = strlen(suffix);
    return s.size() >= len && !s.compare(s.size() - len, len, suffix);
}

/* Files of a directory (recursive) or the file itself, sorted */
void list_files(const std::string& path, std::vector<std::string>& files)
{
#ifdef _WIN32
    WIN32_FIND_DATAA data;
    HANDLE h = FindFirstFileA((path + "\\*").c_str(), &data);
    if (h == INVALID_HANDLE_VALUE) {
        files.push_back(path);
        return;
    }
    std::vector<std::string> names;
    do {
        if (strcmp(data.cFileName, ".") && strcmp(data.cFileName, "..")) {
            names.push_back(data.cFileName);
        }
    } while (FindNextFileA(h, &data));
    FindClose(h);
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i < names.size(); i++) {
        list_files(path + "\\" + names[i], files);
    }
#else
    struct stat st;
    if (stat(path.c_str(), &st) || !S_ISDIR(st.st_mode)) {
        files.push_back(path);
        return;
    }
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return;
    }
    std::vector<std::string> names;
    while (struct dirent* entry = readdir(dir)) {
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            names.push_back(entry->d_name);
        }
    }
    closedir(dir);
    std::sort(names.begin(), names.end());
    for (size_t i = 0; i < names.size(); i++) {
        list_files(path + "/" + names[i], files);
    }
#endif
}

/* Dump of all modes (a dump identical to the one of the first mode is not repeated) */
std::string dump_all(const std::string& data, bool reference)
{
    std::string out, first;
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        std::string dump;
#ifdef TFSXML_TINYXML2_REFERENCE
        if (reference) {
            dump = dump_reference((const uint8_t*)data.data(), data.size(), modes[i]);
        }
        else
#else
        (void)reference;
#endif
        {
            dump = dump_compat((const uint8_t*)data.data(), data.size(), modes[i]);
        }
        char buf[64];
        if (i && dump == first) {
            snprintf(buf, sizeof(buf), "#### mode %u: same as mode 0\n", (unsigned)i);
            out += buf;
            continue;
        }
        snprintf(buf, sizeof(buf), "#### mode %u\n", (unsigned)i);
        out += buf;
        out += dump;
        if (!i) {
            first = dump;
        }
    }
    return out;
}

/* First different line */
void print_difference(const std::string& expected, const std::string& result)
{
    size_t pos = 0, line = 1, line_start = 0;
    while (pos < expected.size() && pos < result.size() && expected[pos] == result[pos]) {
        if (expected[pos] == '\n') {
            line++;
            line_start = pos + 1;
        }
        pos++;
    }
    const size_t end1 = expected.find('\n', line_start);
    const size_t end2 = result.find('\n', line_start);
    printf("  line %u\n  expected: %s\n  result:   %s\n", (unsigned)line,
           expected.substr(line_start, end1 == std::string::npos ? std::string::npos : end1 - line_start).c_str(),
           result.substr(line_start, end2 == std::string::npos ? std::string::npos : end2 - line_start).c_str());
}

} /* namespace */

int main(int argc, char* argv[])
{
    bool update = false, reference = false, dump = false;
    std::vector<std::string> files;
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        if (arg == "--update") {
            update = true;
        }
        else if (arg == "--reference") {
#ifndef TFSXML_TINYXML2_REFERENCE
            fprintf(stderr, "--reference: tinyxml2 is not available in this build\n");
            return 1;
#endif
            reference = true;
        }
        else if (arg == "--dump") {
            dump = true;
        }
        else if (!arg.empty() && arg[0] == '-') {
            fprintf(stderr, "Usage: %s [--update] [--reference] [--dump] files or directories\n", argv[0]);
            return 1;
        }
        else {
            list_files(arg, files);
        }
    }

    unsigned checked = 0, failed = 0;
    for (size_t i = 0; i < files.size(); i++) {
        const std::string& name = files[i];
        std::string data, expected;
        if (ends_with(name, ".txt") || ends_with(name, ".md")) {
            continue;
        }
        if (!read_file(name, data)) {
            printf("%s: can not be read\n", name.c_str());
            failed++;
            continue;
        }
        checked++;
        const std::string result = dump_all(data, false);
        if (dump) {
            printf("######## %s\n%s", name.c_str(), result.c_str());
        }
        if (update) {
            if (!write_file(name + ".txt", result)) {
                printf("%s.txt: can not be written\n", name.c_str());
                failed++;
            }
            continue;
        }
        if (reference) {
            expected = dump_all(data, true);
        }
        else if (!read_file(name + ".txt", expected)) {
            printf("%s.txt: missing\n", name.c_str());
            failed++;
            continue;
        }
        if (result != expected) {
            printf("%s: different from %s\n", name.c_str(), reference ? "tinyxml2" : (name + ".txt").c_str());
            print_difference(expected, result);
            failed++;
        }
    }
    printf("Files: %u checked, %u failed%s\n", checked, failed, update ? " (updated)" : reference ? " (compared with tinyxml2)" : "");
    return failed ? 1 : 0;
}
