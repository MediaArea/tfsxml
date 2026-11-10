#!/bin/sh

# Same output with the whole file in memory and with the file read by chunks
# Test files: see regression.sh
files="${TFSXML_TESTING_FILES:-${srcdir:-.}/../../../../tfsxml-RegressionTestingFiles}/Files"
if test ! -d "${files}" ; then
    echo "Test files not found in ${files}, skipped"
    exit 77
fi
status=0
for file in "${files}"/Formats/*.xml "${files}"/Basic/Nested.xml ; do
    for names in "" "title author audioBlockFormatID gain position BaseURL" ; do
        whole="$(./test/HowToUse "${file}" ${names} 2>&1; echo "exit $?")"
        for chunk in 1 7 4096 ; do
            streamed="$(./test/HowToUse_Stream "${file}" ${chunk} ${names} 2>&1; echo "exit $?")"
            if test "${whole}" != "${streamed}" ; then
                echo "FAILED: ${file}, chunks of ${chunk}, names '${names}'"
                status=1
            fi
        done
    done
done
exit ${status}
