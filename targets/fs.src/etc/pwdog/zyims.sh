#!/bin/sh
#echo "zyims.sh ENTER"
vapidtmp=`pidof -s voiceApp`
if [ "$vapidtmp" != "" ] ; then
	echo $vapidtmp
	kill $vapidtmp
	sleep 5
fi

mmpidtmp=`pidof -s mm.exe`
if [ "$mmpidtmp" != "" ] ; then
	echo $mmpidtmp
	kill $mmpidtmp
fi

icfpidtmp=`pidof -s icf.exe`
if [ "$icfpidtmp" != "" ] ; then
	echo $icfpidtmp
	kill $icfpidtmp
fi

# Media Management
 test -e /bin/mm.exe && /bin/mm.exe &
 test -e /bin/mm.exe && sleep 5
 test -e /bin/icf.exe && /bin/icf.exe & 

#echo "zyims.sh EXIT"

exit	
