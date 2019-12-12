/*
	netfs_setup

	(C) 2009-2016 NG-Team
	(C) 2016 NI-Team

	License: GPL

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gui/netfs_setup.h>

#include <gui/filebrowser.h>
#include <gui/widget/menue.h>
#include <gui/widget/hintbox.h>
#include <gui/widget/keyboard_input.h>
#include <driver/screen_max.h>

#include <fstream>
#include <iostream>

#include <system/helpers.h>
#include <global.h>

#include <sys/mount.h>
#include <mymenu.h>
#include <neutrino.h>
#include <neutrino_menue.h>
#include <zapit/client/zapittools.h>

extern const char * mntRes2Str(CFSMounter::MountRes res);
extern const char * mntRes2Str(CFSMounter::UMountRes res);

CNETFSMountGui::CNETFSMountGui()
{
	// FIXME #warning move probing from exec() to fsmounter
	m_nfs_sup = CFSMounter::FS_UNPROBED;
	m_cifs_sup = CFSMounter::FS_UNPROBED;
	
	width = 50;

	fstabPath = "/var/etc/fstab";
	autoPath = "/var/etc/auto.net";
}

std::string CNETFSMountGui::getEntryString(int mt, int i)
{
	std::string res;
	switch(g_settings.netfs[mt][i].type) {
		case CFSMounter::NFS: res = "NFS "     + g_settings.netfs[mt][i].ip + ":"; break;
		case CFSMounter::CIFS: res = "CIFS //" + g_settings.netfs[mt][i].ip + "/"; break;
	}
	if (g_settings.netfs[mt][i].dir.empty() || g_settings.netfs[mt][i].local_dir.empty())
		return "";
	return res
		+ FILESYSTEM_ENCODING_TO_UTF8(g_settings.netfs[mt][i].dir)
		+ " -> "
		+ FILESYSTEM_ENCODING_TO_UTF8(g_settings.netfs[mt][i].local_dir)
		+ " (auto: "
		+ g_Locale->getText(g_settings.netfs[mt][i].active ? LOCALE_MESSAGEBOX_YES : LOCALE_MESSAGEBOX_NO)
		+ ")";
}

int CNETFSMountGui::vinit(int mt)
{
	for(int i=0 ; i < NETFS_NR_OF_ENTRIES ; i++)
	{
		g_settings.netfs[mt][i].ip = "";
		g_settings.netfs[mt][i].dir = "";
		g_settings.netfs[mt][i].local_dir= "";
		g_settings.netfs[mt][i].type = 0;
		g_settings.netfs[mt][i].username = "";
		g_settings.netfs[mt][i].password = "";
		g_settings.netfs[mt][i].options1 = "soft";
		g_settings.netfs[mt][i].options2 = "nolock";
		g_settings.netfs[mt][i].active = 0;
		g_settings.netfs[mt][i].dump = "0";
		g_settings.netfs[mt][i].pass = "0";
	}
	return(0);
}

int CNETFSMountGui::read_config(int mt)
{
	FILE *fp;
	int i = 0;
	char *line_buffer;
	ssize_t read;
	size_t len;
	std::string config_file = (mt == SNeutrinoSettings::FSTAB)?fstabPath:autoPath;

	line_buffer=NULL;
	if((fp = fopen(config_file.c_str(), "r")))
	{
		while ((read = getline(&line_buffer, &len, fp)) != -1)
		{
			if (line_buffer[0] == '#' && !strstr(line_buffer,"-fstype="))
				continue;

			char device[100] = "";
			std::string device_dir = "";
			char mountpt[100] = "";
			char fstype[6] = "";
			char options[200] = "";
			char options1[31] = "";
			char options2[171] = "";
			char dump[3] = "";
			char pass[3] = "";
			char user[31] = "";
			char passwd[31] = "";

			if(mt == SNeutrinoSettings::FSTAB)
			{
				/*
				# file system		mount pt	type	options									dump	pass
				192.168.0.10:nfs	/srv		nfs	rw,soft,nolock					0	0
				//192.168.1.99/folder	/mnt/folder	cifs	rw,soft,nolock,user=USER,password=PASSWD	0	0 */

				sscanf(line_buffer,"%99s%99s%5s%199s%2s%2s", (char *) &device, (char *) &mountpt, (char *) &fstype, (char *) &options, (char *) &dump, (char *) &pass);
			}
			else
			{
				/*
				# MountPoint	Type, options						NetFS 
				srv		-fstype=nfs,soft,nolock					192.168.0.10:/nas/video
				pictures	-fstype=cifs,soft,user=USER,password=PASSWD		//192.168.0.11/pictures */

				sscanf((line_buffer[0]=='#')?line_buffer+1:line_buffer,"%99s%*[^-]-fstype=%5[^,],%199s%99s", (char *) &mountpt, (char *) &fstype, (char *) &options, (char *) &device);
			}

			if( strcmp( device, "" ) != 0 )
			{
				char * pch;
				char * found;
				int active = line_buffer[0] == '#' ? 0 : 1;
				int y = 0;

				pch = strtok (device,":/\\");
				while (pch != NULL)
				{
					if (y==0)
					{
						strcpy(device,pch);
					}
					else
					{
						device_dir += "/";
						device_dir += pch;
					}
					pch = strtok (NULL,":/\\");
					y++;
				}

				pch = strtok (options,",");
				while (pch != NULL)
				{

					if ((found = strstr(pch, "username="))) {
						 sscanf(found+9, "%30[^\n]", (char *) &user);
					}
					else if ((found = strstr(pch, "user="))) {
						 sscanf(found+5, "%30[^\n]", (char *) &user);
					}
					else if ((found = strstr(pch, "password="))) {
						sscanf(found+9, "%30[^\n]", (char *) &passwd);
					}
					else if (strstr(pch, "noauto")) {
						active = 0;
					}
					else if ((strlen(options1) + strlen(pch) + 1) <= 30)
					{
						if((strlen(options1)!=0))
							  strcat(options1,",");

						strcat(options1,pch);
						
					}
					else if ( (strlen(options2) + strlen(pch) + 1) <= 170)
					{
						if((strlen(options2)!=0))
							strcat(options2,",");

						strcat(options2,pch);
					}
					pch = strtok (NULL, ",");
				}

				if(mt == SNeutrinoSettings::FSTAB)
				{
					printf("CNETFSMountGui::read_config fstab: %s:%s\t%s\t%s\t%s%s%s\t%s\t%s\n", device, device_dir.c_str(), mountpt, fstype, options1, ((strlen(options2)==0)? "":","), options2, dump, pass);
				}
				else
				{
					printf("CNETFSMountGui::read_config auto.net: \n%s\t-fstype=%s\t%s%s%s\t%s:%s\n", mountpt,fstype, options1,options2,((strlen(options2)==0)? "":","),device,device_dir.c_str());
				}

				g_settings.netfs[mt][i].ip		= (strcmp(device,"0.0.0.0")==0)		? "" : device;
				g_settings.netfs[mt][i].dir		= ((device_dir=="/netfsDir")		? "" : device_dir.c_str());
				g_settings.netfs[mt][i].local_dir	= ((strcmp(mountpt,"mountPt")==0)	? "" : mountpt);
				g_settings.netfs[mt][i].type		= (strcmp(fstype,"nfs")==0)		? (int) CFSMounter::NFS : (int) CFSMounter::CIFS;
				g_settings.netfs[mt][i].username	= user;
				g_settings.netfs[mt][i].password	= passwd;
				g_settings.netfs[mt][i].options1	= options1;
				g_settings.netfs[mt][i].options2	= options2;
				g_settings.netfs[mt][i].dump		= dump;
				g_settings.netfs[mt][i].pass		= pass;
				g_settings.netfs[mt][i].active	= active;

				if(i == NETFS_NR_OF_ENTRIES-1) {
					//printf("CNG_NetfsMenu::read_config: to many entries in config file (max. %d)\n",NETFS_NR_OF_ENTRIES);
					break;
				}
				else {
					i++;
				}
			}

		}
		fclose(fp);
		if(line_buffer)
			free(line_buffer);
	}
	else
	{
		return(-1);
	}
	return(0);
}

int CNETFSMountGui::write_config(int mt)
{
	std::ofstream config_file((mt == SNeutrinoSettings::FSTAB)?fstabPath.c_str():autoPath.c_str());

	if (config_file.is_open())
	{
		if(mt == SNeutrinoSettings::FSTAB)
		{
			config_file	<< "# " << fstabPath << ": static file system information (generated from NetFS setup).\n"
					<< "#\n"
					<< "# <file system> <mount point> <type> <options> <dump> <pass>\n";

			for(int i=0 ; i < NETFS_NR_OF_ENTRIES; i++)
			{
				if (g_settings.netfs[mt][i].type == (int) CFSMounter::NFS)
				{
					//"%s:%s\t%s\t\tnfs\t%s%s%s%s\t\t%s\t%s\n"
					config_file 	<< ((g_settings.netfs[mt][i].ip.empty())		? "0.0.0.0" : g_settings.netfs[mt][i].ip) << ":"
							<< ((g_settings.netfs[mt][i].dir.empty())		? "netfsDir": g_settings.netfs[mt][i].dir) << '\t'
							<< ((g_settings.netfs[mt][i].local_dir.empty())	? "mountPt" : g_settings.netfs[mt][i].local_dir) << '\t'
							<< '\t' << "nfs" << '\t'
							<< (g_settings.netfs[mt][i].active			?"":"noauto,")
							<< g_settings.netfs[mt][i].options1
							<< ((g_settings.netfs[mt][i].options2.empty())	? "" : ",")
							<< g_settings.netfs[mt][i].options2 << '\t' << '\t'
							<< g_settings.netfs[mt][i].dump << '\t'
							<< g_settings.netfs[mt][i].pass << '\n';
				}
				else
				{
					//"//%s%s%s\t%s\t\tcifs\t%susername=%s,password=%s%s%s%s%s\t%s\t%s\n"
					config_file	<< "//"
							<< ((g_settings.netfs[mt][i].ip.empty()) 		? "0.0.0.0" : g_settings.netfs[mt][i].ip)
							<< (((!g_settings.netfs[mt][i].dir.empty()) && g_settings.netfs[mt][i].dir[0]!='/') ? "/" : "")
							<< ((g_settings.netfs[mt][i].dir.empty()) 		? "netfsDir": g_settings.netfs[mt][i].dir) << '\t'
							<< ((g_settings.netfs[mt][i].local_dir.empty()) 	? "mountPt" : g_settings.netfs[mt][i].local_dir) << '\t'
							<< '\t' << "cifs" << '\t'
							<< (g_settings.netfs[mt][i].active			?"":"noauto,")
							<< "username=" << g_settings.netfs[mt][i].username << ",password=" << g_settings.netfs[mt][i].password
							<< (g_settings.netfs[mt][i].options1.empty() 	? "" : ",")
							<< g_settings.netfs[mt][i].options1
							<< (g_settings.netfs[mt][i].options2.empty() 	? "" : ",")
							<< g_settings.netfs[mt][i].options2 << '\t' << '\t'
							<< g_settings.netfs[mt][i].dump << '\t'
							<< g_settings.netfs[mt][i].pass << '\n';
				}
			}
		}
		else
		{
			config_file	<< "# " << autoPath << ": dynamic file system information (generated from NetFS setup).\n"
					<< "#\n"
					<< "# Every single mount point below will be available in /mnt/autofs during start\n"
					<< "# or possibly even later if you request it.\n"
					<< "#\n"
					<< "# <mount point> -fstype=<type>,<options> <file system>\n";

			for(int i=0 ; i < NETFS_NR_OF_ENTRIES; i++)
			{
				if (g_settings.netfs[mt][i].type == (int) CFSMounter::NFS)
				{
					//"%s%s\t-fstype=nfs,%s%s%s\t\t%s:%s\n"
					config_file	<< (g_settings.netfs[mt][i].active			? "" : "#")
							<< ((g_settings.netfs[mt][i].local_dir.empty()) 	? "mountPt" : g_settings.netfs[mt][i].local_dir) << '\t'
							<< "-fstype=nfs,"
							<< g_settings.netfs[mt][i].options1
							<< ((g_settings.netfs[mt][i].options2.empty()) 	? "" : ",")
							<< g_settings.netfs[mt][i].options2 << '\t' << '\t'
							<< ((g_settings.netfs[mt][i].ip.empty()) 		? "0.0.0.0" : g_settings.netfs[mt][i].ip) << ':'
							<< ((g_settings.netfs[mt][i].dir.empty()) 		? "netfsDir": g_settings.netfs[mt][i].dir) << '\n';
				}
				else
				{
					//"%s%s\t-fstype=cifs,user=%s,password=%s%s%s%s%s\t\t\t//%s%s%s\n"
					config_file	<< (g_settings.netfs[mt][i].active?"":"#")
							<< ((g_settings.netfs[mt][i].local_dir.empty()) 	? "mountPt" : g_settings.netfs[mt][i].local_dir) << '\t'
							<< "-fstype=cifs,user=" << g_settings.netfs[mt][i].username << ",password=" << g_settings.netfs[mt][i].password
							<< (g_settings.netfs[mt][i].options1.empty() 	? "" : ",")
							<< g_settings.netfs[mt][i].options1
							<< (g_settings.netfs[mt][i].options2.empty() 	? "" : ",")
							<< g_settings.netfs[mt][i].options2 << '\t' << '\t'
							<< "://" << ((g_settings.netfs[mt][i].ip.empty()) 	? "0.0.0.0" : g_settings.netfs[mt][i].ip)
							<< (((!g_settings.netfs[mt][i].dir.empty()) && g_settings.netfs[mt][i].dir[0]!='/') ? "/" : "")
							<< ((g_settings.netfs[mt][i].dir.empty()) 		? "netfsDir" : g_settings.netfs[mt][i].dir) << '\n';
				}
			}
		}
		config_file.close(); 
	} 
	else { 
		printf("CNETFSMountGui::write_config, error open %s\n", ((mt == SNeutrinoSettings::FSTAB)?fstabPath.c_str():autoPath.c_str()));
	}
	return(0);
}

int CNETFSMountGui::exec( CMenuTarget* parent, const std::string & actionKey )
{
	printf("CNETFSMountGui exec: %s\n", actionKey.c_str());
	int returnval = menu_return::RETURN_REPAINT;

	if (m_nfs_sup == CFSMounter::FS_UNPROBED)
		m_nfs_sup = CFSMounter::fsSupported(CFSMounter::NFS);

	if (m_cifs_sup == CFSMounter::FS_UNPROBED)
		m_cifs_sup = CFSMounter::fsSupported(CFSMounter::CIFS);

	printf("SUPPORT: NFS: %d, CIFS: %d\n", m_nfs_sup, m_cifs_sup);

	if (actionKey.empty() || actionKey.substr(0,4)=="menu")
	{
		int mt = (actionKey.substr(5)=="fstab") ? SNeutrinoSettings::FSTAB : SNeutrinoSettings::AUTOMOUNT;
		parent->hide();

		vinit(mt);
		read_config(mt);

		for(int i=0 ; i < NETFS_NR_OF_ENTRIES; i++)
		{
			m_entry[i] = getEntryString(mt,i);
		}
		returnval = menu(mt);
	}
	else if(actionKey.substr(0,10)=="mountentry")
	{
		int mt = actionKey[10]-'0'; //convert char (49) to int (1)
		int nr = actionKey[11]-'0';

		parent->hide();
		returnval = menuEntry(mt, nr);
		for(int i=0 ; i < NETFS_NR_OF_ENTRIES; i++)
		{
			m_entry[i] = getEntryString(mt,i);
			ISO_8859_1_entry[i] = ZapitTools::UTF8_to_Latin1(m_entry[i]);
		}
	}
	else if(actionKey.substr(0,7)=="domount")
	{
		parent->hide();
		if (actionKey.substr(8,5)=="start")
		{
			if (my_system(2, "/etc/init.d/fstab", "start_netfs") != 0)
				perror("/etc/init.d/fstab failed");
		}
		else
		{
			if (my_system(2, "/etc/init.d/fstab", "stop_netfs") != 0)
				perror("/etc/init.d/fstab failed");
		}

		ShowHint(LOCALE_NETFS_FSTAB_HEAD,g_Locale->getText((actionKey.substr(8,5)=="start")? LOCALE_NETFS_FSTAB_DOMOUNT_INFO : LOCALE_NETFS_FSTAB_DOUMOUNT_INFO),450,3);

		for(int i=0 ; i < NETFS_NR_OF_ENTRIES ; i++)
		{
			if (CFSMounter::isMounted(g_settings.netfs[SNeutrinoSettings::FSTAB][i].local_dir))
			{
				mountMenuEntry[i]->iconName = NEUTRINO_ICON_MOUNTED;
			} else
			{
				mountMenuEntry[i]->iconName = NEUTRINO_ICON_NOT_MOUNTED;
			}
		}
	}
	else if (actionKey.substr(0,12)=="write config")
	{
		int mt = actionKey[12]-'0';

		parent->hide();
		CNeutrinoApp::getInstance()->saveSetup(NEUTRINO_SETTINGS_FILE);
		write_config(mt);

		if(mt==SNeutrinoSettings::FSTAB)
		{
			ShowHint(LOCALE_NETFS_FSTAB_HEAD,g_Locale->getText(LOCALE_NETFS_FSTAB_WRITE_INFO),450,3);
		}
		else
		{
			if (my_system(2, "/etc/init.d/autofs", "restart") != 0)
				perror("autofs failed");

			//FIXME please wait ...
			CHintBox hintBox(LOCALE_NETFS_AUTOMOUNT_HEAD,g_Locale->getText(LOCALE_AUDIOPLAYER_RECEIVING_LIST));
			hintBox.paint();
			sleep(3); //give automounter a few seconds

			for(int i=0 ; i < NETFS_NR_OF_ENTRIES ; i++)
			{
				std::string localdir = "";
				if(!g_settings.netfs[mt][i].local_dir.empty())
				{
					localdir = "/mnt/autofs/" + g_settings.netfs[mt][i].local_dir;
					my_system(2, "ls", localdir.c_str()); //mount
				}

				if (CFSMounter::isMounted(localdir.c_str()))
				{
					mountMenuEntry[i]->iconName = NEUTRINO_ICON_MOUNTED;
				} else
				{
					mountMenuEntry[i]->iconName = NEUTRINO_ICON_NOT_MOUNTED;
				}
			}
			hintBox.hide();
		}
		returnval = menu_return::RETURN_REPAINT;
	}
	else if (actionKey.substr(0,3)=="dir")
	{
		int mt = actionKey[3]-'0';
		int nr = actionKey[4]-'0';

		parent->hide();
		chooserDir(g_settings.netfs[mt][nr].local_dir, false, NULL);
		returnval = menu_return::RETURN_REPAINT;
	}
	return returnval;
}

int CNETFSMountGui::menu(int mt)
{
	std::string hint = (mt == SNeutrinoSettings::FSTAB) ? g_Locale->getText(LOCALE_MENU_HINT_NETFS_FSTAB_WRITE) : g_Locale->getText(LOCALE_MENU_HINT_NETFS_AUTOMOUNT_WRITE);
	std::string txt;

	CMenuWidget mountMenuW(mt == SNeutrinoSettings::FSTAB ? LOCALE_NETFS_FSTAB_HEAD : LOCALE_NETFS_AUTOMOUNT_HEAD, NEUTRINO_ICON_NETWORK, width, mt == SNeutrinoSettings::FSTAB ? MN_WIDGET_ID_NETFS_FSTAB : MN_WIDGET_ID_NETFS_AUTOMOUNT);
	mountMenuW.addIntroItems();

	if(mt==SNeutrinoSettings::FSTAB)
	{
		CMenuForwarder * mf = new CMenuForwarder(LOCALE_NETFS_FSTAB_WRITE, true, NULL, this , ("write config" + to_string(mt)).c_str(), CRCInput::RC_red);
		txt = str_replace("%s", fstabPath, hint);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, txt.c_str());
		mountMenuW.addItem(mf);

		mf = new CMenuForwarder(LOCALE_NETFS_FSTAB_DOMOUNT, true, NULL, this , "domount start", CRCInput::RC_green);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_FSTAB_DOMOUNT);
		mountMenuW.addItem(mf);

		mf = new CMenuForwarder(LOCALE_NETFS_FSTAB_DOUMOUNT, true, NULL, this , "domount stop", CRCInput::RC_yellow);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_FSTAB_DOUMOUNT);
		mountMenuW.addItem(mf);
	}
	else
	{
		CMenuForwarder * mf = new CMenuForwarder(LOCALE_NETFS_AUTOMOUNT_WRITE, true, NULL, this , ("write config" + to_string(mt)).c_str(), CRCInput::RC_red);
		txt = str_replace("%s", autoPath, hint);
		mf->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, txt.c_str());
		mountMenuW.addItem(mf);
	}

	mountMenuW.addItem(GenericMenuSeparatorLine);

	//FIXME please wait ...
	CHintBox hintBox(LOCALE_NETFS_AUTOMOUNT_HEAD,g_Locale->getText(LOCALE_AUDIOPLAYER_RECEIVING_LIST));
	hintBox.paint();

	for(int i=0 ; i < NETFS_NR_OF_ENTRIES ; i++)
	{
		std::string s2 = "mountentry" + to_string(mt) + to_string(i);
		ISO_8859_1_entry[i] = ZapitTools::UTF8_to_Latin1(m_entry[i]);
		mountMenuEntry[i] = new CMenuForwarder("", true, ISO_8859_1_entry[i], this, s2.c_str());

		std::string localdir = "";
		if(!g_settings.netfs[mt][i].local_dir.empty())
		{
			if(mt == SNeutrinoSettings::AUTOMOUNT)
			{
				localdir = "/mnt/autofs/" + g_settings.netfs[mt][i].local_dir;
				my_system(2, "ls", localdir.c_str()); //mount
			} else
			{  
				localdir = g_settings.netfs[mt][i].local_dir;
			}
		}

		if (CFSMounter::isMounted(localdir.c_str()))
		{
			mountMenuEntry[i]->iconName = NEUTRINO_ICON_MOUNTED;
		} else
		{
			mountMenuEntry[i]->iconName = NEUTRINO_ICON_NOT_MOUNTED;
		}
		mountMenuW.addItem(mountMenuEntry[i]);
	}
	hintBox.hide();

	int ret=mountMenuW.exec(this,"");
	return ret;
}

const CMenuOptionChooser::keyval NFS_TYPE_OPTIONS[] =
{
	{ CFSMounter::NFS , LOCALE_NFS_TYPE_NFS },
	{ CFSMounter::CIFS, LOCALE_NFS_TYPE_CIFS }
};
#define NFS_TYPE_OPTION_COUNT (sizeof(NFS_TYPE_OPTIONS)/sizeof(CMenuOptionChooser::keyval))

int CNETFSMountGui::menuEntry(int mt, int nr)
{
	CMenuWidget mountMenuEntryW(mt == SNeutrinoSettings::FSTAB ? LOCALE_NETFS_FSTAB_HEAD : LOCALE_NETFS_AUTOMOUNT_HEAD, NEUTRINO_ICON_SETTINGS, width, MN_WIDGET_ID_NETFS_ENTRYS);
	mountMenuEntryW.addIntroItems(mt == SNeutrinoSettings::FSTAB ? LOCALE_NETFS_FSTAB_INTRO_MOUNTS : LOCALE_NETFS_AUTOMOUNT_INTRO_MOUNTS);

	CIPInput ipInput(LOCALE_NFS_IP, &g_settings.netfs[mt][nr].ip, LOCALE_IPSETUP_HINT_1, LOCALE_IPSETUP_HINT_2);
	CKeyboardInput dirInput(LOCALE_NFS_DIR, &g_settings.netfs[mt][nr].dir);
	CKeyboardInput local_dirInput(LOCALE_NFS_LOCALDIR, &g_settings.netfs[mt][nr].local_dir);

	CKeyboardInput userInput(LOCALE_NFS_USERNAME, &g_settings.netfs[mt][nr].username);
	CMenuForwarder *username_fwd = new CMenuForwarder(LOCALE_NFS_USERNAME, (g_settings.netfs[mt][nr].type != (int)CFSMounter::NFS), g_settings.netfs[mt][nr].username, &userInput);

	CKeyboardInput passwdInput(LOCALE_NFS_PASSWORD, &g_settings.netfs[mt][nr].password);
	CMenuForwarder *password_fwd = new CMenuForwarder(LOCALE_NFS_PASSWORD, (g_settings.netfs[mt][nr].type != (int)CFSMounter::NFS), NULL, &passwdInput);

	CKeyboardInput options1Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.netfs[mt][nr].options1, 30);
	CMenuForwarder *options1_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, g_settings.netfs[mt][nr].options1, &options1Input);

	CKeyboardInput options2Input(LOCALE_NFS_MOUNT_OPTIONS, &g_settings.netfs[mt][nr].options2);
	CMenuForwarder *options2_fwd = new CMenuForwarder(LOCALE_NFS_MOUNT_OPTIONS, true, g_settings.netfs[mt][nr].options2, &options2Input);

	CStringInputSMS dumpInput(LOCALE_NETFS_DUMP, &g_settings.netfs[mt][nr].dump, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder *dump_fwd = new CMenuForwarder(LOCALE_NETFS_DUMP, (mt == SNeutrinoSettings::FSTAB), g_settings.netfs[mt][nr].dump, &dumpInput);

	CStringInputSMS passInput(LOCALE_NETFS_PASS, &g_settings.netfs[mt][nr].pass, 2, NONEXISTANT_LOCALE, NONEXISTANT_LOCALE, "0123456789");
	CMenuForwarder *pass_fwd = new CMenuForwarder(LOCALE_NETFS_PASS, (mt == SNeutrinoSettings::FSTAB), g_settings.netfs[mt][nr].pass, &passInput);

	CMenuOptionChooser *activeInput= new CMenuOptionChooser(LOCALE_NETFS_ACTIVE_ONOFF, &g_settings.netfs[mt][nr].active, MESSAGEBOX_NO_YES_OPTIONS, MESSAGEBOX_NO_YES_OPTION_COUNT, true);

	COnOffNotifier notifier(CFSMounter::NFS);
	notifier.addItem(username_fwd);
	notifier.addItem(password_fwd);

	mountMenuEntryW.addItem(new CMenuOptionChooser(LOCALE_NFS_TYPE, &g_settings.netfs[mt][nr].type, NFS_TYPE_OPTIONS, NFS_TYPE_OPTION_COUNT,	true, &notifier));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_IP,	true, g_settings.netfs[mt][nr].ip,		&ipInput));
	mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_DIR,	true, g_settings.netfs[mt][nr].dir,		&dirInput));

	if(mt == SNeutrinoSettings::FSTAB)
		mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_LOCALDIR,	true, g_settings.netfs[mt][nr].local_dir,	this, ("dir" + to_string(mt) + to_string(nr)).c_str()));
	else
		mountMenuEntryW.addItem(new CMenuForwarder(LOCALE_NFS_LOCALDIR,	true, g_settings.netfs[mt][nr].local_dir,	&local_dirInput));
	

	mountMenuEntryW.addItem(username_fwd);
	mountMenuEntryW.addItem(password_fwd);
	mountMenuEntryW.addItem(options1_fwd);
	mountMenuEntryW.addItem(options2_fwd);
	mountMenuEntryW.addItem(dump_fwd);
	mountMenuEntryW.addItem(pass_fwd);
	mountMenuEntryW.addItem(activeInput);

	int ret = mountMenuEntryW.exec(this,"");
	return ret;
}

int CNETFSSetup::exec( CMenuTarget* parent, const std::string & actionKey )
{
	if (actionKey.empty())
	{
		CNETFSMountGui mountGui;

		CMenuWidget sm_menu(LOCALE_NETFS_MENU_MAIN_HEAD, NEUTRINO_ICON_NETWORK, width, MN_WIDGET_ID_NETFS_MAIN);
		sm_menu.addIntroItems(LOCALE_NETFS_MENU_MAIN_INTRO);

		CMenuForwarder *fstab = new CMenuForwarder(LOCALE_NETFS_MENU_MAIN_EDIT, true, fstabPath, &mountGui, "menu fstab", CRCInput::RC_red);
		CMenuForwarder *automount = new CMenuForwarder(LOCALE_NETFS_MENU_MAIN_EDIT, true, autoPath, &mountGui, "menu automount", CRCInput::RC_green);

		fstab->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_MENU_MAIN_FSTAB);
		automount->setHint(NEUTRINO_ICON_HINT_IMAGELOGO, LOCALE_MENU_HINT_NETFS_MENU_MAIN_AUTOMOUNT);

		sm_menu.addItem(fstab);
		sm_menu.addItem(automount);

		return sm_menu.exec(parent, actionKey);
	}

	return menu_return::RETURN_REPAINT;
}
