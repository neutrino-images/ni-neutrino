// Rows for the self test of the DERIVED arms column, read by the scan only.
constexpr Descriptor kSettings[] =
{
#if 0
	boolRow("scan_arms_dead")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.field(COREAPI_NO_FIELD),
#endif
#ifdef SCAN_ARMS_OPTION
	boolRow("scan_arms_gated")
		.section("fixture")
		.label("label")
		.defaultValue(0)
		.field(COREAPI_NO_FIELD),
#endif
};
