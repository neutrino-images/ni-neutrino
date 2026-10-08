// A site under a negated group that names the row's arm: no conjunct of it is that arm.
#if !(SCAN_ARMS_A && defined(SCAN_ARMS_OPTION) && SCAN_ARMS_B)
	addSetting(menu, "scan_arms_gated");
#endif
