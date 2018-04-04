#!/bin/sh
# 
# (C) 2009 by Pablo Neira Ayuso <pablo@netfilter.org>
#
# This software may be used and distributed according to the terms
# of the GNU General Public License, incorporated herein by reference.
#

#
# This is the node ID, must be >= 1 and <= 2. You have to CHANGE IT according
# to the number of node where you are.
#
NODEID=1

CONNTRACKD_BIN="/usr/sbin/conntrackd"
CONNTRACKD_LOCK="/var/lock/conntrack.lock"
CONNTRACKD_CONFIG="/etc/conntrackd/conntrackd.conf"

ETHER1="eth1"
ETHER2="eth2"

state_primary()
{
    #
    # commit the external cache into the kernel table
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -c
    if [ $? -eq 1 ]
    then
        logger "ERROR: failed to invoke conntrackd -c"
    fi

    #
    # flush the internal and the external caches
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -f
    if [ $? -eq 1 ]
    then
    	logger "ERROR: failed to invoke conntrackd -f"
    fi

    #
    # resynchronize my internal cache to the kernel table
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -R
    if [ $? -eq 1 ]
    then
    	logger "ERROR: failed to invoke conntrackd -R"
    fi

    #
    # send a bulk update to backups 
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -B
    if [ $? -eq 1 ]
    then
        logger "ERROR: failed to invoke conntrackd -B"
    fi
}

state_backup() {
    #
    # is conntrackd running? request some statistics to check it
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -s
    if [ $? -eq 1 ]
    then
        #
	# something's wrong, do we have a lock file?
	#
    	if [ -f $CONNTRACKD_LOCK ]
	then
	    logger "WARNING: conntrackd was not cleanly stopped."
	    logger "If you suspect that it has crashed:"
	    logger "1) Enable coredumps"
	    logger "2) Try to reproduce the problem"
	    logger "3) Post the coredump to netfilter-devel@vger.kernel.org"
	    rm -f $CONNTRACKD_LOCK
	fi
	$CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -d
	if [ $? -eq 1 ]
	then
	    logger "ERROR: cannot launch conntrackd"
	    exit 1
	fi
    fi
    #
    # shorten kernel conntrack timers to remove the zombie entries.
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -t
    if [ $? -eq 1 ]
    then
    	logger "ERROR: failed to invoke conntrackd -t"
    fi

    #
    # request resynchronization with master firewall replica (if any)
    # Note: this does nothing in the alarm approach.
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -n
    if [ $? -eq 1 ]
    then
    	logger "ERROR: failed to invoke conntrackd -n"
    fi
}

state_fault() {
    #
    # shorten kernel conntrack timers to remove the zombie entries.
    #
    $CONNTRACKD_BIN -C $CONNTRACKD_CONFIG -t
    if [ $? -eq 1 ]
    then
    	logger "ERROR: failed to invoke conntrackd -t"
    fi
}

iptables_add_cluster_rule() {
    iptables -I CLUSTERDEV1 -t mangle -m cluster \
    --cluster-total-nodes 2 --cluster-local-node $1 \
    --cluster-hash-seed 0xdeadbeed -j MARK --set-mark 0xffff
    iptables -I CLUSTERDEV2 -t mangle -m cluster \
    --cluster-total-nodes 2 --cluster-local-node $1 \
    --cluster-hash-seed 0xdeadbeed -j MARK --set-mark 0xffff
}

iptables_del_cluster_rule() {
    iptables -D CLUSTERDEV1 -t mangle -m cluster \
    --cluster-total-nodes 2 --cluster-local-node $1 \
    --cluster-hash-seed 0xdeadbeed -j MARK --set-mark 0xffff
    iptables -D CLUSTERDEV2 -t mangle -m cluster \
    --cluster-total-nodes 2 --cluster-local-node $1 \
    --cluster-hash-seed 0xdeadbeed -j MARK --set-mark 0xffff
}

iptables_start_cluster_rule() {
    iptables -N CLUSTERDEV1 -t mangle
    iptables -N CLUSTERDEV2 -t mangle
    iptables_add_cluster_rule $1
    iptables -A CLUSTERDEV1 -t mangle -m mark ! --mark 0xffff -j DROP
    iptables -A CLUSTERDEV2 -t mangle -m mark ! --mark 0xffff -j DROP
    iptables -I PREROUTING -t mangle -p vrrp -j ACCEPT
    iptables -A PREROUTING -t mangle -i $ETHER1 -j CLUSTERDEV1
    iptables -A PREROUTING -t mangle -i $ETHER2 -j CLUSTERDEV2
}

iptables_stop_cluster_rule() {
    iptables -D PREROUTING -t mangle -i $ETHER1 -j CLUSTERDEV1
    iptables -D PREROUTING -t mangle -i $ETHER2 -j CLUSTERDEV2
    iptables -D PREROUTING -t mangle -p vrrp -j ACCEPT 
    iptables -F CLUSTERDEV1 -t mangle
    iptables -F CLUSTERDEV2 -t mangle
    iptables -X CLUSTERDEV1 -t mangle
    iptables -X CLUSTERDEV2 -t mangle
}

# this can be called without options
case "$1" in
  start)
    iptables_start_cluster_rule $NODEID
    exit 0
    ;;
  stop)
    iptables_stop_cluster_rule $NODEID
    exit 0
    ;;
esac

if [ $# -ne 2 ]
then
    logger "ERROR: missing arguments"
    echo "Usage: $0 {primary|backup|fault|start|stop} {nodeid}"
    exit 1
fi

case "$1" in
  primary)
    #
    # We are entering the MASTER state, it may be for G1 or G2, but we
    # commit the external cache anyway.
    #
    state_primary
    iptables_add_cluster_rule $2
    ;;
  backup)
    #
    # We are entering the BACKUP state. We can enter it from G1 or G2.
    # Assuming that we are node 1 and that we have entered BACKUP in G2,
    # this means that node 2 has come back to life. In that case, skip
    # state_backup because we are still in MASTER state for G1.
    #
    if [ $NODEID -eq $2 ]
    then
       state_backup
    fi
    iptables_del_cluster_rule $2
    ;;
  fault)
    #
    # We are entering the FAULT state, something bad is happening to us.
    #
    state_fault
    iptables_del_cluster_rule $2
    ;;
  *)
    logger "ERROR: unknown state transition"
    echo "Usage: $0 {primary|backup|fault|start|stop} {nodeid}"
    exit 1
    ;;
esac

exit 0
