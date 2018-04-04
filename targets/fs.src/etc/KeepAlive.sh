#!/bin/sh

gateway=`ip route | grep default | awk '{ print ""$3""}'`
interface=`cat /proc/net/route | awk '{ print ""$1""}' | sed -n '$p'`

if [ "$interface" != "eth4.1" ] && [ "$interface" != "atm0" ] && [ "$interface" != "ptm0.1" ]; then
	return
else
	echo "arping "$gateway" -I "$interface" -f -c 2 -w 3" | sh -x
fi
