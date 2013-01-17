static __inline void
setreplay(struct tdb *tdb, int idx, u_int32_t diff, u_int32_t packet,
    int wupdate)
{
	if (wupdate) {
		if (diff < TDB_REPLAYMAX - TDB_REPLAYWASTE) {
			int i = (tdb->tdb_rpl % TDB_REPLAYMAX) / 32;
			while (i != idx) {
				i = (i + 1) % howmany(TDB_REPLAYMAX, 32);
				tdb->tdb_seen[i] = 0;
			}
		} else
			memset(tdb->tdb_seen, 0, sizeof(tdb->tdb_seen));
	}
	tdb->tdb_seen[idx] |= packet;
}

int
checkreplaywindow(struct tdb *tdb, u_int32_t seq, u_int32_t *seqhigh,
    int commit)
{
	u_int32_t	tl, th, wl;
	u_int32_t	diff, packet, seqh, window;
	int		idx, esn = tdb->tdb_flags & TDBF_ESN;

	tl = (u_int32_t)tdb->tdb_rpl;
	th = (u_int32_t)(tdb->tdb_rpl >> 32);

	/* Zero SN is not allowed */
	if ((esn && seq == 0 && tl == 0 && th == 0) ||
	    (!esn && seq == 0))
		return (1);

	window = TDB_REPLAYMAX - TDB_REPLAYWASTE;

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
		seqh = th;
		if (seq > tl) {
			if (commit) {
				setreplay(tdb, idx, seq - tl, packet, 1);
				tdb->tdb_rpl = seq;
			}
		} else {
			if (tl - seq >= window)
				return (2);
			if (tdb->tdb_seen[idx] & packet)
				return (3);
			if (commit)
				setreplay(tdb, idx, tl - seq, packet, 0);
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
		seqh = th - 1;
		diff = (u_int32_t)((((u_int64_t)th << 32) | tl) -
		    (((u_int64_t)seqh << 32) | seq));
		if (tdb->tdb_seen[idx] & packet)
			return (3);
		if (commit)
			setreplay(tdb, idx, diff, packet, 0);
		return (0);
	}

	/*
	 * SN has wrapped and the last authenticated SN is in the old
	 * subspace.
	 */
	if (tl + seq + 1 >= window + 1)
		return (2);
	seqh = *seqhigh = th + 1;
	if (seqh == 0)		/* Don't let high bit to wrap */
		return (1);

	if (commit) {
		diff = (u_int32_t)((((u_int64_t)seqh << 32) | seq) -
		    (((u_int64_t)th << 32) | tl));
		setreplay(tdb, idx, diff, packet, 0);
		tdb->tdb_rpl = ((u_int64_t)seqh << 32) | seq;
	}

	return (0);
}
