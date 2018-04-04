include $(top_srcdir)/Make_global.am

YACC=@YACC@ -d

CLEANFILES = read_config_yy.c read_config_lex.c

sbin_PROGRAMS = conntrack conntrackd

conntrack_SOURCES = conntrack.c
conntrack_LDADD = ../extensions/libct_proto_tcp.la ../extensions/libct_proto_udp.la ../extensions/libct_proto_udplite.la ../extensions/libct_proto_icmp.la ../extensions/libct_proto_icmpv6.la ../extensions/libct_proto_sctp.la ../extensions/libct_proto_dccp.la ../extensions/libct_proto_gre.la ../extensions/libct_proto_unknown.la
conntrack_LDFLAGS = $(all_libraries) @LIBNETFILTER_CONNTRACK_LIBS@

conntrackd_SOURCES = alarm.c main.c run.c hash.c queue.c rbtree.c \
		    local.c log.c mcast.c udp.c netlink.c vector.c \
		    filter.c fds.c event.c process.c origin.c date.c \
		    cache.c cache_iterators.c \
		    cache_timer.c \
		    sync-mode.c sync-alarm.c sync-ftfw.c sync-notrack.c \
		    traffic_stats.c stats-mode.c \
		    network.c cidr.c \
		    build.c parse.c \
		    channel.c multichannel.c channel_mcast.c channel_udp.c \
		    tcp.c channel_tcp.c \
		    external_cache.c external_inject.c \
		    internal_cache.c internal_bypass.c \ 
		    read_config_yy.y read_config_lex.l

# yacc and lex generate dirty code
read_config_yy.o read_config_lex.o: AM_CFLAGS += -Wno-missing-prototypes -Wno-missing-declarations -Wno-implicit-function-declaration -Wno-nested-externs -Wno-undef -Wno-redundant-decls

conntrackd_LDFLAGS = $(all_libraries) @LIBNETFILTER_CONNTRACK_LIBS@

EXTRA_DIST = read_config_yy.h
