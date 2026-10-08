namespace coreapi
{
namespace
{
constexpr const char *const kOtherKeys[] = { "scan_alpha", "scan_beta" };
constexpr ApplyGroup kGroup = { "other", ApplyPhase::Decoders, COREAPI_KEYS(kOtherKeys), &runScan };
}
}
