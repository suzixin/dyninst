/*
 * This file contains the implementation of runtime dynamic instrumentation
 *   functions for a processor running UNIX.
 *
 * $Log: RTunix.c,v $
 * Revision 1.33  1996/07/29 21:00:06  naim
 * Fixing race condition - naim
 *
 * Revision 1.32  1996/06/27  14:35:03  naim
 * Preventing race condition in DYNINSTgenerateTraceRecord (change made by Xu
 * and commited by Oscar) - naim
 *
 * Revision 1.31  1996/05/08  23:56:52  mjrg
 * added support for handling fork and exec in the application
 *
 * Revision 1.30  1996/04/09 22:20:56  newhall
 * changed DYNINSTgetWallTime to DYNINSTgetWalltime to fix undefined symbol
 * errors when applications are linked with libdyninstRT_cp.a
 *
 * Revision 1.29  1996/03/08  18:48:21  newhall
 * added wall and process time args to DYNINSTgenerateTraceRecord.  This fixes
 * a bug that occured when the appl. is paused between reading a timer to compute
 * a metric value and reading a timer again to compute a header value.
 *
 * Revision 1.28  1996/02/15  14:55:51  naim
 * Minor changes to timers and cost model - naim
 *
 * Revision 1.27  1996/02/01  17:49:54  naim
 * Fixing some problems related to timers and race conditions. I also tried to
 * make a more standard definition of certain procedures (e.g. reportTimer)
 * across all platforms - naim
 *
 * Revision 1.26  1995/11/03  00:06:25  newhall
 * initialize sampling rate to BASESAMPLEINTERVAL (defined in util/h/sys.h)
 * changed type of all DYNINSTsampleMultiple to "volatile int"
 *
 * Revision 1.25  1995/10/27  01:06:20  zhichen
 * Included <stdlib.h> to remove some warnings
 *
 * Revision 1.24  1995/10/19  22:44:45  mjrg
 * Removed breakpoint from DYNINSTinit.
 *
 * Revision 1.23  1995/05/18  11:08:28  markc
 * added guard prevent timer start-stop during alarm handler
 * added version number
 *
 * Revision 1.22  1995/02/16  09:07:33  markc
 * Made Boolean type RT_Boolean to prevent picking up a different boolean
 * definition.
 *
 * Revision 1.21  1994/11/11  10:39:14  markc
 * Commented out non-emergency printfs
 *
 * Revision 1.20  1994/10/09  21:26:05  hollings
 * Fixed cycles to time conversion.
 *
 * Revision 1.19  1994/09/20  18:26:51  hollings
 * added DYNINSTcyclesPerSecond to get cost values no matter the clock speed.
 *
 * removed a race condition in DYNINSTcyclesPerSecond.
 *
 * Revision 1.18  1994/08/22  16:05:37  markc
 * Removed lastValue array.
 * Added lastValue variable to timer structure.
 * Added error messages for timer regression.
 * Removed lastValue array.
 * Added lastValue variable to timer structure.
 * Added error messages for timer regression.
 *
 * Revision 1.17  1994/08/17  17:16:45  markc
 * Increased the size of the lastValue and lastTime arrays.  lastValue is
 * referenced by the timer id, which can be greater than 200.  There should
 * be some way of enforcing a limit between paradynd and rtinst.
 *
 * Revision 1.16  1994/08/02  18:18:57  hollings
 * added code to save/restore FP state on entry/exit to signal handle
 * (really jcargill, but commited by hollings).
 *
 * changed comparisons on time regression to use 64 bit int compares rather
 * than floats to prevent fp rounding error from causing false alarms.
 *
 * Revision 1.15  1994/07/26  20:04:48  hollings
 * removed slots used variables.
 *
 * Revision 1.14  1994/07/22  19:24:54  hollings
 * added actual paused time for CM-5.
 *
 * Revision 1.13  1994/07/16  03:39:31  hollings
 * stopped using assembly version of clocks (temporary).
 *
 * Revision 1.12  1994/07/14  23:35:36  hollings
 * added return of cost model record.
 *
 * Revision 1.11  1994/07/11  22:47:51  jcargill
 * Major CM5 commit: include syntax changes, some timer changes, removal
 * of old aggregation code, old pause code, added signal-driven sampling
 * within node processes
 *
 * Revision 1.10  1994/07/05  03:25:11  hollings
 * obsereved cost model.
 *
 * Revision 1.9  1994/05/18  00:53:28  hollings
 * added flush's after error printfs to force data out pipes on the way
 * to paradyn.
 *
 * Revision 1.8  1994/04/07  01:21:30  markc
 * Cleaned up writes.  Writes interrupted by system calls get retried, others
 * do not.
 *
 * Revision 1.7  1994/03/25  16:03:11  markc
 * Added retry to write which could be interrupted by a signal.
 *
 * Revision 1.6  1994/02/16  00:07:24  hollings
 * Added a default sampling interval of 500msec.  Previous default was not
 * to collect any data.
 *
 * Revision 1.5  1994/02/02  00:46:14  hollings
 * Changes to make it compile with the new tree.
 *
 * Revision 1.4  1993/12/13  19:48:12  hollings
 * force records to be word aligned.
 *
 * Revision 1.3  1993/10/19  15:29:58  hollings
 * new simpler primitives.
 *
 * Revision 1.2  1993/08/26  19:43:17  hollings
 * added uarea mapping code.
 *
 * Revision 1.1  1993/07/02  21:49:35  hollings
 * Initial revision
 *
 *
 */
#include <stdlib.h> /* atoi() */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/param.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <nlist.h>
#include <unistd.h>

#include "kludges.h"
#include "rtinst/h/rtinst.h"
#include "rtinst/h/trace.h"
#include "util/h/sys.h"

#define MILLION	1000000
extern int DYNINSTmappedUarea;
extern float DYNINSTcyclesToUsec;
extern int DYNINSTnumReported;
extern int DYNINSTtotalAlaramExpires;
extern int DYNINSTtotalSamples;
extern float DYNINSTsamplingRate;
extern time64 DYNINSTtotalSampleTime;
extern time64 DYNINSTlastCPUTime;
extern time64 DYNINSTlastWallTime;

/* prevents timers from being triggered during sample reporting */
extern int DYNINSTin_sample;

FILE *DYNINSTtraceFp;
tTimer DYNINSTelapsedTime;
tTimer DYNINSTelapsedCPUTime;
int64 DYNINSTgetObservedCycles(RT_Boolean);
time64 DYNINSTtotalSampleTime;
int64 DYNINSTgetObservedCycles();

/* clockWord must be volatile becuase it changes on clock interrups. 
 *    -- added volatile jkh 7/3/94
 *
 */
typedef volatile unsigned int clockWord;
extern clockWord *_p_1, *_p_2, *_p_3, *_p_4;

/*
 * Missing stuff.
 *
 */
extern int getrusage(int who, struct rusage *rusage);

/*
 * return cpuTime in useconds.
 *
 */
time64 DYNINSTgetCPUtime()
{
    time64 now;
    static time64 previous=0;
    struct rusage ru;
    int userTimeSec, systemTimeSec;

    if (DYNINSTmappedUarea) {
retry:
	 userTimeSec = *_p_1;
         systemTimeSec = *_p_3;
	 now = (time64)userTimeSec + (time64)systemTimeSec;
	 now *= (time64)1000000;
	 now += (time64)(*_p_2) + (time64)(*_p_4); /* User and System times */
                                                   /* in usecs              */
	 if (*_p_1 != userTimeSec ||             
             *_p_3 != systemTimeSec) goto retry;    
         if (now<previous) {
           goto retry;
	 }
         previous=now;
         return(now);
    }

try_again:    
    if (!getrusage(RUSAGE_SELF, &ru)) {
      now = (time64)ru.ru_utime.tv_sec + (time64)ru.ru_stime.tv_sec;
      now *= (time64)1000000;
      now += (time64)ru.ru_utime.tv_usec + (time64)ru.ru_stime.tv_usec;
      if (now<previous) {
        goto try_again;
      }
      previous=now;
      return(now);
    }
    else {
      perror("getrusage");
      abort();
    }
}

time64 DYNINSTgetWalltime()
{
     time64 now;
     struct timeval tv;

     gettimeofday(&tv, NULL);
     now = (time64) tv.tv_sec;
     now *= (time64) 1000000;
     now += (time64) tv.tv_usec;

     return(now);
}

inline time64 DYNINSTgetUserTime()
{
    int userTimeSec, systemTimeSec;
    time64 now;

    if (DYNINSTmappedUarea) {
retry:
	 userTimeSec = *_p_1;
         systemTimeSec = *_p_3;
	 now = (time64)userTimeSec + (time64)systemTimeSec;
	 now *= (time64)1000000;
	 now += (time64)(*_p_2) + (time64)(*_p_4); /* User and System times */
                                                   /* in usecs              */
	 if (*_p_1 != userTimeSec ||             
             *_p_3 != systemTimeSec) goto retry;         
     } else {
	 now = DYNINSTgetCPUtime();
     }
     return(now);
}


void DYNINSTbreakPoint()
{
/*     printf ("Process %d about to stop self...\n", getpid()); */
/*     fflush (stdout); */
    kill(getpid(), SIGSTOP);
/*     printf ("Process %d has been restarted...\n", getpid()); */
/*     fflush (stdout); */
}

void DYNINSTstartProcessTimer(tTimer *timer)
{
    /* events that trigger timer starts during trace flushes are to be 
       ignored */

    if (DYNINSTin_sample) return;

    if (timer->counter == 0) {
	 timer->start = DYNINSTgetCPUtime();
	 timer->normalize = MILLION;
    }
    /* this must be last to prevent race conditions */
    timer->counter++;
}

void DYNINSTstopProcessTimer(tTimer *timer)
{
    time64 now;

    /* events that trigger timer stops during trace flushes are to be 
       ignored */
    if (DYNINSTin_sample) return;

    /* don't stop a counter that is not running */
    if (!timer->counter) return;

    /* Warning - there is a window between setting now, and mutex that
       can cause time to go backwards by the time to execute the
       instructions between these two points.  This is not a cummlative error
       and should not affect samples.  This was done (rather than re-sampling
       now because the cost of computing now is so high).
    */
    if (timer->counter == 1) {
	now = DYNINSTgetCPUtime();
	timer->snapShot = now - timer->start + timer->total;
	timer->mutex = 1;
        /*                 
         * The reason why we do the following line in that way is because
         * a small race condition: If the sampling alarm goes off
         * at this point (before timer->mutex=1), then time will go backwards 
         * the next time a sample is take (if the {wall,process} timer has not
         * been restarted).
         */
	timer->total = DYNINSTgetCPUtime() - timer->start + timer->total;
	if (now < timer->start) {
            printf("id=%d, snapShot=%f total=%f, \n start=%f  now=%f\n",
                   timer->id.id, (double)timer->snapShot,
                   (double)timer->total, 
                   (double)timer->start, (double)now);
            printf("RTunix: process timer rollback\n"); fflush(stdout);
            abort();
	}
	timer->counter = 0;
	timer->mutex = 0;
    } else {
	timer->counter--;
    }
}


void DYNINSTstartWallTimer(tTimer *timer)
{
    /* if "write" is instrumented to start timers, a timer could be started */
    /* when samples are being written back */
    if (DYNINSTin_sample) return;       
    if (timer->counter == 0) {
        timer->start     = DYNINSTgetWalltime();
        timer->normalize = MILLION;
    }
    timer->counter++;
}

void DYNINSTstopWallTimer(tTimer *timer)
{
    /* if "write" is instrumented to start timers, a timer could be started */
    /* when samples are being written back */
    if (DYNINSTin_sample) return;       

    if (!timer->counter) {
        return;
    }

    if (timer->counter == 1) {
        time64 now = DYNINSTgetWalltime();

        timer->snapShot = now - timer->start + timer->total;
        timer->mutex    = 1;
        /*                 
         * The reason why we do the following line in that way is because
         * a small race condition: If the sampling alarm goes off
         * at this point (before timer->mutex=1), then time will go backwards 
         * the next time a sample is take (if the {wall,process} timer has not
         * been restarted).
         */
        timer->total    = DYNINSTgetWalltime() - timer->start + timer->total;
        if (now < timer->start) {
            printf("id=%d, snapShot=%f total=%f, \n start=%f  now=%f\n",
                   timer->id.id, (double)timer->snapShot,
                   (double)timer->total, 
                   (double)timer->start, (double)now);
            printf("RTunix: wall timer rollback\n");
            fflush(stdout);
            abort();
        }
        timer->counter  = 0;
        timer->mutex    = 0;
    }
    else {
        timer->counter--;
    }
}

time64 startWall;

volatile int DYNINSTpauseDone = 0;

/*
 * change the variable to let the process proceed.
 *
 */
void DYNINSTcontinueProcess()
{
    DYNINSTpauseDone = 1;
}

#ifdef notdef
int DYNINSTbufsiz;
int DYNINSTprofile;
#endif

/*
 * pause the process and let only USR2 signal handlers run until a SIGUSR1.
 *    arrives.
 *
 */
void DYNINSTpauseProcess()
{
    int mask;
    int sigs;

    sigs = ((1 << (SIGUSR2-1)) | (1 << (SIGUSR1-1)) | (1 << (SIGTSTP-1)));
    mask = ~sigs;
    DYNINSTpauseDone = 0;
    while (!DYNINSTpauseDone) {
#ifdef notdef
       sigpause(mask);
       /* temporary busy wait until we figure out what the TSD is up to.  */
       printf("out of sigpuase\n");
#endif
    }
}


void DYNINSTinit(int skipBreakpoint)
{
    int val;
#ifdef n_def
    char *interval;
#endif
    struct sigvec alarmVector;
    extern int DYNINSTmapUarea();
    extern float DYNINSTcyclesPerSecond();
    extern void DYNINSTalarmExpire();

    startWall = 0;

    /*
     * clear global base tramp and cycle counters.
     *
     */
#ifdef notdef
    asm("mov 0, %g7");
#endif

    /* init these before the first alarm can expire */
    DYNINSTcyclesToUsec = (1.0/DYNINSTcyclesPerSecond()) * 1000000;
    DYNINSTlastCPUTime = DYNINSTgetCPUtime();
    DYNINSTlastWallTime = DYNINSTgetWalltime();

    /* define the alarm signal vector. We block all signals while sampling.  
     *  This prevents race conditions where signal handlers cause timers to 
     *  be started and stopped.
     */
    alarmVector.sv_handler = DYNINSTalarmExpire;
    alarmVector.sv_mask = ~0;
    alarmVector.sv_flags = 0;

    sigvec(SIGALRM, &alarmVector, NULL);

    /* All the signal-initiating stuff is now in the paradynd. */

#ifdef n_def

    /* default is 500msec */
    val = 500000;
    interval = (char *) getenv("DYNINSTsampleInterval");
    if (interval) {
	val = atoi(interval);
    }
#endif

    /* set sampling interval to default value in util/h/sys.h */
    val = BASESAMPLEINTERVAL;

    DYNINSTsamplingRate = val/1000000.0;
    sigvec(SIGALRM, &alarmVector, NULL);
    ualarm(val, val);

    DYNINSTmappedUarea = DYNINSTmapUarea();

    /*
     * pause the process and wait for additional info.
     *
     */
    /* printf("Time at main %f\n", ((float) DYNINSTgetCPUtime())/1000000.0); */
    /* if (!skipBreakpoint) DYNINSTbreakPoint(); */

    DYNINSTstartWallTimer(&DYNINSTelapsedTime);
    DYNINSTstartProcessTimer(&DYNINSTelapsedCPUTime);

}


void DYNINSTexit()
{
}

void DYNINSTflushTrace()
{
    if (DYNINSTtraceFp) fflush(DYNINSTtraceFp);
}

static int pipeGone;

/*
 * generate a trace record onto the named stream.
 *
 */
void DYNINSTgenerateTraceRecord(traceStream sid, short type, short length,
    void *eventData, int flush,time64 wall_time,time64 process_time)
{
    int ret;
    int count;
    char buffer[1024];
    static unsigned inDYNINSTgenerateTraceRecord = 0;
    traceHeader header;
    struct timeval tv;

    if (inDYNINSTgenerateTraceRecord) return;
    inDYNINSTgenerateTraceRecord = 1;

    if (pipeGone) {
      inDYNINSTgenerateTraceRecord = 0;
      return;
    } 

#ifdef ndef
    gettimeofday(&tv, NULL);
    header.wall = tv.tv_sec;
    header.wall *= (time64) MILLION;
    header.wall += tv.tv_usec;
    header.wall -= startWall;
    header.process = DYNINSTgetCPUtime();
#endif
    header.wall = wall_time;
    header.process = process_time;

#ifdef notdef
    if (DYNINSTmappedUarea) {
	header.process = *_p_1;
	header.process *= MILLION;
	header.process += *_p_2;
    } else {
    }
#endif

    /* round length off to a word aligned unit */
    length = ALIGN_TO_WORDSIZE(length);

    header.type = type;
    header.length = length;
    count = 0;
    memcpy(&buffer[count], &sid, sizeof(traceStream));
    count += sizeof(traceStream);

    memcpy(&buffer[count], &header, sizeof(header));
    count += sizeof(header);

    count = ALIGN_TO_WORDSIZE(count);
    memcpy(&buffer[count], eventData, length);
    count += length;

    /* on this platorm, we have a pipe to the controller process */
    errno = 0;

    if (!DYNINSTtraceFp || (type == TR_EXIT)) {
	DYNINSTtraceFp = fdopen(dup(CONTROLLER_FD), "w");
    }
    ret = fwrite(buffer, count, 1, DYNINSTtraceFp);
    if (ret != 1) {
	extern char *sys_errlist[];
	printf("unable to write trace record %s\n", sys_errlist[errno]);
	printf("disabling further data logging, pid=%d\n", getpid());
	fflush(stdout);
	/* pipeGone = True; */
    }
    if (flush)
       DYNINSTflushTrace();
    inDYNINSTgenerateTraceRecord = 0;
}


/* time64 lastValue[10000]; */
/* double lastTime[200]; */

void DYNINSTreportTimer(tTimer *timer)
{
    time64 now=0;
    time64 total;
    traceSample sample;
    time64 process_time;
    time64 wall_time;

    process_time = DYNINSTgetCPUtime();
    wall_time = DYNINSTgetWalltime();

    if (timer->mutex) {
	total = timer->snapShot;
    } else if (timer->counter) {
	/* timer is running */
	if (timer->type == processTime) {
	    now = process_time;
	} else {
            now = wall_time;
	}
	total = now - timer->start + timer->total;
    } else {
	total = timer->total;
    }

    if (total < timer->lastValue) {
	if (timer->type == processTime) {
	    printf("process ");
	} else {
	    printf("wall ");
	}
	printf("RTunix: time regressed timer %d, total = %f, last = %f\n",
	       timer->id.id, (float) total, (float) timer->lastValue);
	if (timer->counter) {
	    printf("timer was active\n");
	} else {
	    printf("timer was inactive\n");
	}
        printf("mutex=%d, counter=%d, sampled=%d, snapShot=%f\n\n",
	       (int)timer->mutex, (int)timer->counter, (int)timer->sampled,
	       (double) timer->snapShot);
	printf("now = %f\n start = %f\n total = %f\n",
	       (double) now, (double) timer->start, (double) timer->total);
	fflush(stdout);
	abort();
    }
    timer->lastValue = total;

    sample.id = timer->id;
    sample.value = ((double) total) / (double) timer->normalize;
    DYNINSTtotalSamples++;

    wall_time -= startWall;

    DYNINSTgenerateTraceRecord(0, TR_SAMPLE, sizeof(sample), &sample, 0,
			       wall_time, process_time);
    /* printf("raw sample %d = %f\n", sample.id.id, sample.value); */
}

void DYNINSTfork(void *arg, int pid)
{
    int sid = 0;
    traceFork forkRec;
    time64 process_time;
    time64 wall_time;

    process_time = DYNINSTgetCPUtime();
    wall_time = DYNINSTgetWalltime();
    wall_time -= startWall;

    printf("fork called with pid = %d\n", pid);
    fflush(stdout);
    if (pid > 0) {
	forkRec.ppid = getpid();
	forkRec.pid = pid;
	forkRec.npids = 1;
	forkRec.stride = 0;
	DYNINSTgenerateTraceRecord(sid,TR_FORK,sizeof(forkRec), &forkRec, 1,
				wall_time, process_time);
    } else {
	/* set up signals and stop at a break point */
	DYNINSTinit(1);
	sigpause(0);
    }
}

void DYNINSTexec() {
    /* not implemented on this platform */
}

void DYNINSTexecFailed() {
    /* not implemented on this platform */
}

void DYNINSTprintCost()
{
    FILE *fp;
    time64 now;
    int64 value;
    struct rusage ru;
    struct endStatsRec stats;
    time64 process_time;
    time64 wall_time;

    process_time = DYNINSTgetCPUtime();
    wall_time = DYNINSTgetWalltime();
    wall_time -= startWall;

    DYNINSTstopProcessTimer(&DYNINSTelapsedCPUTime);
    DYNINSTstopWallTimer(&DYNINSTelapsedTime);

    value = DYNINSTgetObservedCycles(0);
    stats.instCycles = value;

    value *= DYNINSTcyclesToUsec;

    stats.alarms = DYNINSTtotalAlaramExpires;
    stats.numReported = DYNINSTnumReported;
    stats.instTime = ((double) value)/1000000.0;
    stats.handlerCost = ((double) DYNINSTtotalSampleTime)/1000000.0;

    now = DYNINSTgetCPUtime();
    stats.totalCpuTime = ((double) DYNINSTelapsedCPUTime.total)/MILLION;
    stats.totalWallTime = ((double) DYNINSTelapsedTime.total/MILLION);

    stats.samplesReported = DYNINSTtotalSamples;
    stats.samplingRate = DYNINSTsamplingRate;

    stats.userTicks = 0;
    stats.instTicks = 0;

    fp = fopen("stats.out", "w");

#ifdef notdef
    if (DYNINSTprofile) {
	int i;
	int limit;
	int pageSize;
	int startInst;
	extern void DYNINSTreportCounter();


	limit = DYNINSTbufsiz/sizeof(short);
	/* should really be _DYNINSTendUserCode */
	startInst = (int) &DYNINSTreportCounter;
	fprintf(fp, "startInst = %x\n", startInst);
	for (i=0; i < limit; i++) {
	    if (DYNINSTprofBuffer[i])
		fprintf(fp, "%x %d\n", i*2, DYNINSTprofBuffer[i]);
	    if (i * 2 > startInst) {
		stats.instTicks += DYNINSTprofBuffer[i];
	    } else {
		stats.userTicks += DYNINSTprofBuffer[i];
	    }
	}

	/* fwrite(DYNINSTprofBuffer, DYNINSTbufsiz, 1, fp); */
	fprintf(fp, "stats.instTicks %d\n", stats.instTicks);
	fprintf(fp, "stats.userTicks %d\n", stats.userTicks);

    }
#endif
    getrusage(RUSAGE_SELF, &ru);

    fprintf(fp, "DYNINSTtotalAlaramExpires %d\n", stats.alarms);
    fprintf(fp, "DYNINSTnumReported %d\n", stats.numReported);
    fprintf(fp,"Raw cycle count = %f\n", (double) stats.instCycles);
    fprintf(fp,"Total instrumentation cost = %f\n", stats.instTime);
    fprintf(fp,"Total handler cost = %f\n", stats.handlerCost);
    fprintf(fp,"Total cpu time of program %f\n", stats.totalCpuTime);
    fprintf(fp,"Total system time of program %f\n", 
	 ru.ru_stime.tv_sec * 1000000.0 + ru.ru_stime.tv_usec);
    fprintf(fp,"Elapsed wall time of program %f\n",
	stats.totalWallTime/1000000.0);
    fprintf(fp,"total data samples %d\n", stats.samplesReported);
    fprintf(fp,"sampling rate %f\n", stats.samplingRate);
    fprintf(fp,"Application program ticks %d\n", stats.userTicks);
    fprintf(fp,"Instrumentation ticks %d\n", stats.instTicks);

    fclose(fp);

    /* record that we are done -- should be somewhere better. */
    DYNINSTgenerateTraceRecord(0, TR_EXIT, sizeof(stats), &stats, 1,
				wall_time,process_time);
}


/*
 * These are null routines for Unix: signal handler semantics
 * guarantee that the FPU state is saved/restored.  Not so on CM5 nodes! 
 */

void saveFPUstate(float *base)
{
}


void restoreFPUstate(float *base)
{
}


/*
 * If we can't read it, try to guess it.
 */
static float guessClock()
{
    return(0.0);
}

#define PATTERN	"\tclock-frequency:"
/*
 * find the number of cycles per second on this machine.
 */
float DYNINSTcyclesPerSecond()
{
    FILE *fp;
    char *ptr;
    int speed;
    char line[80];

    fp = popen("/usr/etc/devinfo -vp", "r");
    if (!fp) {
	/* can't run command so guess the clock rate. */
    } else {
	while (fgets(line, sizeof(line), fp)) {
	    if (!strncmp(PATTERN, line, strlen(PATTERN))) {
		ptr = strchr(line, ' ');
		if (!ptr) return(guessClock());
		speed = strtol(ptr, NULL, 16);
		break;
	    }
	}
    }
    pclose(fp);
    /* printf(line, "Clock = %d\n", speed); */
    return(speed);
}
