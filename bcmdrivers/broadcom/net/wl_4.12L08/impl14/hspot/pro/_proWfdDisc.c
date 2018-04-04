/*
 * Test harness for WiFi-Direct discovery state machine.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "trace.h"
#include "test.h"
#include "dsp.h"
#include "wlu_api.h"
#include "proWfdDisc.h"

TEST_DECLARE();

/* --------------------------------------------------------------- */

void testProWfdDisc(void)
{
	proWfdDiscT *disc;

	TEST(proWfdDiscInitialize(), "proWfdDiscInitialize failed");
	disc = proWfdDiscCreate(0, 11);
	TEST(disc != 0, "proWfdDiscCreate failed");

	/* discovery */
	TEST(proWfdDiscStartDiscovery(disc), "proWfdDiscStart failed");
	sleep(15);
	TEST(proWfdDiscReset(disc), "proWfdDiscReset failed");

	/* extended listen */
	TEST(proWfdDiscStartExtListen(disc, 500, 4500), "proWfdDiscStartExtListen failed");
	sleep(15);
	TEST(proWfdDiscReset(disc), "proWfdDiscReset failed");

	/* listen */
	TEST(proWfdDiscStartExtListen(disc, 5000, 0), "proWfdDiscStartExtListen failed");
	sleep(15);
	TEST(proWfdDiscReset(disc), "proWfdDiscReset failed");

	TEST(proWfdDiscDestroy(disc), "proWfdDiscDestroy failed");
	TEST(proWfdDiscDeinitialize(), "proWfdDiscDeinitialize failed");
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	TRACE_LEVEL_SET(TRACE_ERROR | TRACE_DEBUG);
	TEST_INITIALIZE();

	testProWfdDisc();

	/* disable wlan */
	wlFree();

	/* terminate dispatcher */
	dspFree();

	TEST_FINALIZE();
	return 0;
}
