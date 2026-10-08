// A guard a block comment runs on from: the || after the comment is part of it.
#if defined(SCAN_ARMS_OPTION) /* the arm,
	and another */ || SCAN_ARMS_B
	addSetting(menu, "scan_arms_gated");
#endif
