/*
 * Copyright (c) 2007-2008 Silicon Graphics, Inc All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
 * Sunnyvale, CA  94085, or:
 *
 * http://www.sgi.com
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/genetlink.h>
#include <linux/taskstats.h>
#include <netlink/msg.h>
#include "csa.h"
#include "csa_nl.h"

#define SELECT_TIMEOUT 5

int dbg = 0;
#define PRINTF(level, args...) {	\
    if (dbg)				\
	syslog(level, args);		\
}

static int get_family_id(struct nl_sock *nls);
static void print_delayacct(struct taskstats *t);

static int family_id = -1;

/*
 *  Create a raw netlink socket and bind
 */
struct nl_sock *
csa_nl_create(void)
{
    struct nl_sock *nls;

    nls = nl_socket_alloc();
    if (nls) {
	nl_socket_disable_seq_check(nls);
	nl_connect(nls, NETLINK_GENERIC);

	if (family_id == -1) {
	    family_id = get_family_id(nls);
	    if (family_id == -1) {
		nl_close(nls);
		nl_socket_free(nls);
		nls = NULL;
	    }
	}
    }

    return nls;
}

void
csa_nl_cleanup(struct nl_sock *nls)
{
    if (nls != NULL) {
	nl_close(nls);
	nl_socket_free(nls);
    }
}

/*
 *  Probe the controller in genetlink to find the family id
 *  for the TASKSTATS family
 */
static int
get_family_id(struct nl_sock *nls)
{
    struct nlmsghdr req = {
	.nlmsg_type = GENL_ID_CTRL
    };
    struct genlmsghdr genlh = {
	.cmd = CTRL_CMD_GETFAMILY,
	.version = 0x1
    };
    struct nl_msg *msg = NULL;
    fd_set nlss;
    struct timeval tv;
    struct sockaddr_nl peer;
    unsigned char *rmsg = NULL;
    struct ucred *creds;
    struct nlmsghdr *rep;
    int sd, ret, n, len, id = -1;
    struct nlattr *nla;

    msg = nlmsg_inherit(&req);

    /* Add genetlink header */
    nlmsg_append(msg, &genlh, GENL_HDRLEN, 0);

    /* Add attributes */
    ret = nla_put_string(msg, CTRL_ATTR_FAMILY_NAME, TASKSTATS_GENL_NAME);
    if (ret < 0)
	goto cleanup;
    ret = nl_send_auto_complete(nls, msg);
    if (ret < 0)
	goto cleanup;

    FD_ZERO(&nlss);
    sd = csa_nl_fd(nls);
    tv.tv_sec = SELECT_TIMEOUT;
    tv.tv_usec = 0;
    FD_SET(sd, &nlss);
    ret = select(sd + 1, &nlss, 0, 0, &tv);
    if (ret < 0) {
	PRINTF(LOG_ERR, "get_family_id: no response from netlink\n");
	goto cleanup;
    }

    /* go ahead and receive select returns >= 0 */
    n = nl_recv(nls, &peer, &rmsg, &creds);
    if (n <= 0) {
	rmsg = NULL;
	goto cleanup;
    }
    rep = (struct nlmsghdr *) rmsg;
    while (nlmsg_ok(rep, n)) {
	nla = nlmsg_attrdata(rep, GENL_HDRLEN);
	len = nlmsg_attrlen(rep, GENL_HDRLEN);
	if (nla_ok(nla, len)) {
	    nla = nla_find(nla, len, CTRL_ATTR_FAMILY_ID);
	    if (nla) {
		id = nla_get_u16(nla);
		goto cleanup;
	    }
	}
	rep = nlmsg_next(rep, &n);
    }

cleanup:
    if (msg)
	nlmsg_free(msg);
    if (rmsg)
	free(rmsg);

    return id;
}

int
csa_nl_cpumask(struct nl_sock *nls, int cmd_type, char *mask)
{
    struct nlmsghdr req = {
	.nlmsg_type = family_id,
	.nlmsg_pid = getpid()
    };
    struct genlmsghdr genlh = {
	.cmd = TASKSTATS_CMD_GET,
	.version = 0x0
    };
    struct nl_msg *msg = NULL;
    int ret = 0;

    if (mask == NULL)
	return -1;

    msg = nlmsg_inherit(&req);

    /* Add genetlink header */
    nlmsg_append(msg, &genlh, GENL_HDRLEN, 0);

    /* Add attributes */
    ret = nla_put_string(msg, cmd_type, mask);
    if (ret < 0) {
	PRINTF(LOG_ERR, "csa_nl_cpumask: putstring error %d\n", ret);
	goto cleanup;
    }

#if LIBNL_NEW_API
    ret = nl_send_auto_complete(nls, msg);
#else
    ret = nl_send_auto_complete(nls, nlmsg_hdr(msg));
#endif
    if (ret < 0) {
	PRINTF(LOG_ERR, "csa_nl_cpumask: autocomplete error %d\n", ret);
	goto cleanup;
    }

    ret = nl_wait_for_ack(nls);
    if (ret < 0) {
	PRINTF(LOG_ERR, "csa_nl_cpumask: acknowledge error %d\n", ret);
	goto cleanup;
    }

cleanup:
    if (msg)
	nlmsg_free(msg);

    return ret;
}

/*
 *  Get statistics for the specified pid/tgid
 */
void
csa_nl_stats(struct nl_sock *nls, struct taskstats **task)
{
    int n;
    struct sockaddr_nl peer;
    unsigned char *rmsg = NULL;
    struct ucred *creds;
    struct nlmsghdr *rep;
    struct nlattr *nla, *nnla;	/* netlink and next/nested netlink */
    int len, rtid;		/* rtid, returned task id */
    static int delay_acct = -1;
    int done;

    if (delay_acct == -1)
	delay_acct = (getenv("CSA_PRINT_DELAY") == NULL) ? 0 : 1;

    *task = NULL;
    done = 0;

#if LIBNL_NEW_API
    n = nl_recv(nls, &peer, &rmsg, &creds);
#else
    n = nl_recv(nls, &peer, &rmsg);
#endif
    if (n <= 0)
	return;
    rep = (struct nlmsghdr *) rmsg;

    while (nlmsg_ok(rep, n) && !done) {
	nla = nlmsg_attrdata(rep, GENL_HDRLEN);
	len = nlmsg_attrlen(rep, GENL_HDRLEN);
	while (nla_ok(nla, len)) {
	    nnla = nla_find(nla, len, TASKSTATS_TYPE_AGGR_PID);
	    if (nnla) {
		if (delay_acct)
		    printf("type PID\t");
	    }
	    else {
		nnla = nla_find(nla, len, TASKSTATS_TYPE_AGGR_TGID);
		if (delay_acct)
		    printf("type TGID\t");
	    }

	    /* It is a nested attribute, move down */
	    if (nnla) {
		nnla = nla_data(nnla);
		rtid = nla_get_u32(nnla);
		if (delay_acct)
		    printf(" %d\n", rtid);
		nnla = nla_find(nnla, len, TASKSTATS_TYPE_STATS);
		*task = (struct taskstats *) malloc(sizeof(struct taskstats));
		memcpy(*task, nla_data(nnla), sizeof(struct taskstats));
		if (delay_acct)
		    print_delayacct(*task);
		done = 1;
		break;
	    } else {
		PRINTF(LOG_ERR, "csa_nl_stats: could not find any data\n");
		break;
	    }
	    nla = nla_next(nla, &len);
	}

	if (!nla_ok(nla, len))
	    break;

	rep = nlmsg_next(rep, &n);
    }

    if (rmsg)
	free(rmsg);
}

static void
print_delayacct(struct taskstats *t)
{
    printf("\n\nCPU   %15s%15s%15s%15s\n"
	   "      %15llu%15llu%15llu%15llu\n"             
	   "IO    %15s%15s\n"
	   "      %15llu%15llu\n"
	   "MEM   %15s%15s\n"
	   "      %15llu%15llu\n\n",
	   "count", "real total", "virtual total", "delay total",
	   t->cpu_count, t->cpu_run_real_total,
	   t->cpu_run_virtual_total, t->cpu_delay_total,
	   "count", "delay total",
	   t->blkio_count, t->blkio_delay_total,
	   "count", "delay total",
	   t->swapin_count, t->swapin_delay_total);
    fflush(stdout);
}

/*
 *  Return the file descriptor for the netlink socket
 */
int
csa_nl_fd(struct nl_sock *nls)
{
    return nl_socket_get_fd(nls);
}
