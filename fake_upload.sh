#!/bin/bash

# Small script to simulate an FCGI environment,
# reading the file data from STDIN with CONTENT_LENGTH set

DEBUGGER="valgrind --leak-check=full"

export CONTENT_LENGTH=`du -b "$1" | cut -d"	" -f1`
echo "fake_upload.sh: CONTENT_LENGTH = "$CONTENT_LENGTH
$DEBUGGER ./main < "$1"
