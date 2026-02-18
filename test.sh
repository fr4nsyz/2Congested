./build/peer 3000 3001 > out &
pid1=$!

./build/peer 3001 3000 > out2 &
pid2=$!
