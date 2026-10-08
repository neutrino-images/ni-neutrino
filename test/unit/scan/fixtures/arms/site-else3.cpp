// A site in the else of three conditions, the row's among them: built where the row may not be.
#if SCAN_ARMS_A && defined(SCAN_ARMS_OPTION) && SCAN_ARMS_B
#else
	addSetting(menu, "scan_arms_gated");
#endif
