# tfsxml

Tiny Fast Streamable XML parser

- **Tiny**: 1 C file and 1 header, C89, no allocation, no dependency
  (`memchr()` from the C library is used for speed, `TFSXML_NO_LIBC` removes it)
- **Fast**: no copy, no tree, the content is decoded only on request,
  long content is scanned with `memchr()`
- **Streamable**: the content can be provided in several chunks, the parser
  says when it needs more data and resumes where it stopped

Used by [MediaInfoLib](https://github.com/MediaArea/MediaInfoLib) for ADM,
XMP and other XML based metadata.

A [compatibility layer with the tinyxml2 API](Source/Doc/TinyXml2.md)
(C++, read-only DOM, same results as tinyxml2 6.2.0) is also provided.

## Usage

```c
#include "tfsxml/tfsxml.h"

/* <library><book id="1"><title>Notes on Clocks</title></book></library> */
tfsxml_string p, n, v;
if (tfsxml_init(&p, buf, len, 0))
    return; /* Not XML */
while (!tfsxml_next(&p, &n)) {                        /* Elements of the first level */
    if (!tfsxml_strcmp_charp(n, "library") && !tfsxml_enter(&p)) {
        while (!tfsxml_next(&p, &n)) {                /* Elements of the second level */
            if (tfsxml_strcmp_charp(n, "book"))
                continue;                             /* Content is skipped */
            while (!tfsxml_attr(&p, &n, &v)) {}       /* Attributes */
            if (!tfsxml_enter(&p)) {
                while (!tfsxml_next(&p, &n)) {        /* Elements of the third level */
                    if (!tfsxml_strcmp_charp(n, "title") && !tfsxml_value(&p, &v))
                        tfsxml_decode(out, &v, append); /* Entities and CDATA decoded */
                }
            }
        }
    }
}
```

Return values of the functions: `0` = OK, `-1` = nothing (more) at this level,
`1` = need of more data.

`tfsxml_next()` also returns processing instructions (`?target data?`), comments
(`!-- text --`) and other declarations (`!DOCTYPE ...`), and the name in the
end tag when it returns `-1` at the end of an element. `tfsxml_value()`
returns the content until the first sub-element, comment or processing
instruction, and skips the rest of the element. `tfsxml_text()` returns the
text before the next element or markup (mixed content, e.g. `<p>a <b>b</b> c</p>`).

Strings are compared with `tfsxml_strcmp_charp()`, `tfsxml_strncmp_charp()` and
`tfsxml_strstr_charp()`. For names with a namespace prefix,
`tfsxml_strcmp_local_charp()` compares the local name (`dc:title` and `title`
match `"title"`), `tfsxml_strcmp_prefix_charp()` compares the prefix
(`dc:title` matches `"dc"`, `title` matches `""`), `tfsxml_local_name()` and
`tfsxml_prefix()` return these parts.

See [HowToUse.c](Source/Example/HowToUse.c) for a complete example.

## Streaming

When a function returns `1`, `p.buf` and `p.len` are the part of the buffer not
yet consumed by the parser. Provide a buffer starting with this part followed by
more data, set `p.buf` and `p.len` to this buffer (`p.flags` must be kept), and
call the same function again. Strings returned before point to the previous
buffer.

See [HowToUse_Stream.c](Source/Example/HowToUse_Stream.c) for a complete example.

## Limitations

tfsxml is not a validating parser:
- UTF-8 (or ASCII compatible) content only, a UTF-16 content is rejected
- no DTD processing (a DOCTYPE internal subset with `>` is not supported), no custom entities
- names are not decoded, namespaces are not processed (prefixes are not
  resolved to namespace URIs, they can only be compared)
- end tags are counted, not matched with start tags (the name in the end tag
  is provided for checking it)
- malformed content is parsed as much as possible, without crash or hang
- nesting depth is limited to 2^30 levels when data is provided in several chunks

## Build

CMake:
```sh
cmake -S Project/CMake -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build --config Release
ctest --test-dir Build -C Release
```

GNU autotools:
```sh
cd Project/GNU/Library
./autogen.sh
./configure
make
make check
```

Visual Studio: `Project/MSVC2026/tfsxml.slnx` (Visual Studio 2026) or
`Project/MSVC2022/tfsxml.sln` (Visual Studio 2022)

The library can also be used by adding `Source/tfsxml/tfsxml.c` and
`Source/tfsxml/tfsxml.h` to a project.

The tinyxml2 compatibility layer (`tfsxml_tinyxml2` library, C++11) is built
by default, `-DTFSXML_BUILD_TINYXML2=OFF` (CMake) or `--disable-tinyxml2`
(GNU autotools) disable it.

## Tests

The regression test tool checks the API, the expected traces of the test files,
and that the results do not depend on how the content is split in chunks. A
fuzzer is also available. The tinyxml2 compatibility layer has its own expected
results, and can be compared with tinyxml2.

The test files are in the
[tfsxml-RegressionTestingFiles](https://github.com/MediaArea/tfsxml-RegressionTestingFiles)
repository. The CMake and GNU autotools builds look for it next to the tfsxml
directory, without it only the API unit tests are run:
```sh
git clone https://github.com/MediaArea/tfsxml-RegressionTestingFiles.git ../tfsxml-RegressionTestingFiles
```

See the [regression test protocol](Source/Doc/RegressionTest.md).

## License

MIT, see [License.txt](License.txt).
