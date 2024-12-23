/^\t/ {
    printf "Error: Tab character found in file %s at line %d\n", FILENAME, FNR
    err = 1
}

!(FILENAME in exceptions) {
    if (length($0) > 80) {
        printf "Error: Line longer than 80 characters found in file %s at line %d\n", FILENAME, FNR
        err = 1
    }
}

END {
  exit err
}
