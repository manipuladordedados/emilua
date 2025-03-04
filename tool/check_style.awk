/^\t/ {
    if (FILENAME in exceptions) {
        tab_counts[FILENAME]++
        if (tab_counts[FILENAME] > exceptions[FILENAME]) {
            printf "Error: File %s exceeds the allowed tab count (%d). Found tab at line %d\n", FILENAME, exceptions[FILENAME], FNR
            err = 1
        }
    } else {
        printf "Error: Tab character found in file %s at line %d\n", FILENAME, FNR
        err = 1
    }
}

length($0) > 80 && !(FILENAME in exceptions) {
    printf "Error: Line longer than 80 characters found in file %s at line %d\n", FILENAME, FNR
    err = 1
}

END {
  exit err
}
