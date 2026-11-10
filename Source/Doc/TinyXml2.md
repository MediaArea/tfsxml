# tinyxml2 compatibility layer

`Source/tfsxml/tfsxml_tinyxml2.h` and `Source/tfsxml/tfsxml_tinyxml2.cpp`
provide the read part of the [tinyxml2](https://github.com/leethomason/tinyxml2)
6.2.0 API, implemented with tfsxml: same classes and methods in the `tinyxml2`
namespace, same accepted documents, same nodes and values, same errors, same
printed content.

A program reading XML with tinyxml2 (e.g. MediaInfoLib) can use tfsxml
instead, without changing its code and with the same results.

## Usage

- Add `Source/tfsxml/tfsxml.c` and `Source/tfsxml/tfsxml_tinyxml2.cpp` (C++11)
  to the project, or link the `tfsxml_tinyxml2` library (CMake target,
  `libtfsxml_tinyxml2` with GNU autotools, `tfsxml_tinyxml2` Visual Studio
  project) and the `tfsxml` library.
- Include `tfsxml/tfsxml_tinyxml2.h` instead of `tinyxml2.h`.
- `TFSXML_TINYXML2` is defined, `TINYXML2_MAJOR_VERSION` (and minor, patch)
  are the ones of the emulated version (6.2.0) if not already defined.
- `TFSXML_TINYXML2_NAMESPACE` changes the namespace (default `tinyxml2`), for
  using the original tinyxml2 in the same program (this is how the tests
  compare both).

```cpp
#include "tfsxml/tfsxml_tinyxml2.h"
using namespace tinyxml2;

XMLDocument doc;
if (doc.Parse(data, size) != XML_SUCCESS)
    return; /* doc.ErrorID(), doc.ErrorStr() */
for (const XMLElement* e = doc.RootElement()->FirstChildElement("book"); e; e = e->NextSiblingElement("book"))
    printf("%s %s\n", e->Attribute("id"), e->FirstChildElement("title")->GetText());
```

## Provided API

- `XMLDocument`: `Parse()`, `LoadFile()`, `SaveFile()`, `Print()`, `Accept()`,
  `RootElement()`, `Error()`, `ErrorID()`, `ErrorName()`, `ErrorIDToName()`,
  `ErrorStr()`, `PrintError()`, `ErrorLineNum()`, `ClearError()`, `Clear()`,
  `HasBOM()`, `SetBOM()`, `ProcessEntities()`, `WhitespaceMode()`
- `XMLNode`: `Value()`, `GetLineNum()`, `GetDocument()`, `Parent()`,
  `NoChildren()`, `FirstChild()`, `LastChild()`, `PreviousSibling()`,
  `NextSibling()`, `FirstChildElement()`, `LastChildElement()`,
  `PreviousSiblingElement()`, `NextSiblingElement()`, `ToElement()`,
  `ToText()`, `ToComment()`, `ToDocument()`, `ToDeclaration()`, `ToUnknown()`,
  `Accept()`, `SetUserData()`, `GetUserData()`
- `XMLElement`: `Name()`, `Attribute()`, `FindAttribute()`, `FirstAttribute()`,
  `GetText()`, `ClosingType()`, typed attributes (`IntAttribute()`,
  `QueryIntAttribute()`, `QueryAttribute()`, `QueryStringAttribute()`...),
  typed texts (`IntText()`, `QueryIntText()`...)
- `XMLAttribute`: `Name()`, `Value()`, `Next()`, `GetLineNum()`, typed values
  (`IntValue()`, `QueryIntValue()`...)
- `XMLText` (`CData()`, `SetCData()`), `XMLComment`, `XMLDeclaration`, `XMLUnknown`
- `XMLHandle`, `XMLConstHandle`, `XMLVisitor`, `XMLPrinter` (memory or `FILE*`
  output, streaming API), `XMLUtil`, `XMLError`, `Whitespace`

Not provided (a program using them does not compile): creation and
modification of nodes (`NewElement()`, `InsertEndChild()`, `SetAttribute()`,
`SetText()`, `SetValue()`, `DeleteChild()`...), clones and comparisons
(`ShallowClone()`, `DeepClone()`, `ShallowEqual()`, `DeepCopy()`).

## Behavior

The behavior of tinyxml2 6.2.0 is reproduced, including where tinyxml2
accepts or decodes content differently than the XML specification:

- Accepted content and errors: same error IDs, `ErrorStr()` texts and line
  numbers, same line numbers of nodes and attributes
  - whitespaces are space, tab, LF, CR, and also vertical tab and form feed
  - names start with a letter, `_`, `:` or any byte from 0x80, followed by
    these characters, digits, `.` or `-`
  - whitespaces are accepted after `<` (`< a>`, `< /a>`), attributes do not
    need whitespaces between them (`a="1"b="2"`)
  - end tags may have attributes (ignored), `</a/>` is an empty element `a`
  - an end tag at the document level ends the parsing without error (the rest
    of the content is ignored)
  - declarations (`<?...?>`) are accepted only at the start of the document
  - text is accepted at the document level, a text must be followed by `<`,
    a text with only whitespaces is not a node, other texts keep their
    leading and trailing whitespaces
  - content after a NUL byte is ignored, whitespaces then a UTF-8 BOM (in this
    order) are skipped at the start, a BOM followed by whitespaces only is an
    empty document without error
  - elements with content are limited to 98 nesting levels
    (`XML_ELEMENT_DEPTH_EXCEEDED`, `TINYXML2_MAX_ELEMENT_DEPTH`)
- Values, decoded when they are read for the first time
  - line endings: CR LF, LF CR and CR become LF (texts, attribute values,
    CDATA sections, comments, declarations, unknown nodes)
  - entities: `&quot;`, `&amp;`, `&apos;`, `&lt;`, `&gt;`, decimal and
    hexadecimal (with `x`, not `X`) character references, with the quirks of
    tinyxml2: digits are read backward from the first `;` (`&#x41&#x42;` is
    `B`), an unknown entity keeps the byte at the same position of the decoded
    value (`&lt;&foo;` is `<lfoo;`), `&#` at the end of a value is removed,
    references above U+1FFFFF are removed, `&#0;` ends the C string
  - character references are computed with `unsigned long` as tinyxml2 does,
    so decimal references with values above 2^32 give different results on
    Windows (32-bit `unsigned long`) and on other 64-bit platforms
  - `processEntities` false: entities are kept as is; `COLLAPSE_WHITESPACE`:
    whitespaces of texts are collapsed
  - typed conversions use `sscanf()` as tinyxml2 does
- `XMLPrinter` prints the same content (indentation, escaping), including the
  behavior of tinyxml2 6.2.0 when `processEntities` is false (texts and
  attribute values are not printed when the document is printed)
- After an error, `Parse()` removes all nodes and `LoadFile()` keeps the
  complete nodes at the document level, as tinyxml2 does

No difference with tinyxml2 is known.

Other differences (no impact on the results):
- the parsed content is copied (as tinyxml2 does) but not modified, decoded
  values are written in a separate memory; nodes, attributes and decoded
  values are freed with the document
- about 2 times faster than tinyxml2 for parsing, about 1.6 times for parsing
  and reading all names and values (ARM64, GCC 15 -O2, ADM-like, subtitle-like
  and element-heavy content of 5 to 15 MB)
- content bigger than 4 GiB is supported (tfsxml is called with windows of
  data)

As with tinyxml2, a document is not thread safe, even for reading (values are
decoded when they are read for the first time).

## Implementation

tfsxml provides the text (`tfsxml_text()`, CDATA sections are then split in
separate nodes), the markup (`tfsxml_next()`), the end tag names and the
attributes (`tfsxml_attr()`). Each start and end tag is checked with the
tinyxml2 grammar. When tfsxml and tinyxml2 would not split a tag the same way
(malformed tags, e.g. whitespace after `<` or a vertical tab in a tag), the tag
is parsed with the tinyxml2 grammar and tfsxml is restarted after it
(`tfsxml_init()` at the next `<`). When tfsxml needs more data at the end of
the content, the rest is parsed with the tinyxml2 grammar for having the same
errors as tinyxml2.

## Tests

See the [regression test protocol](RegressionTest.md): expected results of the
test files (`TinyXml2` directory of
[tfsxml-RegressionTestingFiles](https://github.com/MediaArea/tfsxml-RegressionTestingFiles)),
comparison with tinyxml2 on all
test files, differential fuzzing with tinyxml2, MediaInfoLib integration.

Results when the layer was written:
- 138 test files and 95 tfsxml test files: same results as tinyxml2 for each
  of the 5 parsing options of the tests (`Parse()` with a size, `Parse()` of a
  string ended by NUL, `LoadFile()`, `processEntities` false,
  `COLLAPSE_WHITESPACE`)
- differential fuzzing (GCC, AddressSanitizer, UndefinedBehaviorSanitizer):
  about 2.5 million inputs (each one with the 5 parsing options), no
  difference with tinyxml2
- MediaInfoLib with tinyxml2 and with the compatibility layer (both with the
  same version of tfsxml): same MediaInfo output on 4790 files (samples of
  the formats read with tinyxml2 with variants of encodings, line endings and
  malformed content, tfsxml test files, fuzzing corpus)

## Use in MediaInfoLib

1. Copy `tfsxml.c`, `tfsxml.h`, `tfsxml_tinyxml2.cpp` and
   `tfsxml_tinyxml2.h` to `Source/ThirdParty/tfsxml/`.
2. Replace `#include "tinyxml2.h"` by
   `#include "ThirdParty/tfsxml/tfsxml_tinyxml2.h"`.
3. Remove `Source/ThirdParty/tinyxml2`, replace `tinyxml2.cpp` by
   `tfsxml_tinyxml2.cpp` in the project files (CMake, GNU autotools, Visual
   Studio, C++Builder, Qt), remove the options for using the tinyxml2 library
   of the system.

The MediaInfoLib code using tinyxml2 (`File__Analyze::FileHeader_Begin_XML()`,
the parsers of DCP, IMF, TTML, DASH, ISM, HDS, P2, XDCAM, Dolby Vision
metadata, property lists, MediaInfo XML, `XMLPrinter` for TTML events,
`XmlUtils`) does not need any change.
