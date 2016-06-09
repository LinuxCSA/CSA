/*
 * Copyright (c) 2007 Silicon Graphics, Inc All Rights Reserved.
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

#ifndef _CSA_CTL_H
#define _CSA_CTL_H

#include "csa.h"

struct csa_ctl_hdr {
    ac_request req;
    int err;
    int size;
    int update;
};

union csa_ctl_data {
    struct actstat stat;
    struct actctl ctl;
    struct actwra wra;
};

#define CSA_CTL_SOCKET "/var/run/csad/csa-ctl-socket"

#endif /* _CSA_CTL_H */
