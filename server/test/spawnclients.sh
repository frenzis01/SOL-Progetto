#!/bin/bash
#TEST2 (aka eviction test)
BWHT="\033[1;37m"
REG="\x1b[0m"

clients=(
    'bin/client 0 -f sock -w mock/3/,n=3 -D test/evicted -c mock/3/3.rtf -l mock/3/8.pdf'
    'bin/client 0 -f sock -w mock/3/,n=3 -D test/evicted -R -d test/read'
    'bin/client 0 -f sock -W mock/3/7.pdf,mock/3/8.pdf -D test/evicted'
    'bin/client 0 -f sock -R n=3 -d test/read -r mock/3/7.pdf,mock/3/8.pdf,mock/3/5.rtf'
    'bin/client 0 -f sock -w mock/3,n=2 -l mock/3/1.doc,mock/3/2.doc -t 20 -u mock/3/1.doc,mock/3/2.doc'
    'bin/client 0 -f sock -w mock/3,n=1 -l mock/3/3.rtf,mock/3/4.rtf -t 20 -u mock/3/3.rtf,mock/3/4.rtf'
    'bin/client 0 -f sock -r mock/3/1.doc,mock/3/2.doc -l mock/3/7.pdf,mock/3/8.pdf -t 20 -u mock/3/7.pdf,mock/3/8.pdf'
    'bin/client 0 -f sock -E test/evicted -c mock/3/7.pdf,mock/3/8.pdf -W mock/3/3.rtf,mock/3/4.rtf'
    )

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ${clients[i]}
done