/*
 * driver utility functions
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_p2p.c 275511 2011-08-04 00:45:30Z $
 */


#include <typedefs.h>
#include <wlc_utils.h>

void
wlc_uint64_add(uint32* high, uint32* low, uint32 inc_high, uint32 inc_low)
{
	uint32 old_l;
	uint32 new_l;

	old_l = *low;
	new_l = old_l + inc_low;
	*low = new_l;
	if (new_l < old_l) {
		/* carry */
		inc_high += 1;
	}
	*high += inc_high;
}

void
wlc_uint64_sub(uint32* a_high, uint32* a_low, uint32 b_high, uint32 b_low)
{
	if (b_low > *a_low) {
		/* low half needs a carry */
		b_high += 1;
	}
	*a_low -= b_low;
	*a_high -= b_high;
}

bool
wlc_uint64_lt(uint32 a_high, uint32 a_low, uint32 b_high, uint32 b_low)
{
	return (a_high < b_high ||
		(a_high == b_high && a_low < b_low));
}

/* Given the beacon interval in kus, and a 64 bit TSF in us,
 * return the offset (in us) of the TSF from the last TBTT
 */
uint32
wlc_calc_tbtt_offset(uint32 bp, uint32 tsf_h, uint32 tsf_l)
{
	uint32 k, btklo, btkhi, offset;

	/* TBTT is always an even multiple of the beacon_interval,
	 * so the TBTT less than or equal to the beacon timestamp is
	 * the beacon timestamp minus the beacon timestamp modulo
	 * the beacon interval.
	 *
	 * TBTT = BT - (BT % BIu)
	 *      = (BTk - (BTk % BP)) * 2^10
	 *
	 * BT = beacon timestamp (usec, 64bits)
	 * BTk = beacon timestamp (Kusec, 54bits)
	 * BP = beacon interval (Kusec, 16bits)
	 * BIu = BP * 2^10 = beacon interval (usec, 26bits)
	 *
	 * To keep the calculations in uint32s, the modulo operation
	 * on the high part of BT needs to be done in parts using the
	 * relations:
	 * X*Y mod Z = ((X mod Z) * (Y mod Z)) mod Z
	 * and
	 * (X + Y) mod Z = ((X mod Z) + (Y mod Z)) mod Z
	 *
	 * So, if BTk[n] = uint16 n [0,3] of BTk.
	 * BTk % BP = SUM((BTk[n] * 2^16n) % BP , 0<=n<4) % BP
	 * and the SUM term can be broken down:
	 * (BTk[n] *     2^16n)    % BP
	 * (BTk[n] * (2^16n % BP)) % BP
	 *
	 * Create a set of power of 2 mod BP constants:
	 * K[n] = 2^(16n) % BP
	 *      = (K[n-1] * 2^16) % BP
	 * K[2] = 2^32 % BP = ((2^16 % BP) * 2^16) % BP
	 *
	 * BTk % BP = BTk[0-1] % BP +
	 *            (BTk[2] * K[2]) % BP +
	 *            (BTk[3] * K[3]) % BP
	 *
	 * Since K[n] < 2^16 and BTk[n] is < 2^16, then BTk[n] * K[n] < 2^32
	 */

	/* BTk = BT >> 10, btklo = BTk[0-3], bkthi = BTk[4-6] */
	btklo = (tsf_h << 22) | (tsf_l >> 10);
	btkhi = tsf_h >> 10;

	/* offset = BTk % BP */
	offset = btklo % bp;

	/* K[2] = ((2^16 % BP) * 2^16) % BP */
	k = (uint32)(1<<16) % bp;
	k = (uint32)(k * 1<<16) % (uint32)bp;

	/* offset += (BTk[2] * K[2]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	/* BTk[3] */
	btkhi = btkhi >> 16;

	/* k[3] = (K[2] * 2^16) % BP */
	k = (k << 16) % bp;

	/* offset += (BTk[3] * K[3]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	offset = offset % bp;

	/* convert offset from kus to us by shifting up 10 bits and
	 * add in the low 10 bits of tsf that we ignored
	 */
	offset = (offset << 10) + (tsf_l & 0x3FF);

#ifdef DEBUG_TBTT
	{
	uint32 offset2 = tsf_l % ((uint32)bp << 10);
	/* if the tsf is still in 32 bits, we can check the calculation directly */
	if (offset2 != offset && tsf_h == 0) {
		WL_ERROR(("tbtt offset calc error, offset2 %d offset %d\n",
		          offset2, offset));
	}
	}
#endif /* DEBUG_TBTT */

	return offset;
}

/* estimate the next position based on the current TSF time and the interval */
/* 'cur' must be in local TSF time */
uint32
wlc_calc_next_pos32(uint32 tsf_l, uint32 cur, uint32 interval, bool wrap)
{
	uint32 pos;

	/* adjust to sometime in the future if the 'cur' time is in the past */
	if (tsf_l > cur || wrap)
		pos = tsf_l + (interval - ((tsf_l - cur) % interval));
	else
	        pos = cur;

	return pos;
}

/* use the 64 bit tsf_timer{low,high} to extrapolahe 64 bit tbtt
 * to avoid any inconsistency between the 32 bit tsf_cfpstart and
 * the 32 bit tsf_timerhigh.
 * 'bcn_int' is in 1024TU.
 */
void
wlc_tsf64_to_next_tbtt64(uint32 bcn_int, uint32 *tsf_h, uint32 *tsf_l)
{
	uint32 bcn_offset;

	/* offset to last tbtt */
	bcn_offset = wlc_calc_tbtt_offset(bcn_int, *tsf_h, *tsf_l);
	/* last tbtt */
	wlc_uint64_sub(tsf_h, tsf_l, 0, bcn_offset);
	/* next tbtt */
	wlc_uint64_add(tsf_h, tsf_l, 0, bcn_int << 10);
}

/* per bss tbtt conversion function from 21 bits to 32 or 64 bits */
void
wlc_tbtt21_to_tbtt32(uint32 tsf_l, uint32 *tbtt_l)
{
	uint32 tbtt_adj;

	if (*tbtt_l & 0xFFE00000) {
		/* make sure tbtt_l passed in is not more than 21 bits */
		return;
	}

	/* tbtt should be > tsf_l and
	 * difference between tbtt and tsf_l should be < bcn_period
	 */
	/* convert tbtt from 21 bits to 32 bits */
	tbtt_adj = tsf_l & 0x001FFFFF;
	/* compare low 21 bits of tsf_l and tbtt_l */
	if (tbtt_adj >= *tbtt_l) {
		/* add one to the upper bits */
		tbtt_adj = (1 << 21);
	}
	else {
		tbtt_adj = 0;
	}
	*tbtt_l |= ((tsf_l & 0xFFE00000) + tbtt_adj);
}

void
wlc_tbtt21_to_tbtt64(uint32 tsf_h, uint32 tsf_l, uint32 *tbtt_h, uint32 *tbtt_l)
{
	*tbtt_h = tsf_h;
	wlc_tbtt21_to_tbtt32(tsf_l, tbtt_l);

	if (*tbtt_l < tsf_l) {
		(*tbtt_h)++;
	}
}

/* rsn parms lookup */
bool
wlc_rsn_akm_lookup(struct rsn_parms *rsn, uint8 akm)
{
	uint count;

	for (count = 0; count < rsn->acount; count++) {
		if (rsn->auth[count] == akm)
			return TRUE;
	}
	return FALSE;
}

bool
wlc_rsn_ucast_lookup(struct rsn_parms *rsn, uint8 auth)
{
	uint i;

	for (i = 0; i < rsn->ucount; i++) {
		if (rsn->unicast[i] == auth)
			return TRUE;
	}

	return FALSE;
}
