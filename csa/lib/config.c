/*
 * Copyright (c) 2000-2004 Silicon Graphics, Inc and LANL  All Rights Reserved.
 * Copyright (c) 2007 Silicon Graphics, Inc  All Rights Reserved.
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

/*
 *	config.c:  library routine for binary release variables.
 *			Looks at file __CONFIG__FILE__ or CSACONFIG and returns
 *			requested values for the tokens specified.
 *
 *	Parameters:
 *	    param	Token name
 *
 *	Return Values:
 *	      NULL	Error or invalid system parameter value requested.
 *	    string	Character string associated with the token.
 *			 If there are multiple values, the values are 
 *			 separated by a comma.
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif	/* HAVE_CONFIG_H */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include "csa_conf.h"
#include "acctmsg.h"

#define FUNC_NAME	"config()"	/* library function name */

#define CARRIAGE_RETURN		'\n'

int		db_flag2;		/* debug level - testing only */

static char	resp_string[4096];	/* configfile value assoc w/ token */

static char	*__getent__();
static FILE	*__openfile__();
static int	__get_config();

static int	config_size;		/* size of configuration file */
static char	*config_data;		/* configuration file contents */
static int	config_offset;		/* current position in file */
static int	line;			/* line number in file */


/*
 *	Returns a character pointer to the value associated with
 *	param.  This string can contain alphanumerics and embedded
 *	".".
 */
char *
config(char *param)
{
	char	*resp_ptr;

	/*
	 * 	Simple argument checking.
	 */
	if (*param == '\0') {
		acct_err(ACCT_CAUT,
		       _("%s: A parameter requested of the config() routine was null."),
			FUNC_NAME);
		return(NULL);
	}

	/*
	 * 	Read configuration file if needed.
	 */
	if (!config_size && (__get_config() == -1)) {
		return(NULL);
	}

	/*
	 *	Search configuration file for parameter match.
	 *	If found, get the associated parameter value.
	 */
	if ((resp_ptr = __getent__(param)) == NULL) {
		return(NULL);
	}

	/*
	 *	Return the requested system parameter value.
	 */
	return(resp_string);
}


/*
 *	Open __CONFIG__FILE__ or CSACONFIG, and do some basic error checks
 *		on it.
 *	Read the file into memory, then close the file.
 */
static int
__get_config() {
	int		fd;
	char		config_file[4096];
	char		*cp;
	struct stat	fst;

	/*
	 * 	See if CSACONFIG environment variable is set.
	 *	If it is, use that name for the configuration file.
	 *	Otherwise use __CONFIG__FILE__.
	 */
	cp = &(config_file[0]);
	if ((cp = getenv("CSACONFIG")) == NULL) {
		strcpy(config_file, __CONFIG__FILE__);
	} else {
		strcpy(config_file, cp);
	}

	/*
	 *	Open the file and read it in.
	 */
	if ((fd = open(config_file, O_RDONLY)) < 0) {
		acct_perr(ACCT_CAUT, errno,
			_("An error occurred during the opening of file '%s'."),
			config_file);
		return(-1);
	}

	if (fstat(fd, &fst) < 0) {
		close(fd);
		acct_perr(ACCT_CAUT, errno,
			_("An error occurred getting the status of file '%s'."),
			config_file);
		return(-1);
	}

	if ((config_size = fst.st_size) == 0) {
		close(fd);
		acct_err(ACCT_CAUT,
		       _("%s: The configuration file '%s' is empty."),
			FUNC_NAME, config_file);
		return(-1);
	}

	if ((config_data = malloc(config_size+1)) == NULL) {
		close(fd);
		acct_perr(ACCT_CAUT, errno,
			_("%s: There was insufficient memory available when allocating '%s'."),
			FUNC_NAME,
			"config_data table");
		return(-1);
	}

	if (read(fd, config_data, config_size) != config_size) {
		close(fd);
		acct_perr(ACCT_CAUT, errno,
			_("An error occurred during the reading of file '%s'."),
			config_file);
		return(-1);
	}
	close(fd);

	/*
	 *	File has been read in.
	 *	Now go through the buffer and replace new-line characters
	 *	with NULLs
	 */
	for (cp = config_data; cp < &config_data[config_size]; cp++)
		if (*cp == CARRIAGE_RETURN)
			*cp = __NULL_CHAR__;
	config_data[config_size] = __NULL_CHAR__;	/* just to be sure */

	return(0);
}

/*
 *	Get the next non-comment line from the file buffer.
 */
static int
__getline__(char *buf, int size)
{
	int	len;

	while (config_offset < config_size) {
		line++;
		len = strlen(&config_data[config_offset]);
		if (config_data[config_offset] == __COMMENT_CHAR__) {
			config_offset += len + 1;
			continue;
		}
		if (len >= size)
			return(0);
		memcpy(buf, &config_data[config_offset], len + 1);
		config_offset += len + 1;
		return((int)1);
	}
	return(0);
}

/*
 *	Get the value associated with the name.  The value is returned
 *	in resp_string[].  The value can contain alphanumerics and embedded
 *	".".
 */
static char *
__getent__(char *name, FILE *fd)
{
	char	*parambuf, *ncp, *cp, *cp2;
	long	match_flag = 0;
	int	delimited;

	/*
	 *	Get memory buffer for parameter lines.
	 */
	if ((parambuf = malloc(__PARAMSIZ__ + 1)) == NULL) {
		acct_perr(ACCT_CAUT, errno,
			_("%s: There was insufficient memory available when allocating '%s'."),
			FUNC_NAME,
			"config buffers");
		return(NULL);
	}

	/*
	 *	Scan each line.
	 */
	line = config_offset = 0;
	while (__getline__(parambuf, __PARAMSIZ__) != 0) {

		/*
		 *	Get rid of the newline if necessary.
		 */
		if (parambuf[strlen(parambuf)-1] == CARRIAGE_RETURN) {
			parambuf[strlen(parambuf)-1] = __NULL_CHAR__;
		}

		if (db_flag2 > 5) {
			Ndebug("parambuf line is: %s\n", parambuf);
		}
		/*
		 *	Remove data after comments.
		 */
		if ((cp = strchr(parambuf, __COMMENT_CHAR__)) != NULL) {
			*cp = __NULL_CHAR__;
		}

		/*
		 *	Scan line until first non-white character is seen.
		 */
		for (cp = &parambuf[0]; *cp != __NULL_CHAR__; cp++) {
			for (cp2 = &(__WHITE_CHARS__[0]); *cp2 != __NULL_CHAR__;
 					cp2++) {
				if (*cp == *cp2) {
					break;
				}
			}
			if (*cp == *cp2) {
				continue;
			}
			break;
		}

		/*
		 *	No non-white characters; skip line.
		 */
		if (*cp == __NULL_CHAR__) {
			if (db_flag2 > 5) {
				Ndebug("Skipping line %d\n", line);
			}
			continue;
		}

		/*
		 *	First token is the system parameter; extract it.
		 */
		ncp = cp;
		if (!isalpha(*ncp) && !(*ncp == __LAB_SPECIAL__)) {
			acct_err(ACCT_CAUT,
			       _("%s: Parameter token found on line %d starts with a non-alpha character."),
				FUNC_NAME, line);
			return(NULL);
		}

		for (; isalnum(*cp) || (*(cp) == __LAB_SPECIAL__); cp++);
		if (*cp != __NULL_CHAR__) {
			*cp = __NULL_CHAR__;
			cp++;
		}

		if (db_flag2 > 5) {
			Ndebug("__getent__: param <%s> rest <%s>\n", ncp, cp);
		}

		/*
		 *	A match!
		 *	Now get the associated value, otherwise try 
		 *		next line for a match.
		 */
		if (db_flag2 > 5) {
			Ndebug("name <%s> ncp <%s>\n", name, ncp);
		}
		if (!strcmp(name, ncp)) {
			++match_flag;

			/*
			 *	Skip the white space and separators between the
			 *	token and value.
			 */
			for (; *cp != __NULL_CHAR__; cp++) {
				for (cp2 = &(__WHITE_CHARS__[0]);
					*cp2 != __NULL_CHAR__; cp2++) {
					if (*cp == *cp2) {
						break;
					}
				}
				if (*cp == *cp2) {
					continue;
				}
				for (cp2 = &(__SEPARATORS__[0]);
					*cp2 != __NULL_CHAR__; cp2++) {
					if (*cp == *cp2) {
						break;
					}
				}
				if (*cp == *cp2) {
					continue;
				}
				break;
			}

			/*
			 *	Get the associated value.
			 */
			if (db_flag2 > 5) {
				Ndebug("cp is: <%s>\n", cp);
			}
			ncp = cp;

			/*
			 *	Get rid of the leading delimiter, if any.
			 */
			if (delimited = (*cp == __DELIMITER__)) {
				ncp = ++cp;
			}

			/*
			 *	Find the end of the value.
			 */
			for (; *cp != __NULL_CHAR__; cp++) {
				for (cp2 = &(__SPECIALS__[0]);
					*cp2 != __NULL_CHAR__; cp2++) {
					if (*cp == *cp2) {
						break;
					}
				}
				if (*cp == *cp2) {
					continue;
				}
				if (isalnum(*cp)) {
					continue;
				}
				if (delimited && (*cp == __DELIMITER__)) {
					break;
				}
			}

			/*
			 *	Last character in the string should be
			 *	a NULL.  Replace delimiter with this, if
			 *	necessary.
			 */
			if ((*cp != __NULL_CHAR__) || (*cp == __DELIMITER__)) {
				*cp = __NULL_CHAR__;
				cp++;
			}

			if (db_flag2 > 4) {
			    Ndebug("__getent__: Matched token <%s> ", name);
			    Ndebug(" with value ncp <%s>\n", ncp);
			}
			break;
		}
	}

	
	/*
	 *	Free the config memory buffer and return.
	 */
	if (match_flag == 0) {
		free(parambuf);
		return(NULL);
	}

	strcpy(resp_string, ncp);
	free(parambuf);
	return("Okay");
}
