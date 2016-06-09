#! /bin/ksh
#
#	(C) COPYRIGHT 2000, 2006 SILICON GRAPHICS, INC.
#	UNPUBLISHED PROPRIETARY INFORMATION.
#	ALL RIGHTS RESERVED.
#
###############################################################
#
#     OS Testing - Silicon Graphics, Inc.
#
#     TEST IDENTIFIER   : csacom_cpu
#
#     TEST TITLE        : CPU Data From csacom
#
#     PARENT DOCUMENT   : csacom.tds
#
#     TEST CASE TOTAL   : 3
#
#     AUTHOR            : Janet Clegg
#
#     CO-PILOT(s)       : n/a
#
#     DATE STARTED      : 03/2000
#
#     TEST CASES
#	Verify the following fields on the csacom -t command:
#       1.) Elapsed Sconds (Real)
#       2.) Sys CPU Seconds
#       3.) User CPU Seconds
#
#
#     INPUT SPECIFICATIONS (optional)
#       Description of all command line options or arguments that this
#       test will understand.
#
#	n/a
#
#
#     OUTPUT SPECIFICATIONS (optional)
#       Description of the format of this test program's output.
#
#	o Test output is the standard cuts output.
#
#	o The following files will be created in the temporary test directory.
#		cpu.out - output from the cpubound command ($CPUout)
#		csacom.out - output from the csacom command ($Csacomout)
#		Tsetup.csa - info on original CSA status ($CSAstatus)
#		Info - collected input for tst_res function ($Info)
#		Tsetup.out - file with info on test setup ($Setup)
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the -t option on the csadom command shows the correct 
#		values for REAL (elapsed) time, user CPU time, and system 
#		CPU time.
#
#
#     SPECIAL REQUIREMENTS
#       A description of any non-standard requirements/needs that will
#       help a user in executing and or maintaining this test program,
#       and a brief explanation of the reasons behind the
#       requirement(s).
#
#	Needs the following resources:
#
#       CSA - This test may change the CSA configuration, therefore
#               needs exclusive use of the resource.
#
#
#     UPDATE HISTORY
#       This should contain the description, author, and date of any
#       "interesting" modifications (i.e. info should helpful in
#       maintaining/enhancing this test program).
#       user     date  description
#       ----------------------------------------------------------------
#       janetc   03/2000 Initial Version
#       janetc   07/2000 Change analysis of time to include tenths of
#                        a second, and allow a variance of a second.
#       janetc   11/2000 Modify for Linux
#
#     BUGS/LIMITATIONS
#       All known bugs/limitations should be filled in here.
#
#
###############################################################


###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csacom_cpu"		# test case identifier
TST_TOTAL=3			# total number of test cases

Cpwd=""				# current working directory for this script

typeset int TCnt=0		# test item count, used for index
Info="Info"			# file with info for test analysis
Setup="Tsetup.out"		# file with info on test setup
CSAstatus="Tsetup.csa"		# file with info on original CSA status

CPUout="cpu.out"		# output of cpubound process

Csacomout="csacom.out"		# output of "csacom -t" command
Csacomcmd=""			# full path to command

Stime=0				# time the test started, used with csacom
Etime=0				# time the test ended, used with csacom


###############################################################################
#
# FUNCTIONS
#
###############################################################################

###############################################################################
#
# cleanup - clean up after running the test and exit
#
# INPUT:   n/a
#
# OUTPUT:  n/a
#
# CUTS OUTPUT: n/a
#
# RETURN:  n/a
#
###############################################################################
cleanup() {

	restore_csa
	tst_rmdir
	tst_exit

}

###############################################################################
#
# verify_conf - verify correct installation and configuration.  Creates the
#		file ${CSAstatus} which restore_conf() will use to restore
#		the original configuration.
#	
# INPUT:   n/a  
#
# OUTPUT:  n/a
#
# CUTS OUTPUT: see below
#
# RETURN:  n/a
#
# EXITS with:
#      	TBROK if CSA is installed but can't be turned on.
#      	TBROK if CSA is installed but io or memory acct can't be turned on
#      	TBROK if CSA is on but thresholds can't be turned off
#
###############################################################################
verify_conf() {

	# if CSA is configured off, turn it on
	check_csa csa On
	ret=$?
	if [[ $ret != 0 ]] ; then
		tst_brkloop TBROK cleanup "Failed to turn on CSA accounting" \
			$CSAsetup
		tst_exit
	fi

	check_csa mem On
	ret=$?
	if [[ $ret != 0 ]] ; then
		tst_brkloop TBROK cleanup "Failed to turn on CSA accounting" \
			$CSAsetup
		tst_exit
	fi
	check_csa io On
	ret=$?
	if [[ $ret != 0 ]] ; then
		tst_brkloop TBROK cleanup "Failed to turn on CSA accounting" \
			$CSAsetup
		tst_exit
	fi

	# verify thresholds are turned off; they will prevent smaller records
	# from being written
	check_csa memt Off
	ret=$?
	if [[ $ret != 0 ]] ; then
		tst_brkloop TBROK cleanup "Failed to turn on CSA accounting" \
			$CSAsetup
		tst_exit
	fi
	check_csa time Off
	ret=$?
	if [[ $ret != 0 ]] ; then
		tst_brkloop TBROK cleanup "Failed to turn on CSA accounting" \
			$CSAsetup
		tst_exit
	fi

}


###############################################################################
#
# setup - run the test program cpubound & get the csacom report
#
# INPUT:  n/a
#
# OUTPUT: the file $CPUout with the output of the cpubound command
#	  the file $Csacomout with the csacom report
#
# CUTS OUTPUT:
#	TWARN if cpubound returns with an error
#
# RETURN: n/a
#
# EXITS with:
#	TBROK if csacom returns with an error
#
###############################################################################
setup() {

	tty=`tty | tr "/" " " | awk '{ print $NF }'`
	set -A day `$Csacomcmd -W | grep "last record" |cut -f1 -d:`
	Today=${day[1]}

	# run the testprogram.  We're allowing a rounding error of
	# a second, so make these times a larger number
	tst_res TINFO "Running cpubound task for 120 seconds"
	Stime="D${Today}:"`date +%H:%M:%S`
	date > $CPUout
	sleep 1
	cpubound -a 120 >> $CPUout
	status=$?
	sleep 1
	Etime="D${Today}:"`date +%H:%M:%S`
	date >> $CPUout
	if [[ $status != 0 ]] ; then
		tst_res TWARN "cpubound returned $status" $CPUout
	fi

	Mycmd="$Csacomcmd -tu $LOGNAME -n cpubound -S $Stime -E $Etime"
	if [[ $OS != Linux ]] ; then
		Mycmd="$Mycmd -l $tty"
	fi
	$Mycmd > $Csacomout 2>&1
	ret=$?
	echo "\ncsacom command is: \n$Mycmd" >> $Csacomout 2>&1
	if [[ $ret != 0 ]] ; then
		tst_brk TBROK cleanup "csacom returned $ret" $Csacomout
	fi

}

###############################################################################
#
# runtest - compare time from cpubound output with time from csacom output.  
#           multiple the number of seconds by 60 and add the tenths of
#           a second.  If the difference is less than 60, the difference
#           is less than a second.  Do it this way since ksh doesn't have
#           floating point numbers, and cpubound uses clock ticks which
#           introduces a rounding error.
#
# INPUT
#	the file ${CPUout}
#	the file ${Csacomout}
#       $1 the test case name.  Must match output of cpubound:
#               Process wall clock time
#               User space CPU time
#               System CPU time
#       $2 the number of the field to use in the ja output.  The command
#               name is in the 0th field, "Started At" is the 1st field,
#               "Elapsed Seconds" the 2d...
#
# OUTPUT:  n/a
#
# CUTS OUTPUT: 
#	BROK if input files don't exist 
#	BROK if $1 and $2 aren't passed in
#	BROK if $1 (the test case) not found in output of cpubound
#	PASS if cpubound and csacom report the same time
#	FAIL if not
#
# RETURN:  n/a
#
#
###############################################################################
runtest() {

	if [[ ! -f ${CPUout} ]] ; then
		tst_res TBROK "File with cpubound output does not exist"
		return
	fi
	if [[ ! -f ${Csacomout} ]] ; then
		tst_res TBROK "File with csacom report does not exist"
		return
	fi
	if (( $# < 2 )); then
		tst_res TBROK "Insufficient parameters passed to runtest()"
		return
	fi

	# this must be the message as displayed by cpubound
	tcase="$1"
	fnum=$2

	# cputime		# value from cpubound output
	# rpttime		# value from ja output
	# cpusecs, rptsecs	# integer portion of the times
	# cpufrac, rptfrac	# tenths of a second
	# cpu, rpt		# seconds times 60 plus tenths of a sec
				# (a diff of 30 is a half second)

	# check the output from the cpubound process
	num=`grep -c "$tcase" $CPUout`
	if (( num == 0 )) ; then
		tst_res TBROK "invalid test case: $tcase"
		return
	fi

	cat $CPUout > $Info
	cputime=`grep "$tcase" $CPUout|cut -f2 -d=|cut -f2 -d' '`
	echo "cpubound reported $tcase time of $cputime" >> $Info
	cpusecs=`echo $cputime|cut -f1 -d.`
	cpufrac=`echo $cputime |cut -f2 -d. |cut -c 1`
	(( cpu = (cpusecs*10) + cpufrac ))

	# check the output from the csacom command
	cat $Csacomout >> $Info
	set -A line `grep cpubound $Csacomout`
	rpttime=`echo ${line[$fnum]}`
	echo "csacom reported $tcase time of $rpttime" >> $Info
	rptsecs=`echo $rpttime|cut -f1 -d.`
	rptfrac=`echo $rpttime |cut -f2 -d. |cut -c 1`
	(( rpt = (rptsecs*10) + rptfrac ))

	(( diff = $rpt - $cpu )) 
	echo "Difference is $diff (in tenths of a second)\n" >> $Info
	# allow a rounding error of a half second either direction
	if (( $diff >= -5 && $diff <= 5 )) ; then
		tst_res TPASS "csacom -t $tcase time"
	else
		tst_res TFAIL "csacom -t $tcase time" $Info
	fi

}

###############################################################################
#
# End of routines, start of mainline code
#
###############################################################################

# SETUP
# Defines functions tst_res, tst_brk, tst_brkloop, tst_exit, tst_rmdir
# and variable Tst_count.  Creates a tmp dir and cd's into it.
. tst_setup
(( Tst_lpstart = 0 ))
(( Tst_lptotal = $TST_TOTAL ))

# CSA SETUP
# Defines functions: verify_csa(), restore_csa(), check_csa(),
# and variables: CSAsetup, CSAstatus, CSAadmin, CSAuser, Today
# and runs verify_csa
. csa_setup

Csacomcmd=${CSAuser}/csacom	# full path to command

Cpwd=`pwd`	# save the current working directory for use by $Script

verify_conf
setup


# 1.) verify Real (elapsed) Seconds
if [[ $OS = Linux ]] ; then
	runtest "wall clock" 4
else
	runtest "wall clock" 5
fi

# 2.) verify Sys CPU Seconds
if [[ $OS = Linux ]] ; then
	runtest "System CPU" 5
else
	runtest "System CPU" 6
fi

# 3.) verify User CPU Seconds
if [[ $OS = Linux ]] ; then
	runtest "User space CPU" 6
else
	runtest "User space CPU" 7
fi

cleanup
