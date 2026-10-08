namespace coreapi
{
namespace
{
constexpr const char *const kScanKeys[] = { "scan_alpha", "scan_gamma" };
constexpr ApplyGroup kGroup = { "scan", ApplyPhase::Decoders, COREAPI_KEYS(kScanKeys), &runScan };
}
}
