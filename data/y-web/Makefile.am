SUBDIRS = images scripts languages styles

# NI-yWeb
SUBDIRS += info jquery

installdir = $(PRIVATE_HTTPDDIR)

install_DATA =	channels.js \
		channels.txt \
		extentions.txt \
		favicon.ico \
		index.html \
		prototype.js \
		robots.txt \
		Y_About.yhtm \
		Y_Baselib.js \
		Y_blank.htm \
		Y_Blocks.txt \
		Y_Boxcontrol_Bouquet_Editor_Edit.yhtm \
		Y_Boxcontrol_Bouquet_Editor_Main.yhtm \
		Y_Boxcontrol_Bouquetlist.yhtm \
		Y_Boxcontrol_Bouquets.yhtm \
		Y_Boxcontrol_Channellist.yhtm \
		Y_Boxcontrol_Menue.yhtm \
		Y_Boxcontrol_Messages.yhtm \
		Y_Dist.css \
		Y_Dyn_Pages.yhtm \
		Y_EPG.js \
		Y_EPG_Plus.yhtm \
		Y_Ext_Menue.yhtm \
		Y_Ext_Settings.yhtm \
		Y_Ext_Uninstall.yhtm \
		Y_Ext_Update.js \
		Y_Ext_Update_refresh.yhtm \
		Y_Ext_Update.yhtm \
		Y_Flash.js \
		Y_Info_Help.yhtm \
		Y_Info_Menue.yhtm \
		Y_Info_Updates.yhtm \
		Y_Live_DoubleView.yhtm \
		Y_Live_EPG_Info.yhtm \
		Y_Live_EPG.yhtm \
		Y_Live.js \
		Y_Live_Menue.yhtm \
		Y_Live_Msg.yhtm \
		Y_Live_Record.yhtm \
		Y_Live_Transcode.yhtm \
		Y_LiveViewFull.yhtm \
		Y_Live.yhtm \
		Y_Main.css \
		Y_Menue.yhtm \
		Y_neutrino_Blocks.txt \
		Y_Refresh.yhtm \
		Y_Settings_automount_liste.yhtm \
		Y_Settings_automount.yhtm \
		Y_Settings_Backup.yhtm \
		Y_Settings_buttons.yhtm \
		Y_Settings_glcd.yhtm \
		Y_Settings_lcd.yhtm \
		Y_Settings_Live.yhtm \
		Y_Settings_Menue.yhtm \
		Y_Settings_mount_liste.yhtm \
		Y_Settings_mount.yhtm \
		Y_Settings_neutrino_forms.yhtm \
		Y_Settings_nhttpd.yhtm \
		Y_Settings_personalize.yhtm \
		Y_Settings_Timer.yhtm \
		Y_Settings_umount_liste.yhtm \
		Y_Settings_video_audio.yhtm \
		Y_Settings_VNC.yhtm \
		Y_Settings_wol.yhtm \
		Y_Settings_yWeb.yhtm \
		Y_Settings_zapit.yhtm \
		Y_StreamInfo.yhtm \
		Y_Timer_Edit.yhtm \
		Y_Timer_List.yhtm \
		Y_Tools_Bootlogo.yhtm \
		Y_Tools_Boxcontrol.yhtm \
		Y_Tools_Check_Install.yhtm \
		Y_Tools_Cmd.yhtm \
		Y_Tools_Flash_Menue.yhtm \
		Y_Tools_Flash_Upload.yhtm \
		Y_Tools_Glcd_Screenshot.yhtm \
		Y_Tools_Info_Menue.yhtm \
		Y_Tools_Installer.yhtm \
		Y_Tools_lcshot.yhtm \
		Y_Tools_Menue.yhtm \
		Y_Tools_Rcsim.yhtm \
		Y_Tools_Screenshot.yhtm  \
		Y_Tools_Timer_Sync.js \
		Y_Tools_Timer_Sync.yhtm \
		Y_Tools_tvshot.yhtm \
		Y_User.css \
		Y_Version.txt \
		Y_VLC.js \
		Y_Wait.yhtm \
		ywidget.css \
		ywidget.js \
		Y_yweb.js

# file manager
install_DATA +=	accordion.js \
		effects.js \
		Y_Filemgr.yhtm \
		Y_Filemgr_Edit.yhtm \
		Y_Filemgr_blocks.txt

# NI-yWeb
install_DATA += Y_NIlib.js \
		Y_NI.css \
		Y_NI_About.yhtm \
		Y_NI_blank.yhtm \
		Y_NI_Blocks.txt \
		Y_NI_Camd-control.yhtm \
		Y_NI_Dyn_Pages.yhtm \
		Y_NI_Infoicons-control.yhtm \
		Y_NI_m3uConInfo.yhtm \
		Y_NI_m3uConvert.yhtm \
		Y_NI_Menue.yhtm \
		Y_NI_NetFS-control.yhtm \
		Y_NI_NetFS-edit.yhtm \
		Y_NI_NetFS-init.yhtm \
		Y_NI_Plugin-control.yhtm \
		Y_NI_PZapit-control.yhtm

install-data-hook:
	@# Badass hack, I know. If anyone knows a better way -- please send patch!
	find $(DESTDIR)$(PRIVATE_HTTPDDIR)/ -type f -print0 | xargs --no-run-if-empty -0 \
		sed -i \
			-e 's|%(CONFIGDIR)|$(CONFIGDIR)|g' \
			-e 's|%(DATADIR)|$(DATADIR)|g' \
			-e 's|%(CONTROLDIR)|$(CONTROLDIR)|g' \
			-e 's|%(CONTROLDIR_VAR)|$(CONTROLDIR_VAR)|g' \
			-e 's|%(FONTDIR)|$(FONTDIR)|g' \
			-e 's|%(FONTDIR_VAR)|$(FONTDIR_VAR)|g' \
			-e 's|%(LIBDIR)|$(LIBDIR)|g' \
			-e 's|%(GAMESDIR)|$(GAMESDIR)|g' \
			-e 's|%(ICONSDIR)|$(ICONSDIR)|g' \
			-e 's|%(ICONSDIR_VAR)|$(ICONSDIR_VAR)|g' \
			-e 's|%(LOCALEDIR)|$(LOCALEDIR)|g' \
			-e 's|%(LOCALEDIR_VAR)|$(LOCALEDIR_VAR)|g' \
			-e 's|%(PLUGINDIR)|$(PLUGINDIR)|g' \
			-e 's|%(PLUGINDIR_MNT)|$(PLUGINDIR_MNT)|g' \
			-e 's|%(PLUGINDIR_VAR)|$(PLUGINDIR_VAR)|g' \
			-e 's|%(LUAPLUGINDIR)|$(LUAPLUGINDIR)|g' \
			-e 's|%(LUAPLUGINDIR_VAR)|$(LUAPLUGINDIR_VAR)|g' \
			-e 's|%(THEMESDIR)|$(THEMESDIR)|g' \
			-e 's|%(THEMESDIR_VAR)|$(THEMESDIR_VAR)|g' \
			-e 's|%(WEBRADIODIR)|$(WEBRADIODIR)|g' \
			-e 's|%(WEBRADIODIR_VAR)|$(WEBRADIODIR_VAR)|g' \
			-e 's|%(WEBTVDIR)|$(WEBTVDIR)|g' \
			-e 's|%(WEBTVDIR_VAR)|$(WEBTVDIR_VAR)|g' \
			-e 's|%(LOGODIR)|$(LOGODIR)|g' \
			-e 's|%(LOGODIR_VAR)|$(LOGODIR_VAR)|g' \
			-e 's|%(PRIVATE_HTTPDDIR)|$(PRIVATE_HTTPDDIR)|g' \
			-e 's|%(PUBLIC_HTTPDDIR)|$(PUBLIC_HTTPDDIR)|g' \
			-e 's|%(HOSTED_HTTPDDIR)|$(HOSTED_HTTPDDIR)|g' \
			-e 's|%(FLAGDIR)|$(FLAGDIR)|g' \
	;
	@# NI-yWeb
	cp $(DESTDIR)$(PRIVATE_HTTPDDIR)/styles/Y_Dist-NI_dark.css $(DESTDIR)$(PRIVATE_HTTPDDIR)/Y_Dist.css
