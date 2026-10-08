// A change of a settings list with no guard in front of it, which the scan has to refuse.
void rebuild(const std::list<std::string> &chosen)
{
	g_settings.webtv_xml.clear();
	g_settings.webtv_xml = chosen;
}
