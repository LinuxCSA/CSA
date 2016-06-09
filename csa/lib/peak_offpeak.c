/*
 * Copyright (c) 2004-2007 Silicon Graphics, Inc. All Rights Reserved.
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
 * This code calculates the ammount of peak and off-peak time used. All 
 * holidays and times are limited to this file.
 */

/* 
 * Original Author: Richard Offer
 * From a spec by: Jay Lan
 *
 */

#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "acctdef.h"
#include "acctmsg.h"

typedef struct day {
	time_t midnight;
	time_t start_peak;
	time_t start_offpeak;
	int wday; /* day of week, an optmization for weekday accounting */
} Day;

Day Days[366];

#define MAXLINELEN 1024
#define IS_WEEKDAY(d) ((d).wday > 0 && (d).wday < 6)
#define IS_SATURDAY(d) ((d).wday == 6)
#define IS_SUNDAY(d) ((d).wday == 0)

/* hack: we add 10 to tm_wday to indicate that the day is a holiday */
#define IS_HOLIDAY(d) ((d).wday > 7 )

#define PRIME 0
#define NONPRIME 1

static int parse_holiday_file(const char *filename);
static void parse_pop_times(const char *line, const char *field, time_t *peak, time_t *offpeak);


/* Given the start and the duration times, calculate how much corresponding
 * peak and off-peak time is used.
 *
 * Inputs:
 * 		start - seconds from 1970 when activity starts
 * 		duration - seconds 
 * Results
 *		pop_split[2] - first elelment is ammount of peak time,
 *                     second the off-peak time, both in seconds
 *
 * Basic Algoritm
 *
 *   We create a 366 element array (one element for every day of the year)
 *   and initially populate it with the time the day starts (Day.midnight)
 *
 *   Next we parse the holiday file to set the the default weekday peak/
 *   offpeak times. then do the same for Sat and Sun.
 *
 *   Finally we parse the Holidays listed in the file, we use the day
 *   number to lookup into the array when to set the holiday flag (but we
 *   also do a quick check to ensure the day or year corresponds to the date
 *   specified).
 *
 *
 *   That's it for the one-off parsing.
 *
 *   When we come to the calculation, its a simple matter of looping
 *   though every second from start to start+duration and checking to 
 *   see which category it fits in.
 */
void
pop_calc(time_t start, time_t duration, time_t pop_split[2])
{

	static int parsed_file=0;
	int i,j;

	pop_split[PRIME] = 0L;
	pop_split[NONPRIME] = 0L;



	/* only parse the holiday file once */
	if ( parsed_file == 0 ) {

		char *holiday_file = init_char(HOLIDAY_FILE, CSAHOLIDAYS, FALSE);

		if ( parse_holiday_file(holiday_file) )
			return;

		parsed_file = 1;
	}

	/* 
	 * okay we have parsed the HOLIDAY_FILE, so now calculate the peak/off-peak
	 * times...
	 */ 

	i=0;
	while ( Days[i].midnight < start ) {
		i++;
	}
	
	i--;

	for (j=start; j< start + duration ; j++ ) {

		if ( j == Days[i].midnight + SECSPERDAY ) {
			pop_split[NONPRIME]++;
			i++;
		}

		if (  j > Days[i].midnight && 
				j < ( Days[i].start_peak) ) {

			pop_split[NONPRIME]++;

		}
		else if ( j >= ( Days[i].start_peak )  && 
				j < ( Days[i].start_offpeak) ) {

			pop_split[PRIME]++;
		}

		else if ( j >= ( Days[i].start_offpeak ) && 
				j < ( Days[i].midnight + SECSPERDAY) ) {

			pop_split[NONPRIME]++;
		}
#ifdef DEBUG
		else {
			printf("%d %d %d %d %d\n",i, j, 
					Days[i].midnight, Days[i].start_peak,
					Days[i].start_offpeak,
					Days[i].midnight + SECSPERDAY );
		}
#endif

	}
	
}


static int
parse_holiday_file(const char *file)
{

	FILE *fp;
	char line[MAXLINELEN];
	int version=-1;
	int year=0;
	int current_year;
	struct tm *now, *jan1;
	time_t jan1_secs, now_secs;
	time_t peak_secs, offpeak_secs;
	time_t sat_secs, offsat_secs, sun_secs, offsun_secs;
	int i;


	fp = fopen(file, "r");

	now_secs = time(NULL);

	now = localtime(&now_secs);
	jan1 = localtime(&now_secs);
	/* reset to beginning of year */
	jan1->tm_sec = 0;
	jan1->tm_min = 0;
	jan1->tm_hour = 0;
	jan1->tm_mday = 1;
	jan1->tm_mon = 0;

	jan1_secs = mktime(jan1);
	
	current_year = now->tm_year+1900;
	
	
	/* if no holiday file is present, then what ? */
	if ( fp == NULL ) {

		perror("parse_holiday_file()");
		return 1;
	}


	while ( fgets(line,MAXLINELEN,fp) != NULL ) {

		char *s;
			
		if ( *line && line[0] != '*'  ) {

			if ( strstr(line, "HOLIDAYFILE_VERSION1" ) ) {
				version = 1;
			}
		
			if ( (s = strstr(line, "YEAR" )) != NULL ) {
				if ( sscanf(&s[4],"%4d",&year) == 0 ) {

					char wildcard[2];

					sscanf(&s[4],"%1s",wildcard);

					if ( wildcard[0] == '*' )
						year=current_year;

				}

			}

		}
	}

	if ( version != 1 ) {
		acct_err(ACCT_FATAL, "The holiday file does not specify that it is VERSION1");
		fclose(fp);
		return 1;
	}

	if ( year == 0 ) {
		acct_err(ACCT_FATAL, "The holiday file does not specify the YEAR.");
		fclose(fp);
		return 1;
	}

	/* initialize array of Days[] */

	for (i=0; i <= 366 ; i++ ) {

		Days[i].midnight = jan1_secs + (i*SECSPERDAY);
		Days[i].wday = (jan1->tm_wday + i) % 7 ;

	}


	/* rewind to start of the holiday file, just in case */

	rewind(fp);
	while ( fgets(line,MAXLINELEN,fp) != NULL ) {

		if ( *line && line[0] != '*'  ) {

			parse_pop_times(line, "weekday", &peak_secs, &offpeak_secs);
			parse_pop_times(line, "saturday", &sat_secs, &offsat_secs);
			parse_pop_times(line, "sunday", &sun_secs, &offsun_secs);

		}

	}

	/* rewind to start of the holiday file, just in case */
	/* now looking for holidays */
	rewind(fp);
	while ( fgets(line,MAXLINELEN,fp) != NULL ) {

		int hdoy,hday,hname;
		char hmon[MAXLINELEN];
		int ret;
		struct tm holiday;


		if ( *line && line[0] != '*'  ) {

			holiday.tm_year = year - 1900;


			if ( (ret=sscanf(line,"%d %6c %n",&hdoy, hmon, &hname)) == 2 ) {
				hmon[7] = '\0';

				strptime(hmon,"%b %d",&holiday);
				
				/* We ignore "Day of Year" field from the
				 * holidays file. Use the value from strptime
				 * instead.
				 */
				hdoy = holiday.tm_yday +1;

				Days[hdoy].wday += 10;

			}
#ifdef DEBUG
			else {
				printf("%d %s",ret,line);
			}
#endif

		}
	}

	/* 
	 * Look for all week/sat/sun/holidays, and update the days with correct 
	 * peak/off-peak times 
	 */
	for (i=0; i <= 366 ; i++ ) {

		if ( IS_WEEKDAY(Days[i]) ) {
			Days[i].start_peak = Days[i].midnight + peak_secs ;
			Days[i].start_offpeak = Days[i].midnight + offpeak_secs ;
		}

		if ( IS_SATURDAY(Days[i]) ) {
			Days[i].start_peak = Days[i].midnight + sat_secs ;
			Days[i].start_offpeak = Days[i].midnight + offsat_secs ;
		}

		if ( IS_SUNDAY(Days[i]) ) {
			Days[i].start_peak = Days[i].midnight + sun_secs ;
			Days[i].start_offpeak = Days[i].midnight + offsun_secs ;
		}

		if ( IS_HOLIDAY(Days[i]) ) {
			Days[i].start_peak = Days[i].midnight + SECSPERDAY - 1 ;
			Days[i].start_offpeak = Days[i].midnight + SECSPERDAY - 1 ;
		}

#ifdef DEBUG
		printf("%d %d %d %d %d\n", i, 
				Days[i].wday, Days[i].midnight, 
				Days[i].start_peak-Days[i].midnight,
				Days[i].start_offpeak-Days[i].midnight); 
#endif	
	}

	fclose(fp);

	return 0;
}

static void
parse_pop_times(const char *line, const char *field, time_t *peak, time_t *offpeak)
{

	char *s;
		
	if ( (s = strstr(line, field )) != NULL ) {
		int ph, pm, oh, om;
		char ps[16], ops[16];
		
		*peak = 0;
		*offpeak = 0;
	
		if ( sscanf(&s[strlen(field)],"%4s %4s",ps,ops) ) {

			if ( strstr(ps,"none" )) {
				*peak=SECSPERDAY-1;
				*offpeak=SECSPERDAY-1;
			}
			else if ( strstr(ps,"all")) {
				*peak=0;
				*offpeak=SECSPERDAY-1;
			}
			else {
				if ( sscanf(ps,"%02d%02d",&ph,&pm) == 0 )
					acct_err(ACCT_FATAL,"error parsing '%s'\n",ps);
				else
					*peak = (ph * 3600) + (pm * 60); 
				
				if ( sscanf(ops,"%02d%02d",&oh,&om) == 0 )
					acct_err(ACCT_FATAL,"error parsing '%s'\n",ops);
				else
					*offpeak = (oh * 3600) + (om * 60); 
			}
		}

		if ( *peak > *offpeak ) {
			acct_err(ACCT_FATAL, "%s time specification is incorrect, peak start time must be earlier than off-peak start time.", field);
		}

	}
	
}

#ifdef MAIN

/* A simple test harness for the algorithm */

int
main(int argc, char **argv)
{
	time_t start;
	time_t duration;

	time_t t[2];


	if ( argc != 3 )  {
		printf("Usage: %s <start time (time_t)> <duration>\n",argv[0]);
		printf("  TIme is now: %d\n",time(NULL));
		exit(1);
	}

	errno=0;
	start=strtol(argv[1], NULL, 10);
	duration=strtol(argv[2], NULL, 10);

	if ( errno ) {
		printf("Usage: %s <start time> <duration>\n",argv[0]);
		exit(1);
	}
	
	pop_calc(start, duration, t);

	printf("\n%sDuration:\t%d secs\n",ctime(&start), duration);
	printf("Peak Usage:\t%d\n",t[PRIME]);
	printf("Off-Peak Usage: %d\n",t[NONPRIME]);
}

#include "config.c"
#include "error.c"
#include "init_value.c"

#endif
