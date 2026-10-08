namespace coreapi
{
constexpr ApplyGroup kOwn = { "own", ApplyPhase::Decoders, COREAPI_KEYS(kOwnKeys), &runOwn };
}
