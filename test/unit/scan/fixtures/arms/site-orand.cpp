// An || at the top level: the && after it is no conjunct of the whole.
#if 1 || SCAN_ARMS_A && defined(SCAN_ARMS_OPTION) && SCAN_ARMS_B
	addSetting(menu, "scan_arms_gated");
#endif
