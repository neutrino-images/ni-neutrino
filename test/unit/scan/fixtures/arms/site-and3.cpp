// A site under three conditions, the row's among them: built only where the row is.
#if SCAN_ARMS_A && defined(SCAN_ARMS_OPTION) && SCAN_ARMS_B
	addSetting(menu, "scan_arms_gated");
#endif
