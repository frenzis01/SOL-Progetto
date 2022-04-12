#!/bin/bash
#TEST3 (aka stress test)
BWHT="\033[1;37m"
REG="\x1b[0m"
TIMER=5
export BWHT

echo "
    ...Removing log.txt and cleaning read/evicted folders...
"
rm -r -f test/read/* test/evicted/* log.txt

#run server in background
bin/server test/conf/3.txt x &
export S_PID=$!

echo -e $BWHT "

    STARTING TEST3
    10 Clients will run simultaneously without '-p' option for ${TIMER}s $REG

    Errors, if any, will be printed

    
    Clients who notice that the server is down, will write a message on stdout

"

start=$SECONDS

for i in {1..10}; do
    test/spawnclients.sh &
done

sleep ${TIMER}

echo -e "
    KILLING SERVER and SPAWNCLIENTS
"
killall -9 spawnclients.sh > /dev/null 2>/dev/null
kill -2 $S_PID

echo -e $BWHT '

    ...Clients who are waiting for openConnection() to fail will die in 10s...

' $REG
sleep 12

duration=$(( SECONDS - start ))

echo -e $BWHT "

    Well done! (${duration}s)

" $REG