#! /bin/ksh
#
# $Header: /plroot/linux-csa/stout708/.RCS/PL/csa/tests/RCS/csacom_io.ks,v 1.2 2007/11/10 00:25:43 jlim Exp $ 
#
#	(C) COPYRIGHT SILICON GRAPHICS, INC.
#	UNPUBLISHED PROPRIETARY INFORMATION.
#	ALL RIGHTS RESERVED.
#
###############################################################
#
#     OS Testing - Silicon Graphics, Inc.
#
#     TEST IDENTIFIER   : csacom_io
#
#     TEST TITLE        : I/O Data From csacom
#
#     PARENT DOCUMENT   : csacom.tds 
#
#     TEST CASE TOTAL   : 6 
#
#     AUTHOR            : Janet Clegg
#
#     CO-PILOT(s)       : N/A
#
#     DATE STARTED      : 01/2000
#
#     TEST CASES
#	Verify the following fields on the csacom -i command:
#		1.) Characters Written, as reported by csacom -i
#		2.) Characters Read, as reported by csacom -i
#
#	Repeat the above scenarios for each of the following file sizes:
#		1 megabytes
#		2 megabytes
#		3 megabytes
#		10 megabytes
#	for a total of 8 test cases 
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
#		Tsetup.out - file with info on test setup ($Setup)
#		Tsetup.csa - file with info on inital CSA settings ($CSAstatus)
#		cc.out - output from the cc command
#		csacom.out - output of csacom -i command ($Csacomout)
#		ls.out - output of "ls -ls $Myfile" command ($lsout)
#		myread - C program to read the file created by mywrite
#		myread.c - c program text
#		myread.out.N - output from myread -- num bytes read
#		mywrite - C program to create a file of specified size
#		mywrite.c - c program text
#		mywrite.out - output from the 1st mywrite--should be empty
#		readsize.out - output of csacom -i for simple program
#		simple - simple C program with one command: exit(0)
#		simple.c - c program text
#		testfile - file created to generate write records ($Myfile)
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#	1. Cat out, compile, and run programs that:
#		a) (simple) merely exits, to get default read size
#		b) (mywrite) that creates files of specified sizes
#		c) (myread) that reads the file created by mywrite
#
#	2. Run the above programs to create a files of different sizes and read
#		them back.
#
#       3. Verify the -i option on the csacom command shows the correct 
#		values for each test case
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
#	CSA - This test may change the CSA configuration, therefore it
#		needs exclusive use of the resource.
#
#     UPDATE HISTORY
#       This should contain the description, author, and date of any
#       "interesting" modifications (i.e. info should helpful in
#       maintaining/enhancing this test program).
#       user     date  description
#       ----------------------------------------------------------------
#       janetc   01/2000 Initial Version
#       janetc   10/2000 modify to eliminate the -l option and TTY
#				column for Linux
#       janetc   11/2000 Break logio.c into 2 programs: 
#				one that writes a file
#				and one that reads a file.
#			 Add a program that just exits, to get default
#				read chars just for reading in the program
#			 Added variables ReadCol and WriteCol to store which
#				column in csacom output has the desired data
#			 Changed number of test cases and read/write sizes 
#
#     BUGS/LIMITATIONS
#       All known bugs/limitations should be filled in here.
#
#
#
###############################################################


###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csacom_io"		# test case identifier
TST_TOTAL=8			# total number of test cases 

typeset int TCnt=0		# test item count, used for index
Info="Info"			# file with info for test analysis
Setup="Tsetup.out"		# file with info on test setup
CSAstatus="Tsetup.csa"		# file with info on original CSA status

Megabytes=100			# num of megabytes to write, default to 100
Myfile="testfile"		# file created to generate write records
lsout="ls.out"			# output of "ls -ls $Myfile" command

Csacomout="csacom.out"		# output of "csacom -i" command

ReadSize=0			# num chars just to read in a program
ReadCol=6			# Read chars is the 6th column (on Linux)
WriteCol=7			# Write chars is the 7th column (on Linux)

Stime=0				# time the test started, used with csacom
Etime=0				# time the test ended, used with csacom

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

#debug
#pwd
#rm $Myfile		# be sure to delete this big file if in debug mode

	restore_csa
#	tst_rmdir
	tst_exit

}

###############################################################################
#
# verify_conf - verify correct installation and configuration.  
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
# getreadsize - Cat out, compile and run:
#		simple.c to run a simple program and check #chars read
#		Then run csacom to get the number of chars read on a simple
#		program.  This tells us the number of read chars just to
#		read in a program.  It should be 4096 on Linux and 512 on Irix
#
###############################################################################
getreadsize() {

	cat > simple.c <<- !EOF
		int
		main()
		{
			exit(0);
	
		}
	!EOF
	# end of simple C program

	cc -o simple simple.c > cc.out 2>&1
	status=$?
	if [[ $status != 0 || ! -x simple ]] ; then
		# uh-oh, this is a setup error
		print "The cc command returned $status" >> cc.out
		tst_brk TBROK cleanup "simple did not compile correctly" cc.out
	fi

	Stime="D${Today}:"`date +%H:%M:%S`
	sleep 1			# don't want start and end time the same
	./simple
	sleep 1
	Etime="D${Today}:"`date +%H:%M:%S`
	
	cmd="$Csacomcmd -u $LOGNAME -S $Stime -E $Etime -in simple"

	# echo pwd=`pwd`
	$cmd > readsize.out 2>&1
	# command must come last so it doesn't interfere with getting ReadSize
	echo "\nCommand was:\n$cmd" >> readsize.out

	# should pick up only one process (plus the command line)
	# so cnt should equal 2
	cnt=`grep -c simple readsize.out`
	if (( cnt > 2 )) ; then
		tst_brk TBROK cleanup "found multiple records" readsize.out
	elif (( cnt < 2 )) ; then
		tst_brk TBROK cleanup "did not find a record" readsize.out
	fi

	# Get the CHARS READ column from the csacom output to
	# calculate the #chars just to read in the program
	set -A tmpvar `grep simple readsize.out`
	ReadSize=${tmpvar[$ReadCol]}
	if [[ -z $ReadSize || $ReadSize = 0 ]] ; then
		tst_brk TBROK cleanup "ReadSize set to /$ReadSize/" readsize.out
	else
		tst_res TINFO "Init CHARS READ is $ReadSize"
	fi
}


###############################################################################
#
# setup - cat out and compile:
#		mywrite.c to open a file and perform write calls
#		myread.c to open a file and perform read calls
#
###############################################################################
setup() {


	# output the program to call read(2)
	cat > myread.c <<- !EOF1

	/*
	 * read $Myfile (which must be created by the mywrite program)
	 * and determine how big the file is
	 */

	#include <stdio.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <fcntl.h>

	int
	main()
	{

		int myfd;
		char myfile[] = "$Myfile";
		ssize_t nbufs = 1;	/* return code from read(2)	*/
		char bigbuf[1025];
		int numbytes=1024;	/* num of bytes to read	each time */
		int total=0;		/* total num of bytes read	*/

		myfd = open(myfile, O_RDONLY);
		if (( myfd == -1 )) {
			printf("open() returned %d \n", myfd);
			perror("open() error");
			exit(1);
		}

		while ( nbufs > 0 ) {
			nbufs = read (myfd, bigbuf, numbytes);
			total = total + nbufs;
		}

		printf("Read %d bytes \n", total);

		close(myfd);

	}

	!EOF1
	# end of myread C program

	# output the program to call write(2)
	cat > mywrite.c <<- !EOF2

	/*
	 * write a file using the buffer size specified by the -b option
	 * write the buffer $iter times (specified by the -n option)
	 * filesize should be $iter times the buffer size (10240 default)
	 */

	#include <stdio.h>
	#include <errno.h>
	#include <string.h>
	#include <limits.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/fcntl.h>
	#include <unistd.h>

	#define OPTIONS   "b:n:hv"
	#define MAXBYTES  1024

	char *Progname;			/* name of this program */

	/* function declarations */
	void help();

	int
	main (int argc, char *argv[])
	{
		int myfd;
		int myerr;
		int ret;		/* return value of function call */
		int iter = 10;		/* num of iterations to write() */
		int numbytes = MAXBYTES; /* num of bytes to write	*/
		int numwrites = 0;	/* num of write calls made so far */
		int verbose = 0;	/* =1 if verbose mode		*/
		size_t len = 0;		/* string length		*/
		ssize_t nbufs = 1;	/* return code from write(2)	*/
		off_t position;		/* offset position from lseek	*/
		unsigned long total = 0;
		char myfile[] = "$Myfile";
		char *chrptr;		/* char pointer used to walk
					 * through string
					 */
		/* this is 64 bytes plus terminating null char */
		char smallbuf[64] =
	"123456789a123456789b123456789c123456789d123456789e123456789f123";
		/* this is 1024 bytes plus terminating null char */
		char bigbuf[1025];

		/* Assign Progname the filename of the executable */
		if ((chrptr=strrchr(argv[0],'/')) == NULL) {
			Progname = argv[0];
		} else {
			Progname = ++chrptr;
		}

		while ((ret=getopt(argc, argv, OPTIONS)) != EOF)
		{
		switch(ret) {
		case 'h' :
		/* help */
			help();
			exit(0);
			break;
		case 'b' :
			/* num of bytes to write */
			numbytes = atoi(optarg);
			if (numbytes > MAXBYTES ) {
				numbytes = MAXBYTES;
			}
			break;
		case 'n' :
		/* num of iterations to call write() */
			iter = atoi(optarg);
			break;
		case 'v' :
			/* verbose */
			verbose = 1;
			break;
		case '?' :
			fprintf(stderr,
				"%s: Invalid option or missing argument.\n",
				Progname);
			help();
			exit(1);
		}       /* end of switch */
		}       /* end of while */

		if (verbose) {
			printf("\nWRITE I/O TEST \n");
			printf("NOTE: verbose mode has more write chars!!\n");
			printf("%d iterations of %d bytes\n", iter, numbytes);
			printf("output file is %s\n", myfile);
		}

		myfd = open(myfile, O_CREAT |  O_TRUNC |  O_RDWR);
		if (( myfd == -1 )) {
			printf("%s: open() returned %d \n", Progname, myfd);
			perror("open() error");
			exit(1);
		}

		myerr = chmod(myfile, 00600);
		if (( myerr < 0 )) {
			printf("%s: chmod() returned %d \n", Progname, myerr);
			perror("chmod() error");
			exit(1);
		}

		sprintf(bigbuf, "%s%s%s%s%s%s%s%s", smallbuf,smallbuf,smallbuf,
			smallbuf, smallbuf, smallbuf, smallbuf, smallbuf);

		nbufs=1;
		while ( iter > numwrites ) {
			nbufs = write (myfd, bigbuf, numbytes);
			if (( nbufs == -1 )) {
				perror("write() error");
				exit(1);
			}
			numwrites++;
			total = total + nbufs ;
		}

		if (verbose) {
			printf("%d write calls \n", numwrites );
			printf("filesize %d \n", total);
		}

		close(myfd);

        } /* main() */

	/* help routine */
	void
	help()
	{

	printf("Usage: %s [-b] [-n] \n", Progname);
	printf("     -b bytes       Number of bytes to write/read (%d max) \n",
		MAXBYTES);
	printf("     -h             Display this help message \n");
	printf("     -n iterations  Number of iterations of write \n");
	printf("     -o filename    Name of output file \n");
	printf("     -v             Specifies verbose mode \n");

	}

	!EOF2
	# end of mywrite C program

	# shell program resumes here

	cc -o myread myread.c > cc.out 2>&1
	status=$?
	if [[ $status != 0 || ! -x myread ]] ; then
		# uh-oh, this is a setup error
		print "The cc command returned $status" >> cc.out
		tst_brk TBROK cleanup "myread did not compile correctly" cc.out
	fi
	cc -o mywrite mywrite.c > cc.out 2>&1
	status=$?
	if [[ $status != 0 || ! -x mywrite ]] ; then
		# uh-oh, this is a setup error
		print "The cc command returned $status" >> cc.out
		tst_brk TBROK cleanup "mywrite did not compile correctly" cc.out
	fi

}

###############################################################################
#
# run_test - this executes the commands under test
#	1. Get the starting time 
#	2. Run the mywrite and myread program; output to mywrite.out
#		and myread.out.$Megabytes
#	3. Get the file size from ls; output to $lsout
#	4. Get the ending time 
#	5. Get the csacom report; output to $Csacomout
#
###############################################################################
run_test() {

	# multiply $Megabytes by 1024 and use 1k buffers to reach the 
	# total requested number of Megabytes
	(( iter = Megabytes * 1024 ))

	# get the starting time of the first process, for csacom 
	Stime="D${Today}:"`date +%H:%M:%S`
	sleep 1

	# get the tty
	tty=`tty | tr "/" " " | awk '{ print $NF }'`

	./mywrite -n $iter -b 1024 > mywrite.out 2>&1
	ret=$?
	if [[ $ret != 0 ]] ; then
		# no test cases will be accurate if this command failed
		tst_brkloop TBROK cleanup \
		  "mywrite -n $iter -b 1024 returned $ret" mywrite.out
	fi
	# myread will return the number of bytes read
	./myread > myread.out.$Megabytes 2>&1
	ret=$?
	wait
	if [[ $ret != 0 ]] ; then
		# no test cases will be accurate if this command failed
		tst_brkloop TBROK cleanup \
				"myread returned $ret" my.out
	fi

	# get the file size in blocks and bytes
	ls -ls $Myfile > $lsout 2>&1
	ret=$?
	if [[ $ret != 0 ]] ; then
		# check_results relies on this file.  Gotta be there
		print "Above output is from the command: " >> $lsout
		print "ls -ls $Myfile" >> $lsout
		tst_brkloop TBROK cleanup \
			"write program failed to create file $Myfile" $lsout
	fi

	sleep 1
	# end time for csacom
	Etime="D${Today}:"`date +%H:%M:%S`

	# get the csacom report; if there is no tty, don't use it
	# Linux doesn't have the -l option on csacom
	if [[ "$OS" = "Linux" ]] ; then
		cmd="$Csacomcmd -u $LOGNAME -S $Stime -E $Etime -in my" 
	elif [[ "$tty" = "?" ]] ; then
		cmd="$Csacomcmd -u $LOGNAME -S $Stime -E $Etime -in my"
	else
		cmd="$Csacomcmd -u $LOGNAME -S $Stime -E $Etime -in my"
		cmd="$cmd -l $tty"
	fi
	$cmd > $Csacomout 2>&1
	ret=$?

	if [[ $ret != 0 ]] ; then
		tst_brk TBROK cleanup "$cmd returned $ret" $Csacomout
	fi
}

###############################################################################
#
# check_results - check results according to variables set by calling 
#		function.
#
###############################################################################
check_results() {

	reported=$1		# the value found from the command
	expected=$2		# the expected value
	tc="$3"			# the test case
	tmsg="$4"		# message to display on the test result line

	if (( $# != 4 )) ; then
		print "check_results function needs 5 parameters" > $Info
		print "$# Parameters passed to check_results" >> $Info
		print "Parameters were $@" >> $Info
		tst_res TBROK "Insufficient parameters to check_results()" $Info

	elif [[ $reported == $expected ]] ; then
		tst_res TPASS "$tmsg"

	else
		print "Expecting $tc of $expected " > $Info
		print "Reported  $tc of $reported " >> $Info

		echo "" >> $Info		# add a blank line 
		print "$Csacomcmd -i command shows:" >> $Info
		print "Command was:" >> $Info
		print "$Csacomcmd -u $LOGNAME -S $Stime -E $Etime -in my" \
			>> $Info
		cat $Csacomout >> $Info
		echo "" >> $Info		# add a blank line 


		print "ls -ls command shows:" >> $Info
		cat $lsout >> $Info
		echo "" >> $Info		# add a blank line to end
		tst_res TFAIL "$tmsg" $Info
	fi

}

###############################################################################
#
# csacom_chars_w - verify number of characters read.  It should be
#	the same as the filesize displayed by ls -l AND the output
#	from myread (in myread.out)
#
###############################################################################
csacom_chars_w() {
	
	testcase="Chars Written"
	msg="csacom -i $testcase $Megabytes meg file"

	# the output of the ls command should show the number chars written
	set -A line `grep $Myfile $lsout`
	expect=${line[5]}

	# Get the "CHARS WRITTEN" column from the csacom output
	set -A line `grep mywrite $Csacomout`
	report=${line[$WriteCol]}

	check_results "$report" "$expect" "$testcase" "$msg"
}

###############################################################################
#
# csacom_chars_r - verify number of characters read.  It should be
#	the same as the filesize displayed by ls -l
#
###############################################################################
csacom_chars_r() {

	testcase="Chars Read"
	msg="csacom -i $testcase $Megabytes meg file"

	set -A line `grep $Myfile $lsout`
	(( expect = ${line[5]} ))
	set -A line `grep Read myread.out.$Megabytes`
	(( expect2 = ${line[1]} ))

	# Get the CHARS READ column from the csacom output
	set -A line `grep myread $Csacomout`
	report=${line[$ReadCol]}

	if (( expect != expect2 )); then
		# something's wrong
		echo "The myread program read $Myfile and reported:" > $Info
		cat myread.out.$Megabytes >> $Info
		echo "But output from ls shows $expect filesize" >> $Info
		cat $lsout >> $Info
		tst_res TBROK "$msg" $Info

	else
		# the program read a file file that was $expect chars 
		# but we also need to add the read chars for the program itself
		(( expect = expect + $ReadSize ))
		check_results "$report" "$expect" "$testcase" "$msg"
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
# and variables: CSAsetup, CSAstatus, CSAadmin, CSAuser, Today, OS
. csa_setup

Csacomcmd="${CSAuser}/csacom"		# full path to csacom command

if [[ "$OS" = "Linux" ]] ; then
	ReadCol=6		# Read chars is the 6th column on Linux
	WriteCol=7		# Write chars is the 7th column on Linux
else
	ReadCol=7		# Read chars is the 6th column on Irix
	WriteCol=8		# Write chars is the 7th column on Irix
fi

verify_conf
setup

# get the # chars read for a program that has no reads
getreadsize

# Scenario 1
Megabytes=1				# run test with 1 megabytes
run_test 

# now verify the accounting records for:
# 1.) Characters Written
# 2.) Characters Read
csacom_chars_r 
csacom_chars_w


# make sure there's at least a second between test cases, don't overlap
sleep 1

# Scenario 2
Megabytes=2				# run test with 2 megabytes
run_test

# now verify the accounting records for:
# 3.) Characters Written
# 4.) Characters Read
csacom_chars_r 
csacom_chars_w 

# make sure there's at least a second between test cases, don't overlap
sleep 1


# Scenario 3
Megabytes=3				# run test with 3 megabytes
run_test 

# now verify the accounting records for:
# 5.) Characters Written
# 6.) Characters Read
csacom_chars_r 
csacom_chars_w 

# make sure there's at least a second between test cases, don't overlap
sleep 1


# Scenario 4
Megabytes=10				# run test with 10 megabytes
run_test 

# now verify the accounting records for:
# 5.) Characters Written
# 6.) Characters Read
csacom_chars_r 
csacom_chars_w 


cleanup

