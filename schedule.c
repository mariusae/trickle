/*
 * schedule.c
 *
 * Copyright (c) 2009-2015 Matthew Blythe <mblythester@gmail.com>
 * All rights reserved.
 *
 */

#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif /* HAVE_SYS_TIME_H */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif /* HAVE_STDINT_H */
#if defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <time.h>
#endif /* defined(HAVE_TIME_H) && defined(TIME_WITH_SYS_TIME) */
#include <stdlib.h>

#include "trickle.h"
#include "schedule.h"

void
schedString(char* sched, uint* bwList, const char* updown, print_func print)
{
	int i,j;
	int day[8];
	char* place=NULL;
	uint defaultBw = atoi(sched)*1024;
	print(1,"Default %s Bandwidth: %u bps",updown,defaultBw);
	for(i=0; i<(7*24*DIVS_PER_HOUR); i++){
		bwList[i] = defaultBw;
	}
	while(1){
		for(i=0;i<7;i++)
			day[i]=0;
		day[7]=1;
		while(sched[0]!='\0' && sched[0]!=':')
			sched++;
		if(sched[0]=='\0')
			return;
		sched++;
		while(place != sched){
			place = sched;
			if(sched[0]=='S' && sched[1]=='u'){
				day[0]=1;
				day[7]=0;
				sched += 2;
			}
			if(sched[0]=='M'){
				day[1]=1;
				day[7]=0;
				sched++;
			}
			if(sched[0]=='T'){
				day[2]=1;
				day[7]=0;
				sched++;
			}
			if(sched[0]=='W'){
				day[3]=1;
				day[7]=0;
				sched++;
			}
			if(sched[0]=='T' && sched[1]=='h'){
				day[4]=1;
				day[7]=0;
				sched += 2;
			}
			if(sched[0]=='F'){
				day[5]=1;
				day[7]=0;
				sched++;
			}
			if(sched[0]=='S' && sched[1]=='a'){
				day[6]=1;
				day[7]=0;
				sched += 2;
			}
		}
		uint time1,hr1,div1,time2,hr2,div2,bw;
		time1 = atoi(sched);
		hr1 = (time1/100)%24;
		div1 = ((time1%100)/(60/DIVS_PER_HOUR))%DIVS_PER_HOUR;
		while(sched[0]!='\0' && sched[0]!=',')
			sched++;
		if(sched[0]=='\0')
			return;
		sched++;
		time2 = atoi(sched);
		hr2 = (time2/100)%24;
		div2 = ((time2%100)/(60/DIVS_PER_HOUR))%DIVS_PER_HOUR;
		while(sched[0]!='\0' && sched[0]!=',')
			sched++;
		if(sched[0]=='\0')
			return;
		sched++;
		bw = atoi(sched);
		bw *= 1024;
		for(i=0; i<7; i++){
			if(day[i]||day[7]){
				 print(1,"changing %s bandwidth on day %u, between %u:%02u and %u:%02u to %u bps",
						 updown, i, hr1, div1*(60/DIVS_PER_HOUR), hr2, div2*(60/DIVS_PER_HOUR), bw);
				 for(j = i*(24*DIVS_PER_HOUR) + hr1*DIVS_PER_HOUR + div1;
						 j < i*(24*DIVS_PER_HOUR) + hr2*DIVS_PER_HOUR + div2; j++){
					 bwList[j] = bw;
				 }
			}
		}
	}
}

uint
getSchedIndex()
{
	time_t rawtime;
	struct tm * timeinfo;
	uint index;

	time ( &rawtime );
	timeinfo = localtime ( &rawtime );
	index = (timeinfo->tm_wday*24*DIVS_PER_HOUR)
		+ (timeinfo->tm_hour*DIVS_PER_HOUR)
		+ (timeinfo->tm_min/(60/DIVS_PER_HOUR));

	return index;
}

