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
 * Reference implementation for differential fuzzing
 *
 * Build the fuzzer with TFSXML_FUZZ_REFERENCE defined as the path of another
 * tfsxml.c (e.g. from the previous release, with its tfsxml.h in the same
 * directory) and this file, for comparing the traces of both implementations.
 *     -DTFSXML_FUZZ_REFERENCE=\"/path/to/reference/tfsxml.c\"
 */

#ifdef TFSXML_FUZZ_REFERENCE

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif

/* Reference API renamed */
#define tfsxml_init ref_tfsxml_init
#define tfsxml_next ref_tfsxml_next
#define tfsxml_attr ref_tfsxml_attr
#define tfsxml_value ref_tfsxml_value
#define tfsxml_enter ref_tfsxml_enter
#define tfsxml_leave ref_tfsxml_leave
#define tfsxml_decode ref_tfsxml_decode
#define tfsxml_strcmp_charp ref_tfsxml_strcmp_charp
#define tfsxml_strncmp_charp ref_tfsxml_strncmp_charp
#define tfsxml_strstr_charp ref_tfsxml_strstr_charp

/* Walker renamed */
#define REGRESSIONTEST_PREFIX ref_

#include TFSXML_FUZZ_REFERENCE
#include "RegressionTest/RegressionTest_Trace.c"

#else

/* Nothing to build, avoid an empty translation unit */
typedef int tfsxml_fuzz_reference_unused;

#endif
