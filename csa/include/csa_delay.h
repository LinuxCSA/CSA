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

#ifndef _CSA_DELAY_H
#define _CSA_DELAY_H

struct acctdelay {
    /* Delay accounting fields start
     *
     * All values, until comment "Delay accounting fields end" are
     * available only if delay accounting is enabled, even though the last
     * few fields are not delays
     *
     * xxx_count is the number of delay values recorded
     * xxx_delay_total is the corresponding cumulative delay in nanoseconds
     *
     * xxx_delay_total wraps around to zero on overflow
     * xxx_count incremented regardless of overflow
     */
    struct achead ac_hdr;

    /* Delay waiting for cpu, while runnable
     * count, delay_total NOT updated atomically
     */
    uint64_t ac_cpu_count;
    uint64_t ac_cpu_delay_total;

    /* Following four fields atomically updated using task->delays->lock */

    /* Delay waiting for synchronous block I/O to complete
     * does not account for delays in I/O submission
     */
    uint64_t ac_blkio_count;
    uint64_t ac_blkio_delay_total;

    /* Delay waiting for page fault I/O (swap in only) */
    uint64_t ac_swapin_count;
    uint64_t ac_swapin_delay_total;

    /* cpu "wall-clock" running time
     * On some architectures, value will adjust for cpu time stolen
     * from the kernel in involuntary waits due to virtualization.
     * Value is cumulative, in nanoseconds, without a corresponding count
     * and wraps around to zero silently on overflow
     */
    uint64_t ac_cpu_run_real_total;

    /* cpu "virtual" running time
     * Uses time intervals seen by the kernel i.e. no adjustment
     * for kernel's involuntary waits due to virtualization.
     * Value is cumulative, in nanoseconds, without a corresponding count
     * and wraps around to zero silently on overflow
     */
    uint64_t ac_cpu_run_virtual_total;
    /* Delay accounting fields end */
};

#endif /* _CSA_DELAY_H */
