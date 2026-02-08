./app 3000 3001 > out &
pid1=$!

./app 3001 3000 > out2 &
pid2=$!

sleep 20
kill $pid1 $pid2
