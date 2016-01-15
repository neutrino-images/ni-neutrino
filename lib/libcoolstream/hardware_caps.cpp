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
#include "hardware_caps.h"

static int initialized = 0;
static bool frontend_check = false;
static hw_caps_t caps;

hw_caps_t *get_hwcaps(void)
{

	if (initialized && frontend_check)
		return &caps;

	unsigned int system_rev = cs_get_revision();

	CFEManager* fem = CFEManager::getInstance();
	int frontendCount = fem->getFrontendCount();
	frontend_check = frontendCount > 0;

	caps.has_fan = (system_rev < 8);
	caps.has_HDMI = 1;
	caps.has_SCART = (system_rev != 10);
	caps.has_SCART_input = 0;
	caps.has_YUV_cinch = 1;
	caps.can_shutdown = (system_rev > 7);
	caps.can_cec = 1;
	caps.display_type = (system_rev != 10) ? HW_DISPLAY_LINE_TEXT : HW_DISPLAY_NONE;
	caps.display_xres = 12;
	caps.display_yres = 0;
	caps.can_set_display_brightness = 1;


	strcpy(caps.boxvendor, "CST");
	const char* boxname;
	switch (system_rev) {
		case 6:
			boxname = "HD1";
			break;
		case 7:
			boxname = "BSE";
			break;
		case 8:
			boxname = "Neo";
			break;
		case 10:
			boxname = "Zee";
			break;
		default:
			char buffer[512];
			snprintf(buffer, sizeof(buffer)-1, "Unknown nr. %u\n", system_rev);
			boxname = buffer;
			break;
	}
	strcpy(caps.boxname, boxname);

	CFrontend *frontend = fem->getFE(0);
	uint32_t mask = frontend->getSupportedDeliverySystems();
	std::string tuner = "";
	if (frontendCount > 1)
		tuner += "Twin ";
	tuner += ((mask & DVB_C)  == DVB_C) ? g_Locale->getText(LOCALE_SATSETUP_FE_DELSYS_MODE_CABLE) : "Sat";
	strcpy(caps.frontend, tuner.c_str());

	strcpy(caps.chipset, "Nevis");

	initialized = 1;
	return &caps;
}

