/*
 * This test checks the checkreplaywindow() function...
 */

#include <sys/limits.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef TDB_REPLAYWASTE
#define	TDB_REPLAYWASTE 32
#endif
#ifndef TDB_REPLAYMAX
#define TDB_REPLAYMAX	(2048+TDB_REPLAYWASTE)
#endif

#ifndef AH_HMAC_INITIAL_RPL
#define AH_HMAC_INITIAL_RPL 1
#endif

#define	MIN(a,b) (((a) < (b)) ? (a) : (b))

struct tdb {
	u_int32_t	tdb_flags;
	u_int64_t	tdb_rpl;
	u_int32_t	tdb_seen[howmany(TDB_REPLAYMAX, 32)];
} tdb;

#define TDBF_ESN                0x100000 /* 64-bit sequence numbers (ESN) */

#ifdef ESN
#include "esn.c"
#else
#include "no-esn.c"
#endif

int called;
int fail;
int maxwin = TDB_REPLAYMAX;

static int
checkreplay(struct tdb *tdb, u_int32_t seq)
{
	u_int32_t	esn;

	called++;
	return (checkreplaywindow(tdb, seq, &esn, 1));
}

static void
teq(int should, int have, const char *fmt, ...)
{
	int i;
	va_list args;

	if (have != should) {
		fprintf(stderr, "should %d != have %d for: ", should, have);
		va_start(args, fmt);
		vfprintf(stderr, fmt, args);
		va_end(args);
		fprintf(stderr, " [rpl %llu/%llu/%llu]\n",
		    tdb.tdb_rpl, (tdb.tdb_rpl%TDB_REPLAYMAX)/32, tdb.tdb_rpl&31);
		for (i = 0; i < TDB_REPLAYMAX/32; i++)
			fprintf(stderr, "%d: %8.8x\n", i, tdb.tdb_seen[i]);
		fail++;
	}
}

static void
tdb_reset(void)
{
	bzero(&tdb, sizeof(tdb));
	tdb.tdb_rpl = AH_HMAC_INITIAL_RPL;
}

void
runtests(void)
{
	u_int32_t i, j, step, done;

	tdb_reset();
	teq(1, checkreplay(&tdb, 0), "zero");
	for (i = 1; i < 3* maxwin; i++)
		teq(0, checkreplay(&tdb, i), "ok 1-3w: %d", i);
	i = 3 * maxwin - 1;
	teq(3, checkreplay(&tdb, i), "3w-1");
	for (i = 1; i < 2*maxwin; i++)
		teq(2, checkreplay(&tdb, i), "old 1-2w: %d", i);
	for (i = 2*maxwin; i < 3*maxwin; i++)
		teq(3, checkreplay(&tdb, i), "dup 2w-3w: %d", i);
	for (i = 3*maxwin; i < 10*maxwin; i++)
		teq(0, checkreplay(&tdb, i), "ok 3w-10w: %d", i);
	for (i = 10*maxwin; i < 11*maxwin; i+=2)
		teq(0, checkreplay(&tdb, i), "ok 10w-11w/2: %d", i);
	for (i = 10*maxwin; i < 11*maxwin; i+=2)
		teq(3, checkreplay(&tdb, i), "dup 10w-11w/2: %d", i);
	for (i = 10*maxwin+1; i < 11*maxwin; i+=2)
		teq(0, checkreplay(&tdb, i), "ok 10w-11w+1/2: %d", i);
	for (i = 10*maxwin+1; i < 11*maxwin; i+=2)
		teq(3, checkreplay(&tdb, i), "dup 10w-11w+1/2: %d", i);
	for (i = 1; i < 10*maxwin; i++)
		teq(2, checkreplay(&tdb, i), "old 1-10w+1/2: %d", i);
	for (i = 10*maxwin; i < 11*maxwin; i++)
		teq(3, checkreplay(&tdb, i), "dup 10w-11w: %d", i);
	for (i = 20*maxwin; i < 20*maxwin+maxwin/2; i++)
		teq(0, checkreplay(&tdb, i), "ok 20w-20.5w: %d", i);
	i = 20 * maxwin+maxwin/2 - 1;
	teq(3, checkreplay(&tdb, i), "dup 20.5w-1: %d", i);
	i = 20 * maxwin+maxwin/2 - 0;
	teq(0, checkreplay(&tdb, i), "ok 20.5w+0: %d", i);
	teq(3, checkreplay(&tdb, i), "dup 20.5w+0: %d", i);
	i = 20 * maxwin+maxwin/2 + 1;
	teq(0, checkreplay(&tdb, i), "ok 20.5w+1: %d", i);
	for (i = 20*maxwin+maxwin/2 +2; i < 21*maxwin; i++)
		teq(0, checkreplay(&tdb, i), "ok 20.5w-21: %d", i);
	i = 21*maxwin;
	teq(0, checkreplay(&tdb, i), "ok 21w: %d", i);
	i -= maxwin;
	teq(2, checkreplay(&tdb, i), "old 21w-w: %d", i);
	i++;
	teq(3, checkreplay(&tdb, i), "dup 21w-w+1: %d", i);

	tdb_reset();
	for (step = 1; step < 2*maxwin; step++) {
		for (j = 0; j < step; j++) {
			for (i = step*maxwin+j; i < (step+1)*maxwin; i+=step)
				teq(0, checkreplay(&tdb, i),
				    "step %d j %d i %d/%d", step, j, i, i%maxwin);
			for (i = step*maxwin+j; i < (step+1)*maxwin; i+=step)
				teq(3, checkreplay(&tdb, i),
				   "dup %d j %d i %d", step, j, i);
		}
	}

	tdb_reset();
	for (i = 1; i < 1000*maxwin; i+=2*maxwin+maxwin/3)
		teq(0, checkreplay(&tdb, i), "ok big %d", i);

	tdb_reset();
	for (i = 1; i < 100000*maxwin; i+=maxwin-1) {
		teq(0, checkreplay(&tdb, i), "ok clearmany %d", i);
		if (i%999 != 0)
			continue;
		for (j = i-1; j > i-maxwin+1 && j > 0; j--)
			teq(0, checkreplay(&tdb, j),
			    "ok clearmany i %d j %d delta %d", i, j, i-j);
	}

	tdb_reset();
	done = 0;
	for (i = maxwin-10; i < 100*1024*1024; i++) {
		teq(0, checkreplay(&tdb, i), "ok many %d", i);
		if (done)
			teq(3, checkreplay(&tdb, i-1), "dup many %d", i-1);
		else
			done =1;
	}
}

int
main(int argc, char **argv)
{
	maxwin=TDB_REPLAYMAX-TDB_REPLAYWASTE;
	printf("maxwin %d [%d,%d]\n", maxwin, TDB_REPLAYMAX, TDB_REPLAYWASTE);
	runtests();
	printf("%d times called checkreplaywindow(), %d errors\n", called, fail);
	return (fail);
}
