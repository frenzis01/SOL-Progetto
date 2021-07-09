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
bash -c "sleep 30 ; killall -9 spawnclients.sh>/dev/null; echo -e \"$BWHT
    KILLING SERVER
\" ; kill -2 $S_PID" &
C_PID=$!

for i in {1..10}; do
    test/spawnclients.sh &
done

wait $C_PID
sleep 0.1

echo -e $BWHT '

    Well done!

' $REG
