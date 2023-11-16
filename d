#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <binary_file>"
    exit 1
fi

binary_file="$1"

od -t x1 -A n "$binary_file"
