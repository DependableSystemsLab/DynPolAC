#!/bin/sh
a=1
/etc/init.d/minicloudserver restart

while [ $a -lt 11 ]
do
   if [ $a -lt 7 ]
   then
      discreteEventSimulator -l 0.5 -m 4 -s 2 -E 5000 -n $a -p /ubc/Mehdi/rule$a.csv 
   fi

   /etc/init.d/minicloudserver restart
   discreteEventSimulator -l 0.5 -E 5000 -f $a -p /ubc/Mehdi/lambda$a.csv
   /etc/init.d/minicloudserver restart

   a=`expr $a + 1`
done
