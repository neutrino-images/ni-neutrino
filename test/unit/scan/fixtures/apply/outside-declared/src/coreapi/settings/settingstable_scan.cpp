namespace coreapi
{
constexpr Descriptor kScan[] =
{
	boolRow("scan_alpha")
		.section("scan")
		.readOutside()
		.field(COREAPI_NUMBER_FIELD(scan_alpha)),
	boolRow("scan_beta")
		.section("scan")
		.field(COREAPI_NUMBER_FIELD(scan_beta))
};
}
