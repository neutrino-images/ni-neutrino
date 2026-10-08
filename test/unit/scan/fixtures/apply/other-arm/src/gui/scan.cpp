bool CScanNotifier::changeNotify(const neutrino_locale_t OptionName, void *)
{
	if (g_settings.scan_alpha)
		tellTheDriver(g_settings.scan_alpha);
	return false;
}
void CScanScreen::paint()
{
#if HAVE_SCAN_ARM
	draw(g_settings.scan_alpha);
#endif
}
