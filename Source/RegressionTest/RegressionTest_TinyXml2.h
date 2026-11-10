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
 * Dump of a content parsed with the tinyxml2 API: errors, nodes, values, line
 * numbers, results of the API functions and printed content, for several
 * parsing options (see modes)
 *
 * Used for comparing the tinyxml2 compatibility layer with tinyxml2, or with
 * a previous dump.
 * TFSXML_TINYXML2_REFERENCE: tinyxml2 (tinyxml2.h from the tinyxml2 project) is available,
 * the compatibility layer must then use another namespace (TFSXML_TINYXML2_NAMESPACE).
 */

#ifndef REGRESSIONTEST_TINYXML2_H
#define REGRESSIONTEST_TINYXML2_H

#ifdef TFSXML_TINYXML2_REFERENCE
    #include "tinyxml2.h"
    #ifndef TFSXML_TINYXML2_NAMESPACE
        #define TFSXML_TINYXML2_NAMESPACE tfsxml_tinyxml2
    #endif
#endif
#include "tfsxml/tfsxml_tinyxml2.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <type_traits>
#include <stdint.h>

namespace regressiontest_tinyxml2
{

typedef std::map<const void*, int> id_map;

inline void put(std::string& out, const char* s)
{
    if (!s) {
        out += "(null)";
        return;
    }
    out += '"';
    for (; *s; s++) {
        const unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == '"') {
            out += '\\';
            out += (char)c;
        }
        else if (c < 0x20 || c >= 0x7F) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        }
        else {
            out += (char)c;
        }
    }
    out += '"';
}

inline void put(std::string& out, const char* format, double value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), format, value);
    out += buf;
}

inline void put(std::string& out, long long value)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", value);
    out += buf;
}

inline int id(const id_map& ids, const void* p)
{
    if (!p) {
        return -1;
    }
    id_map::const_iterator it = ids.find(p);
    return it == ids.end() ? -2 : it->second;
}

template<class Node>
inline void collect(const Node* node, id_map& ids)
{
    const int i = (int)ids.size();
    ids[node] = i;
    for (const Node* child = node->FirstChild(); child; child = child->NextSibling()) {
        collect(child, ids);
    }
}

template<class Element>
inline void dump_element(std::string& out, const Element* e, const id_map& ids)
{
    out += " closing=";
    put(out, (long long)e->ClosingType());
    out += " text=";
    put(out, e->GetText());

    /* Text conversions */
    int i = -7;
    unsigned u = 7;
    int64_t i64 = -7;
    bool b = true;
    double d = -7.5;
    float f = 7.5f;
    out += " int=";
    put(out, (long long)e->QueryIntText(&i));
    out += ',';
    put(out, (long long)i);
    out += " unsigned=";
    put(out, (long long)e->QueryUnsignedText(&u));
    out += ',';
    put(out, (long long)u);
    out += " int64=";
    put(out, (long long)e->QueryInt64Text(&i64));
    out += ',';
    put(out, (long long)i64);
    out += " bool=";
    put(out, (long long)e->QueryBoolText(&b));
    out += ',';
    put(out, (long long)b);
    out += " double=";
    put(out, (long long)e->QueryDoubleText(&d));
    put(out, ",%.17g", d);
    out += " float=";
    put(out, (long long)e->QueryFloatText(&f));
    put(out, ",%.9g", f);

    /* Navigation */
    const Element* first = e->FirstChildElement();
    const Element* last = e->LastChildElement();
    out += " nav=";
    put(out, (long long)id(ids, first));
    out += ',';
    put(out, (long long)id(ids, last));
    out += ',';
    put(out, (long long)id(ids, e->NextSiblingElement()));
    out += ',';
    put(out, (long long)id(ids, e->PreviousSiblingElement()));
    out += ',';
    put(out, (long long)id(ids, e->NextSiblingElement(e->Name())));
    out += ',';
    put(out, (long long)id(ids, e->PreviousSiblingElement(e->Name())));
    if (last) {
        out += ',';
        put(out, (long long)id(ids, e->FirstChildElement(last->Name())));
        out += ',';
        put(out, (long long)id(ids, e->LastChildElement(last->Name())));
    }
    out += ',';
    put(out, (long long)id(ids, e->FirstChildElement("")));
    out += ',';
    put(out, (long long)id(ids, e->FirstChildElement("a")));
    out += '\n';

    /* Attributes */
    int index = 0;
    for (const auto* a = e->FirstAttribute(); a; a = a->Next(), index++) {
        out += "  @";
        put(out, a->Name());
        out += '=';
        put(out, a->Value());
        out += " line=";
        put(out, (long long)a->GetLineNum());
        out += " values=";
        put(out, (long long)a->IntValue());
        out += ',';
        put(out, (long long)a->UnsignedValue());
        out += ',';
        put(out, (long long)a->Int64Value());
        out += ',';
        put(out, (long long)a->BoolValue());
        put(out, ",%.17g", a->DoubleValue());
        put(out, ",%.9g", a->FloatValue());

        /* Search by name */
        int found = 0;
        for (const auto* f2 = e->FirstAttribute(); f2 && f2 != e->FindAttribute(a->Name()); f2 = f2->Next()) {
            found++;
        }
        out += " find=";
        put(out, (long long)found);
        out += " attr=";
        put(out, (long long)(e->Attribute(a->Name()) == a->Value()));
        put(out, (long long)(e->Attribute(a->Name(), a->Value()) == a->Value()));
        put(out, (long long)(e->Attribute(a->Name(), "1") != 0));
        int qi = 3;
        const char* qs = 0;
        out += " query=";
        put(out, (long long)e->QueryIntAttribute(a->Name(), &qi));
        out += ',';
        put(out, (long long)qi);
        out += ',';
        put(out, (long long)e->QueryStringAttribute(a->Name(), &qs));
        put(out, (long long)(qs == a->Value()));
        out += ',';
        put(out, (long long)e->IntAttribute(a->Name(), 5));
        out += ',';
        put(out, (long long)e->BoolAttribute(a->Name(), true));
        out += '\n';
    }
    int qi = 3;
    out += "  missing=";
    put(out, (long long)e->QueryIntAttribute("missing", &qi));
    put(out, (long long)qi);
    put(out, (long long)(e->Attribute("missing") != 0));
    put(out, (long long)(e->FindAttribute("missing") != 0));
    out += '\n';
}

template<class Node>
inline void dump_node(std::string& out, const Node* node, const id_map& ids, int level)
{
    out.append((size_t)level * 2, ' ');
    if (node->ToDocument()) {
        out += "document";
    }
    else if (node->ToElement()) {
        out += "element";
    }
    else if (node->ToText()) {
        out += node->ToText()->CData() ? "cdata" : "text";
    }
    else if (node->ToComment()) {
        out += "comment";
    }
    else if (node->ToDeclaration()) {
        out += "declaration";
    }
    else if (node->ToUnknown()) {
        out += "unknown";
    }
    else {
        out += "?";
    }
    out += ' ';
    put(out, node->Value());
    out += " line=";
    put(out, (long long)node->GetLineNum());
    out += " ids=";
    put(out, (long long)id(ids, node->Parent()));
    out += ',';
    put(out, (long long)id(ids, node->FirstChild()));
    out += ',';
    put(out, (long long)id(ids, node->LastChild()));
    out += ',';
    put(out, (long long)id(ids, node->PreviousSibling()));
    out += ',';
    put(out, (long long)id(ids, node->NextSibling()));
    out += ',';
    put(out, (long long)node->NoChildren());
    put(out, (long long)(node->GetDocument() == node->GetDocument()->ToDocument()));
    if (node->ToElement()) {
        dump_element(out, node->ToElement(), ids);
    }
    else {
        out += '\n';
    }
    for (const Node* child = node->FirstChild(); child; child = child->NextSibling()) {
        dump_node(out, child, ids, level + 1);
    }
}

template<class Printer, class Node>
inline void dump_print(std::string& out, const char* name, const Node* node, bool compact, int depth)
{
    Printer printer(0, compact, depth);
    node->Accept(&printer);
    out += name;
    out += ' ';
    put(out, (long long)printer.CStrSize());
    out += ' ';
    const char* s = printer.CStr();
    const size_t len = strlen(s);
    if (len <= 1024) {
        put(out, s);
    }
    else {
        /* Long content: hash */
        unsigned long long hash = 14695981039346656037ULL;
        for (size_t i = 0; i < len; i++) {
            hash = (hash ^ (unsigned char)s[i]) * 1099511628211ULL;
        }
        char buf[32];
        snprintf(buf, sizeof(buf), "fnv1a:%016llx", hash);
        out += buf;
    }
    out += '\n';
}

/* load: 0 Parse(data, size), 1 Parse(data) (ended by NUL), 2 LoadFile(FILE*) */
template<class Document, class Printer, class Whitespace>
inline std::string dump(const uint8_t* data, size_t size, int load, bool processEntities, Whitespace whitespace)
{
    std::string out;
    Document doc(processEntities, whitespace);
    int result = 0;
    switch (load) {
        case 0:
            result = doc.Parse((const char*)data, size);
            break;
        case 1: {
            const std::string s((const char*)data, size);
            result = doc.Parse(s.c_str());
            break;
        }
        default: {
            FILE* f = tmpfile();
            if (!f) {
                return "tmpfile() failed";
            }
            if (size) {
                fwrite(data, 1, size, f);
            }
            result = doc.LoadFile(f);
            fclose(f);
        }
    }
    out += "result=";
    put(out, (long long)result);
    out += " error=";
    put(out, (long long)doc.ErrorID());
    out += ',';
    put(out, (long long)doc.Error());
    out += ',';
    put(out, doc.ErrorName());
    out += ',';
    put(out, (long long)doc.ErrorLineNum());
    out += " bom=";
    put(out, (long long)doc.HasBOM());
    out += "\nstr=";
    put(out, doc.ErrorStr());
    out += '\n';

    typedef typename std::remove_pointer<decltype(doc.FirstChild())>::type node_type;
    const node_type* root = &doc;
    id_map ids;
    collect(root, ids);
    dump_node(out, root, ids, 0);
    out += "root=";
    put(out, (long long)id(ids, doc.RootElement()));
    out += '\n';

    /* Printing */
    dump_print<Printer>(out, "print", root, false, 0);
    dump_print<Printer>(out, "compact", root, true, 0);
    if (doc.RootElement()) {
        dump_print<Printer>(out, "root", static_cast<const node_type*>(doc.RootElement()), false, 0);
        dump_print<Printer>(out, "root_depth", static_cast<const node_type*>(doc.RootElement()), false, 2);
        if (doc.RootElement()->FirstChildElement()) {
            dump_print<Printer>(out, "child", static_cast<const node_type*>(doc.RootElement()->FirstChildElement()), true, 0);
        }
    }

    /* Reuse of the document */
    doc.Clear();
    out += "clear=";
    put(out, (long long)doc.ErrorID());
    out += ',';
    put(out, doc.ErrorStr());
    out += ',';
    put(out, (long long)(doc.FirstChild() == 0));
    out += '\n';
    return out;
}

struct mode {
    int         load;
    bool        processEntities;
    bool        collapse;
};
static const mode modes[] = {
    { 0, true,  false },
    { 1, true,  false },
    { 2, true,  false },
    { 0, false, false },
    { 0, true,  true  },
};

inline std::string dump_compat(const uint8_t* data, size_t size, const mode& m)
{
    return dump<TFSXML_TINYXML2_NAMESPACE::XMLDocument, TFSXML_TINYXML2_NAMESPACE::XMLPrinter>(data, size, m.load, m.processEntities,
        m.collapse ? TFSXML_TINYXML2_NAMESPACE::COLLAPSE_WHITESPACE : TFSXML_TINYXML2_NAMESPACE::PRESERVE_WHITESPACE);
}

#ifdef TFSXML_TINYXML2_REFERENCE
inline std::string dump_reference(const uint8_t* data, size_t size, const mode& m)
{
    return dump<tinyxml2::XMLDocument, tinyxml2::XMLPrinter>(data, size, m.load, m.processEntities,
        m.collapse ? tinyxml2::COLLAPSE_WHITESPACE : tinyxml2::PRESERVE_WHITESPACE);
}
#endif

/* Readable version of a content */
inline std::string escape(const uint8_t* data, size_t size)
{
    std::string out;
    for (size_t i = 0; i < size; i++) {
        char buf[8];
        if (data[i] < 0x20 || data[i] >= 0x7F || data[i] == '\\') {
            snprintf(buf, sizeof(buf), "\\x%02X", data[i]);
        }
        else {
            buf[0] = (char)data[i];
            buf[1] = '\0';
        }
        out += buf;
    }
    return out;
}

} /* namespace */

#endif
