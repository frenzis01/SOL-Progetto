#!/bin/bash
#TEST3 (aka stress test)
BWHT="\033[1;37m"
REG="\x1b[0m"

export BWHT

#run server in background
bin/server test/conf/2_1.txt &
export S_PID=$!
# export S_PID

echo -e $BWHT "


STARTING SERVER $S_PID


" $REG
sleep 1

#hide kill termination message using redirection
(bash -c "sleep 30 ; killall -9 spawnclients.sh; echo -e \"$BWHT
    KILLING SERVER and CLIENTS
\" ; kill -2 $S_PID" &) > /dev/null
C_PID=$!

for i in {1..10}; do
    test/spawnclients.sh &
done

wait $C_PID
echo -e $BWHT '

    ...Clients who are trying to connect to the server will die in 10s...

' $REG
sleep 13

echo -e $BWHT '

    Well done!

' $REG
