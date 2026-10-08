// A box source installed after the startup load: the scan has to refuse it.
int CNeutrinoApp::loadSetup(const char *fname)
{
	const coreapi::Descriptor *hdd_fs_row = coreapi::settings::findRow("hdd_fs");
	g_settings.hdd_fs = configfile.getInt32("hdd_fs", hdd_fs_row != NULL ? coreapi::defaultInt(*hdd_fs_row) : 0);
	return 0;
}

int CNeutrinoApp::run(int argc, char **argv)
{
	int loadSettingsErg = loadSetup(NEUTRINO_SETTINGS_FILE);
	coreapi::installRealSystemSource();
	return loadSettingsErg;
}
