BWHT="\033[1;37m"
REG="\033[0m"

echo "$BWHT 
  SERVER STATS
"

# Successes of each operation
for op in open read readN append write lock unlock close remove
do
  succ=$(grep -o ".*__REQ .*: ${op}.*__Success" log.txt | wc -l)
  echo "Successful $op requests: $succ"
done

# Average write size
writeSum=$(grep -o ".*__REQ .*: [(append|write)].*__Success" log.txt | cut -d' ' -f 10 | paste -sd+ | bc)
nWrite=$(grep -o ".*__REQ .*: [(append|write)].*__Success" log.txt | wc -l)

echo "
Average write size: " $((writeSum / nWrite)) "MB"

# Average read size
readSum=$(grep -o ".*Sending to.*" log.txt | cut -d'_' -f 7 | paste -sd+ | bc)
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



