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
#     TEST IDENTIFIER   : csa_ja-o
#
#     TEST TITLE        : pid and ppid from ja -o command
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
#       1.) display of the process id
#       2.) display of the parent process id
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
#		Tscript - script to run to create pacct records ($Script)
#		Tscript.out - stdout/stderr of $Script ($Scriptout)
#		jafile.$$ - ja pacct file ($Jafile)
#		info - file to use for input for tst_res function ($Info)
#			- only has data for the last test case to be run
#		ps.out - output of the ps command in $Script
#		ja.out - output of the ja -o command
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the ja -o command shows the correct data for process id 
#		and parent process id.  Run a script with ja commands, and the 
#		script $$ should be the parent id of all commands in the
#		ja -o report.  Run a ps command and compare its pid from
#		the ps output with its pid in the ja -o report.
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
#	n/a
#
###############################################################################


###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csa_ja-o"			# test case identifier
TST_TOTAL=2			# total number of test cases

Cpwd=""				# current working directory
Jafile=""			# ja pacct file

Script="Tscript"		# script to run 
Scriptout="${Script}.out"	# script output
Numcmds=6			# num commands in $Script creating ja records

Info="info"			# file with info for test analysis


###############################################################################
#
# FUNCTIONS
#
###############################################################################

###############################################################################
#
# cleanup - clean up after running the test
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

	tst_rmdir
	tst_exit
}


###############################################################################
#
# Set up for the test by creating the script indicated by $Script, 
# and running the script in a different job as the test user $Nuser.
#
# The test cases for test will verify the job acct records that are
# created when this script runs.  This will create pacct records for 
# each of the commands in the script ($Numcmds) starting with the
# ja command.  The "ja -o" command is not included in the report.
#
# NOTE: you must change the variable Numcmds if you add/delete commands
#	in ${Script}!
#
###############################################################################
setup() {

	# output the test script to be run by the test user
	cat > $Script <<- !EOF1
	#!/bin/ksh
	# the following line will tell us the ppid for the rest of the commands
	print "Pid for $Script is : \$$"
	ja $Jafile
	jstat
	# make sure there's only one ps command in $Script
	ps > ps.out 2>&1
	tty
	date
	ls
	ja -o $Jafile > $Cpwd/ja.out 2>&1
	!EOF1


	chmod +x $Script
	# Doesn't matter who runs the script; we're using ja
	./${Script} > $Scriptout 2>&1
	rtn=$?
	if [[ $rtn != 0 ]] ; then
		tst_brkloop TBROK cleanup "setup() runcmd returned $rtn" \
			$Scriptout
		tst_exit
	fi

	numerr=`grep -c "not found" $Scriptout`
	if [[ $numerr != 0 ]] ; then
		tst_brkloop TBROK cleanup \
			"setup() Some commands were not found" $Scriptout
		tst_exit
	fi
}


###############################################################################
#
# ja_pid - tests the Process ID column of the ja -o report
#	
#
###############################################################################
ja_pid() {

	spid=0
	japid=0
	msg="process id"

	cp ja.out $Info
	japid=`grep ^ps ja.out |awk '{print $4}'`
	print "According to ja, the pid of the ps command is $japid\n" >> $Info

	cat ps.out >> $Info
	spid=`grep ps ps.out |awk '{print $1}'`
	print "The pid of the ps command in ps.out is $spid\n" >> $Info

	if (( $spid == $japid )); then
		tst_res TPASS "$msg" 
	else
		tst_res TFAIL "$msg" $Info
	fi
}

###############################################################################
#
# ja_ppid - tests the "Parent ProcID" column of the ja -o report
#	The pid of $Script should be the parent of the commands in the script
#
###############################################################################
ja_ppid() {

	eppid=0
	numppid=0
	msg="parent process id"

	# get the ppid
	# get the expected parent pid from the script output
	eppid=`grep "Pid for $Script" $Scriptout |cut -f2 -d:`

	# $eppid should be the parent of the $Numcmds processes in the 
	# script.  
	numppid=`grep -c $eppid ja.out`

	if (( $Numcmds == $numppid )) ; then
		tst_res TPASS "$msg" 
	else
		cp ja.out $Info
		echo "Expected $Numcmds records with a ppid of $eppid" >> $Info
		echo "     Got $numppid records with a ppid of $eppid" >> $Info
		tst_res TFAIL "$msg" $Info
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
# Verifies CSA is installed and enabled.
. csa_setup

Cpwd=`pwd`	# save the current working directory for use by $Script
Jafile="$Cpwd/jacct.$$"

setup				# cat out and run the script 

# 1.) display of process id
ja_pid

# 2.) display of parent process id
ja_ppid

cleanup
