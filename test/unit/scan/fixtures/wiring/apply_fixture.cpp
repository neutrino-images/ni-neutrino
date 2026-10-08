namespace coreapi
{
namespace
{
const char *const kScanWiringKeys[] = { "scan_wiring_grouped" };
const ApplyGroup kScanWiringGroup = { "scanwiring", ApplyPhase::Decoders, COREAPI_KEYS(kScanWiringKeys), &runScanWiring };
}
}
