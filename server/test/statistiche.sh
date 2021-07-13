BWHT="\033[1;37m"
REG="\033[0m"

if [ ! -f log.txt ]; then
    echo "log.txt not found!"
    exit 0
fi


echo "$BWHT 
  SERVER STATS
"

# Successes of each operation
for op in open read readN append write lock unlock close remove
do
  succ=$(grep -o ".*__REQ .*: ${op}.*__Success" log.txt | wc -l)
  echo "Successful $op requests: $succ"
done
succ=$(grep -o ".*__REQ .*: [(open 2|open 3)].*__Success" log.txt | wc -l)
  echo "Successful open-lock requests: $succ"

# Average write size
writeSum=$(grep -o ".*__REQ .*: [(append|write)].*__Success" log.txt | cut -d' ' -f 10 | paste -sd+ | bc)
nWrite=$(grep -o ".*__REQ .*: [(append|write)].*__Success" log.txt | wc -l)

echo "
Average write size: " $((writeSum / nWrite)) "MB"

# Average read size
readSum=$(grep -o ".*Sending to.*" log.txt | cut -d':' -f 2 | paste -sd+ | bc)
nRead=$(grep -o ".*Sending to.*" log.txt | wc -l)

echo "Average read size: " $((readSum / nRead)) "MB"

# Some other stats
statsLine=$(grep -o ".*-STATS:.*" log.txt)
maxNfiles=$(echo $statsLine | cut -d' ' -f 6)
maxSize=$(echo $statsLine | cut -d' ' -f 7)
nEvicted=$(echo $statsLine | cut -d' ' -f 8)
maxNclients=$(echo $statsLine | cut -d' ' -f 9)

echo "
maxNfiles = $maxNfiles
maxSize = $maxSize MB
nEvicted = $nEvicted
maxNclients = $maxNclients
" $REG



