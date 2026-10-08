// A walk of a settings list on a thread that does not write it, with no copy and no guard,
// which the scan has to refuse; a copy and a handed over pointer are fine beside it.
void walk()
{
	std::list<std::string> copy = settingsCopy(g_settings.webtv_xml);
	zapit.SetWebTVXML(&g_settings.webtv_xml);
	for (std::vector<timer_remotebox_item>::iterator it = g_settings.timer_remotebox_ip.begin(); it != g_settings.timer_remotebox_ip.end(); ++it)
		use(*it);
}
