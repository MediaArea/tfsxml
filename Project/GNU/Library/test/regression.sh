#!/bin/sh

# Unit tests, expected traces of the test files, traces with data in chunks
# Test files: clone of https://github.com/MediaArea/tfsxml-RegressionTestingFiles
# next to the tfsxml directory, or in the directory set by TFSXML_TESTING_FILES
files="${TFSXML_TESTING_FILES:-${srcdir:-.}/../../../../tfsxml-RegressionTestingFiles}/Files"
if test ! -d "${files}" ; then
    echo "Test files not found in ${files}, only the API unit tests are run"
    exec ./test/RegressionTest
fi
exec ./test/RegressionTest "${files}"
