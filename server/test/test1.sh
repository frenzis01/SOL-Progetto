#!/bin/bash
#TEST1 (aka valgrind test)

#run server in background
valgrind --leak-check=full bin/server test/conf/1.txt &
# gdb --args bin/server test/conf/1.txt
S_PID=$!
# export S_PID

#all options at once
#   note: -D, -d, -E are useless, there won't be any evicted files
bin/client -h -f sock -t 100 -p -E test/evicted -w mock/1,n=2 -W mock/1/3,mock/2/8 -D test/evicted -r mock/1/3,mock/2/8 -d test/read -R -d test/read -w mock/1 -l mock/2/5,mock/2/6 -u mock/2/6 -c mock/2/5
#   note: client.c will automatically generate the absolute paths
echo '

---------------------------------

'
#let's be more specific

#clean 'read' dir

#writeFile/appendToFile + readNfiles     -> test/read contains 1/1,...,1/4, 2/5,...2/8
bin/client -t 100 -p -f sock -w mock/1 -w mock/2 -R -d test/read
echo '

---------------------------------

'
ls test/read/home/francis/Documenti/SOL/server/mock/1 test/read/home/francis/Documenti/SOL/server/mock/2
echo '

---------------------------------

'
#writeFile/readFile
bin/client -t 100 -p -f sock -W mock/2/5,mock/2/6 -r mock/2/5,mock/2/6 -d test/read
echo '

---------------------------------

'
#concurrent lockFile + unlockFile/removeFile

# TODO this is a brutal deadlock! but the server can't do anything about it
# bin/client -t 200 -p -f sock -l mock/2/7,mock/2/8 -u mock/2/7 -c mock/2/8 &
# bin/client -t 198 -p -f sock -l mock/2/7,mock/2/8 -u mock/2/7 -c mock/2/7
echo '

---------------------------------

'
bin/client -t -p -f sock -W mock/2/7
bin/client -t -p 100 -f sock -l mock/2/7 -u mock/2/7 &
bin/client -t -p 120 -f sock -l mock/2/7 -c mock/2/7
echo '

---------------FINE--------------

'

sleep 1
# bash -c 'kill -1 $S_PID'
kill -1 $S_PID

echo 'Well done!'