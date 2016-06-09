#! /bin/ksh
#
# Copyright (c) 2006-2007 Silicon Graphics, Inc All Rights Reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it would be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
#
# Contact information:  Silicon Graphics, Inc., 1140 East Arques Avenue,
# Sunnyvale, CA  94085, or:
#
# http://www.sgi.com
#

#########################################################################
#
# Name: csa-test.ks
#
# This test script invokes these tests:
#   binary:	csatest_api
#   scripts:	/usr/sbin/csarun
#		csa_ja-n.ks
#		csa_ja-o.ks
#		csa_ja-s.ks
#		csa_ja-t.ks
#		csa_ja_cpu.ks
#		csacom_io.ks
#		csacom_cpu.ks
#
# Please see README for test environment requirements.
#
#########################################################################


CSATESTD=/var/csa/tests
runcmd -u root $CSATESTD/csatest_api
if (( $? != 0 )); then
	echo 'Test "csatest_api" failed. Exit.'
	exit
fi

runcmd -u root /usr/sbin/csarun
if (( $? != 0 )); then
        echo 'Test "csarun" failed. Exit.'
	exit
else
	echo 'Done "csarun" testing:            SUCCESS'
fi
				
$CSATESTD/csa_ja-n.ks
if (( $? != 0 )); then
	echo 'Test "csa_ja-n" failed. Exit.'
	exit
fi

$CSATESTD/csa_ja-o.ks
if (( $? != 0 )); then
	echo 'Test "csa_ja-o" failed. Exit.'
	exit
fi

$CSATESTD/csa_ja-s.ks
if (( $? != 0 )); then
	echo 'Test "csa_ja-s" failed. Exit.'
	exit
fi

$CSATESTD/csa_ja-t.ks
if (( $? != 0 )); then
	echo 'Test "csa_ja-t" failed. Exit.'
	exit
fi

$CSATESTD/csa_ja_cpu.ks
if (( $? != 0 )); then
	echo 'Test "csa_ja_cpu" failed. Exit.'
	exit
fi

$CSATESTD/csacom_io.ks
if (( $? != 0 )); then
	echo 'Test "csacom_io" failed. Exit.'
	exit
fi

$CSATESTD/csacom_cpu.ks
if (( $? != 0 )); then
	echo 'Test "csacom_cpu" failed. Exit.'
	exit
fi


echo CSA Tests Completed Successfully!
