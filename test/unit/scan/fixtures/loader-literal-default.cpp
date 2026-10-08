// A loader that keeps its own number for a default the box decides: the scan has to refuse it.
int CNeutrinoApp::loadSetup(const char *fname)
{
	g_settings.hdd_fs = configfile.getInt32("hdd_fs", 0);
	return 0;
}

int CNeutrinoApp::run(int argc, char **argv)
{
	coreapi::installRealSystemSource();
	int loadSettingsErg = loadSetup(NEUTRINO_SETTINGS_FILE);
	return loadSettingsErg;
}
