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

#ifndef _CSA_NL_H
#define _CSA_NL_H

#include <netlink/netlink.h>

struct nl_sock *csa_nl_create(void);
void csa_nl_cleanup(struct nl_sock *nls);
int csa_nl_cpumask(struct nl_sock *nls, int cmd_type, char *mask);
void csa_nl_stats(struct nl_sock *nls, struct taskstats **task);
int csa_nl_fd(struct nl_sock *nls);

#define csa_nl_cpumask_on(nls, mask) \
    csa_nl_cpumask(nls, TASKSTATS_CMD_ATTR_REGISTER_CPUMASK, mask)
#define csa_nl_cpumask_off(nls, mask) \
    csa_nl_cpumask(nls, TASKSTATS_CMD_ATTR_DEREGISTER_CPUMASK, mask)

#endif /* _CSA_NL_H */
