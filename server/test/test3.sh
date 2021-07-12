#!/bin/bash
#TEST3 (aka stress test)
BWHT="\033[1;37m"
REG="\x1b[0m"
TIMER=30
export BWHT

echo "
    ...Removing log.txt and read/evicted folders...
"
rm -r -f test/read/* test/evicted/* log.txt

#run server in background
bin/server test/conf/2_1.txt &
export S_PID=$!

echo -e $BWHT "

    STARTING TEST3
    10 Clients will run simultaneously without '-p' option for ${TIMER}s $REG

    Errors, if any, will be printed

"

start=$SECONDS

for i in {1..10}; do
    test/spawnclients.sh &
done

sleep ${TIMER}

echo -e "
    KILLING SERVER and CLIENTS
"
kill -2 $S_PID
killall -9 spawnclients.sh

# wait $C_PID
echo -e $BWHT '

    ...Clients who are waiting for openConnection() to fail will die in 10s...

' $REG
sleep 12

duration=$(( SECONDS - start ))

echo -e $BWHT "

    Well done! (${duration}s)

" $REG