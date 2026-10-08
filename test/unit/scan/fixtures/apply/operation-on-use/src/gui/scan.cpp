int CFlashExpertSetup::exec(CMenuTarget *parent, const std::string &actionKey)
{
	if (actionKey == "readmtd0")
	{
		if (g_settings.scan_alpha == 1)
			return 1;
		return 0;
	}
	return 0;
}
