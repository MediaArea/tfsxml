# tfsxml regression test protocol

This document describes how tfsxml is tested, and what must be run before
merging a change and before a release.

## What is checked

| Check | Tool | What fails it |
|---|---|---|
| API unit tests | `RegressionTest` | `tfsxml_init`, string helpers, `tfsxml_decode`, call sequences different from the expected results |
| Expected traces | `RegressionTest` | trace of a test file different from its `.xml.txt` file |
| Streaming | `RegressionTest` | trace with data in chunks different from the trace with all data at once |
| Memory and undefined behavior | `RegressionTest` and `Fuzz` built with sanitizers | any sanitizer report |
| Robustness | `Fuzz` (coverage-guided) | crash, hang, sanitizer report, any check of the fuzz target |
| Behavior changes | `Fuzz` built with a reference version | trace different from the trace of the reference version |
| Portability | CI | warning or failure with GCC, Clang, MSVC, C89, C++, signed and unsigned `char`, without C library |
| Speed | `RegressionTest --speed` | slower than the reference version |
| tinyxml2 compatibility layer | `RegressionTest_TinyXml2` | dump of a test file different from its `.xml.txt` file |
| Same results as tinyxml2 | `RegressionTest_TinyXml2_Reference`, `Fuzz_TinyXml2` built with tinyxml2 | dump different from the dump of tinyxml2 |
| Integration | MediaInfoLib | different MediaInfo output on its regression files |

### Traces

The tests are based on traces of the API results. A walk calls the API on a
content following a call sequence, each result is a line:

```
init 0
next 0 "?xml version=\"1.0\"?" @21
next 0 "root" @27
attr 0 "a" "1 &amp; 2" -> "1 & 2" @39
attr -1 @40
enter 0 @40
  next -1 "root" @47
next 1 @48
end 0
```

- call name, returned value, returned strings (escaped: `\"`, `\\`, `\n`,
  `\r`, `\t`, `\xHH` for other bytes), decoded string after `->` if different
- `next -1` at the end of an element: name in the end tag
- `@N`: count of bytes consumed by the parser after the call
- indentation: depth entered by the walk
- `end N`: count of bytes not consumed at the end of the walk

Walk modes (each expected trace has all of them, see `RegressionTest_Trace.c`):

| Mode | Calls |
|---|---|
| `tree` | `next`, all attributes, `enter` all elements |
| `value` | `next`, all attributes, `value` of all elements |
| `skip` | `next` only (attributes and content are skipped by the parser) |
| `enter` | `next` and `enter` only (attributes are skipped by the parser) |
| `mixed` | `text` before each `next`, all attributes, `enter` all elements (as for building a tree) |
| `random N` | pseudo-random valid sequence with seed N (1 to 3): some attributes, `text`, `value`, `enter`, `leave` at random places, calls after the end of attributes or after value |

The walk enters at most 32 levels, deeper content is skipped by the parser.

### Streaming checks

For each file and each walk mode, the trace must be the same when the data is
provided in chunks (`RegressionTest_Trace.c`, `check_chunks` in `RegressionTest.c`):
- 2 chunks, for all split points
- byte per byte
- 8 random sizes sequences

Files bigger than 16 KiB (e.g. real files with `--check-only`) are tested with
16 split points and 2 random sequences of chunks up to 64 KiB only.

Each new buffer is allocated with the exact size and contains only the bytes
not consumed by the parser followed by the next chunk, so the sanitizers detect
a read outside of what the caller provided and a reuse of a previous buffer.
After each call, the parser position must be in the buffer and the end of its
remaining data must be the end of the buffer.

### Fuzz target

`Source/Fuzz/Fuzz.c` has the libFuzzer interface. For each input:
- all walk modes, all data at once, then random chunk sizes, then byte per byte
  (inputs up to 256 bytes): traces must be the same
- walk with random calls in any order (including calls not following the API
  rules) and random buffer refills: only memory safety and parser position are
  checked
- `tfsxml_strcmp_charp`, `tfsxml_strncmp_charp`, `tfsxml_strstr_charp`,
  `tfsxml_local_name`, `tfsxml_prefix`, `tfsxml_strcmp_local_charp`,
  `tfsxml_strcmp_prefix_charp` compared to naive implementations
- decoded content must not be longer than the encoded content
- if built with `TFSXML_FUZZ_REFERENCE`: traces with all data at once must be
  the same as the ones of the reference implementation (position after a "need
  more data" at the end of the content is not compared)

The seed corpus is the test files, with the dictionary `Source/Fuzz/Fuzz.dict`.
The first directory given to the fuzzer receives the new inputs (libFuzzer
behavior): always give a corpus directory first, never the test files.

## When to run what

| Change | Minimum |
|---|---|
| Documentation, build files | CI |
| Test tool or test files | CI, `RegressionTest` with sanitizers |
| `tfsxml.c` or `tfsxml.h` | all above, fuzzing (1 hour per core, at least 4 cores), differential fuzzing with the previous version (30 minutes per core), speed test, comparison of the tinyxml2 compatibility layer with tinyxml2 |
| `tfsxml_tinyxml2.cpp` or `tfsxml_tinyxml2.h` | CI, comparison with tinyxml2 on all test files, differential fuzzing with tinyxml2 (30 minutes per core) |
| Release | all above, fuzzing for 8 hours per core, MSVC build and tests, MediaInfoLib integration |

A change of the behavior must be intended: new or updated test files show it,
the expected traces are updated in tfsxml-RegressionTestingFiles in a branch
having the same name as the tfsxml branch (the CI of tfsxml uses the branch
with the same name in the fork of the author if it exists, else main), and
History.txt lists it.

## Commands

### Test files

The test files are in the `Files` directory (tfsxml) and in the `TinyXml2`
directory (tinyxml2 compatibility layer) of
[tfsxml-RegressionTestingFiles](https://github.com/MediaArea/tfsxml-RegressionTestingFiles).
The CMake and GNU autotools builds look for this repository next to the tfsxml
directory, another location can be set with `-DTFSXML_TESTING_FILES=DIR`
(CMake) or with the `TFSXML_TESTING_FILES` environment variable (`make check`).
Without it, only the API unit tests are run. The commands below are run from
the tfsxml directory, with the test files in `../tfsxml-RegressionTestingFiles`:
```sh
git clone https://github.com/MediaArea/tfsxml-RegressionTestingFiles.git ../tfsxml-RegressionTestingFiles
```

### Regression tests

CMake:
```sh
cmake -S Project/CMake -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build --config Release
ctest --test-dir Build -C Release --output-on-failure
```

GNU autotools (`make check` also checks that the streaming example gives the
same output as the whole file example):
```sh
cd Project/GNU/Library
./autogen.sh
./configure
make check
```

With sanitizers, and with the other `char` signedness (`-fsigned-char` on ARM,
`-funsigned-char` on x86):
```sh
cmake -S Project/CMake -B Build_San -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fsigned-char"
cmake --build Build_San
ctest --test-dir Build_San --output-on-failure
```

Visual Studio: build `Project/MSVC2026/tfsxml.slnx` (or `Project/MSVC2022/tfsxml.sln`),
run `RegressionTest.exe ..\..\..\..\..\tfsxml-RegressionTestingFiles\Files` from its output
directory (e.g. `Project/MSVC2026/ARM64/Release`). MSVC `char` is signed on all
architectures.

Direct use of the tool:
```sh
RegressionTest Files                           # Unit tests, expected traces, streaming
RegressionTest --no-streaming Files            # Faster, without streaming checks
RegressionTest --check-only Dir                # All files of Dir, streaming and harness checks only
RegressionTest --dump File.xml                 # Traces of all walk modes
RegressionTest --dump --mode=random --seed=7 --chunks=1,3,5 File.xml
RegressionTest --update Files/New              # Write expected traces
```

### Fuzzing

With Clang (libFuzzer):
```sh
cmake -S Project/CMake -B Build_Fuzz -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTFSXML_BUILD_FUZZER=ON -DTFSXML_BUILD_TESTS=OFF -DTFSXML_BUILD_EXAMPLES=OFF
cmake --build Build_Fuzz
mkdir -p Corpus
Build_Fuzz/tfsxml_Fuzz -fork=8 -max_total_time=3600 -max_len=4096 -timeout=20 \
  -dict=Source/Fuzz/Fuzz.dict Corpus ../tfsxml-RegressionTestingFiles/Files
```

With GCC, the same commands build the standalone driver (`Fuzz_Main.c`) with
coverage-guided fuzzing (`-fsanitize-coverage=trace-pc`). It runs one process,
for several cores run several processes with different `-seed` and output
directories, then merge the directories:
```sh
for i in 1 2 3 4; do
  mkdir -p Corpus_$i
  Build_Fuzz/tfsxml_Fuzz -seed=$i -max_total_time=3600 -dict=Source/Fuzz/Fuzz.dict \
    -artifact_prefix=./ Corpus_$i Corpus ../tfsxml-RegressionTestingFiles/Files &
done
wait
cp -n Corpus_*/* Corpus/
```

With Visual Studio 2026 (libFuzzer and AddressSanitizer): build the `Fuzz`
project, run `Fuzz.exe -max_total_time=3600 -dict=... Corpus ..\..\..\..\tfsxml-RegressionTestingFiles\Files`
(the directory of `clang_rt.asan_dynamic-*.dll` must be in `PATH`).

Replay of a corpus or of an artifact (any build of the fuzzer):
```sh
Build_Fuzz/tfsxml_Fuzz -runs=0 crash-0123abcd Corpus
```

### Differential fuzzing with the previous version

```sh
mkdir -p Reference
git show v0.1.0:Source/tfsxml/tfsxml.c > Reference/tfsxml.c
git show v0.1.0:Source/tfsxml/tfsxml.h > Reference/tfsxml.h
cmake -S Project/CMake -B Build_Diff -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTFSXML_BUILD_FUZZER=ON -DTFSXML_BUILD_TESTS=OFF -DTFSXML_BUILD_EXAMPLES=OFF \
  -DTFSXML_FUZZ_REFERENCE=$PWD/Reference/tfsxml.c
cmake --build Build_Diff
Build_Diff/tfsxml_Fuzz -fork=8 -max_total_time=1800 -dict=Source/Fuzz/Fuzz.dict Corpus ../tfsxml-RegressionTestingFiles/Files
```

A difference stops the fuzzer with both traces. If the difference is intended,
it must be in the test files, and the differential fuzzing must be run again
with a reference having the same intended change (or the inputs showing only
the intended difference must be checked by hand).

With a reference version without `tfsxml_text()` and without the name of the
end tag returned by `tfsxml_next()` (e.g. the version from MediaInfoLib before
0.1.0), add `-DCMAKE_C_FLAGS=-DREGRESSIONTEST_LEGACY`: the walks of both
versions do not call `tfsxml_text()` and ignore the end tag names.

### tinyxml2 compatibility layer

The test tool parses each test file with the API of the compatibility layer
with 5 parsing options (`Parse()` with a size, `Parse()` of a string ended by
NUL, `LoadFile()`, `processEntities` false, `COLLAPSE_WHITESPACE`), and dumps
the results (`RegressionTest_TinyXml2.h`): errors, error strings and line
numbers, all nodes with their values and line numbers, results of the
navigation and conversion functions, `XMLPrinter` outputs. The dump must be the
one in `FILE.txt`. The expected results of the `TinyXml2` directory of
tfsxml-RegressionTestingFiles were checked against tinyxml2 6.2.0.

```sh
RegressionTest_TinyXml2 ../tfsxml-RegressionTestingFiles/TinyXml2  # Expected results (CTest, make check)
RegressionTest_TinyXml2 --dump File.xml                     # Dump
RegressionTest_TinyXml2 --update Dir                        # Write expected results
```

With tinyxml2 6.2.0 sources, the tool is also built with tinyxml2 for
comparing the dumps of both (`--reference`, on any file), and the fuzzer for
the differential fuzzing (`tfsxml_Fuzz_TinyXml2`):
```sh
git clone --depth 1 --branch 6.2.0 https://github.com/leethomason/tinyxml2.git Reference_tinyxml2
cmake -S Project/CMake -B Build_TinyXml2 -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DTFSXML_TINYXML2_REFERENCE=$PWD/Reference_tinyxml2 -DTFSXML_BUILD_FUZZER=ON
cmake --build Build_TinyXml2
ctest --test-dir Build_TinyXml2 --output-on-failure      # Comparison on all test files
Build_TinyXml2/tfsxml_RegressionTest_TinyXml2_Reference --reference Dir
mkdir -p Corpus_TinyXml2
Build_TinyXml2/tfsxml_Fuzz_TinyXml2 -max_total_time=1800 -dict=Source/Fuzz/Fuzz.dict \
  Corpus_TinyXml2 ../tfsxml-RegressionTestingFiles/TinyXml2 ../tfsxml-RegressionTestingFiles/Files
```

Decimal character references with values above 2^32 give different results
on Windows and on other platforms (as with tinyxml2, `unsigned long` is used),
the expected results must not depend on them.

MediaInfoLib integration: build MediaInfoLib 3 times, with tinyxml2 and the
current tfsxml, with tinyxml2 and the new tfsxml, with the compatibility layer
and the new tfsxml (see [TinyXml2.md](TinyXml2.md) for the changes in
MediaInfoLib). The last 2 builds must give the same MediaInfo output on files
of the formats read with tinyxml2 (DCP, IMF, TTML, DASH, ISM, HDS, P2, XDCAM,
Dolby Vision metadata, property lists, MediaInfo XML), including malformed
ones.

### Speed

```sh
RegressionTest --speed --duration=2                            # Synthetic content (32 MiB each)
RegressionTest --speed=File.xml --value-depth=3 --duration=2   # A real file
RegressionTest --speed-files=Dir                               # Write the synthetic content in Dir
RegressionTest --speed=Dir/adm.xml --value-depth=6 --whole     # Without the tests with chunks
```

Value depths of the synthetic files: adm 6, text 3, markup 1, deep 100.

Results are in MB/s for the walks `tree` (all elements entered), `value`
(elements entered until a depth, value of the elements at this depth), `skip`
(content of the root element skipped by the parser), `decode` (same as value,
with decoding), with all data at once and with chunks of 4 KiB.
The value of an element is parsed again from its start after each new chunk
until it is complete, so a big element with a value is slow with small chunks
(by design, its value is returned in a single buffer).

Build the tool with the same compiler and
options with the current and the reference `tfsxml.c`, run both on an idle
machine, alternate runs (at least 5 rounds, compare the best and the mean of
each result, results of a single run may vary by more than 10%). A slowdown of
more than 5% on one of the results must be explained.

Reference results (ARM64, GCC 15 -O2, WSL, all data at once, best of 3 to 6
interleaved runs, MB/s) of the version from MediaInfoLib and of version 0.1.0:

| Content | tree | value | skip | decode |
|---|---|---|---|---|
| adm | 635 → 859 | 620 → 793 | 623 → 1118 | 618 → 706 |
| markup | 695 → 931 | 674 → 887 | 697 → 1053 | 387 → 433 |
| deep | 386 → 389 | 442 → 479 | 519 → 669 | 438 → 453 |
| text | 361 → 50307 | 361 → 24948 | 358 → 50024 | 343 → 10011 |

With data in chunks of 4 KiB, the version from MediaInfoLib gives wrong
results in the `skip` walk (the content after a skipped element split
between 2 chunks is wrong).

### MediaInfoLib integration

1. Copy `Source/tfsxml/tfsxml.c` and `Source/tfsxml/tfsxml.h` to
   `MediaInfoLib/Source/ThirdParty/tfsxml/`.
2. Build MediaInfoLib and MediaInfo CLI with the previous and the new version.
3. Run both on the MediaInfoLib regression files having XML content (ADM in
   WAV/BWF and in XML, S-ADM, XMP, MPEG-DASH, TTML, HDR Vivid, 360 video...),
   with `--Output=XML` and `--Details=1` if needed, and compare the outputs.
   Expected differences are only the ones from the intended changes.

## Handling a failure

1. Keep the input (fuzzer artifact or file).
2. Reduce it (libFuzzer: `tfsxml_Fuzz -minimize_crash=1 -runs=100000 crash-...`,
   or by hand) and get the trace with `RegressionTest --dump`.
3. Fix the issue.
4. Add the reduced input in `Files/Regression` of tfsxml-RegressionTestingFiles
   with a name describing the issue, generate its expected trace with
   `--update`, check by reading it that the trace is the expected behavior.
5. Run the complete protocol for a change of `tfsxml.c`.

## Adding test files

Test files are in tfsxml-RegressionTestingFiles. They must be small and written
for the test suite (no third-party content).
After adding `Name.xml`, run `RegressionTest --update` on its directory, read
the generated `Name.xml.txt` for checking it, then run `RegressionTest` on all
files.

Test files of the tinyxml2 compatibility layer are in the `TinyXml2` directory
of tfsxml-RegressionTestingFiles: after adding `Name.xml`, run
`RegressionTest_TinyXml2 --update` on this directory and check the results
with `RegressionTest_TinyXml2_Reference --reference` (build with tinyxml2).
