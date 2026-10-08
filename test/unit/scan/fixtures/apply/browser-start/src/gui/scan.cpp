int CScanScreen::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "add")
	{
		CFileBrowser fileBrowser;
		if (fileBrowser.exec(wide ? g_settings.scan_alpha.c_str() : g_settings.scan_beta.c_str()))
			return 1;
		return 0;
	}
	return 0;
}
