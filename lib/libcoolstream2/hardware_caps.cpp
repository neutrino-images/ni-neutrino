/*
 * determine the capabilities of the hardware.
 * part of libstb-hal
 *
 * (C) 2010-2012 Stefan Seyfried
 * (C)      2016 M. Liebmann
 *
 * License: GPL v2 or later
 */

#include <global.h>
#include <zapit/femanager.h>
#include <sys/utsname.h>
#include "hardware_caps.h"

static int initialized = 0;
static bool frontend_check = false;
static hw_caps_t caps;

std::string getTuner()
{
	std::string	tuner;
	char 		buff1[64];
	CFEManager*	fem = CFEManager::getInstance();
	int		frontendCount = fem->getFrontendCount();
	frontend_check = frontendCount > 0;

	int satCount    = 0;
	int cableCount  = 0;
	int dvbtCount   = 0;
	int dvbc_tCount = 0;
	for (int i = 0; i < frontendCount; i++) {
		bool has_dvb_s	= false;
		bool has_dvb_s2	= false;
		bool has_dvb_c	= false;
		bool has_dvb_t	= false;
		bool has_dvb_t2	= false;
//		bool has_dtmb	= false;

		CFrontend *frontend = fem->getFE(i);
		uint32_t mask = frontend->getSupportedDeliverySystems();

		if ((mask & DVB_S)  == DVB_S)   has_dvb_s   = true;
		if ((mask & DVB_S2) == DVB_S2)  has_dvb_s2  = true;
		if ((mask & DVB_C)  == DVB_C)   has_dvb_c   = true;
		if ((mask & DVB_T)  == DVB_T)   has_dvb_t   = true;
		if ((mask & DVB_T2) == DVB_T2)  has_dvb_t2  = true;
//		if ((mask & DTMB)   == DTMB)    has_dtmb    = true;

		if (has_dvb_s || has_dvb_s2) satCount++;
		if (has_dvb_c && (has_dvb_t || has_dvb_t2))
			dvbc_tCount++;
		else {
			if (has_dvb_c) cableCount++;
			if (has_dvb_t || has_dvb_t2) dvbtCount++;
		}
	}

	if (satCount > 0) {
		memset(buff1, 0, sizeof(buff1));
		snprintf(buff1, sizeof(buff1)-1, "%dx Sat", satCount);
		tuner += buff1;
	}
	if (cableCount > 0) {
		if (satCount > 0) tuner += ", ";
		memset(buff1, 0, sizeof(buff1));
		snprintf(buff1, sizeof(buff1)-1, "%dx %s", cableCount, g_Locale->getText(LOCALE_SATSETUP_FE_DELSYS_MODE_CABLE));
		tuner += buff1;
	}
	if (dvbtCount > 0) {
		if ((satCount > 0) || (cableCount > 0)) tuner += ", ";
		memset(buff1, 0, sizeof(buff1));
		snprintf(buff1, sizeof(buff1)-1, "%dx DVB-T", dvbtCount);
		tuner += buff1;
	}
	if (dvbc_tCount > 0) {
		if ((satCount > 0) || (cableCount > 0) || (dvbtCount > 0)) tuner += ", ";
		memset(buff1, 0, sizeof(buff1));
		snprintf(buff1, sizeof(buff1)-1, "%dx %s/DVB-T", dvbc_tCount, g_Locale->getText(LOCALE_SATSETUP_FE_DELSYS_MODE_CABLE));
		tuner += buff1;
	}
	return tuner;
}

hw_caps_t *get_hwcaps(void)
{
	if (!frontend_check)
		strcpy(caps.frontend, getTuner().c_str());

	if (initialized)
		return &caps;

	struct utsname u;
	unsigned int system_rev = cs_get_revision();

	caps.has_fan = 0;
	caps.has_HDMI = 1;
	caps.has_SCART = (system_rev != 10);
	caps.has_SCART_input = 0;
	caps.has_YUV_cinch = 1;
	caps.can_shutdown = 1;
	caps.can_cec = 1;
	switch (system_rev) {
		case 9:  /* Tank */
		case 12: /* Zee2 */
			caps.display_type = HW_DISPLAY_LINE_TEXT;
			break;
		case 11: /* Trinity */
			caps.display_type = HW_DISPLAY_NONE;
			break;
		case 13: /* Link */
			caps.display_type = HW_DISPLAY_LED_NUM;
			break;
		default:
			caps.display_type = HW_DISPLAY_NONE;
			break;
	}
	caps.display_xres = 12;
	caps.display_yres = 0;
	caps.can_set_display_brightness = 1;

	strcpy(caps.boxvendor, "CST");
	const char* boxname;
	switch (system_rev) {
		case 9:
			boxname = "Tank";
			break;
		case 11:
			boxname = "Trinity";
			break;
		case 12:
			boxname = "Zee2";
			break;
		case 13:
			boxname = "Link";
			break;
		default:
			char buffer[512];
			snprintf(buffer, sizeof(buffer)-1, "Unknown nr. %u\n", system_rev);
			boxname = buffer;
			break;
	}
	strcpy(caps.boxname, boxname);

	const char* chipset;
	switch (system_rev) {
		case 9:
			chipset = "Apollo";
			break;
		case 11:
			chipset = "Shiner";
			break;
		case 12:
		case 13:
			chipset = "Kronos";
			break;
		default:
			chipset = "";
			break;
	}
	strcpy(caps.chipset, chipset);

	if (! uname(&u))
		strncpy(caps.boxarch, u.machine, sizeof(caps.boxarch));
	else
		fprintf(stderr, "%s: uname() failed: %m\n", __func__);

	initialized = 1;
	return &caps;
}
