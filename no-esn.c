int
checkreplaywindow(struct tdb *tdb, u_int32_t seq, u_int32_t *seqhigh,
    int commit)
{
	u_int32_t window, diff, packet;
	int i, idx;

	if (tdb->tdb_flags & TDBF_ESN) {
		*seqhigh = 0;
		return (1);	/* XXX FIXME */
	}

	window = TDB_REPLAYMAX - TDB_REPLAYWASTE;	/* wasted bits */
	if (seq == 0)
		return (1);	/* invalid */
	if (seq > tdb->tdb_rpl) {
		if (commit) {
			diff = seq - tdb->tdb_rpl;
			idx = (seq % TDB_REPLAYMAX)/32;
			if (diff > window) {
				memset(tdb->tdb_seen, 0, sizeof(tdb->tdb_seen));
			} else {
				i = (tdb->tdb_rpl % TDB_REPLAYMAX)/32;
				while (i != idx) {
					i = (i+1) % (TDB_REPLAYMAX/32);
					tdb->tdb_seen[i] = 0;
				}
			}
			packet = 1 << (31-(seq&31));
			tdb->tdb_seen[idx] |= packet;
			tdb->tdb_rpl = seq;
		}
		return (0);
	}
	diff = tdb->tdb_rpl - seq;
	if (diff >= window) 
		return (2);	/* too old */
	idx = (seq % TDB_REPLAYMAX)/32;
	packet = 1 << (31-(seq&31));
	if (tdb->tdb_seen[idx] & packet)
		return (3);	/* duplicate */
	if (commit) 
		tdb->tdb_seen[idx] |= packet;
	return (0);
}
