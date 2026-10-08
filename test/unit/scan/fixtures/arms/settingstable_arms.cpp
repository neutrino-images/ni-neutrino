// Rows for the self test of the DERIVED arms column, read by the scan only.
const Descriptor kSettings[] =
{
#if 0
	{
		"scan_arms_dead", ValueType::Bool, "fixture",
		"label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD
	},
#endif
#ifdef SCAN_ARMS_OPTION
	{
		"scan_arms_gated", ValueType::Bool, "fixture",
		"label", NULL,
		0, 1, NULL, 0, 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD
	},
#endif
};
