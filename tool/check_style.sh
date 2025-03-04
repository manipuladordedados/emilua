#!/bin/sh

cd "$(dirname "$0")"

filtered_files=$(find "../" -type f \( \
  -name "*.cpp" -path "../src/*" -o \
  -name "*.ypp" -path "../src/*" -o \
  -name "*.hpp" -path "../include/*" -o \
  -name "*.lua" -path "../bytecode/*" \
\))

awk -f exceptions.awk -f check_style.awk $filtered_files
