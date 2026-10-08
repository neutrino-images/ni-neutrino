// A reload that reads the file past the apply groups: the scan has to refuse it.
int CNeutrinoApp::handleMsg(const neutrino_msg_t msg, neutrino_msg_data_t data)
{
	if (msg == NeutrinoMessages::RELOAD_SETUP) {
		loadSetup(NEUTRINO_SETTINGS_FILE);
		return messages_return::handled;
	}
	return messages_return::unhandled;
}
