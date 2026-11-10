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
 * Unit tests of the functions not covered by the traces of XML files
 */

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS
#endif
#include "RegressionTest.h"
#include "tfsxml/tfsxml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(COND) check_result((COND) ? 1 : 0, #COND, __LINE__)

static void check_result(int ok, const char* text, int line) {
    if (!ok) {
        printf("FAILED: %s (RegressionTest_Api.c:%d)\n", text, line);
        fflush(stdout);
        failures++;
    }
}

static tfsxml_string make(const char* s) {
    tfsxml_string r;
    r.buf = s;
    r.len = (unsigned)strlen(s);
    r.flags = 0;
    return r;
}

static int sign(int value) {
    return value < 0 ? -1 : (value > 0 ? 1 : 0);
}

/** -------------------------------------------------------------------------
        tfsxml_init
------------------------------------------------------------------------- **/

static int init_offset(const char* buf, unsigned len) {
    tfsxml_string p;
    memset(&p, 0, sizeof(p));
    if (tfsxml_init(&p, buf, len, 0)) {
        return -1;
    }
    if (p.buf + p.len != buf + len) {
        return -2; /* The parser must see the whole buffer */
    }
    return (int)(p.buf - buf);
}

static void test_init(void) {
    tfsxml_string p;
    CHECK(tfsxml_init(&p, "<a/>", 4, 1) == -1);
    CHECK(init_offset("", 0) == -1);
    CHECK(init_offset("a", 1) == -1);
    CHECK(init_offset(" a<b/>", 6) == -1);
    CHECK(init_offset("<", 1) == 0);
    CHECK(init_offset("<a/>", 4) == 0);
    CHECK(init_offset(" \t\r\n<a/>", 8) == 4);
    CHECK(init_offset("\xEF\xBB\xBF<a/>", 7) == 3);
    CHECK(init_offset("\xEF\xBB\xBF \n<a/>", 9) == 5);
    CHECK(init_offset("\xEF\xBB\xBF", 3) == -1);
    CHECK(init_offset("\xEF\xBB", 2) == -1);
    CHECK(init_offset("\xEF\xBB\xBF\xEF\xBB\xBF<a/>", 10) == -1);
    CHECK(init_offset("\xFF\xFE<\0a\0/\0>\0", 10) == -1);
    CHECK(init_offset("\xFE\xFF\0<\0a\0/\0>", 10) == -1);
}

/** -------------------------------------------------------------------------
        Helpers
------------------------------------------------------------------------- **/

static void test_strcmp(void) {
    CHECK(tfsxml_strcmp_charp(make("abc"), "abc") == 0);
    CHECK(tfsxml_strcmp_charp(make(""), "") == 0);
    CHECK(sign(tfsxml_strcmp_charp(make("abc"), "abd")) == -1);
    CHECK(sign(tfsxml_strcmp_charp(make("abd"), "abc")) == 1);
    CHECK(sign(tfsxml_strcmp_charp(make("ab"), "abc")) == -1);
    CHECK(sign(tfsxml_strcmp_charp(make("abc"), "ab")) == 1);
    CHECK(sign(tfsxml_strcmp_charp(make(""), "a")) == -1);
    CHECK(sign(tfsxml_strcmp_charp(make("a"), "")) == 1);
    CHECK(sign(tfsxml_strcmp_charp(make("\xC3\xA9"), "e")) == 1); /* Same as strcmp, bytes are unsigned */
    CHECK(sign(tfsxml_strcmp_charp(make("e"), "\xC3\xA9")) == -1);
    CHECK(sign(tfsxml_strcmp_charp(make("ab\xC3\xA9"), "ab")) == 1);
    {
        /* Not NUL terminated */
        tfsxml_string a;
        a.buf = "abcdef";
        a.len = 3;
        a.flags = 0;
        CHECK(tfsxml_strcmp_charp(a, "abc") == 0);
        CHECK(sign(tfsxml_strcmp_charp(a, "abcd")) == -1);
        CHECK(sign(tfsxml_strcmp_charp(a, "ab")) == 1);
    }
}

static void test_strncmp(void) {
    CHECK(tfsxml_strncmp_charp(make("abcdef"), "abc", 3) == 0);
    CHECK(tfsxml_strncmp_charp(make("abc"), "abc", 3) == 0);
    CHECK(tfsxml_strncmp_charp(make("abc"), "abc", 10) == 0);
    CHECK(tfsxml_strncmp_charp(make("abc"), "abd", 2) == 0);
    CHECK(tfsxml_strncmp_charp(make("abc"), "abcd", 3) == 0); /* Same as strncmp */
    CHECK(tfsxml_strncmp_charp(make("abc"), "xyz", 0) == 0);
    CHECK(tfsxml_strncmp_charp(make(""), "", 5) == 0);
    CHECK(sign(tfsxml_strncmp_charp(make("abcdef"), "abd", 3)) == -1);
    CHECK(sign(tfsxml_strncmp_charp(make("ab"), "abc", 3)) == -1);
    CHECK(sign(tfsxml_strncmp_charp(make("abc"), "ab", 3)) == 1);
    CHECK(tfsxml_strncmp_charp(make("dc:title"), "dc:", 3) == 0);
    CHECK(tfsxml_strncmp_charp(make("ITU-R_BS.2076-2"), "ITU-R_BS.2076-", 14) == 0);
}

static int strstr_offset(const char* a, const char* b) {
    tfsxml_string s = make(a);
    tfsxml_string r = tfsxml_strstr_charp(s, b);
    if (!r.buf) {
        return r.len ? -2 : -1;
    }
    if (r.buf < s.buf || r.buf + r.len != s.buf + s.len) {
        return -3; /* Must be the end of a */
    }
    return (int)(r.buf - s.buf);
}

static void test_strstr(void) {
    CHECK(strstr_offset("hello world", "world") == 6);
    CHECK(strstr_offset("hello", "ll") == 2);
    CHECK(strstr_offset("hello", "h") == 0);
    CHECK(strstr_offset("hello", "o") == 4);
    CHECK(strstr_offset("hello", "hello") == 0);
    CHECK(strstr_offset("hello", "xyz") == -1);
    CHECK(strstr_offset("hello", "hellos") == -1);
    CHECK(strstr_offset("hello", "lo!") == -1);
    CHECK(strstr_offset("aab", "ab") == 1);
    CHECK(strstr_offset("ababc", "abc") == 2);
    CHECK(strstr_offset("", "a") == -1);
    CHECK(strstr_offset("abc", "") == 0);
    CHECK(strstr_offset("urn:ebu:metadata-schema:ebucore", "ebucore") == 24);
    {
        /* Not NUL terminated */
        tfsxml_string a, r;
        a.buf = "abcdef";
        a.len = 4;
        a.flags = 0;
        r = tfsxml_strstr_charp(a, "ef");
        CHECK(!r.buf && !r.len);
        r = tfsxml_strstr_charp(a, "de");
        CHECK(!r.buf && !r.len);
        r = tfsxml_strstr_charp(a, "cd");
        CHECK(r.buf == a.buf + 2 && r.len == 2);
    }
}

/** -------------------------------------------------------------------------
        Namespaces in names
------------------------------------------------------------------------- **/

/* Position and length of the local name and of the prefix of a, compared to the expected ones (-1 for a NULL prefix) */
static int name_parts(const char* a, int local_pos, int local_len, int prefix_len) {
    tfsxml_string s = make(a);
    tfsxml_string l = tfsxml_local_name(s);
    tfsxml_string p = tfsxml_prefix(s);
    if (l.buf != s.buf + local_pos || l.len != (unsigned)local_len || l.flags != s.flags) {
        return 0;
    }
    if (prefix_len < 0) {
        return !p.buf && !p.len;
    }
    return p.buf == s.buf && p.len == (unsigned)prefix_len && p.flags == s.flags;
}

static void test_names(void) {
    CHECK(name_parts("dc:title", 3, 5, 2));
    CHECK(name_parts("title", 0, 5, -1));
    CHECK(name_parts(":title", 1, 5, 0));   /* Empty prefix */
    CHECK(name_parts("dc:", 3, 0, 2));      /* Empty local name */
    CHECK(name_parts("a:b:c", 4, 1, 3));    /* Last ":" */
    CHECK(name_parts(":", 1, 0, 0));
    CHECK(name_parts("", 0, 0, -1));

    CHECK(tfsxml_strcmp_local_charp(make("dc:title"), "title") == 0);
    CHECK(tfsxml_strcmp_local_charp(make("title"), "title") == 0);
    CHECK(tfsxml_strcmp_local_charp(make("ebucore:title"), "title") == 0);
    CHECK(tfsxml_strcmp_local_charp(make("dc:"), "") == 0);
    CHECK(sign(tfsxml_strcmp_local_charp(make("dc:titles"), "title")) == 1);
    CHECK(sign(tfsxml_strcmp_local_charp(make("dc:tit"), "title")) == -1);
    CHECK(sign(tfsxml_strcmp_local_charp(make("dc:title"), "dc:title")) == 1); /* "title" compared */

    CHECK(tfsxml_strcmp_prefix_charp(make("dc:title"), "dc") == 0);
    CHECK(tfsxml_strcmp_prefix_charp(make("xmlns:dc"), "xmlns") == 0);
    CHECK(tfsxml_strcmp_prefix_charp(make("title"), "") == 0);   /* No prefix */
    CHECK(tfsxml_strcmp_prefix_charp(make(":title"), "") == 0);  /* Empty prefix */
    CHECK(sign(tfsxml_strcmp_prefix_charp(make("title"), "dc")) == -1);
    CHECK(sign(tfsxml_strcmp_prefix_charp(make("dc:title"), "")) == 1);
    CHECK(sign(tfsxml_strcmp_prefix_charp(make("dcx:title"), "dc")) == 1);
    CHECK(sign(tfsxml_strcmp_prefix_charp(make("d:title"), "dc")) == -1);
    CHECK(sign(tfsxml_strcmp_prefix_charp(make("dc:title"), "dc:")) == -1);

    {
        /* Not NUL terminated */
        tfsxml_string a;
        a.buf = "dc:title";
        a.len = 4;
        a.flags = 0;
        CHECK(tfsxml_strcmp_local_charp(a, "t") == 0);
        CHECK(tfsxml_strcmp_prefix_charp(a, "dc") == 0);
        a.len = 2; /* ":" after the end */
        CHECK(tfsxml_strcmp_local_charp(a, "dc") == 0);
        CHECK(tfsxml_strcmp_prefix_charp(a, "") == 0);
        CHECK(!tfsxml_prefix(a).buf);
    }
    {
        /* NULL string (e.g. not found by tfsxml_strstr_charp) */
        tfsxml_string a, r;
        a.buf = NULL;
        a.len = 0;
        a.flags = 0;
        r = tfsxml_local_name(a);
        CHECK(!r.buf && !r.len);
        r = tfsxml_prefix(a);
        CHECK(!r.buf && !r.len);
        CHECK(tfsxml_strcmp_local_charp(a, "") == 0);
        CHECK(tfsxml_strcmp_prefix_charp(a, "") == 0);
        CHECK(sign(tfsxml_strcmp_local_charp(a, "a")) == -1);
    }
    {
        /* Names from the parser */
        static const char xml[] = "<dc:title xml:lang=\"en\" id=\"1\">T</dc:title>";
        tfsxml_string p, n, v;
        CHECK(!tfsxml_init(&p, xml, (unsigned)(sizeof(xml) - 1), 0));
        CHECK(!tfsxml_next(&p, &n));
        CHECK(!tfsxml_strcmp_local_charp(n, "title") && !tfsxml_strcmp_prefix_charp(n, "dc"));
        CHECK(!tfsxml_attr(&p, &n, &v));
        CHECK(!tfsxml_strcmp_local_charp(n, "lang") && !tfsxml_strcmp_prefix_charp(n, "xml"));
        CHECK(!tfsxml_attr(&p, &n, &v));
        CHECK(!tfsxml_strcmp_local_charp(n, "id") && !tfsxml_strcmp_prefix_charp(n, ""));
        CHECK(tfsxml_attr(&p, &n, &v) == -1);
        CHECK(!tfsxml_enter(&p));
        CHECK(tfsxml_next(&p, &n) == -1);
        CHECK(!tfsxml_strcmp_local_charp(n, "title") && !tfsxml_strcmp_prefix_charp(n, "dc")); /* End tag */
    }
}

/** -------------------------------------------------------------------------
        tfsxml_decode
------------------------------------------------------------------------- **/

static void decode_append(void* s, const char* buf, unsigned len) {
    trace_append((trace*)s, buf, len);
}

static void check_decode(const char* raw, size_t raw_len, unsigned flags, const char* expected, size_t expected_len, int line) {
    tfsxml_string v;
    trace out;
    v.buf = raw;
    v.len = (unsigned)raw_len;
    v.flags = flags;
    trace_init(&out);
    tfsxml_decode(&out, &v, decode_append);
    if (out.len != expected_len || (expected_len && memcmp(out.buf, expected, expected_len))) {
        printf("FAILED: decode of \"%s\" gives \"%s\" (RegressionTest_Api.c:%d)\n", raw, out.buf ? out.buf : "", line);
        fflush(stdout);
        failures++;
    }
    trace_free(&out);
}

#define DECODE(RAW, EXPECTED) check_decode(RAW, sizeof(RAW) - 1, 1, EXPECTED, sizeof(EXPECTED) - 1, __LINE__)
#define DECODE_RAW(RAW) check_decode(RAW, sizeof(RAW) - 1, 1, RAW, sizeof(RAW) - 1, __LINE__)

static void test_decode(void) {
    /* Not flagged as encoded */
    check_decode("a&amp;b", 7, 0, "a&amp;b", 7, __LINE__);
    check_decode("", 0, 0, "", 0, __LINE__);
    check_decode("", 0, 1, "", 0, __LINE__);

    /* Predefined entities */
    DECODE("&amp;", "&");
    DECODE("&lt;&gt;&quot;&apos;", "<>\"'");
    DECODE("a&amp;b&lt;c", "a&b<c");
    DECODE("&amp;amp;", "&amp;");
    DECODE("&amp;&amp;", "&&");
    DECODE_RAW("&unknown;");
    DECODE_RAW("&amp");
    DECODE_RAW("&");
    DECODE_RAW("&;");
    DECODE_RAW("&am;");
    DECODE_RAW("&ampx;");
    DECODE_RAW("&AMP;");
    DECODE("&&amp;", "&&");
    DECODE("&a&lt;", "&a<");
    DECODE("&lt&lt;", "&lt<");

    /* Character references */
    DECODE("&#65;", "A");
    DECODE("&#x41;", "A");
    DECODE("&#X41;", "A");
    DECODE("&#x6a;&#x6A;", "jj");
    DECODE("&#0065;", "A");
    DECODE("&#x7F;", "\x7F");
    DECODE("&#x80;", "\xC2\x80");
    DECODE("&#xE9;", "\xC3\xA9");
    DECODE("&#233;", "\xC3\xA9");
    DECODE("&#x7FF;", "\xDF\xBF");
    DECODE("&#x800;", "\xE0\xA0\x80");
    DECODE("&#x20AC;", "\xE2\x82\xAC");
    DECODE("&#xFFFD;", "\xEF\xBF\xBD");
    DECODE("&#x10000;", "\xF0\x90\x80\x80");
    DECODE("&#x1F600;", "\xF0\x9F\x98\x80");
    DECODE("&#x10FFFF;", "\xF4\x8F\xBF\xBF");
    DECODE("&#1114111;", "\xF4\x8F\xBF\xBF");
    DECODE("&#x0000000041;", "A");
    DECODE_RAW("&#x110000;");
    DECODE_RAW("&#1114112;");
    DECODE_RAW("&#x7FFFFFFF;");
    DECODE_RAW("&#99999999999999999999;");
    DECODE_RAW("&#xFFFFFFFFFFFFFFFF41;");
    DECODE_RAW("&#xZZ;");
    DECODE_RAW("&#12a;");
    DECODE_RAW("&#-1;");
    DECODE_RAW("&#x 41;");
    DECODE_RAW("&#;");
    DECODE_RAW("&#x;");
    DECODE_RAW("&#0;");
    DECODE_RAW("&#x0;");
    DECODE_RAW("&#xD800;");
    DECODE_RAW("&#xDFFF;");
    DECODE("&#xD7FF;", "\xED\x9F\xBF");
    DECODE("&#xE000;", "\xEE\x80\x80");
    DECODE("x&#65;y&#66;z", "xAyBz");

    /* CDATA */
    DECODE("<![CDATA[a<b]]>", "a<b");
    DECODE("x<![CDATA[]]>y", "xy");
    DECODE("<![CDATA[&amp;]]>", "&amp;");
    DECODE("a<![CDATA[b]]>c<![CDATA[d]]>e", "abcde");
    DECODE("<![CDATA[a]]]>", "a]");
    DECODE("<![CDATA[]]]]>", "]]");
    DECODE("<![CDATA[a]]b]]>", "a]]b");
    DECODE("<![CDATA[a]>b]]>", "a]>b");
    DECODE("&lt;<![CDATA[&lt;]]>&lt;", "<&lt;<");
    DECODE("<![CDATA[<![CDATA[x]]>", "<![CDATA[x");
    DECODE_RAW("<![CDATA[ab");
    DECODE_RAW("<![CDATA[");
    DECODE_RAW("<![CDATA[]");
    DECODE_RAW("<![CDATA[]]");
    DECODE_RAW("<![CDATA[a]]");
    DECODE("&amp;<![CDATA[", "&<![CDATA[");
    DECODE_RAW("<![CDATA");
    DECODE_RAW("<![cdata[a]]>");
    DECODE_RAW("a<b");
    DECODE_RAW("<");

    /* Long content without entity end */
    {
        size_t size = 100000;
        char* raw = (char*)malloc(size);
        if (raw) {
            memset(raw, '&', size);
            check_decode(raw, size, 1, raw, size, __LINE__);
            free(raw);
        }
    }
}

/** -------------------------------------------------------------------------
        Call sequences
------------------------------------------------------------------------- **/

static tfsxml_string seq_p, seq_n, seq_v;

static int start(const char* xml) {
    return tfsxml_init(&seq_p, xml, (unsigned)strlen(xml), 0);
}

static int next_is(const char* name) {
    return !tfsxml_next(&seq_p, &seq_n) && !tfsxml_strcmp_charp(seq_n, name);
}

static int attr_is(const char* name, const char* value) {
    return !tfsxml_attr(&seq_p, &seq_n, &seq_v) && !tfsxml_strcmp_charp(seq_n, name) && !tfsxml_strcmp_charp(seq_v, value);
}

static int value_is(const char* value) {
    return !tfsxml_value(&seq_p, &seq_v) && !tfsxml_strcmp_charp(seq_v, value);
}

static int text_is(const char* value) {
    return !tfsxml_text(&seq_p, &seq_v) && !tfsxml_strcmp_charp(seq_v, value);
}

static int attr_end(void) {
    return tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1;
}

static void test_sequences(void) {
    /* Leave the entered element, its content not yet parsed is skipped */
    CHECK(!start("<r><a x='1'><b/></a><c/></r><z/>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(next_is("a"));
    CHECK(!tfsxml_leave(&seq_p)); /* Attributes of a not read */
    CHECK(next_is("z"));

    CHECK(!start("<r><a x='1'><b/></a><c/></r><z/>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(next_is("a"));
    CHECK(attr_is("x", "1"));
    CHECK(tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1);
    CHECK(!tfsxml_leave(&seq_p)); /* All attributes of a read */
    CHECK(next_is("z"));

    CHECK(!start("<r><a x='1'><b/></a><c/></r><z/>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(next_is("a"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(!tfsxml_leave(&seq_p)); /* Leave a */
    CHECK(next_is("c"));
    CHECK(tfsxml_next(&seq_p, &seq_n) == -1);
    CHECK(next_is("z"));

    CHECK(!start("<r><a/><c/></r><z/>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(next_is("a"));
    CHECK(tfsxml_enter(&seq_p) == -1); /* Empty element */
    CHECK(!tfsxml_leave(&seq_p)); /* Leave r */
    CHECK(next_is("z"));

    /* Value */
    CHECK(!start("<r><a>x&amp;y<b/>z</a><c>1</c></r>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(next_is("a"));
    CHECK(value_is("x&amp;y"));
    CHECK(seq_v.flags & 1);
    CHECK(tfsxml_enter(&seq_p) == -1); /* Content already parsed */
    CHECK(tfsxml_value(&seq_p, &seq_v) == -1);
    CHECK(next_is("c"));
    CHECK(value_is("1"));
    CHECK(!(seq_v.flags & 1));
    CHECK(tfsxml_next(&seq_p, &seq_n) == -1);
    CHECK(tfsxml_next(&seq_p, &seq_n) == 1);

    /* Attributes are available only after next */
    CHECK(!start("<r a='1'><b c='2'/></r>"));
    CHECK(tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1);
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1);
    CHECK(next_is("b"));
    CHECK(attr_is("c", "2"));
    CHECK(tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1);
    CHECK(tfsxml_attr(&seq_p, &seq_n, &seq_v) == -1);

    /* Strings returned by next have no flag */
    CHECK(!start("<r/>"));
    seq_n.flags = 0xFFFF;
    CHECK(next_is("r"));
    CHECK(!seq_n.flags);

    /* Text (mixed content) and end tag names */
    CHECK(!start("<p>One <b>two</b> three<!--c-->four<![CDATA[<5>]]>&amp;six</p >"));
    CHECK(next_is("p"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(text_is("One "));
    CHECK(!(seq_v.flags & 1));
    CHECK(next_is("b"));
    CHECK(text_is(" three")); /* Content of b is skipped */
    CHECK(next_is("!--c--"));
    CHECK(text_is("four<![CDATA[<5>]]>&amp;six"));
    CHECK(seq_v.flags & 1);
    CHECK(tfsxml_text(&seq_p, &seq_v) == -1);
    CHECK(tfsxml_next(&seq_p, &seq_n) == -1 && !tfsxml_strcmp_charp(seq_n, "p"));
    CHECK(tfsxml_text(&seq_p, &seq_v) == 1);
    CHECK(tfsxml_next(&seq_p, &seq_n) == 1);

    CHECK(!start("<r><a>x<c/>y</a>z</r>"));
    CHECK(next_is("r"));
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(tfsxml_text(&seq_p, &seq_v) == -1);
    CHECK(next_is("a"));
    CHECK(attr_end());
    CHECK(!tfsxml_enter(&seq_p));
    CHECK(text_is("x"));
    CHECK(next_is("c"));
    CHECK(text_is("y")); /* After an empty element */
    CHECK(tfsxml_next(&seq_p, &seq_n) == -1 && !tfsxml_strcmp_charp(seq_n, "a"));
    CHECK(text_is("z"));
    CHECK(tfsxml_next(&seq_p, &seq_n) == -1 && !tfsxml_strcmp_charp(seq_n, "r"));

    /* Processing instructions end with "?>" */
    CHECK(!start("<?pi a > b?><r/>"));
    CHECK(next_is("?pi a > b?"));
    CHECK(next_is("r"));
}

/** -------------------------------------------------------------------------
        Main
------------------------------------------------------------------------- **/

int test_api(void) {
    failures = 0;
    test_init();
    test_strcmp();
    test_strncmp();
    test_strstr();
    test_names();
    test_decode();
    test_sequences();
    return failures;
}
