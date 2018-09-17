#!/bin/sh
a=1
/etc/init.d/minicloudserver restart

while [ $a -lt 12 ]
do
   echo $a
   sleep 1
   a=`expr $a + 1`
done