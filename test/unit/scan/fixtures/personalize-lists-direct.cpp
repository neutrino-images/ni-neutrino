// The menu as it wrote the lists before they went through one batch: the scan has to refuse it.
int CPersonalizeGui::ShowPersonalizationMenu()
{
	if (show_pluginmenu) {
		setSettingsText(g_settings.plugins_disabled, "");
		for (int i = 0; i < pcount; i++) {
			if (pltype[i] & CPlugins::P_TYPE_DISABLED) {
				appendSettingsText(g_settings.plugins_disabled, g_Plugins->getFileName(i));
				g_Plugins->setType(i, CPlugins::P_TYPE_DISABLED);
			}
		}
	}
	return 0;
}
