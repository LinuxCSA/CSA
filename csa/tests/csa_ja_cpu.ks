#! /bin/ksh
#
#
#	(C) COPYRIGHT SILICON GRAPHICS, INC.
#	UNPUBLISHED PROPRIETARY INFORMATION.
#	ALL RIGHTS RESERVED.
#
###############################################################
#
#     OS Testing - Silicon Graphics, Inc.
#
#     TEST IDENTIFIER   : csa_ja_cpu
#
#     TEST TITLE        : CPU Data From ja and csacom
#
#     PARENT DOCUMENT   : csaja.tds
#
#     TEST CASE TOTAL   : 6
#
#     AUTHOR            : Janet Clegg
#
#     DATE STARTED      : 01/2000
#
#     TEST CASES
#	Verify the following fields on the ja -c command:
#       1.) Elapsed Sconds
#       3.) User CPU Seconds
#       5.) Sys CPU Seconds
#
#	Verify the following fields on the ja -cl command:
#       2.) Elapsed Sconds
#       4.) User CPU Seconds
#       6.) Sys CPU Seconds
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
#		Info - collected input for tst_res function ($Info)
#		ja-c.out - output of ja -c command ($JaoutC)
#		ja-cl.out - output of ja -cl command ($JaoutCL)
#		ja.init - output from initial ja command
#		jacct.$$ - job accounting file ($Jafile)
#		Tsetup.out - file with info on test setup ($Setup)
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the -c and -cl options on the ja command show the correct 
#		values for Elapsed time, user CPU time, and system CPU time.
#
#
#     SPECIAL REQUIREMENTS
#       A description of any non-standard requirements/needs that will
#       help a user in executing and or maintaining this test program,
#       and a brief explanation of the reasons behind the
#       requirement(s).
#
#	n/a
#
#
#     UPDATE HISTORY
#       This should contain the description, author, and date of any
#       "interesting" modifications (i.e. info should helpful in
#       maintaining/enhancing this test program).
#       user     date  description
#       ----------------------------------------------------------------
#       janetc   01/2000 Initial Version
#	jlan	 02/2006 Ported to csa-test-SGINoShip rpm
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
TCID="csa_ja_cpu"		# test case identifier
TST_TOTAL=6			# total number of test cases

Cpwd=""				# current working directory for this script

typeset int TCnt=0		# test item count, used for index
Info="Info"			# file with info for test analysis
Setup="Tsetup.out"		# file with info on test setup

CPUout="cpu.out"		# output of the cpubound process
Jafile=""			# ja pacct file
JaoutC="ja-c.out"		# output of ja -c command
JaoutCL="ja-cl.out"		# output of ja -cl command

###############################################################################
#
# FUNCTIONS
#
###############################################################################

###############################################################################
#
# clean up after running the test
#
###############################################################################
cleanup() {

	tst_rmdir
	tst_exit

}


###############################################################################
#
# setup 
#	- turn on job accounting
#	- run the test program cpubound
#	- get the ja -c report 
#	- get the ja -cl report 
#
###############################################################################
setup() {

	# turn on job accounting 
	$Cmdloc $Jafile > ja.init 2>&1	
	status=$?
	if [[ $status != 0 ]] ; then
		tst_brk TBROK cleanup "Failed to enable ja, returned $status" \
			ja.init
	fi

	# run the cpubound program
	tst_res TINFO "Running cpubound task for 120 seconds"
	cpubound -a 120 > $CPUout
	status=$?
	if [[ $status != 0 ]] ; then
		tst_res TWARN "cpubound returned $status" $CPUout
	fi

	# get the ja -c report
	$Cmdloc -c $Jafile > $JaoutC 2>&1
	status=$?
	if [[ $status != 0 ]] ; then
		tst_brk TBROK cleanup "ja -c command returned $status" $JaoutC
	fi

	# get the ja -cl report
	$Cmdloc -cl $Jafile > $JaoutCL 2>&1
	status=$?
	if [[ $status != 0 ]] ; then
		tst_brk TBROK cleanup "ja -cl command returned $status" $JaoutCL
	fi


}

###############################################################################
#
# runtest - compare time from cpubound output with time from ja -c 
#		and -cl output.  
#		Use only the integer portion of the number (which is likely 
#		to be 0 for system time).
# INPUT
#	$1 the test case name.  Must match output of cpubound:
#		Process wall clock time
#		User space CPU time
#		System CPU time
#	$2 the number of the field to use in the ja output.  The command
#		name is in the 0th field, "Started At" is the 1st field, 
#		"Elapsed Seconds" the 2d...
#	$3 the name of the file containing the ja output
#	$4 the message to use on the test result line
#		
#
###############################################################################
runtest() {

	if (( $# < 4 )); then
		tst_res TBROK "Insufficient parameters passed to runtest()"
		return
	fi

	# this must be the message as displayed by cpubound
	tcase="$1"
	fnum=$2
	ofile=$3
	msg="$4"

	# cputime		# value from cpubound output
	# jatime		# value from ja output
	# cpusecs, jasecs	# integer portion of the times
	# cpufrac, jafrac	# tenths of a second
	# cpu, ja		# seconds times 10 plus tenths of a sec
				# a half sec is .5 so stay in base 10 not 60

	# check the output from the cpubound process
	num=`grep -c "$tcase" $CPUout`
	if (( num == 0 )) ; then
		tst_res TBROK "invalid test case: $tcase"
		return
	fi

	cat $CPUout > $Info
	cputime=`grep "$tcase" $CPUout|cut -f2 -d=|cut -f2 -d' '`
	cpusecs=`echo $cputime |cut -f1 -d.`
	cpufrac=`echo $cputime |cut -f2 -d. |cut -c 1`
	(( cpu = (cpusecs*10) + cpufrac ))

	# check the output from ja in the indicated file
	head -8 $ofile >> $Info
	grep cpubound $ofile >> $Info
	set -A line `grep cpubound $ofile`
	jatime=`echo ${line[$fnum]}`

	jasecs=`echo $jatime |cut -f1 -d.`
	jafrac=`echo $jatime |cut -f2 -d. |cut -c 1`
	(( ja = (jasecs*10) + jafrac ))

	(( diff = $ja - $cpu ))
	print "cpubound reported $tcase time of $cputime" >> $Info
	print "      ja reported $tcase time of $jatime" >> $Info
	print "Difference is $diff (in tenths of a second)\n" >> $Info
	# allow a rounding error of a half second either direction
	if (( $diff >= -5 && $diff <= 5 )) ; then
		tst_res TPASS "$msg $tcase time"
	else
		tst_res TFAIL "$msg $tcase time" $Info
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
# Verifies CSA is installed and enabled
. csa_setup

Cmdloc=${CSAuser}/ja		# full path to command

Cpwd=`pwd`	# save the current working directory for use by $Script
Jafile="$Cpwd/jacct.$$"

setup

# 1.) -c to verify Elapsed Sconds
runtest "wall clock" 2 $JaoutC "ja -c"

# 2.) -cl to verify Elapsed Sconds
runtest "wall clock" 2 $JaoutCL "ja -cl"

# 3.) -c to verify User CPU Seconds
runtest "User space CPU" 3 $JaoutC "ja -c"

# 4.) -cl to verify User CPU Seconds
runtest "User space CPU" 3 $JaoutCL "ja -cl"
	
# 5.) -c to verify Sys CPU Seconds
runtest "System CPU" 4 $JaoutC "ja -c"

# 6.) -cl to verify Sys CPU Seconds
runtest "System CPU" 4 $JaoutCL "ja -cl"

cleanup
