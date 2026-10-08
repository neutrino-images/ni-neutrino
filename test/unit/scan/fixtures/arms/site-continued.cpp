// A guard continued on the next line: the || after the break is part of it, so the
// site is built without the row's arm although the first line names it in an &&.
#if SCAN_ARMS_B && defined(SCAN_ARMS_OPTION) && \
	SCAN_ARMS_C || SCAN_ARMS_D
	addSetting(menu, "scan_arms_gated");
#endif
