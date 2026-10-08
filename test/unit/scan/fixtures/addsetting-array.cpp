const char *const kScanArrayKeys[] =
{
	"scan_array_first", "scan_array_second",
#ifdef SCAN_ARRAY_ARM
	"scan_array_armed",
#endif
	"scan_array_last"
};

void CScanArray::build(CMenuWidget *menu)
{
	for (size_t i = 0; i < sizeof(kScanArrayKeys) / sizeof(kScanArrayKeys[0]); i++)
		addSetting(menu, kScanArrayKeys[i]);
	addSetting(menu, kUnknownKeys[0]);
}
