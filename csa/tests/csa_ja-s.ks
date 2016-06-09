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
#     TEST IDENTIFIER   : csa_ja-s
#
#     TEST TITLE        : ja summary report
#
#     PARENT DOCUMENT   : n/a
#
#     TEST CASE TOTAL   : 11
#
#     AUTHOR            : Janet Clegg
#
#     DATE STARTED      : 08/2000
#
#     TEST CASES
#	Verify the following fields on the ja -s command:
#       1.) Job Accounting File Name
#       2.) Operating System
#       3.) User Name and ID
#       4.) User ID
#       5.) Group Name and ID
#       6.) Group ID
#       7.) Project Name and ID
#       8.) Project ID
#       9.) Array Session Handle (ASH)
#       10.) Job ID (jid)
#       11.) Number of commands
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
#		expect.out - output of command to evaluate ja output ($Expect)
#		err.out - any output to stderr ($Err)
#		Info - collected input for tst_res function ($Info)
#		ja.out - output of ja -s command ($Jaout)
#		ja.init - output from initial ja command
#		jacct.$$ - job accounting file ($Jafile)
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the -s option on the ja command shows the correct 
#		values for the listed test cases.
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
#       janetc   12/2000 Ported to Linux:
#		 * uname -a on LInux includes a processor type which 
#			doesn't appear in ja -s
#		 * Linux doesn't yet have project name/id, but displays
#			the same as if /etc/project file is missing/empth
#		 * No Array Services on Linux yet (and no corresponding
#			display in ja -s)
#		 * the jstat command displays leading zeros, eg. 
#			0x007f010000003ca7 vs 0x7f010000003ca7
#	jlan	 02/2006 Ported to csa-test-SGINoShip rpm
#
#     BUGS/LIMITATIONS
#       All known bugs/limitations should be filled in here.
#
#	o This test uses the sid command which may or may not be ported
#		to Linux.
#
#
###############################################################################

###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csa_ja-s"			# test case identifier
TST_TOTAL=11			# total number of test cases

Cpwd=""				# current working directory for this script
OSname=`uname`

Info="Info"			# file with info for test analysis

Jafile=""			# ja pacct file
Jaout="ja.out"			# output of ja -s command
Expect="expect.out"		# output of command determining expected output
Err="err.out"			# any output to stderr


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

#debug
#pwd
	tst_rmdir
	tst_exit

}

###############################################################################
#
# setup - turn on job accounting and get the jstat -s summary report
#
# INPUT:        n/a
#
# OUTPUT:       the following files:
#		$Jafile - the ja accounting data file
#		$Jaout - output of the ja -s command
#
# CUTS OUTPUT:  
#	       BROK if get non-zero exit status when initiating ja
#	       BROK if get non-zero exit status from "ja -s"
#
# RETURN:       n/a
#
# EXITS with:   n/a
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


	# get the ja -s report
	$Cmdloc -s $Jafile > $Jaout 2>&1
	status=$?
	if [[ $status != 0 ]] ; then
		tst_brk TBROK cleanup "ja -s command returned $status" $Jaout
	fi

}


###############################################################################
#
# verifyresults - verify the test results
#
# INPUT:        The following variables:
#		  $tcase - the test case 
#		  $got - the test case string from "ja -s"
#		  $want - the expected string 
#		The following files:
#		  $Jaout - output of the ja -s command
#		  $Expect - output of the command to determine expected string
#		  $Err - any output to STDERR
#
# OUTPUT:       
#
# CUTS OUTPUT:  
#	       PASS if $got and $want are the same
#	       FAIL if $got and $want are not the same
#              BROK if $Err is not empty
#              BROK if the expected files don't exist
#              BROK if the expected variables aren't set
#
# RETURN:       n/a
#
# EXITS with:   n/a
#
###############################################################################
verifyresults() {

if [[ "$1" = "x" ]] ; then
	set -x
fi

if [[ ! -f $Jaout ]] ; then
	echo "Test Case Error: missing file $Jaout" > $Info
	tst_res TBROK "$tcase" $Info
elif [[ ! -f $Expect ]] ; then
	echo "Test Case Error: missing file $Expect" > $Info
	tst_res TBROK "$tcase" $Info
elif [[ ! -f $Err ]] ; then
	echo "Test Case Error: missing file $Err" > $Info
	tst_res TBROK "$tcase" $Info
elif [[ -z $tcase ]] ; then
	echo "Test Case Error: missing value for variable tcase" > $Info
	tst_res TBROK "$tcase" $Info
elif [[ -z $want ]] ; then
	echo "Test Case Error: missing value for variable want" > $Info
	tst_res TBROK "$tcase" $Info
elif [[ -z $got ]] ; then
	echo "Test Case Error: missing value for variable got" > $Info
	tst_res TBROK "$tcase" $Info
else
	cat $Err >> $Info
	cat $Expect >> $Info
	cat $Jaout >> $Info
	set -A myerr `wc -w $Err`
	if [[ $want = $got ]] ; then
		tst_res TPASS "$tcase"
	elif (( ${myerr[0]} > 0 )) ; then
		tst_res TBROK "$tcase" $Info
	else
		echo "Expected =$want=" >> $Info
		echo "     Got =$got=" >> $Info
		tst_res TFAIL  "$tcase" $Info
	fi
fi

rm $Info $Expect $Err
tcase=""
got=""
want=""
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

# 1.) Job Accounting File Name
tcase="Job Accounting File Name"
echo $Jafile > $Expect
got=`grep "$tcase" $Jaout 2>> $Err | awk '{print $6}' 2>> $Err`
want=$Jafile
verifyresults 

# 2.) Operating System
tcase="Operating System"
if [[ $OSname = Linux ]] ; then
	# uname -a has a processor type which irix doesn't have
	sysname=`uname -s`
	nodename=`uname -n`
	relname=`uname -r`
	vername=`uname -v`
	machine=`uname -m`
	echo "$sysname $nodename $relname $vername $machine" > $Expect
else
	uname -a > $Expect 2> $Err
fi
# this is for linux.  Put them all together, and that is what's expected
# (uname -a has a processor type which irix doesn't have
# OR drop the last word from uname -a (so long as processor type is
# only one word.  Use uname -p to find out)

got=`grep "$tcase" $Jaout 2>>$Err |cut -f2- -d: 2>> $Err |cut -c 2- 2>> $Err`
want=`cat $Expect`
verifyresults

# 3.) User Name and ID
tcase="User Name"
id > $Expect 2> $Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f2 -d: 2>>$Err|cut -f2 -d" " 2>>$Err`
want=`cat $Expect | cut -f2 -d"(" 2>> $Err | cut -f1 -d")" 2>> $Err`
verifyresults

# 4.) User ID
tcase="User Name (ID)"
id > $Expect 2>$Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f3 -d"(" 2>>$Err|cut -f1 -d")" 2>>$Err`
want=`cat $Expect | cut -f2 -d= 2>> $Err | cut -f1 -d"(" 2>> $Err`
verifyresults

#       5.) Group Name 
tcase="Group Name"
id > $Expect 2>$Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f2 -d: 2>>$Err|cut -f2 -d" " 2>>$Err`
want=`cat $Expect | cut -f3 -d"(" 2>> $Err  | cut -f1 -d")" 2>> $Err`
verifyresults

# 6.) Group Name and ID
tcase="Group Name (ID)"
id > $Expect 2>$Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f3 -d"(" 2>>$Err|cut -f1 -d")" 2>>$Err`
want=`cat $Expect | cut -f3 -d= 2>> $Err | cut -f1 -d"(" 2>> $Err`
verifyresults

# 7.) Project Name 
tcase="Project Name"
sid -pn > $Expect 2> $Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f2 -d: 2>>$Err|cut -f2 -d" " 2>>$Err`
want=`cat $Expect`
if [[ "$want" = "" || $want = 0 ]] ; then
	# The project name isn't defined; ja will display a question mark
	want="?"
fi
cnt=`grep -c "$tcase" $Jaout`
verifyresults

# 8.) Project ID
tcase="Project Name (ID)"
sid -p > $Expect 2> $Err
got=`grep "$tcase" $Jaout 2>>$Err|cut -f3 -d"(" 2>>$Err|cut -f1 -d")" 2>>$Err`
want=`cat $Expect`
if [[ "$want" = "" ]] ; then
	# The project name/id isn't defined; ja will display 0
	want="0"
fi
cnt=`grep -c "$tcase" $Jaout`
verifyresults

# 9.) Array Session Handle (ASH)
# Linux may or may not have an ASH, assume it will for now
# Linux may or may not have a sid command, assume it will for now
tcase="Array Session Handle"
if [[ $OS = Linux ]] ; then
	tst_res TCONF "$tcase"
else
	sid -a > $Expect 2> $Err
	got=`grep "$tcase" $Jaout 2>>$Err |cut -f2 -d: 2>>$Err| cut -f2 -d" " 2>>$Err`
	want=`cat $Expect`
	verifyresults
fi

# 10.) Job ID (jid)
typeset -L -Z got2
typeset -L -Z want2
tcase="Job ID"
jstat > $Expect 2> $Err
got1=`grep "$tcase" $Jaout 2>>$Err| cut -f2 -d: 2>>$Err | cut -f2 -d" " 2>>$Err`
want1=`tail -1 $Expect|cut -f1 -d" "`
#strip off the leading  0x and any leading zeros
got2=${got1#0x}
want2=${want1#0x}
# remove 2 trailing blanks, evidently added by removing the 0x
got=`echo $got2`
want=`echo $want2`
verifyresults

# 11.) Number of commands
tcase="Number of Commands"
echo "1" > $Expect 
want="1"
got=`grep "$tcase" $Jaout 2>>$Err|cut -f2 -d: 2>>$Err|awk '{print $1}' 2>>$Err`
verifyresults

cleanup
