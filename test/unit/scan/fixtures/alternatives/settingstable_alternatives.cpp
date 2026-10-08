// Rows for the self test of the row scan, read by the scan only.
constexpr EnumValue kScanAlternatives[] =
{
#if SCAN_ALTERNATIVES_ONE
	option(0).label("scan.alternatives.one"),
	option(5).label("scan.alternatives.five"),
#elif SCAN_ALTERNATIVES_TWO
	option(0).label("scan.alternatives.two"),
#else
	option(3).label("scan.alternatives.other"),
#endif
	option(1).label("scan.alternatives.always"),
#ifdef SCAN_ALTERNATIVES_MORE
	option(2).label("scan.alternatives.more"),
#endif
};

constexpr EnumValue kScanNamedNumber[] =
{
	option(0).label("options.off"),
};

constexpr Descriptor kSettings[] =
{
	enumRow("scan_alternatives")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.values(kScanAlternatives)
		.field(COREAPI_NO_FIELD),
	intRow("scan_named_number")
		.section("fixture")
		.label("label")
		.range(1, 14)
		.defaultValue(1)
		.values(kScanNamedNumber)
		.field(COREAPI_NO_FIELD),
};
