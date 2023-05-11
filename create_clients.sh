#!/bin/bash

i=0

while [[ i -lt $1 ]]
do
  i=$(($i+1))
  echo "Welcome $i times"
  ./Client 127.0.0.1 $2 &    
  sleep 0.3
done
