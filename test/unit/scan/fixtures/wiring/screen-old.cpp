void CScanWiringScreen::build(CMenuWidget *menu)
{
	addSetting(menu, "scan_wiring_grouped", true, this);
	addSetting(menu, "scan_wiring_typed", true, this, CRCInput::RC_nokey, false, true);
	addSetting(menu, "scan_wiring_plain", true, this);
	addSetting(menu, "scan_wiring_defaults", true, NULL, CRCInput::RC_nokey, false, false, false);
	menu->addItem(new CMenuOptionChooser(names[0], &g_settings.scan_wiring_slots[0], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &anotify));
}
