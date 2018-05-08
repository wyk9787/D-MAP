#! bin/bash/

output="bind"

while [ echo $output | grep -q "bind"]
  do	
    output=`./src/server/server`
  done 
  
  
