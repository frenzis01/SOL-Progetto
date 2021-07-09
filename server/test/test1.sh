#!/bin/bash
#TEST1 (aka valgrind test)
BWHT="\033[1;37m"
REG="\x1b[0m"
WHT_LINE="echo -e \"$BWHT
---------------------------------
$REG\""
#run server in background
valgrind --leak-check=full bin/server test/conf/1.txt &
# gdb --args bin/server test/conf/1.txt
S_PID=$!
# export S_PID


echo -e $BWHT '
-------------CLIENT 1------------
' $REG
#all options at once
#   note: -D, -d, -E are useless, there won't be any evicted files
valgrind --leak-check=full bin/client -h -f sock -t 200 -p -E test/evicted -w mock/1,n=2 -W mock/1/3,mock/2/8 -D test/evicted -r mock/1/3,mock/2/8 -d test/read -R -d test/read -w mock/1 -l mock/2/5,mock/2/6 -u mock/2/6 -c mock/2/5
#   note: client.c will automatically generate the absolute paths
eval "$WHT_LINE"
#let's be more specific


echo -e $BWHT '
-------------CLIENT 2------------
' $REG
#writeFile/appendToFile + readNfiles     -> test/read contains 1/1,...,1/4, 2/5,...2/8
valgrind --leak-check=full bin/client -t 200 -p -f sock -w mock/1 -w mock/2 -R -d test/read
echo -e $BWHT '     Have a look at read files...' $REG
ls test/read/home/francis/Documenti/SOL/server/mock/1 test/read/home/francis/Documenti/SOL/server/mock/2
eval "$WHT_LINE"

echo -e $BWHT '
-------------CLIENT 3------------
' $REG
#writeFile/readFile
valgrind --leak-check=full bin/client -t 200 -p -f sock -W mock/2/5,mock/2/6 -r mock/2/5,mock/2/6 -d test/read
eval "$WHT_LINE"

echo -e $BWHT '
-------------CLIENT 4------------
' $REG
#concurrent lockFile + unlockFile/removeFile
valgrind --leak-check=full bin/client -t -p -f sock -W mock/2/7
valgrind --leak-check=full bin/client -t 200 -p -f sock -l mock/2/7 -u mock/2/7 &
valgrind --leak-check=full bin/client -t 220 -p -f sock -l mock/2/7 -c mock/2/7

echo -e "$BWHT
    ...KILLING SERVER...
"
# bash -c 'kill -1 $S_PID'
kill -1 $S_PID

wait $S_PID

eval "$WHT_LINE"
echo -e $BWHT '

---------------FINE--------------

' $REG
echo -e $BWHT 'Well done!
' $REG