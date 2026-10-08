// Rows for the self test of the row scan, read by the scan only.
const EnumValue kScanAlternatives[] =
{
#if SCAN_ALTERNATIVES_ONE
	{ 0, "scan.alternatives.one", NULL, NULL },
	{ 5, "scan.alternatives.five", NULL, NULL },
#elif SCAN_ALTERNATIVES_TWO
	{ 0, "scan.alternatives.two", NULL, NULL },
#else
	{ 3, "scan.alternatives.other", NULL, NULL },
#endif
	{ 1, "scan.alternatives.always", NULL, NULL },
#ifdef SCAN_ALTERNATIVES_MORE
	{ 2, "scan.alternatives.more", NULL, NULL },
#endif
};

const EnumValue kScanNamedNumber[] =
{
	{ 0, "options.off", NULL, NULL },
};

const Descriptor kSettings[] =
{
	{
		"scan_alternatives", ValueType::Enum, "fixture",
		"label", NULL,
		0, 0, COREAPI_ENUM(kScanAlternatives), 0, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD
	},
	{
		"scan_named_number", ValueType::Int, "fixture",
		"label", NULL,
		1, 14, COREAPI_VALUES(kScanNamedNumber), 1, NULL, false, false, COREAPI_ALWAYS,
		COREAPI_NO_FIELD
	},
};
