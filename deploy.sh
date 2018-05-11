#!/bin/bash

output="bind"

lsof ti:60519 | xargs kill -9

while [ -n `./src/server/server | grep -q "bind"` ]
  do	
    echo trying!	
    sleep 2
  done 
  
echo "Ready to run!"
espeak done 
