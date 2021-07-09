#!/bin/bash
#TEST2 (aka eviction test)
BWHT="\033[1;37m"
REG="\x1b[0m"

clients=(
    'bin/client -t 5 -f sock -w mock/3/,n=3 -D test/evicted'
    'bin/client -t 5 -f sock -w mock/3/,n=3 -D test/evicted'
    'bin/client -t 5 -f sock -W mock/3/7.pdf,mock/3/8.pdf -D test/evicted'
    'bin/client -t 5 -f sock -R n=3 -d test/read'
    'bin/client -t 5 -f sock -l mock/3/1.doc,mock/3/2.doc -t 50 -u mock/3/1.doc,mock/3/2.doc'
    'bin/client -t 5 -f sock -l mock/3/3.rtf,mock/3/4.rtf -t 50 -u mock/3/3.rtf,mock/3/4.rtf'
    'bin/client -t 5 -f sock -l mock/3/7.pdf,mock/3/8.pdf -t 50 -u mock/3/7.pdf,mock/3/8.pdf'
    'bin/client -t 5 -f sock -c mock/3/7.pdf,mock/3/8.pdf'
    )

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ${clients[i]}
done