#!/bin/bash
#TEST2 (aka eviction test)
BWHT="\033[1;37m"
REG="\x1b[0m"

clients=(
    'bin/client -f sock -t 0 -w mock/3/,n=3 -D test/evicted'
    'bin/client -f sock -t 0 -w mock/3/,n=3 -D test/evicted -R -d test/read'
    'bin/client -f sock -t 0 -W mock/3/6.rtf,mock/3/8.pdf -D test/evicted -u mock/3/8.pdf'
    'bin/client -f sock -t 0 -R n=3 -d test/read -r mock/3/7.pdf,mock/3/8.pdf'
    'bin/client -f sock -t 0 -R n=-2 -d test/read -l mock/3/1.doc -r mock/3/1.doc -u mock/3/1.doc '
    'bin/client -f sock -t 0 -R n=0 -d test/read -l mock/3/3.rtf -r mock/3/3.rtf -u mock/3/3.rtf '
    'bin/client -f sock -t 0 -r mock/3/1.doc,mock/3/2.doc,mock/3/3.rtf -W mock/3/7.pdf '
    'bin/client -f sock -t 0 -E test/evicted -c mock/3/7.pdf -r mock/3/6.rtf -W mock/3/3.rtf'
    )

while true 
do
    i=$(( RANDOM % ${#clients[@]}))
    ${clients[i]}
done