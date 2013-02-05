#define SEEN_SIZE	howmany(TDB_REPLAYMAX, 32)

#define CHECKREPLAY() ({					\
		if (tdb->tdb_seen[idx] & packet)		\
			return (3);				\
	})

#define SETREPLAY()	tdb->tdb_seen[idx] |= packet

#define UPDATERPL(s)	tdb->tdb_rpl = s

#define UPDATEWND()	({					\
		int 	i = (tl % TDB_REPLAYMAX) / 32;\
								\
		while (i != idx) {				\
			i = (i + 1) % SEEN_SIZE;		\
			tdb->tdb_seen[i] = 0;			\
		}						\
	})

#define CLEARWND()	bzero(tdb->tdb_seen, sizeof(tdb->tdb_seen))

/*
 * return 0 on success
 * return 1 for counter == 0
 * return 2 for very old packet
 * return 3 for packet within current window but already received
 */
int
checkreplaywindow(struct tdb *tdb, u_int32_t seq, u_int32_t *seqhigh,
    int commit)
{
	u_int32_t	tl, th, wl;
	u_int32_t	seqh, packet;
	u_int32_t	window = TDB_REPLAYMAX - TDB_REPLAYWASTE;
	int		idx, esn = tdb->tdb_flags & TDBF_ESN;

	tl = (u_int32_t)tdb->tdb_rpl;
	th = (u_int32_t)(tdb->tdb_rpl >> 32);

	/* Zero SN is not allowed */
	if ((esn && seq == 0 && tl <= AH_HMAC_INITIAL_RPL && th == 0) ||
	    (!esn && seq == 0))
		return (1);

	if (th == 0 && tl < window)
		window = tl;
	/* Current replay window starts here */
	wl = tl - window + 1;

	idx = (seq % TDB_REPLAYMAX) / 32;
	packet = 1 << (31 - (seq & 31));

	/*
	 * We keep the high part intact when:
	 * 1) the SN is within [wl, 0xffffffff] and the whole window is
	 *    within one subspace;
	 * 2) the SN is within [0, wl) and window spans two subspaces.
	 */
	if ((tl >= window - 1 && seq >= wl) ||
	    (tl <  window - 1 && seq <  wl)) {
		seqh = *seqhigh = th;
		if (seq > tl) {
			if (commit) {
				if (seq - tl > window)
					CLEARWND();
				else
					UPDATEWND();
				SETREPLAY();
				UPDATERPL(((u_int64_t)seqh << 32) | seq);
			}
		} else {
			if (tl - seq >= window)
				return (2);
			CHECKREPLAY();
			if (commit)
				SETREPLAY();
		}
		return (0);
	}

	/* Can't wrap if not doing ESN */
	if (!esn)
		return (2);

	/*
	 * SN is within [wl, 0xffffffff] and wl is within
	 * [0xffffffff-window, 0xffffffff].  This means we got a SN
	 * which is within our replay window, but in the previous
	 * subspace.
	 */
	if (tl < window - 1 && seq >= wl) {
		CHECKREPLAY();
		if (*seqhigh == 0)
			return (4);
		seqh = *seqhigh = th - 1;
		if (commit)
			SETREPLAY();
		return (0);
	}

	/*
	 * SN has wrapped and the last authenticated SN is in the old
	 * subspace.
	 */
	seqh = *seqhigh = th + 1;
	if (seqh == 0)		/* Don't let high bit to wrap */
		return (1);
	if (commit) {
		if (seq - tl > window)
			CLEARWND();
		else
			UPDATEWND();
		SETREPLAY();
		UPDATERPL(((u_int64_t)seqh << 32) | seq);
	}

	return (0);
}
