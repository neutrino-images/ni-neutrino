// The same change under the guard, which the scan has to take.
void rebuild(const std::list<std::string> &chosen)
{
	CSettingsTextGuard lock;
	g_settings.webtv_xml = chosen;
	delete g_settings.usermenu[0];
}

void assign(const std::string &name)
{
	setSettingsText(t.glcd_font, name);
}
