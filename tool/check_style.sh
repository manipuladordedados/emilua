#!/bin/sh

filtered_files=$(find "." -type f \( \
  -name "*.cpp" -path "./src/*" -o \
  -name "*.ypp" -path "./src/*" -o \
  -name "*.hpp" -path "./include/*" -o \
  -name "*.lua" -path "./bytecode/*" \
\))

ignore_files="
./src/ip.cpp
./src/main.ypp
./src/tls.cpp
./src/regex.cpp
./src/stream.cpp
./src/core.cpp
./src/file.cpp
./bytecode/ip_dial.lua
"

awk -v ignore_files="$ignore_files" '
BEGIN {
  split(ignore_files, ignored, " ")
  for (i in ignored) ignore[ignored[i]] = 1
}
{
  if (FILENAME in ignore) {
    next
  }
  if (/\t/) {
    printf "Error: Tab character found in file %s at line %d\n", FILENAME, FNR
    err = 1
  }
  if (length($0) > 80) {
    printf "Error: Line longer than 80 characters found in file %s at line %d\n", FILENAME, FNR
    err = 1
  }
}
END { exit err }
' $filtered_files

if [ $? -ne 0 ]; then
  echo "Code style check failed. Fix the issues before submitting the PR."
  exit 1
fi
