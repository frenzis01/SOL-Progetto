#!/bin/bash
#TEST2 (aka eviction test)
BWHT="\033[1;37m"
REG="\x1b[0m"

echo "
    ...Removing log.txt and cleaning read/evicted folders...
"
rm -r -f test/read/* test/evicted/* log.txt

start=$SECONDS

echo -e $BWHT'
---------------LRU--------------
' $REG

#run server in background
bin/server test/conf/2_1.txt &
S_PID=$!

# We want to test LRU replacement algorithm

bin/client -p -t 50 -f sock -W mock/3/1.doc,mock/3/2.doc,mock/3/3.rtf,mock/3/9.rtf,mock/3/10.rtf -r mock/3/3.rtf,mock/3/9.rtf -d test/read
kill -10 $S_PID # Print store statistics

# Writing a 3MB file will cause the eviction of 1.doc and 2.doc
bin/client -p -t 50 -f sock -W mock/3/11.rtf -D test/evicted
echo -e $BWHT "
    Let's check if the client received and stored 1.doc and 2.doc...
"
ls -s test/evicted$PWD/mock/3
echo -e $REG

kill -10 $S_PID # Print store statistics

# Writing a 3MB file will cause the eviction of 10.rtf
bin/client -p -t 50 -f sock -W mock/3/11.rtf -D test/evicted
echo -e $BWHT "
    Let's check if the client received and stored 10.rtf...
"
ls -s test/evicted$PWD/mock/3
echo -e $REG


kill -1 $S_PID
sleep 1

echo -e $BWHT"
---------------FIFO--------------
" $REG

rm -r test/evicted/*    # clean evicted dir

bin/server test/conf/2_2.txt &  # FIFO config file
S_PID=$!

bin/client -p -t 50 -f sock -W mock/3/3.rtf,mock/3/9.rtf,mock/3/10.rtf,mock/3/1.doc,mock/3/2.doc
kill -10 $S_PID # Print store statistics

# Writing a 3MB file will cause the eviction of 3.rtf and 9.rtf
bin/client -p -t 50 -f sock -W mock/3/11.rtf -D test/evicted
echo -e $BWHT "
    Let's check if the client received and stored 3.rtf and 9.rtf
"
ls -s test/evicted$PWD/mock/3
echo -e $REG

kill -10 $S_PID # Print store statistics

# Writing a 3MB file will cause the eviction of 10.rtf
bin/client -p -t 50 -f sock -W mock/3/11.rtf -D test/evicted
echo -e $BWHT "
    Let's check if the client received and stored 10.rtf
"
ls -s test/evicted$PWD/mock/3

kill -1 $S_PID
sleep 1

duration=$(( SECONDS - start ))

echo -e $BWHT "

    Well done! (${duration}s)

" $REG
