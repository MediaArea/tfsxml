#!/bin/sh

# tinyxml2 compatibility layer: expected results of the test files
# Test files: see regression.sh
files="${TFSXML_TESTING_FILES:-${srcdir:-.}/../../../../tfsxml-RegressionTestingFiles}/TinyXml2"
if test ! -d "${files}" ; then
    echo "Test files not found in ${files}, skipped"
    exit 77
fi
exec ./test/RegressionTest_TinyXml2 "${files}"
