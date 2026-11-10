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
 * Fuzzing of the tinyxml2 compatibility layer
 *
 * TFSXML_TINYXML2_REFERENCE: differential fuzzing, the same content is parsed
 * by tinyxml2 (tinyxml2.cpp and tinyxml2.h from the tinyxml2 project) and by
 * tfsxml_tinyxml2.cpp built with TFSXML_TINYXML2_NAMESPACE=tfsxml_tinyxml2,
 * with several parsing options. Errors, nodes, values, line numbers, results
 * of the API functions and printed content must be the same.
 * Without it, the compatibility layer is only checked by the sanitizers.
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "RegressionTest/RegressionTest_TinyXml2.h"

using namespace regressiontest_tinyxml2;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        const std::string compat = dump_compat(data, size, modes[i]);
#ifdef TFSXML_TINYXML2_REFERENCE
        const std::string reference = dump_reference(data, size, modes[i]);
        if (compat != reference) {
            size_t pos = 0;
            while (pos < compat.size() && pos < reference.size() && compat[pos] == reference[pos]) {
                pos++;
            }
            fprintf(stderr, "tinyxml2 compatibility issue (mode %u, %u bytes, first difference at %u)\ninput: %s\n#### tinyxml2\n%s#### tfsxml_tinyxml2\n%s",
                    (unsigned)i, (unsigned)size, (unsigned)pos, escape(data, size).c_str(), reference.c_str(), compat.c_str());
            abort();
        }
#endif
    }
    return 0;
}
