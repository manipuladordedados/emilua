/^\t/ {
    printf "Error: Tab character found in file %s at line %d\n", FILENAME, FNR
    err = 1
 }

length($0) > 80 {
    errors[FILENAME]++
 }

{
  if (FILENAME in exceptions) {
    if (errors[FILENAME] > exceptions[FILENAME]) {
      printf "Error: File %s exceeds the allowed line length error count (%d).\n", FILENAME, exceptions[FILENAME]
      err = 1
      nextfile
    }
  } else {
    if (length($0) > 80) {
      printf "Error: Line longer than 80 characters found in file %s at line %d\n", FILENAME, FNR
      err = 1
    }
  }
}

END {
  exit err
}
