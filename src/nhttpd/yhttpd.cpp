//=============================================================================
// YHTTPD
// Main Program
//=============================================================================

// system
#include <csignal>
#include <unistd.h>
#include <cstdlib>
#include <pwd.h>
#include <grp.h>
#include <syscall.h>
#include <stdio.h>

#include <system/set_threadname.h>
#include <system/helpers.h>
// yhttpd
#include "yconfig.h"
#include <ylogging.h>
#include <ylanguage.h>
#include <yhook.h>

#ifdef Y_CONFIG_USE_YPARSER
#include <mod_yparser.h>
static CyParser yParser;
#endif

//-----------------------------------------------------------------------------
// Setting yhttpd Instance
//-----------------------------------------------------------------------------
#include "yhttpd.h"
static Cyhttpd *yhttpd = NULL;
CStringList Cyhttpd::ConfigList;
//=============================================================================
// HOOKS: Definition & Instance for Hooks, attach/detach Hooks
//=============================================================================

#ifdef Y_CONFIG_USE_AUTHHOOK
#include <mod_auth.h>
static CmAuth *auth = NULL;
#endif

#ifdef Y_CONFIG_USE_WEBLOG
#include <mod_weblog.h>
static CmWebLog *weblog = NULL;
#endif

#ifdef Y_CONFIG_USE_SENDFILE
#include <mod_sendfile.h>
static CmodSendfile *mod_sendfile = NULL;
#endif

#ifdef Y_CONFIG_USE_CACHE
#include <mod_cache.h>
static CmodCache mod_cache; // static instance
#endif

//-----------------------------------------------------------------------------

#ifdef CONFIG_SYSTEM_TUXBOX
#include "neutrinoapi.h"
#include <config.h>
static CNeutrinoAPI *NeutrinoAPI;
#endif

//=============================================================================
// Main: Main Entry, Webserver thread
//=============================================================================

void yhttpd_reload_config() {
	if (yhttpd)
		yhttpd->ReadConfig();
}

//-----------------------------------------------------------------------------
// Main Entry
//-----------------------------------------------------------------------------

void thread_cleanup (void *p)
{
	Cyhttpd *y = (Cyhttpd *)p;
	if (y) {
		y->stop_webserver();
		delete y;
	}
	y = NULL;
}

void * nhttpd_main_thread(void *) {
	set_threadname("yweb:main_thread");
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	aprintf("Webserver %s tid %ld\n", WEBSERVERNAME, syscall(__NR_gettid));
	yhttpd = new Cyhttpd();
	//CLogging::getInstance()->setDebug(true);
	//CLogging::getInstance()->LogLevel = 9;
	if (!yhttpd) {
		aprintf("Error initializing WebServer\n");
		return (void *) EXIT_FAILURE;
	}
	/* we pthread_cancel this thread from the main thread, but still want to clean up */
	pthread_cleanup_push(thread_cleanup, yhttpd);
#ifndef Y_CONFIG_FEATURE_THREADING
	yhttpd->flag_threading_off = true;
#endif

	yhttpd->hooks_attach();
	yhttpd->ReadConfig();
	if (yhttpd->Configure()) {
		// Start Webserver: fork ist if not in debug mode
		aprintf("Webserver starting...\n");
		dprintf("Start in Debug-Mode\n"); // non forked debugging loop

		yhttpd->run();
	}
	pthread_cleanup_pop(0);
	delete yhttpd;
	yhttpd = NULL;

	aprintf("Main end\n");
	return (void *) EXIT_SUCCESS;
}
//=============================================================================
// Class yhttpd
//=============================================================================
Cyhttpd::Cyhttpd() {
	webserver = new CWebserver();
	flag_threading_off = false;
}
//-----------------------------------------------------------------------------
Cyhttpd::~Cyhttpd() {
	if (webserver)
		delete webserver;
	CLanguage::deleteInstance();
	webserver = NULL;
}

//-----------------------------------------------------------------------------
// Change to Root
//-----------------------------------------------------------------------------
bool Cyhttpd::Configure() {

	if (!getuid()) // you must be root to do that!
	{
		// Get user and group data
#ifdef Y_CONFIG_FEATURE_HTTPD_USER
		struct passwd *pwd = NULL;
		struct group *grp = NULL;
		std::string username = ConfigList["server.user_name"];
		std::string groupname= ConfigList["server.group_name"];

		// get user data
		if(!username.empty())
		{
			if((pwd = getpwnam(username.c_str())) == NULL)
			{
				dperror("Dont know user to set uid\n");
				return false;
			}
		}
		// get group data
		if(!groupname.empty())
		{
			if((grp = getgrnam(groupname.c_str())) == NULL)
			{
				aprintf("Can not get Group-Information. Group: %s\n", groupname.c_str());
				return false;
			}
		}
#endif
		// change root directory
#ifdef Y_CONFIG_FEATURE_CHROOT
		if(!ConfigList["server.chroot"].empty())
		{
			log_level_printf(2, "do chroot to dir:%s\n", ConfigList["server.chroot"].c_str() );
			// do change Root
			if(chroot(ConfigList["server.chroot"].c_str()) == -1)
			{
				dperror("Change Root failed\n");
				return false;
			}
			// Set Working Dir
			if(chdir("/") == -1)
			{
				dperror("Change Directory to Root failed\n");
				return false;
			}
		}
#endif
#ifdef Y_CONFIG_FEATURE_HTTPD_USER
		if(!username.empty() && pwd != NULL && grp != NULL)
		{
			log_level_printf(2, "set user and groups\n");

			// drop root privileges
			setgid(grp->gr_gid);
			setgroups(0, NULL);
			// set user group
			if(!groupname.empty())
			initgroups(username.c_str(), grp->gr_gid);
			// set user
			if(setuid(pwd->pw_uid) == -1)
			{
				dperror("Change User Context failed\n");
				return false;
			}
		}
#endif
	}
	return true;
}
//-----------------------------------------------------------------------------
// Main Webserver call
//-----------------------------------------------------------------------------
void Cyhttpd::run() {
	set_threadname("yweb:run");
	if (webserver) {
		if (flag_threading_off)
			webserver->is_threading = false;
		webserver->run();
		stop_webserver();
	} else
		aprintf("Error initializing WebServer\n");
}

//-----------------------------------------------------------------------------
// Show Version Text and Number
//-----------------------------------------------------------------------------
void Cyhttpd::version(FILE *dest) {
	fprintf(dest, "%s - Webserver v%s\n", HTTPD_NAME, HTTPD_VERSION);
}

//-----------------------------------------------------------------------------
// Stop WebServer
//-----------------------------------------------------------------------------
void Cyhttpd::stop_webserver() {
	aprintf("stop requested......\n");
	if (webserver) {
		webserver->stop();
		hooks_detach();
	}
}
//-----------------------------------------------------------------------------
// Attach hooks (use hook order carefully)
//-----------------------------------------------------------------------------
void Cyhttpd::hooks_attach() {
#ifdef Y_CONFIG_USE_AUTHHOOK
	// First Check Authentication
	auth = new CmAuth();
	CyhookHandler::attach(auth);
#endif

#ifdef Y_CONFIG_USE_TESTHOOK
	testhook = new CTesthook();
	CyhookHandler::attach(testhook);
#endif

#ifdef CONFIG_SYSTEM_TUXBOX
	NeutrinoAPI = new CNeutrinoAPI();
	CyhookHandler::attach(NeutrinoAPI->NeutrinoYParser);
	CyhookHandler::attach(NeutrinoAPI->ControlAPI);
#else
#ifdef Y_CONFIG_USE_YPARSER
	CyhookHandler::attach(&yParser);
#endif
#endif

#ifdef Y_CONFIG_USE_CACHE
	CyhookHandler::attach(&mod_cache);
#endif

#ifdef Y_CONFIG_USE_SENDFILE
	mod_sendfile = new CmodSendfile();
	CyhookHandler::attach(mod_sendfile);
#endif
#ifdef Y_CONFIG_USE_WEBLOG
	weblog = new CmWebLog();
	CyhookHandler::attach(weblog);
#endif
}

//-----------------------------------------------------------------------------
// Detach hooks & Destroy
//-----------------------------------------------------------------------------
void Cyhttpd::hooks_detach() {
#ifdef Y_CONFIG_USE_AUTHHOOK
	CyhookHandler::detach(auth);
	delete auth;
#endif

#ifdef Y_CONFIG_USE_TESTHOOK
	CyhookHandler::detach(testhook);
	delete testhook;
#endif

#ifdef CONFIG_SYSTEM_TUXBOX
	CyhookHandler::detach(NeutrinoAPI->NeutrinoYParser);
#else
#ifdef Y_CONFIG_USE_YPARSER
	CyhookHandler::detach(&yParser);
#endif
#endif

#ifdef Y_CONFIG_USE_CACHE
	CyhookHandler::detach(&mod_cache);
#endif

#ifdef Y_CONFIG_USE_SENDFILE
	CyhookHandler::detach(mod_sendfile);
#endif

#ifdef Y_CONFIG_USE_WEBLOG
	CyhookHandler::detach(weblog);
	delete weblog;
#endif
}

//-----------------------------------------------------------------------------
// Read Webserver Configurationfile
// Call "Hooks_ReadConfig" so Hooks can read/write own Configuration Values
//-----------------------------------------------------------------------------
void Cyhttpd::ReadConfig(void) {
	log_level_printf(3, "ReadConfig Start\n");
	CConfigFile *Config = new CConfigFile(',');
	bool have_config = false;
	if (access(HTTPD_CONFIGFILE, R_OK) == 0)
		have_config = true;
	Config->loadConfig(HTTPD_CONFIGFILE);
	// convert old config files
	if (have_config) {
		if (Config->getInt32("configfile.version", 0) == 0) {
			CConfigFile OrgConfig = *Config;
			Config->clear();

			Config->setInt32("server.log.loglevel", OrgConfig.getInt32("LogLevel", 0));
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("webserver.websites", "WebsiteMain");
			Config->setBool("webserver.threading", OrgConfig.getBool("THREADS", true));
			Config->setInt32("WebsiteMain.port", OrgConfig.getInt32("Port", HTTPD_STANDARD_PORT));
			Config->setString("WebsiteMain.directory", OrgConfig.getString("PrivatDocRoot", PRIVATEDOCUMENTROOT));
			if (!OrgConfig.getString("PublicDocRoot", "").empty())
				Config->setString("WebsiteMain.override_directory", OrgConfig.getString("PublicDocRoot", PRIVATEDOCUMENTROOT));
			// mod_auth
			Config->setString("mod_auth.username", OrgConfig.getString("AuthUser", AUTHUSER));
			Config->setString("mod_auth.password", OrgConfig.getString("AuthPassword", AUTHPASSWORD));
			Config->setString("mod_auth.no_auth_client", OrgConfig.getString("NoAuthClient", ""));
			Config->setString("mod_auth.authenticate", OrgConfig.getString("Authenticate", "false"));

			Config->setString("mod_sendfile.mime_types", HTTPD_SENDFILE_EXT);

			Config->saveConfig(HTTPD_CONFIGFILE);

		}
		// Add Defaults for Version 2
		if (Config->getInt32("configfile.version") < 2) {
			Config->setString("mod_sendfile.mime_types", HTTPD_SENDFILE_EXT);
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("mod_sendfile.sendAll", HTTPD_SENDFILE_ALL);
			Config->saveConfig(HTTPD_CONFIGFILE);
		}
		// Add Defaults for Version 4
		if (Config->getInt32("configfile.version") < 4) {
			Config->setInt32("configfile.version", CONF_VERSION);
			Config->setString("Language.selected", HTTPD_DEFAULT_LANGUAGE);
			Config->setString("Language.directory", HTTPD_LANGUAGEDIR);
			if (Config->getString("WebsiteMain.hosted_directory", "").empty())
				Config->setString("WebsiteMain.hosted_directory", HOSTEDDOCUMENTROOT);
			Config->saveConfig(HTTPD_CONFIGFILE);
		}
	}
	// configure debugging & logging
	if (CLogging::getInstance()->LogLevel == 0)
		CLogging::getInstance()->LogLevel = Config->getInt32("server.log.loglevel", 0);
	if (CLogging::getInstance()->LogLevel > 0)
		CLogging::getInstance()->setDebug(true);

//NI
#if 0
#ifdef Y_CONFIG_FEATURE_THREADING
	Config->setBool("webserver.threading", true);
	Config->saveConfig(HTTPD_CONFIGFILE);
#endif
#endif

	// get variables
	webserver->init(Config->getInt32("WebsiteMain.port", HTTPD_STANDARD_PORT), Config->getBool("webserver.threading", true));
	// informational use
	ConfigList["WebsiteMain.port"] = itoa(Config->getInt32("WebsiteMain.port", HTTPD_STANDARD_PORT));
	ConfigList["webserver.threading"] = Config->getString("webserver.threading", "true");
	ConfigList["configfile.version"] = Config->getInt32("configfile.version", CONF_VERSION);
	ConfigList["server.log.loglevel"] = itoa(Config->getInt32("server.log.loglevel", 0));
	ConfigList["server.no_keep-alive_ips"] = Config->getString("server.no_keep-alive_ips", "");
	webserver->conf_no_keep_alive_ips = Config->getStringVector("server.no_keep-alive_ips");

	// MainSite
	ConfigList["WebsiteMain.directory"] = Config->getString("WebsiteMain.directory", PRIVATEDOCUMENTROOT);
	ConfigList["WebsiteMain.override_directory"] = Config->getString("WebsiteMain.override_directory", PUBLICDOCUMENTROOT);
	ConfigList["WebsiteMain.hosted_directory"] = Config->getString("WebsiteMain.hosted_directory", HOSTEDDOCUMENTROOT);

	ConfigList["Tuxbox.DisplayLogos"] = Config->getString("Tuxbox.DisplayLogos", "true");
	// Check location of logos
	if (Config->getString("Tuxbox.LogosURL", "").empty()) {
		if (access(ConfigList["WebsiteMain.override_directory"] + "/logos", R_OK) == 0) {
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.override_directory"] + "/logos");
			have_config = false; //save config
		}
		else if (access(ConfigList["WebsiteMain.directory"] + "/logos", R_OK) == 0){
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.directory"] + "/logos");
			have_config = false; //save config
		}
#ifdef Y_CONFIG_USE_HOSTEDWEB
		else if (access(ConfigList["WebsiteMain.hosted_directory"] + "/logos", R_OK) == 0){
			Config->setString("Tuxbox.LogosURL", ConfigList["WebsiteMain.hosted_directory"] + "/logos");
			have_config = false; //save config
		}
#endif //Y_CONFIG_USE_HOSTEDWEB
	}
	ConfigList["Tuxbox.LogosURL"] = Config->getString("Tuxbox.LogosURL", "");

#ifdef Y_CONFIG_USE_OPEN_SSL
	ConfigList["SSL"] = Config->getString("WebsiteMain.ssl", "false");
	ConfigList["SSL_pemfile"] = Config->getString("WebsiteMain.ssl_pemfile", SSL_PEMFILE);
	ConfigList["SSL_CA_file"] = Config->getString("WebsiteMain.ssl_ca_file", SSL_CA_FILE);

	CySocket::SSL_pemfile = ConfigList["SSL_pemfile"];
	CySocket::SSL_CA_file = ConfigList["SSL_CA_file"];
	if(ConfigList["SSL"] == "true")
	CySocket::initSSL();
#endif
	ConfigList["server.user_name"] = Config->getString("server.user_name", "");
	ConfigList["server.group_name"] = Config->getString("server.group_name", "");
	ConfigList["server.chroot"] = Config->getString("server.chroot", "");

	// language
	ConfigList["Language.directory"] = Config->getString("Language.directory", HTTPD_LANGUAGEDIR);
	ConfigList["Language.selected"] = Config->getString("Language.selected", HTTPD_DEFAULT_LANGUAGE);
	yhttpd->ReadLanguage();

	// Read App specifig settings by Hook
	CyhookHandler::Hooks_ReadConfig(Config, ConfigList);

	// Save if new defaults are set
	if (!have_config)
		Config->saveConfig(HTTPD_CONFIGFILE);
	log_level_printf(3, "ReadConfig End\n");
	delete Config;
}
//-----------------------------------------------------------------------------
// Read Webserver Configurationfile for languages
//-----------------------------------------------------------------------------
void Cyhttpd::ReadLanguage(void) {
	// Init Class vars
	CLanguage *lang = CLanguage::getInstance();
	log_level_printf(3, "ReadLanguage:%s\n", ConfigList["Language.selected"].c_str());
	lang->setLanguage(ConfigList["Language.selected"]);
}
