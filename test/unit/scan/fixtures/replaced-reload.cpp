// The reload and a menu load as the tree writes them: the scan has to pass them.
int CNeutrinoApp::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (msg == NeutrinoMessages::RELOAD_SETUP) {
		coreapi::settings::applyReplaced([this]() { loadSetup(NEUTRINO_SETTINGS_FILE); });
		return messages_return::handled;
	}
	// loadSetup(NEUTRINO_SETTINGS_FILE); in a comment is no call
	CSettingsManager::replaceFromMenu([]() { CNeutrinoApp::getInstance()->loadSetup(NEUTRINO_SETTINGS_FILE); });
	return messages_return::unhandled;
}
