namespace coreapi
{
namespace
{
constexpr const char *const kScanKeys[] = { "scan_alpha" };
constexpr ApplyGroup kGroup = { "scan", ApplyPhase::Decoders, COREAPI_KEYS(kScanKeys), &runScan };
}
}
namespace coreapi
{
constexpr ApplyGroup kBad = { "bad", ApplyPhase::Decoders, kBadKeys, 1, &runBad };
}
