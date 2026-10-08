int CScanScreen::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "pick")
	{
		g_settings.scan_alpha = pick(g_settings.scan_alpha);
		return menu_return::RETURN_REPAINT;
	}
	return 0;
}
