void CScanWiringScreen::build(CMenuWidget *menu)
{
	addSetting(menu, "scan_wiring_grouped");
	addChoiceSetting(menu, "scan_wiring_typed", true, this, CRCInput::RC_nokey, true);
	addSetting(menu, "scan_wiring_plain", true, this);
	addNumberSetting(menu, "scan_wiring_defaults");
	menu->addItem(new CMenuOptionChooser(names[0], &g_settings.scan_wiring_slots[0], OPTIONS_OFF0_ON1_OPTIONS, OPTIONS_OFF0_ON1_OPTION_COUNT, true, &slot));
}
