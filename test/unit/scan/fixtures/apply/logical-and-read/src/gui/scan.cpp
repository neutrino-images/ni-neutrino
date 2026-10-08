bool CScanNotifier::changeNotify(const neutrino_locale_t OptionName, void *)
{
	tellTheDriver(g_settings.scan_alpha);
	return false;
}
void CScanScreen::stop()
{
	if (running && g_settings.scan_alpha)
		finish();
}
