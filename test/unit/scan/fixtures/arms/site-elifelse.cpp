// A site in the else behind an #elif that names the row's arm.
#if SCAN_ARMS_Y
#elif SCAN_ARMS_A && defined(SCAN_ARMS_OPTION) && SCAN_ARMS_B
#else
	addSetting(menu, "scan_arms_gated");
#endif
