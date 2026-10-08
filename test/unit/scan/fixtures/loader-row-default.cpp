// The loader as the tree writes it: the scan has to pass it.
int CNeutrinoApp::loadSetup(const char *fname)
{
	const coreapi::Descriptor *hdd_fs_row = coreapi::settings::findRow("hdd_fs");
	g_settings.hdd_fs = configfile.getInt32("hdd_fs", hdd_fs_row != NULL ? coreapi::defaultInt(*hdd_fs_row) : 0);
	// g_settings.hdd_fs = configfile.getInt32("hdd_fs", 0); in a comment is no read
	return 0;
}

int CNeutrinoApp::run(int argc, char **argv)
{
	coreapi::installRealSystemSource();
	int loadSettingsErg = loadSetup(NEUTRINO_SETTINGS_FILE);
	return loadSettingsErg;
}
