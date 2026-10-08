int CScanManager::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "Record")
	{
		if (g_settings.scan_alpha > limit)
			warn();
		return 0;
	}
	return 0;
}
