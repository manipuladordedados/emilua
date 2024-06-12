#!/bin/sh

cd "$(dirname "$0")"

# Check for xref issues
grep -Fre 'xref:' modules/tutorial/pages/*.adoc | grep -Eve 'xref:(tutorial|ref):' && exit 1

sed -i -E 's/xref(:ref)?:(.*)\.adoc\[([^]]*)\]/<<_\2,\3>>/g' modules/ref/pages/*.adoc modules/tutorial/pages/*.adoc
sed -i -E 's/<<_([^,]*)[.:]/<<_\1_/g' modules/ref/pages/*.adoc modules/tutorial/pages/*.adoc
sed -i 's/✔/icon:check[2x]/g' modules/ref/pages/*.adoc modules/tutorial/pages/*.adoc
