#!/bin/bash

. tests/variables
if [ -z "$VER_NDJSON" ]; then
	exit 0
fi

while IFS= read -r line; do
    if [ -z "$line" ]; then
        continue
    fi

    if ! echo "$line" | $VER_NDJSON 2>/dev/null; then
        echo "Malformed line: $line" >&2
        exit 1
    fi
done

exit 0

