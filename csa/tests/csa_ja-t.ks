#! /bin/ksh
#
#
#	(C) COPYRIGHT SILICON GRAPHICS, INC.
#	UNPUBLISHED PROPRIETARY INFORMATION.
#	ALL RIGHTS RESERVED.
#
###############################################################################
#
#     OS Testing - Silicon Graphics, Inc.
#
#     TEST IDENTIFIER   : csa_ja-t
#
#     TEST TITLE        : ja -t command 
#
#     PARENT DOCUMENT   : n/a
#
#     TEST CASE TOTAL   : 2
#
#     AUTHOR            : Janet Clegg
#
#     DATE STARTED      : 08/2000
#
#     TEST CASES
#	Verify the following on the ja -t command:
#       1.) the default jacct file is removed with ja -t command
#       2.) a user-specified file is not removed with ja -t command
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
#		Info - collected input for tst_res function ($Info)
#		err.out - messages from stderr ($Err)
#		ja.init - output from initial ja command ($Init)
#		ja.summary - output from ja -s command
#		ja.t - output from ja -t command ($JaEnd)
#		jacct.$$ - job accounting file ($Jafile)
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the -t option on the ja command deletes the default
#		jacct file but not a user-specified jacct file
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
#       janetc   08/2000 Initial Version
#	jlan	 02/2006 Ported to csa-test-SGINoShip rpm
#
#     BUGS/LIMITATIONS
#       All known bugs/limitations should be filled in here.
#
#
#
###############################################################################

###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csa_ja-t"			# test case identifier
TST_TOTAL=2			# total number of test cases

Cpwd=""				# current working directory for this script
OSname=`uname`

Jafile=""			# ja pacct file
Info="Info"			# file with info for test analysis
Err="err.out"			# any output to stderr
Init="ja.init"
JaEnd="ja.t"


###############################################################################
#
# FUNCTIONS
#
###############################################################################

###############################################################################
#
# cleanup - clean up after running the test
#
# INPUT:	n/a
#
# OUTPUT:	n/a
#
# CUTS OUTPUT:	n/a
#
# RETURN:	n/a
#
###############################################################################
cleanup() {

	tst_rmdir
	tst_exit

}

###############################################################################
#
# rm_def_file - verify default file is removed by ja -t command
#
# INPUT: the following variables are expected to be set:
#	Cmdloc - full path to the ja command
#	Err - file to store stderr
#	Info - file with collected output for tst_res function
#	Init - file to store output from the initial ja command 
#	JaEnd - file to store output from the ja -t command
#
# OUTPUT: the following files are created:
#	$Err.1 - file to store stderr
#	$Init.1 - file to store output from the initial ja command 
#	$Info.1 - file to collect output for tst_res function
#	$JaEnd - file to store output from the ja -t command
#	ja.summary - file with output from ja -s command
#
# CUTS OUTPUT:	
#       PASS if the file displayed by the -s report is removed after the
#		ja -t command
#       FAIL if the file displayed by the -s report is still there after
#		the ja -t command
#       BROK if any of the ja commands return an error code
#
# RETURN:	n/a
#
###############################################################################
rm_def_file() {
	msg="default jacct file is removed by ja -t"
	$Cmdloc > $Init.1 2>&1
	status=$?
	cp $Init.1 $Info.1
	if [[ $status != 0 ]] ; then
	    echo "$Cmdloc to start job accounting returned $status" >> $Info.1
	    tst_res TBROK "$msg" $Info.1
	    return
	fi

	# get the name of the jacct file from ja -s
	$Cmdloc -s > ja.summary 2>&1
	status=$?
	cat ja.summary >> $Info.1
	if [[ $status != 0 ]] ; then
		echo "ja -s command returned $status" >> $Info.1
		tst_res TBROK "$msg" $Info.1
		return
	fi

	grep "Job Accounting File Name" ja.summary >> $Info.1 2>&1
	status=$?
	if [[ $status != 0 ]] ; then
	    echo "grep of ja summary report returned $status" >> $Info.1
	    echo "Error getting jacct file from ja summary report" >> $Info.1
	    tst_res TBROK "$msg" $Info.1
	    return
	fi

	fn=`grep "Job Accounting File Name" ja.summary 2>> $Err.1 \
		| cut -f2 -d: 2>> $Err.1`
	cat $Err.1 >> $Info.1

	$Cmdloc -t > $JaEnd 2>&1
	status=$?
	cat $JaEnd >> $Info.1

	# the jacct file should be deleted
	ls -l $fn > ls.out 2>> $Err.1 
	cnt=`cat ls.out |wc -l 2>> $Err.1`
	cat ls.out >> $Info.1
	cat $Err.1 >> $Info.1

	# there should be no stderr messages in $Err.1
	errcnt=`cat $Err.1 | wc -l`

	if (( status !=0 )) ; then
		echo "ja -t command returned $status" >> $Info.1
		tst_res TBROK "$msg" $Info.1
	elif ((cnt == 0 )) ; then
		tst_res TPASS "$msg" 
	elif ((errcnt != 0 )) ; then
		cat $Err.1 >> $Info.1
		tst_res TBROK "$msg" $Info.1
	else
		tst_res TFAIL "$msg" $Info.1
	fi
}

###############################################################################
#
# user_def - verify user-defined file is NOT removed by ja -t command
#
# INPUT: the following variables are expected to be set:
#	Cmdloc - full path to the ja command
#	Err - file to store stderr
#	Info - file with collected output for tst_res function
#	Init - file to store output from the initial ja command 
#	JaEnd - file to store output from the ja -t command
#
# OUTPUT: the following files are created:
#	$Err.2 - file to store stderr
#	$Init.2 - file to store output from the initial ja command 
#	$Info.2 - file to collect output for tst_res function
#	$JaEnd - file to store output from the ja -t command
#	ja.summary - file with output from ja -s command
#
# CUTS OUTPUT:	
#       PASS if the file displayed by the -s report is removed after the
#		ja -t command
#       FAIL if the file displayed by the -s report is still there after
#		the ja -t command
#       BROK if any of the ja commands return an error code
#
# RETURN:	n/a
#
###############################################################################
user_def() {
	msg="user-specified jacct file not removed by ja -t"
	$Cmdloc $Jafile > $Init.2 2>&1
	status=$?
	cp $Init.2 $Info.2
	echo "jacct file is $Jafile" >> $Info.2
	if [[ $status != 0 ]] ; then
		echo "$Cmdloc to start job accounting returned $status" >> $Info.2
		tst_res TBROK "$msg" $Info.2
		return
	fi

	$Cmdloc -t $Jafile > $JaEnd 2>&1
	status=$?
	cat $JaEnd >> $Info.2

	# the jacct file should still exist
	cnt=`ls -l $Jafile 2>> $Info.2 |wc -l 2>> $Err.2`
	cat $Err.2 >> $Info.2

	# there should be no stderr messages in $Err.2
	errcnt=`cat $Err.2 | wc -l`

	if (( status !=0 )) ; then
		echo "ja -t command returned $status" >> $Info.2
		tst_res TBROK "$msg" $Info.2
	elif ((cnt == 1 )) ; then
		tst_res TPASS "$msg" 
	elif ((errcnt != 0 )) ; then
		tst_res TBROK "$msg" $Info.2
	else
		tst_res TFAIL "$msg" $Info.2
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
export ACCT_TMPDIR=$Cpwd		# needed for 1st test case
Jafile="$Cpwd/jacct.$$"


# 1.) the default jacct file is removed
rm_def_file

# 2.) a user-specified file is not removed
user_def

cleanup
