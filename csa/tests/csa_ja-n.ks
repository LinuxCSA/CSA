#! /bin/ksh
#
#
#       (C) COPYRIGHT SILICON GRAPHICS, INC.
#       UNPUBLISHED PROPRIETARY INFORMATION.
#       ALL RIGHTS RESERVED.
#
###############################################################
#
#     OS Testing - Silicon Graphics, Inc.
#
#     TEST IDENTIFIER   : csa_ja-n
#
#     TEST TITLE        : ja pattern selection
#
#     PARENT DOCUMENT   : n/a
#
#     TEST CASE TOTAL   : 
#
#     AUTHOR            : Janet Clegg
#
#     DATE STARTED      : 08/2000
#
#     TEST CASES
#	-n option shows only commands matching the following regular
#	expressions, as in ed(1):

#	[1.1 of ed(1) man page] an ordinary character matches itself, ie. 
#	a single letter will match any command containing that letter
#       1.)  single character
#		ja -cn a (matches ainfo, ja, jstat, lpstat, uname but not
#			x1 or A1) 

#	[1.2 of ed(1) man page] a backslash (\) followed by a special 
#	character is a one-character expression that matches the special 
#	character itself.  The special characters are:
#	[1.2] a.	. * [ \ (period, asterisk, left square bracket and 
#			backslash) are always special _except_ when enclosed 
#			within brackets.
#       2.) period following a backslash
#		ja -cn \. (matches all commands)
#		ja -cn x.1 (matches xxx1 x.1 x_1 x.12 xy1 but not xyz1)
#		ja -cn x\.1 (matches the same as x.1 did)
#       3.) period in brackets
#		ja -cn [.] (matches x.1 x.12 1.x )
#       4.) left square bracket following a backslash
#		ja -cn \[ (is an invalid regular expression)
#		ja -cn \[1-3] (matches the same as [1-3] )
#       5.) left square bracket in brackets
#		ja -cn [[] (matches x[1 x[x )
#       6.) asterisk following a backslash
#		ja -cn \* (matches everything)
#		ja -cn x\*1 (matches the same as x.1 did plus xyz1)
#       7.) asterisk in brackets
#		ja -cn [*] (matches y* *x *y x*** y** )
#       8.) backslash following a backslash
#		ja -cn \\
#       9.) backslash in brackets
#		ja -cn [\]

#	[1.2] b.	^ (caret or circumflex), which is special at the 
#			beginning of regular expression or when it immediately 
#			follows the left of a pair of square brackets
#       10.) caret following a backslash
#		ja -cn \^ (matches x^ and ^x)
#		ja -cn ^\^ (matches ^x but not x^)

#	[1.2] c.	$ (dollar sign), which is special at the end of
#			regular expression 
#       11.) dollar sign following a backslash
#		ja -cn \$ (matches x$ and $x)
#		ja -cn \$$ (matches x$)
#		ja -cn x$ (matches $x *x)
#		ja -cn stat$ (matches jstat lpstat qstat stat)

#	[1.2] d.	The character that is special for that specific
#			regular expression (n/a?)

#	[1.3 of ed(1) man page] a non-empty string of characters enclosed in
#	square brackets is a one-character regular expression that
#	matches one character in that string....
#       11.) alpha string enclosed in brackets
#		ja -cn [xyz] (matches x1 1x z1 y1 )
#       12.) numeric string enclosed in brackets
#		ja -cn [14] (matches x1 x4 z1 1x y1 )
#       13.) special characters enclosed in brackets

#	[1.3] ....  If, however, the first character of the string is a
#	circumflex (^) the one-character expression matches any character,
#	except newline and the remaining characters in that string. 
#		ja -cn [^xyz] (matches x1 1x z1 y1 ) NOT
#		ja -cn [^1-4] (matches x1 x2 x3 x4 z1 1x ) NOT

#       .) a group of letters will match any command containing that group
#		ja -cn sta (matches jstat, lpstat, qstat, stat)

#       11.) caret following a backslash

#       11.) ^ at the beginning of an expression matches commands that
#		begin with that expression
#       15.) caret following a backslash
#		ja -cn ^l (matches ls, ln, lpstat)

#       .) ^ preceding a left bracket will match the commands that
#		begin with those characters.
#		ja -cn ^[na] (matches nawk, ainfo)
#       .) $ at the end of an expressions matches commands that end
#		with that expression
#		ja -cn stat$ (matches jstat, lpstat, stat)
#       .) a period (.) matches any single character except newline
#		ja -cn ^.x1 (matches yx1, zx1 but not xxx1 or x1)
#		[1.4 of ed(1) man page]
#	.) a bracket containing a minus sign indicates a range of
#		consecutive characters
#		ja -cn ^[k-n] (matches lpstat, ln, ls, man, more, nawk)
#	.) the special characters . * [ and / stand for themselves when
#		enclosed in brackets
#		ja -cn [.] (matches x.1 or any other command containing
#			a period)
#	.) * (asterisk) substitutes for zero or more characters
#		ja -cn j*t (matches jstat, jlimit)
#
#
#	(negative test cases)
#	.) ja -cn ON (since there is no command ON this should not match
#		a command.  Nor should it match the "CFG   ON" line)
#
#old....
#       1.) complete command name
#       2.) . (period) to substitute for a single character
#       3.) * (asterisk) to substitute for zero or more characters 
#                at the beginning of the command (*stat)
#       4.) * (asterisk) to substitute for zero or more characters (j*)
#                at the end of the command (*stat)
#       5.) * (asterisk) to substitute for zero or more characters (j*)
#                in the middle of the command (j*t)
#       4.) ^ (caret) to indicate expression starts at the beginning
#       	 [stat will match jstat and lpstat' ^stat won't]
#       5.) $ (dollar sign) to indicate expression is at the end
#       6.) characters enclosed in brackets ([]) match any one character
#                *[1-3]
#
#     INPUT SPECIFICATIONS (optional)
#       Description of all command line options or arguments that this
#       test will understand.
#
#       n/a
#
#
#     OUTPUT SPECIFICATIONS (optional)
#       Description of the format of this test program's output.
#
#       o Test output is the standard cuts output.
#
#       o The following files will be created in the temporary test directory.
#
#
#     DESIGN DESCRIPTION
#       High level description of what this test program is attempting
#       to do.
#
#       o Verify the ja -n command shows the 
#
#     SPECIAL REQUIREMENTS
#       A description of any non-standard requirements/needs that will
#       help a user in executing and or maintaining this test program,
#       and a brief explanation of the reasons behind the
#       requirement(s).
#
#       n/a
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
#       n/a
#
###############################################################################


###############################################################################
#
# GLOBAL VARS
#
###############################################################################
TCID="csa_ja-n"                 # test case identifier
TST_TOTAL=2                     # total number of test cases

Cpwd=""                         # current working directory
Jafile=""                       # ja pacct file

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

Cpwd=`pwd`      # save the current working directory for use by $Script
Jafile="$Cpwd/jacct.$$"


cleanup
