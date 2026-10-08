int CScanScreen::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "pick")
	{
		return 1;
	}
	use(g_settings.scan_alpha);
	return 0;
}
