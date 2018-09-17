#!/bin/sh
a=1
/etc/init.d/minicloudserver restart

while [ $a -lt 12 ]
do
   discreteEventSimulator -l 0.5 -m 3 -s 2 -E 5000 -q $a -p /ubc/Mehdi/queue$a.csv
   sleep 1
   /etc/init.d/minicloudserver restart
   sleep 1

   a=`expr $a + 1`
done
