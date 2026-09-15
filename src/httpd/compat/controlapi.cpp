//=============================================================================
// NHTTPD
// Neutrino ControlAPI
//=============================================================================
#include <config.h>
// C
#include <stdio.h>
#include <stdint.h>
#include <cctype>

// C++
#include <string>
#include <fstream>
#include <limits>
#include <map>
#include <utility>
#include <vector>
// system
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/vfs.h> // for statfs
#include <fcntl.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <dirent.h>
// tuxbox
#include <global.h>
#include <neutrinoMessages.h>
#include <zapit/client/zapittools.h>
#include <zapit/zapit.h>
#include <coreapi/channels.h>
#include <coreapi/base/errors.h>
#include <coreapi/storage.h>
#include <coreapi/epg.h>
#include <coreapi/osd.h>
#include <coreapi/system.h>
#include <coreapi/timers.h>
#include <eitd/sectionsd.h>
#include <configfile.h>
#include <system/configure_network.h>
#include <cs_api.h>
#include <gui/plugins.h>//for relodplugins
#include <neutrino.h>
#include <driver/display.h>
#include <driver/screenshot.h>
#include <gui/rc_lock.h>
#include <src/tools/rcsim.h> // rcsim.h is located in $(top_builddir)
#include <driver/pictureviewer/pictureviewer.h>
extern CPictureViewer *g_PicViewer;

#include "httpd/compat/helper.h"
#include "httpd/compat/query.h"
#include "httpd/compat/scriptrunner.h"
#include "httpd/compat/neutrinoapi.h"
#include "httpd/compat/controlapi.h"
#include <hardware/video.h>
#include <zapit/femanager.h>

/* Two of the four files /control/config names, spelled here because the header
   that spelled them belonged to the removed server. The other two are named in
   global.h, which this file already has. Neither of these is that server's
   file: one is the old page set's, one is the movie browser's, and both stay
   under CONFIGDIR, where a box upgraded from an older image still has them. */
#define MOVIEBROWSER_CONFIGFILE CONFIGDIR "/moviebrowser.conf"
#define YWEB_CONFIGFILE CONFIGDIR "/yWeb.conf"

/* The version /control/info?nhttpd_version answers with, which came from the
   same header. A constant here and not derived from anything this program
   carries a version of: it is a number clients compare against, and what they
   compare against is the number that server last reported. */
#define HTTPD_VERSION "3.5.0"

/* The removed server's logging singleton went out with it, and so did the
   calls that went through it here. Every one of them was behind that server's
   debug switch or behind a log level nothing sets any more, so none of them
   printed a line on a box as shipped. The one call that printed whatever the
   level was is a plain printf now, the way the other line in this file that
   reports something an operator can act on already was. */

// CControlAPI now lives in httpd::compat; every definition below still
// writes the bare name, and a qualified-id needs it resolvable unqualified
// to bind to the right class. This one alias does that without bringing the
// rest of the namespace into scope, which is what the enum collision below
// needs kept out.
typedef httpd::compat::CControlAPI CControlAPI;

extern cVideo * videoDecoder;

// A member function defined out of class is anchored, for name lookup, at
// the namespace the class itself was declared in (httpd::compat) rather than
// at this file's own top level. A local "extern CRemoteControl *g_RemoteControl;"
// inside such a function therefore declares httpd::compat::g_RemoteControl,
// a distinct entity from neutrino.cpp's global, even with this declaration
// already visible above it; the three call sites below name the global with
// "::" instead of redeclaring it locally.
extern CRemoteControl * g_RemoteControl;

//NI
#include <system/helpers.h>
#include "gui/infoicons.h"

//NI
#include "gui/update.h"

extern CPlugins *g_Plugins;//for relodplugins
extern CBouquetManager *g_bouquetManager;

#if HAVE_CST_HARDWARE
#ifndef RC_DEVICE
#define RC_DEVICE "/dev/input/nevis_ir"
#endif
#ifndef RC_DEVICE_FALLBACK
#define RC_DEVICE_FALLBACK "/dev/input/event0"
#endif

#elif BOXMODEL_H7
#ifndef RC_DEVICE
#define RC_DEVICE "/dev/input/event2"
#endif
#ifndef RC_DEVICE_FALLBACK
#define RC_DEVICE_FALLBACK "/dev/input/event1"
#endif

#elif BOXMODEL_MULTIBOX || BOXMODEL_MULTIBOXSE || BOXMODEL_OSMIO4K || BOXMODEL_OSMIO4KPLUS
#ifndef RC_DEVICE
#define RC_DEVICE "/dev/input/event0"
#endif
#ifndef RC_DEVICE_FALLBACK
#define RC_DEVICE_FALLBACK "/dev/input/event1"
#endif

#else
#ifndef RC_DEVICE
#define RC_DEVICE "/dev/input/event1"
#endif
#ifndef RC_DEVICE_FALLBACK
#define RC_DEVICE_FALLBACK "/dev/input/event0"
#endif

#endif

//-----------------------------------------------------------------------------
// None of the EPG endpoints has ever taken a window, so every read below asks
// for the whole schedule of the channel. Nothing a request carries sizes it.
// The bounds are the ends of the range rather than the epoch and the far
// future, so that the window holds every event a time_t can express and the
// filter cannot drop one the unwindowed read would have kept.
static const time_t EPG_WHOLE_FROM = std::numeric_limits<time_t>::min();
static const time_t EPG_WHOLE_TO = std::numeric_limits<time_t>::max();

//-----------------------------------------------------------------------------
//=============================================================================
// Initialization of static variables
//=============================================================================
std::string CControlAPI::PLUGIN_DIRS[PLUGIN_DIR_COUNT];

//=============================================================================
// constructor und destructor
//=============================================================================
CControlAPI::CControlAPI(CNeutrinoAPI *_NeutrinoAPI)
{
	NeutrinoAPI = _NeutrinoAPI;
}
//-----------------------------------------------------------------------------
void CControlAPI::init(CyhookHandler *hh)
{
	if(PLUGIN_DIRS[0].empty())
	{	// given in nhttpd.conf
		PLUGIN_DIRS[0]=PLUGIN_DIRS[1]=hh->WebserverConfigList["WebsiteMain.override_directory"];
		PLUGIN_DIRS[1].append("/scripts");
		PLUGIN_DIRS[2]=PLUGIN_DIRS[3]=hh->WebserverConfigList["WebsiteMain.directory"];
		PLUGIN_DIRS[3].append("/scripts");
		PLUGIN_DIRS[4]=GAMESDIR;
		PLUGIN_DIRS[5]=settingsText(g_settings.plugin_hdd_dir);
		PLUGIN_DIRS[6]=PLUGINDIR_MNT;
		PLUGIN_DIRS[7]=PLUGINDIR_VAR;
		PLUGIN_DIRS[8]=PLUGINDIR;
	}
}

//=============================================================================
// Main Dispatcher / Call definitions
//=============================================================================
const CControlAPI::TyCgiCall CControlAPI::yCgiCallList[]=
{
	// channel & bouquet & epg & zapping handling
	{"getservicesxml",	&CControlAPI::GetServicesxmlCGI,	""},
	{"getbouquetsxml",	&CControlAPI::GetBouquetsxmlCGI,	""},
	{"getubouquetsxml",	&CControlAPI::GetUBouquetsxmlCGI,	""},
	{"channellist",		&CControlAPI::ChannellistCGI,		"text/plain"},
	{"logolist",		&CControlAPI::LogolistCGI,		"text/plain"},
	{"getbouquet",		&CControlAPI::GetBouquetCGI,		"+xml"},
	{"getchannel",		&CControlAPI::GetChannelCGI,		""},
	{"getbouquets",		&CControlAPI::GetBouquetsCGI,		"+xml"},
	{"getmode",		&CControlAPI::GetModeCGI,		"text/plain"},
	{"setmode",		&CControlAPI::SetModeCGI,		"text/plain"},
	{"epgsearchxml",	&CControlAPI::EpgSearchXMLCGI,		""},
	{"epgsearch",		&CControlAPI::EpgSearchCGI,		""},
	{"epg",			&CControlAPI::EpgCGI,			""},
	{"zapto",		&CControlAPI::ZaptoCGI,			"text/plain"},
	{"signal",		&CControlAPI::SignalInfoCGI,		"text/plain"},
	{"getonidsid",		&CControlAPI::GetChannelIDCGI,		"text/plain"},
	{"getchannelid",	&CControlAPI::GetChannelIDCGI,		""},
	{"getchannelinfo",	&CControlAPI::GetChannelInfoCGI,	""},
	{"getepgid",		&CControlAPI::GetEpgIDCGI,		""},
	{"currenttpchannels",	&CControlAPI::GetTPChannel_IDCGI,	"text/plain"},
	// boxcontrol - system
	{"standby",		&CControlAPI::StandbyCGI,		"text/plain"},
	{"shutdown",		&CControlAPI::ShutdownCGI,		"text/plain"},
	{"reboot",		&CControlAPI::RebootCGI,		"text/plain"},
	{"restart",		&CControlAPI::RestartCGI,		"text/plain"},
	{"getdate",		&CControlAPI::GetDateCGI,		"text/plain"},
	{"gettime",		&CControlAPI::GetTimeCGI,		"text/plain"},
	{"info",		&CControlAPI::InfoCGI,			"text/plain"},
	{"boxinfo",		&CControlAPI::BoxInfoCGI,		"text/plain"},
	{"version",		&CControlAPI::VersionCGI,		""},
	{"reloadsetup",		&CControlAPI::ReloadNeutrinoSetupCGI,	""},
	{"reloadplugins",	&CControlAPI::ReloadPluginsCGI,		""},
	{"reloadchannels",	&CControlAPI::ReloadChannelsCGI,	""},
#ifdef SCREENSHOT
	{"screenshot",		&CControlAPI::ScreenshotCGI,		""},
#endif
#ifdef ENABLE_GRAPHLCD
	{"glcdscreenshot",	&CControlAPI::GlcdScreenshotCGI,	""},
#endif
	// boxcontrol - devices
	{"volume",		&CControlAPI::VolumeCGI,		"text/plain"},
	{"lcd",			&CControlAPI::LCDAction,		"text/plain"},
	{"system",		&CControlAPI::SystemCGI,		"text/plain"},
	{"message",		&CControlAPI::MessageCGI,		"text/plain"},
	{"rc",			&CControlAPI::RCCGI,			"text/plain"},
	{"rcem",		&CControlAPI::RCEmCGI,			"text/plain"},
	// Start skripts, plugins
	{"startplugin",		&CControlAPI::StartPluginCGI,		"text/plain"},
	{"exec",		&CControlAPI::ExecCGI,			"text/plain"},
	{"yweb",		&CControlAPI::YWebCGI,			"text/plain"},
	// video & Audio handling
	{"aspectratio",		&CControlAPI::AspectRatioCGI,		"text/plain"},
	{"videoformat",		&CControlAPI::VideoFormatCGI,		"text/plain"},
	{"videooutput",		&CControlAPI::VideoOutputCGI,		"text/plain"},
	{"vcroutput",		&CControlAPI::VCROutputCGI,		"text/plain"},
	{"avinputmode",		&CControlAPI::AVInputModeCGI,		"text/plain"},
	{"audio",		&CControlAPI::AudioCGI,			"text/plain"},
	{"crypt",		&CControlAPI::CryptCGI,			"text/plain"},
	// timer
	{"timer",		&CControlAPI::TimerCGI,			"text/plain"},
	{"sendalltimers",		&CControlAPI::TimerSendCGI,			"text/plain"},
	// bouquet editing
	{"setbouquet",		&CControlAPI::setBouquetCGI,		"text/plain"},
	{"savebouquet",		&CControlAPI::saveBouquetCGI,		"text/plain"},
	{"movebouquet",		&CControlAPI::moveBouquetCGI,		"text/plain"},
	{"deletebouquet",	&CControlAPI::deleteBouquetCGI,		"text/plain"},
	{"addbouquet",		&CControlAPI::addBouquetCGI,		"text/plain"},
	{"renamebouquet",	&CControlAPI::renameBouquetCGI,		"text/plain"},
	{"changebouquet",	&CControlAPI::changeBouquetCGI,		"text/plain"},
	{"updatebouquet",	&CControlAPI::updateBouquetCGI,		"text/plain"},
	// xmltv
	{"xmltv.data",		&CControlAPI::xmltvepgCGI,		"+xml"},
	{"xmltv.xml",		&CControlAPI::xmltvepgCGI,		"+xml"},
	{"xmltv.m3u",		&CControlAPI::xmltvm3uCGI,		""},
	{"xmltvlist",		&CControlAPI::xmltvlistCGI,		"text/plain"},
	// utils
	{"build_live_url",	&CControlAPI::build_live_url,		""},
	{"build_playlist",	&CControlAPI::build_playlist,		""},
	{"get_logo",		&CControlAPI::logoCGI,			"text/plain"},
	// settings
	{"config",		&CControlAPI::ConfigCGI,		"text/plain"},
	// filehandling
	{"file",		&CControlAPI::FileCGI,			"+xml"},
	{"statfs",		&CControlAPI::StatfsCGI,		"+xml"},
	{"getdir",		&CControlAPI::getDirCGI,		"+xml"},
	{"getmovies",		&CControlAPI::getMoviesCGI,		"+xml"},
	//NI
	{"infoicons",		&CControlAPI::InfoIconsCGI,		"text/plain"},
	{"updateinfo",		&CControlAPI::UpdateInfoCGI,		""}
};
//-----------------------------------------------------------------------------
// Main Dispatcher
//-----------------------------------------------------------------------------
void CControlAPI::Execute(CyhookHandler *hh)
{
	int index = -1;
	std::string filename = hh->UrlData["filename"];

	// tolower(filename)
	for(unsigned int i = 0; i < filename.length(); i++)
		filename[i] = tolower(filename[i]);

	func_req = filename;

	// get function index
	for(unsigned int i = 0; i < (sizeof(yCgiCallList)/sizeof(yCgiCallList[0])); i++)
		if (func_req == yCgiCallList[i].func_name)
		{
			index = i;
			break;
		}

	if(index == -1) // function not found
	{
		hh->SetError(httpd::compat::HTTP_NOT_IMPLEMENTED, httpd::compat::HANDLED_NOT_IMPLEMENTED);
		return;
	}
	// send header
	else if(std::string(yCgiCallList[index].mime_type).empty())	// decide in function
		;
	else if(std::string(yCgiCallList[index].mime_type) == "+xml")		// Parameter xml?
		if (hh->getOutType() == httpd::compat::xml)
			hh->SetHeader(httpd::compat::HTTP_OK, "text/xml; charset=UTF-8");
		else
			hh->SetHeader(httpd::compat::HTTP_OK, "text/html; charset=UTF-8");
	else
		hh->SetHeader(httpd::compat::HTTP_OK, yCgiCallList[index].mime_type);

	// response
	hh->status = httpd::compat::HANDLED_READY;
	if (hh->Method == httpd::compat::M_HEAD)	// HEAD or function call
		return;
	else
	{
		(this->*yCgiCallList[index].pfunc)(hh);
		return;
	}
}

//-----------------------------------------------------------------------------
size_t CControlAPI::endpointCount()
{
	return sizeof(yCgiCallList) / sizeof(yCgiCallList[0]);
}

const char *CControlAPI::endpointName(size_t index)
{
	return yCgiCallList[index].func_name;
}

//=============================================================================
// CGI Functions
// CyhookHandler contains input/output abstractions
//=============================================================================
void CControlAPI::TimerCGI(CyhookHandler *hh)
{
	hh->outStart();

	if (NeutrinoAPI->Timerd->isTimerdAvailable())
	{
		if (!hh->ParamList.empty() && hh->ParamList["format"].empty())
		{
			if (hh->ParamList["action"] == "new")
				doNewTimer(hh);
			else if (hh->ParamList["action"] == "modify")
				doModifyTimer(hh);
			else if (hh->ParamList["action"] == "remove")
			{
				unsigned removeId = atoi(hh->ParamList["id"].c_str());
				// The answer is ok whatever came back, as it has always been
				// here. The facade tells a removal that happened from an id
				// nobody has; this endpoint has never made that difference and
				// a client that has been reading "ok" cannot start seeing
				// "error" for the same request.
				coreapi::timers::remove((uint32_t) removeId);
				hh->SendOk();
			}
			else if(!hh->ParamList["get"].empty())
			{
				int pre=0,post=0;
				NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
				if(hh->ParamList["get"] == "pre")
					hh->printf("%d\n", pre);
				else if(hh->ParamList["get"] == "post")
					hh->printf("%d\n", post);
				else
					hh->SendError();
			}
		}
		else {
			if (hh->getOutType() == httpd::compat::plain)
				SendTimersPlain(hh);
			else
				SendTimers(hh);
		}
	}
	else
		hh->SendError();
}

void CControlAPI::TimerSendCGI(CyhookHandler *hh)
{
	hh->outStart();

	if (NeutrinoAPI->Timerd->isTimerdAvailable())
	{
		if (!hh->ParamList.empty())
		{
			bool force = (hh->ParamList["force"] == "1") || (hh->ParamList["force"] == "true");
			if(!hh->ParamList["ip"].empty())
			{
				NeutrinoAPI->SendAllTimers(hh->ParamList["ip"],force);
				hh->SendOk();
			}
			else if(!hh->ParamList["name"].empty())
			{
				NeutrinoAPI->SendAllTimers(NeutrinoAPI->GetRemoteBoxIP(decodeString(hh->ParamList["name"])),force);
				hh->SendOk();
			}
			else
				hh->SendError();
		}
	}
	else
		hh->SendError();
}
//-----------------------------------------------------------------------------

void CControlAPI::SetModeCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		if (hh->ParamList["1"] == "status")	// display recoding status
		{
			if (NeutrinoAPI->Zapit->isRecordModeActive())
				hh->WriteLn("on");
			else
				hh->WriteLn("off");
			return;
		}

		if (hh->ParamList["1"] == "radio")	// switch to radio mode
		{
			if(CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby){
				neutrino_msg_data_t mode = NeutrinoModes::mode_radio;
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(neutrino_msg_data_t));
				sleep(1);
				NeutrinoAPI->UpdateBouquets();
			}else{
				::g_RemoteControl->radioMode();
			}
		}
		else if (hh->ParamList["1"] == "tv")	// switch to tv mode
		{
			if(CNeutrinoApp::getInstance()->getMode() != NeutrinoModes::mode_standby){
				neutrino_msg_data_t mode = NeutrinoModes::mode_tv;
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::CHANGEMODE, CEventServer::INITID_HTTPD, (void *)&mode,sizeof(neutrino_msg_data_t));
				sleep(1);
				NeutrinoAPI->UpdateBouquets();
			}else{
				::g_RemoteControl->tvMode();
			}
		}
		else if (hh->ParamList["record"] == "start")	// start record mode
		{
#if 0
			if(hh->ParamList["stopplayback"] == "true")
				NeutrinoAPI->Zapit->stopPlayBack();
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			NeutrinoAPI->Zapit->setRecordMode(true);
#endif
			CTimerd::RecordingInfo recinfo;
			recinfo.eventID = 0;
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RECORD_START, CEventServer::INITID_HTTPD, (void *)&recinfo, sizeof(CTimerd::RecordingInfo));
		}
		else if (hh->ParamList["record"] == "stop")	// stop record mode
		{
			/* This stops nothing, and it answers ok all the same. Two
			   reasons, both below: the block written here is a RecordingInfo
			   and what reads it at the other end reads a RecordingStopInfo,
			   and the identifier in it is nought, which names no recording,
			   so the lookup finds none and the branch that reports that only
			   prints. Left exactly as it is: this surface is reproduced to
			   the byte, and what it has answered for years is part of what is
			   being reproduced. The route that ends a recording and says
			   whether it did is /api/v1/recordings/{id}. */
#if 0
			NeutrinoAPI->Zapit->setRecordMode(false);
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			if (!NeutrinoAPI->Zapit->isPlayBackActive())
				NeutrinoAPI->Zapit->startPlayBack();
#endif
			CTimerd::RecordingInfo recinfo;
			recinfo.eventID = 0; // FIXME must present
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RECORD_STOP, CEventServer::INITID_HTTPD, (void *)&recinfo, sizeof(CTimerd::RecordingInfo));
		}
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::GetModeCGI(CyhookHandler *hh)
{
	hh->outStart();
	std::string result = "";
	std::string key = "mode";

	if (hh->ParamList_exist("channelsmode") && hh->ParamList["channelsmode"] != "false")
	{
		key = "channelsmode";
		int mode = NeutrinoAPI->Zapit->getMode();
		if (mode == CZapitClient::MODE_TV)
			result = "tv";
		else if (mode == CZapitClient::MODE_RADIO)
			result = "radio";
		else
			result = "unknown";
	}
	else
	{
		// An unsettled box and a mode with no name of its own both read as
		// unknown.
		coreapi::Result<int> r = coreapi::channels::mode();
		int mode = r.ok() ? r.value() : NeutrinoModes::mode_unknown;
		if (mode == NeutrinoModes::mode_tv)
			result = "tv";
		else if (mode == NeutrinoModes::mode_radio)
			result = "radio";
		else if (mode == NeutrinoModes::mode_avinput)
			result = "avinput";
		else if (mode == NeutrinoModes::mode_standby)
			result = "standby";
		else if (mode == NeutrinoModes::mode_audio)
			result = "audio";
		else if (mode == NeutrinoModes::mode_pic)
			result = "pic";
		else if (mode == NeutrinoModes::mode_ts)
			result = "ts";
		else if (mode == NeutrinoModes::mode_webtv)
			result = "webtv";
		else if (mode == NeutrinoModes::mode_upnp)
			result = "upnp";
		else if (mode == NeutrinoModes::mode_webradio)
			result = "webradio";
		else
			result = "unknown";
	}

	if (!result.empty())
	{
		if (hh->getOutType() != httpd::compat::plain)
		{
			result = hh->outPair(key, result, false);
			result = hh->outObject("getmode", result);
		}
		hh->SendResult(result);
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::ExecCGI(CyhookHandler *hh)
{
	std::string script, result;
	if (!hh->ParamList.empty() )
	{
		script = hh->ParamList["1"];
		unsigned int len = hh->ParamList.size();
		for(unsigned int y=2; y<=len; y++)
			if(!hh->ParamList[itoa(y)].empty())
			{
				script += " ";
				script += hh->ParamList[itoa(y)];
			}
		result = yExecuteScript(script);
	}
	else
	{
		printf("[nhttpd] [%s] no script given\n", __func__);
		result = "error";
	}

	if (result == "error")
		hh->SetError(httpd::compat::HTTP_NOT_FOUND);
	else
		hh->WriteLn(result);
}

//-----------------------------------------------------------------------------
void CControlAPI::SystemCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		//FIXME: No system information until now
		hh->SendOk();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
// An unsettled box is not in standby, which is what the raw mode read said
// before it had a status of its own to say it with.
static bool isInStandby()
{
	coreapi::Result<int> m = coreapi::channels::mode();
	return m.ok() && m.value() == NeutrinoModes::mode_standby;
}

void CControlAPI::StandbyCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList.empty()))
	{
		bool CEC_HDMI_off = false;
		if (!(hh->ParamList["cec"].empty())){
			if(hh->ParamList["cec"]=="off"){
				CEC_HDMI_off = true;
			}
		}

		// Both arms send up to two commands and answer once, after the last
		// of them, so that a command that did not go out is reported here as
		// it is everywhere else rather than answered with ok.
		if (hh->ParamList["1"] == "on")	// standby mode on
		{
			bool sent = true;

			//dont use CEC with standbyoff (TV off) --- use: control/standby?off&cec=off
			if(g_settings.hdmi_cec_standby && CEC_HDMI_off){
				videoDecoder->SetCECAutoStandby(0);
			}

			if (!isInStandby())
				sent = coreapi::system::standby(true).ok();

			if(g_settings.hdmi_cec_standby && CEC_HDMI_off){//dont use CEC with standbyoff (TV off)
				if (!coreapi::system::hdmiCec(false).ok())
					sent = false;
			}

			if (sent)
				hh->SendOk();
			else
				hh->SendError();
		}
		else if (hh->ParamList["1"] == "off")// standby mode off
		{
			bool sent = true;

			//dont use CEC with with view on (TV on) --- use: control/standby?off&cec=off
			if(g_settings.hdmi_cec_view_on && CEC_HDMI_off){
				videoDecoder->SetCECAutoView(0);
			}

			NeutrinoAPI->Zapit->setStandby(false);
			if (isInStandby())
				sent = coreapi::system::standby(false).ok();

			if(g_settings.hdmi_cec_view_on && CEC_HDMI_off){//dont use CEC with view on (TV on)
				if (!coreapi::system::hdmiCec(true).ok())
					sent = false;
			}

			if (sent)
				hh->SendOk();
			else
				hh->SendError();
		}
		else
			hh->SendError();

	}
	else
		if (isInStandby())
			hh->WriteLn("on");
		else
			hh->WriteLn("off");
}

//-----------------------------------------------------------------------------
void CControlAPI::RCCGI(CyhookHandler *hh)
{
	bool locked = CRCLock::getInstance()->isLocked();

	if (hh->ParamList.empty() || hh->ParamList["1"] == "status")
	{
		if (locked)
			hh->WriteLn("off");
		else
			hh->WriteLn("on");
	}
	else
	{
		if (hh->ParamList["1"] == "lock")
		{
			if (!locked)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::LOCK_RC, CEventServer::INITID_HTTPD);
		}
		else if (hh->ParamList["1"] == "unlock")
		{
			if (locked)
				NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::UNLOCK_RC, CEventServer::INITID_HTTPD);
		}
		else
		{
			hh->SendError();
			return;
		}
		hh->SendOk();
	}
}

//-----------------------------------------------------------------------------
// Get actual Date
//-----------------------------------------------------------------------------
void CControlAPI::GetDateCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		std::string timestr = getNowTimeStr("%d.%m.%Y\n");
		hh->Write(timestr);
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
// Get actual Time
//-----------------------------------------------------------------------------
void CControlAPI::GetTimeCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		std::string timestr = getNowTimeStr("%H:%M:%S\n");
		hh->Write(timestr);
	}
	else if (hh->ParamList["1"].compare("rawtime") == 0)
	{
		time_t now = time(0);
		hh->printf("%ld\n",now);
	}
	else
		hh->SendError();
}
//-----------------------------------------------------------------------------
// send services.xml
void CControlAPI::GetServicesxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(ZAPITDIR "/services.xml");
}

//-----------------------------------------------------------------------------
// send bouquets.xml
void CControlAPI::GetBouquetsxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(ZAPITDIR "/bouquets.xml");
}
//-----------------------------------------------------------------------------

// send ubouquets.xml
void CControlAPI::GetUBouquetsxmlCGI(CyhookHandler *hh)
{
	hh->SendFile(ZAPITDIR "/ubouquets.xml");
}

//-------------------------------------------------------------------------
/** Display channel id's
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getonidsid
 * /control/getchannelid[?format=plain|json|xml]
 * @endcode
 *
 * @par output
 * @code
 * /control/getonidsid
 * @endcode
 *
 * @code
 * 3f300012b66
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getchannelid?format=xml
 * @endcode
 *
 * @code
 * <id>
 *     <id>361d03f300012b66</id>
 *     <short_id>3f300012b66</short_id>
 * </id>
 * @endcode
 */
//-----------------------------------------------------------------------------
// get actual channel_id
void CControlAPI::GetChannelIDCGI(CyhookHandler *hh)
{
	t_channel_id channel_id = CZapit::getInstance()->GetCurrentChannelID();
	if (func_req == "getonidsid") //what a terrible name!
	{
		hh->WriteLn(string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL));
		return;
	}

	hh->outStart();
	std::string result = "";
	result = hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
	result += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL), false);
	result = hh->outObject("id", result);
	hh->SendResult(result);
}

// get actual channel_info
void CControlAPI::GetChannelInfoCGI(CyhookHandler *hh)
{
	// The EPG below is keyed on the id zapit names, and that id still answers
	// when the lookup for it does not, so the two are read apart.
	t_channel_id channel_id = CZapit::getInstance()->GetCurrentChannelID();
	coreapi::Result<coreapi::ChannelInfo> current = coreapi::channels::current();

	hh->outStart();
	std::string result = "";
	// An idle box answers with an empty name rather than no answer.
	result = hh->outObject("name", hh->outValue(current.ok() ? current.value().name : std::string()) + "\n");

	CShortEPGData epg;
	CSectionsdClient::CurrentNextInfo CurrentNext;
	CEitManager::getInstance()->getCurrentNextServiceKey(channel_id, CurrentNext);

	if (CurrentNext.flags & CSectionsdClient::epgflags::has_current)
	{
		result += hh->outObject("epg_now", hh->outValue(CurrentNext.current_name) + "\n");
		result += hh->outObject("duration", string_printf("%d/", (abs(time(NULL) - CurrentNext.current_zeit.startzeit) + 30) / 60) + string_printf("%d\n", CurrentNext.current_zeit.dauer / 60));
	}
	else
	{
		result += hh->outObject("epg_now", "\n");
		result += hh->outObject("duration", "0/0\n");
	}

	if (CurrentNext.flags & CSectionsdClient::epgflags::has_next)
	{
		result += hh->outObject("epg_next", hh->outValue(CurrentNext.next_name) + "\n");
	}
	else
		result += hh->outObject("epg_next", "\n");

	hh->SendResult(result);
}

//-------------------------------------------------------------------------
/** Display epg id's
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getepgid[?format=plain|json|xml]
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getepgid?format=xml
 * @endcode
 *
 * @code
 * <epg_id>
 *     <epg_id>361d03f300012b66</epg_id>
 *     <short_epg_id>3f300012b66</short_epg_id>
 * </id>
 * @endcode
 */
//-----------------------------------------------------------------------------
// get actual epg_id
void CControlAPI::GetEpgIDCGI(CyhookHandler *hh)
{
	// The epg id wanted here is the running channel's own, so this is a channel
	// read and not an EPG one. The running id is asked for only where that read
	// could not resolve it, which is what the answer falls back to; asking for
	// it up front would read it twice for every answer that never needs it.
	coreapi::Result<coreapi::ChannelInfo> current = coreapi::channels::current();
	t_channel_id epg_id = current.ok() ? (t_channel_id) current.value().epg_id
					  : CZapit::getInstance()->GetCurrentChannelID();

	hh->outStart();
	std::string result = "";
	result = hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
	result += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id & 0xFFFFFFFFFFFFULL), false);
	result = hh->outObject("epg_id", result);
	hh->SendResult(result);
}

//-----------------------------------------------------------------------------
void CControlAPI::GetTPChannel_IDCGI(CyhookHandler *hh)
{
	SendChannelList(hh, true);
}

//-----------------------------------------------------------------------------
void CControlAPI::MessageCGI(CyhookHandler *hh)
{
	std::string message;
	coreapi::osd::MessageKind kind = coreapi::osd::MessageKind::Hint;

	if (!(hh->ParamList["popup"].empty()))
	{
		message = hh->ParamList["popup"];
		kind = coreapi::osd::MessageKind::Hint;
	}
	else if (!(hh->ParamList["nmsg"].empty()))
	{
		message = hh->ParamList["nmsg"];
		kind = coreapi::osd::MessageKind::Box;
	}
	else
	{
		hh->SendError();
		return;
	}

	// The timeout rides inside the words and is cut back out of them where the
	// message is drawn, because the event carries one block and no second field.
	if (!(hh->ParamList["timeout"].empty()))
	{
		message += "&timeout=";
		message += hh->ParamList["timeout"];
	}

	if (coreapi::osd::message(kind, message).ok())
		hh->SendOk();
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::InfoCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
		hh->Write(PACKAGE_NAME " " PACKAGE_VERSION "\n");
	else
	{
		if (hh->ParamList["1"] == "streaminfo")		// print streaminfo
			SendStreamInfo(hh);
		else if (hh->ParamList["1"] == "version")	// send version file
			hh->SendFile(IMAGE_VERSION_FILE);
		else if (hh->ParamList["1"] == "httpdversion")	// print httpd version typ (just for compatibility)
			hh->Write("3");
		else if (hh->ParamList["1"] == "nhttpd_version")// print nhttpd version
			hh->printf("%s\n", HTTPD_VERSION);
		else if (hh->ParamList["1"] == "hwinfo")	// print hwinfo
			HWInfoCGI(hh);
		else
			hh->SendError();
	}
}

void CControlAPI::BoxInfoCGI(CyhookHandler *hh)
{
	/* The vendor of one family is answered under its abbreviation and one model
	   is told from its twin tuner version by counting frontends. Both of those
	   are now decided where the box is read, so that every reader gets the same
	   names; Y_Blocks.txt matches on the short vendor. */
	coreapi::Result<coreapi::BoxInfo> r = coreapi::system::info();
	if (!r.ok())
	{
		hh->SendError();
		return;
	}
	const coreapi::BoxInfo &box = r.value();

	hh->printf("vendor=%s\nboxname=%s\nboxarch=%s\n",
		   box.vendor.c_str(), box.model.c_str(), box.chipset.c_str());
}

void CControlAPI::HWInfoCGI(CyhookHandler *hh)
{
	static CNetAdapter netadapter; 
	std::string eth_id = netadapter.getMacAddr();
	std::transform(eth_id.begin(), eth_id.end(), eth_id.begin(), ::tolower);

	std::string boxvendor(g_info.hw_caps->boxvendor);
	/*
	   I don't know the current legal situation.
	   So better let's change the vendor's name to CST.
	*/
	if (boxvendor.compare("Coolstream") == 0)
		boxvendor = "CST";

	hh->printf("%s %s (%s)\nMAC:%s\n", boxvendor.c_str(), g_info.hw_caps->boxname, g_info.hw_caps->boxarch, eth_id.c_str());
}
//-----------------------------------------------------------------------------
void CControlAPI::ShutdownCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		if (coreapi::system::shutdown().ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::RebootCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		if (coreapi::system::reboot().ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::RestartCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		if (coreapi::system::restart().ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------

void CControlAPI::rc_sync(int fd)
{
	struct input_event ev;

// Check if input_event_sec macro is defined
#ifdef input_event_sec
		// Use input_event_sec and input_event_usec macros if they are defined
		struct timeval tv;
		gettimeofday(&tv, NULL);
		ev.input_event_sec = tv.tv_sec;
		ev.input_event_usec = tv.tv_usec;
#else
		// Fallback for older versions that use timeval directly
		gettimeofday(&ev.time, NULL);
#endif

	ev.type = EV_SYN;
	ev.code = SYN_REPORT;
	ev.value = 0;
	write(fd, &ev, sizeof(ev));
}

int CControlAPI::rc_send(int fd, unsigned int code, unsigned int value)
{
	struct input_event ev;

	ev.type = EV_KEY;
	ev.code = code;
	ev.value = value;
	return write(fd, &ev, sizeof(ev));
}

//-----------------------------------------------------------------------------
// The code here is based on rcsim. Thx Carjay!
void CControlAPI::RCEmCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty()) {
		hh->SendError();
		return;
	}
	std::string _keyname = hh->ParamList["1"];
	int sendcode = -1;
	for (unsigned int i = 0; sendcode == -1 && i < sizeof(keyname)
			/ sizeof(key); i++) {
		if (!strcmp(_keyname.c_str(), keyname[i].name))
			sendcode = keyname[i].code;
	}

	if (sendcode == -1) {
		printf("[nhttpd] Key %s not found\n", _keyname.c_str());
		hh->SendError();
		return;
	}
#if 0
	unsigned int repeat = 1;
	unsigned int delay = 250;
	if (!hh->ParamList["delay"].empty())
		delay = atoi(hh->ParamList["delay"].c_str());
	if (!hh->ParamList["duration"].empty())
		repeat = atoi(hh->ParamList["duration"].c_str()) * 1000 / delay;
	if (!hh->ParamList["repeat"].empty())
		repeat = atoi(hh->ParamList["repeat"].c_str());
#endif
#if 1
	int evd = open(RC_DEVICE, O_RDWR);
	if (evd < 0)
		evd = open(RC_DEVICE_FALLBACK, O_RDWR);
	if (evd < 0) {
		perror("opening " RC_DEVICE " failed");
		hh->SendError();
		return;
	}
	sendcode = g_RCInput->translateRevert(sendcode);
	if (rc_send(evd, sendcode, KEY_PRESSED) < 0) {
		perror("writing 'KEY_PRESSED' event failed");
		hh->SendError();
		close(evd);
		return;
	}
	rc_sync(evd);
	if (rc_send(evd, sendcode, KEY_RELEASED) < 0) {
		perror("writing 'KEY_RELEASED' event failed");
		hh->SendError();
		close(evd);
		return;
	}
	rc_sync(evd);
	close(evd);
#else
	/* 0 == KEY_PRESSED in rcinput.cpp */
	g_RCInput->postMsg((neutrino_msg_t) sendcode, 0);
#endif
	hh->SendOk();
}
//-----------------------------------------------------------------------------
void CControlAPI::AspectRatioCGI(CyhookHandler *hh)
{
	hh->printf("%s",(NeutrinoAPI->getVideoAspectRatioAsString()).c_str());
}
//-----------------------------------------------------------------------------
void CControlAPI::VideoFormatCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "status") {
		hh->printf("%s",(NeutrinoAPI->getVideoAspectRatioAsString()).c_str());
		return;
	}
	if (NeutrinoAPI->setVideoAspectRatioAsString(hh->ParamList["1"]) != -1)
		hh->SendOk();
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::VideoOutputCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::VCROutputCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::AVInputModeCGI(CyhookHandler *hh)
{
// FIXME: not implemented
	hh->SendOk();
}

//-----------------------------------------------------------------------------
void CControlAPI::AudioCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "info") {
		hh->printf("%s",(NeutrinoAPI->getAudioInfoAsString()).c_str());
		return;
	}
	//TODO: more
}

//-------------------------------------------------------------------------
void CControlAPI::VolumeCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty()) {//without param: show actual volumen
		coreapi::Result<int> v = coreapi::osd::volume();
		if (v.ok())
			hh->printf("%d", v.value());
		else
			hh->SendError();
	}
	else if (hh->ParamList["1"].compare("mute") == 0)
	{
		if (coreapi::osd::setMuted(true).ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else if (hh->ParamList["1"].compare("unmute") == 0)
	{
		if (coreapi::osd::setMuted(false).ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else if (hh->ParamList["1"].compare("status") == 0) { // Mute status
		coreapi::Result<bool> m = coreapi::osd::muted();
		if (!m.ok())
			hh->SendError();
		else
			m.value() ? hh->Write("1") : hh->Write("0");
	}
	else if(!hh->ParamList["1"].empty()) { //set volume
		/* Read wide and narrowed only once it is known to fit, so that a number
		   too large for an int cannot wrap into the range the setter takes. */
		long value = atol( hh->ParamList["1"].c_str() );
		int percent = (value < 0 || value > 100) ? -1 : (int) value;
		if (coreapi::osd::setVolume(percent).ok())
			hh->SendOk();
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::CryptCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty() || hh->ParamList["1"] == "info") {
		hh->printf("%s",(NeutrinoAPI->getCryptInfoAsString()).c_str());
		return;
	}
	//TODO: more
}

//-----------------------------------------------------------------------------
void CControlAPI::ChannellistCGI(CyhookHandler *hh)
{
	SendChannelList(hh);
}

void CControlAPI::LogolistCGI(CyhookHandler *hh)
{
	hh->outStart();

	std::string result = "";
	bool isFirstLine = true;
	if (!g_bouquetManager) {
		result = hh->outArray("logolist", result);
		hh->SendResult(result);
		return;
	}

	bool files = false;
	unsigned int s = hh->ParamList.size();
	for (unsigned int i = 1; i <= s; i++)
	{
		files = (hh->ParamList[itoa(i)] == "files" && hh->ParamList["files"] != "false");
		if (files)
			break;
	}

	int mode = NeutrinoAPI->Zapit->getMode();
	CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
	for (; !(cit.EndOfChannels()); cit++)
	{
		std::string item = "";
		std::string id = "";
		std::string logo = "";

		std::vector<t_channel_id> v;
		CZapitChannel * channel = *cit;
		size_t pos = std::find(v.begin(), v.end(), channel->getChannelID()) - v.begin();
		if (pos < v.size())
			continue;
		v.push_back(channel->getChannelID());

		std::string logo_used = "";
		std::string logo_real = "";
		if (files)
		{
			char _real[PATH_MAX + 1] = {0};
			if (g_PicViewer->GetLogoName(channel->getChannelID(), NeutrinoAPI->GetServiceName(channel->getChannelID()), logo_used, NULL, NULL))
			{
				realpath(logo_used.c_str(), _real);
				logo_real = std::string(_real);
				if (strcmp(logo_used.c_str(), logo_real.c_str()) == 0)
					logo_real.clear();
			}
		}

		if (hh->outType == httpd::compat::plain)
		{
			std::string outLine = string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS";%s;" PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS"", channel->getChannelID(), channel->getName().c_str(), (channel->getChannelID() & 0xFFFFFFFFFFFFULL));
			if (files)
			{
				if (!logo_used.empty())
					outLine += string_printf(";%s", logo_used.c_str());
				if (!logo_real.empty())
					outLine += string_printf(";%s", logo_real.c_str());
			}
			item = hh->outSingle(outLine);
		}
		else
		{
			item = hh->outPair("name", hh->outValue(channel->getName()), true);

			id = hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID()), true);
			id += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID() & 0xFFFFFFFFFFFFULL), false);
			item += hh->outObject("id", id, files);

			if (files)
			{
				logo = hh->outPair("used", logo_used, true);
				logo += hh->outPair("real", logo_real, false);
				item += hh->outObject("logo", logo);
			}
		}
		if (isFirstLine)
			isFirstLine = false;
		else
			result += hh->outNext();
		result += hh->outArrayItem("channel", item, false);
	}
	result = hh->outArray("logolist", result);

	hh->SendResult(result);
}
//-----------------------------------------------------------------------------
static bool isRadioChannel(coreapi::ServiceKind kind)
{
	return kind == coreapi::ServiceKind::Radio || kind == coreapi::ServiceKind::WebRadio;
}

// The numbers printed beside a channel count positions in the bouquet order,
// which is not the number the channel carries once it sits in more than one
// bouquet.
static int channelsBeforeBouquet(const std::vector<uint32_t> &counts, int index)
{
	int n = 0;
	for (int i = 0; i < index && i < (int) counts.size(); i++)
		n += (int) counts[i];
	return n;
}

//-----------------------------------------------------------------------------
// get actual and next event data for given channel
//-----------------------------------------------------------------------------
std::string CControlAPI::_GetBouquetActualEPGItem(CyhookHandler *hh, t_channel_id channel_id)
{
	std::string result, firstEPG, secondEPG = "";
	t_channel_id current_channel = CZapit::getInstance()->GetCurrentChannelID();
	std::string timestr;
	CShortEPGData epg;

	CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
	CChannelEvent event;
	NeutrinoAPI->GetChannelEvent(channel_id, event);

	bool return_epginfo = (hh->ParamList["epginfo"] != "false");

	result += hh->outPair("isActiveChannel", (channel_id == current_channel) ? "true" : "false", false);

	if (event.eventID) {
		int percentage = 100;
		if (event.duration > 0)
			percentage = 100 * (time(NULL) - event.startTime) / event.duration;
		CEitManager::getInstance()->getCurrentNextServiceKey(channel_id, currentNextInfo);
		timestr = timeString(event.startTime);

		firstEPG += hh->outPair("eventid", string_printf("%llu", currentNextInfo.current_uniqueKey), true);
		firstEPG += hh->outPair("description", hh->outValue(event.description), true);
		if (return_epginfo && CEitManager::getInstance()->getEPGidShort(currentNextInfo.current_uniqueKey, &epg))
		{
			firstEPG += hh->outPair("info1", hh->outValue(epg.info1), true);
			firstEPG += hh->outPair("info2", hh->outValue(epg.info2), true);
		}
		firstEPG += hh->outPair("startTime", timestr, true);
		firstEPG += hh->outPair("timeTotal", string_printf("%d", event.duration / 60), true);
		firstEPG += hh->outPair("timeElapsed", string_printf("%d", (time(NULL) - event.startTime) / 60), true);
		firstEPG += hh->outPair("percentage", string_printf("%d", percentage), false);

		if (currentNextInfo.flags & CSectionsdClient::epgflags::has_next) {
			timestr = timeString(currentNextInfo.next_zeit.startzeit);
			secondEPG += hh->outPair("eventid", string_printf("%llu", currentNextInfo.next_uniqueKey), true);
			secondEPG += hh->outPair("description", hh->outValue(currentNextInfo.next_name), true);
			if (return_epginfo && CEitManager::getInstance()->getEPGidShort(currentNextInfo.next_uniqueKey, &epg))
			{
				secondEPG += hh->outPair("info1", hh->outValue(epg.info1), true);
				secondEPG += hh->outPair("info2", hh->outValue(epg.info2), true);
			}
			secondEPG += hh->outPair("startTime", timestr, true);
			secondEPG += hh->outPair("timeTotal", string_printf("%d", currentNextInfo.next_zeit.dauer / 60), false);
		}
	}

	if(!firstEPG.empty()) {
		result += hh->outNext();
		result += hh->outObject("firstEPG", firstEPG);
	}
	if(!secondEPG.empty()) {
		result += hh->outNext();
		result += hh->outObject("secondEPG", secondEPG);
	}
	return result;
}

//-----------------------------------------------------------------------------
// produce data (collection) for given channel
std::string CControlAPI::_GetBouquetWriteItem(CyhookHandler *hh, t_channel_id channel_id, t_channel_id epg_id, const std::string &name, int bouquetNr, int channelNr)
{
	std::string result = "";
	bool isEPGdetails = !(hh->ParamList["epg"].empty());
	if (hh->outType == httpd::compat::json || hh->outType == httpd::compat::xml) {
		if (channelNr > -1)
			result += hh->outPair("number", string_printf("%u", channelNr), true);
		result += hh->outPair("id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
		result += hh->outPair("short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL), true);
		result += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
		result += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id & 0xFFFFFFFFFFFFULL), true);
		result += hh->outPair("name", hh->outValue(name), true);
		result += hh->outPair("logo", hh->outValue(NeutrinoAPI->getLogoFile(channel_id)), false);
		if (bouquetNr > -1)
		{
			result += hh->outNext();
			result += hh->outPair("bouquetnr", string_printf("%d", bouquetNr), false);
		}
		if (isEPGdetails)
		{
			result += hh->outNext();
			result += _GetBouquetActualEPGItem(hh, channel_id);
		}
		result = hh->outArrayItem("channel", result, false);
	}
	else {
		CChannelEvent event;
		NeutrinoAPI->GetChannelEvent(channel_id, event);

		if (event.eventID && isEPGdetails) {
			result += string_printf("%u "
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %s (%s)\n",
					channelNr,
					channel_id,
					name.c_str(), event.description.c_str());
		} else {
			result += string_printf("%u "
					PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
					" %s\n",
					channelNr,
					channel_id,
					name.c_str());
		}
	}
	return result;
}
//-------------------------------------------------------------------------
/** List all channels for given bouquet (or all) or show actual bouquet number
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * Get bouquet list (all) oder filtered to a given bouquet number
 * Option epg=true for actual and next epg data for each channel
 * @code
 * /control/getbouquet?[bouquet=<bouquet number>][&mode=TV|RADIO][&epg=true[&epginfo=false]]
 * @endcode
 * Get the actual used bouquet number
 * @code
 * /control/getbouquet?actual
 * @endcode
 *
 * @par output (json)
 * @code
 * /control/getbouquet?bouquet=2&format=json
 * @endcode
 * @code
 * {"success": "true",
 * "data": {
 *     "channels": [
 *         {
 *             "number": "1",
 *             "id": "12ea043100016e3d",
 *             "short_id": "43100016e3d",
 *             "epg_id": "12ea043100016e3d",
 *             "short_epg_id": "43100016e3d",
 *             "name": "ARD-TEST-1",
 *             "logo": "",
 *             "bouquetnr": "1"
 *         },
 * [...snip...]
 *         {
 *             "number": "1376",
 *             "id": "ffffffffa8aae742",
 *             "short_id": "ffffa8aae742",
 *             "epg_id": "6e5d040f00012887",
 *             "short_epg_id": "40f00012887",
 *             "name": "Tagesschau24 (WebTV)",
 *             "logo": "/share/tuxbox/neutrino/icons/logo/ffffa8aae742.png",
 *             "bouquetnr": "1"
 *         }
 *     ]
 * }
 * }
 * @endcode
 *
 * @par output (plain) output only (channel number, channel id, channel name)
 * @code
 * /control/getbouquet?bouquet=2
 * @endcode
 * @code
 * 1 12ea043100016e3d ARD-TEST-1
 * [...snip...]
 * 1376 ffffffffa8aae742 Tagesschau24 (WebTV)
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getbouquet?bouquet=2&format=xml
 * @endcode
 * @code
 * <channels>
 * <channel>
 * 	<number>1</number>
 * 	<id>12ea043100016e3d</id>
 * 	<short_id>43100016e3d</short_id>
 * 	<epg_id>12ea043100016e3d</epg_id>
 * 	<short_epg_id>43100016e3d</short_epg_id>
 * 	<name><![CDATA[ARD-TEST-1]]></name>
 * 	<logo><![CDATA[]]></logo>
 * 	<bouquet>1</bouquet>
 * </channel>
 * [...snip...]
 * <channel>
 * 	<number>1376</number>
 * 	<id>ffffffffa8aae742</id>
 * 	<short_id>ffffa8aae742</short_id>
 * 	<epg_id>6e5d040f00012887</epg_id>
 * 	<short_epg_id>40f00012887</short_epg_id>
 * 	<name><![CDATA[Tagesschau24 (WebTV)]]></name>
 * 	<logo><![CDATA[/share/tuxbox/neutrino/icons/logo/ffffa8aae742.png]]></logo>
 * 	<bouquet>1</bouquet>
 * </channel>
 * </channels>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::GetBouquetCGI(CyhookHandler *hh)
{
	if (g_bouquetManager ==  NULL)
		return;

	TOutType outType = hh->outStart(true /*old mode*/);

	std::string result = "";
	if (!(hh->ParamList.empty())) {
		int mode = NeutrinoAPI->Zapit->getMode();

		if (hh->ParamList["mode"].compare("TV") == 0)
			mode = CZapitClient::MODE_TV;
		else if (hh->ParamList["mode"].compare("RADIO") == 0)
			mode = CZapitClient::MODE_RADIO;
		else if (hh->ParamList["mode"].compare("all") == 0)
			mode = CZapitClient::MODE_ALL;

		// Get Bouquet Number. First matching current channel
		if (hh->ParamList["1"] == "actual") {
			int actual = 0;
			for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++) {
				if (g_bouquetManager->existsChannelInBouquet(i, CZapit::getInstance()->GetCurrentChannelID())) {
					actual = i + 1;
					break;
				}
			}
			hh->printf("%d", actual);
		}
		else {
			coreapi::Result<coreapi::BouquetList> bl = coreapi::channels::bouquets();
			if (!bl.ok()) {
				hh->SendError();
				return;
			}
			const coreapi::BouquetList &bouquets = bl.value();

			int BouquetNr = -1; // -1 = all bouquets
			int startBouquet = 0;
			int bsize = (int) bouquets.size();
			if (!hh->ParamList["bouquet"].empty()) {
				// list for given bouquet
				BouquetNr = atoi(hh->ParamList["bouquet"].c_str());
				if (BouquetNr < 0)
					BouquetNr = 0;
				if (BouquetNr > 0)
					BouquetNr--;
				if((BouquetNr > 0) && (BouquetNr >= bsize))
					BouquetNr = bsize-1;

				startBouquet = BouquetNr;
				bsize = BouquetNr+1;
			}
			if (!(hh->ParamList["epg"].empty()))
				NeutrinoAPI->GetChannelEvents();
			const char *json_delimiter = "";

			// Read once, printed twice. A mixed listing walks each bouquet for
			// radio and again for television, and two reads could come from
			// either side of a channel list reload.
			std::vector<coreapi::ChannelList> members;
			// A bouquet that could not be read is not a bouquet with no
			// channels, and the two have to stay apart below.
			std::vector<char> members_read;
			if (bsize > startBouquet) {
				members.resize((size_t) (bsize - startBouquet));
				members_read.resize((size_t) (bsize - startBouquet), 0);
			}
			for (int i = startBouquet; i < bsize; i++) {
				coreapi::Result<coreapi::ChannelList> cl = coreapi::channels::bouquetChannels((uint32_t) i + 1);
				if (cl.ok()) {
					members[(size_t) (i - startBouquet)] = std::move(cl).value();
					members_read[(size_t) (i - startBouquet)] = 1;
				}
			}

			// Counted from the read that produced the channels, so the numbers
			// beside them cannot come from a different channel list. A listing
			// narrowed to one bouquet never reads the ones before it, and
			// their sizes stay those of the bouquet list.
			std::vector<uint32_t> tv_count, radio_count;
			for (size_t b = 0; b < bouquets.size(); b++) {
				tv_count.push_back(bouquets[b].tv_count);
				radio_count.push_back(bouquets[b].radio_count);
			}
			for (int i = startBouquet; i < bsize; i++) {
				if (i < 0 || i >= (int) bouquets.size())
					continue;
				// A bouquet nobody could read keeps the size the bouquet list
				// gave it, so failing to read one costs its own rows and does
				// not renumber every bouquet behind it.
				if (!members_read[(size_t) (i - startBouquet)])
					continue;
				const coreapi::ChannelList &c = members[(size_t) (i - startBouquet)];
				uint32_t radio = 0;
				for (size_t k = 0; k < c.size(); k++)
					if (isRadioChannel(c[k].kind))
						radio++;
				radio_count[(size_t) i] = radio;
				tv_count[(size_t) i] = (uint32_t) c.size() - radio;
			}

			// Radio before television, and every bouquet walked once per kind,
			// because that is the order the listing has.
			for (int pass = 0; pass < 2; pass++) {
				bool tv = (pass == 1);
				int wanted = tv ? CZapitClient::MODE_TV : CZapitClient::MODE_RADIO;
				if (mode != wanted && mode != CZapitClient::MODE_ALL)
					continue;
				for (int i = startBouquet; i < bsize; i++) {
					const coreapi::ChannelList &channels = members[(size_t) (i - startBouquet)];
					int num = 1 + channelsBeforeBouquet(tv ? tv_count : radio_count, i);
					int j = 0;
					for (size_t k = 0; k < channels.size(); k++) {
						if (isRadioChannel(channels[k].kind) == tv)
							continue;
						result += json_delimiter;
						json_delimiter = (outType == httpd::compat::json) ? ",\n" : "";
						result += _GetBouquetWriteItem(hh, (t_channel_id) channels[k].id,
									       (t_channel_id) channels[k].epg_id,
									       channels[k].name, i, num + j);
						j++;
					}
				}
			}
			result = hh->outArray("channels", result);

			hh->SendResult(result);
		}
	}
	else
		hh->SendError("no parameter");
}

//-------------------------------------------------------------------------
/** Show some infos about current or given (by full ID!) channel
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getchannel[?format=plain|xml|json][&id=<channel_id>][&epg=true[&epginfo=false]]
 * @endcode
 *
 * @par output (json)
 * @code
 * /control/getchannel?format=json
 * @endcode
 *
 * @code
 * {
 * "success": "true",
 * "data": {
 *     "channel": [
 *         {
 *             "id": "361d03f300012b66",
 *             "short_id": "3f300012b66",
 *             "epg_id": "361d03f300012b66",
 *             "short_epg_id": "3f300012b66",
 *             "name": "ZDF HD",
 *             "logo": "/share/tuxbox/neutrino/icons/logo/3f300012b66.png"
 *         }
 *     ]
 * }
 * }
 * @endcode
 */
//-------------------------------------------------------------------------
// get actual channel info
void CControlAPI::GetChannelCGI(CyhookHandler *hh)
{
	hh->outStart();

	std::string result = "";

	if (hh->ParamList["id"].empty())
	{
		coreapi::Result<coreapi::ChannelInfo> current = coreapi::channels::current();
		if (!current.ok())
		{
			// Nothing running is one answer. A running channel the list does
			// not hold is the other, and it reads like a bad id, which on this
			// path is the empty one.
			if (current.error().status == coreapi::Status::NotFound)
				hh->SendError();
			else
				hh->SendError(hh->ParamList["id"] + " seems wrong");
			return;
		}
		NeutrinoAPI->GetChannelEvents();
		const coreapi::ChannelInfo &ci = current.value();
		result = _GetBouquetWriteItem(hh, (t_channel_id) ci.id, (t_channel_id) ci.epg_id, ci.name, -1, -1);
		result = hh->outArray("channel", result);
		hh->SendResult(result);
		return;
	}

	t_channel_id channel_id = 0;
	sscanf(hh->ParamList["id"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);

	if (channel_id != 0)
	{
		NeutrinoAPI->GetChannelEvents();
		// The lookup ignores the top sixteen bits, so a caller may name a
		// channel by the short id this module prints beside it.
		CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(channel_id);
		if (channel)
		{
			result = _GetBouquetWriteItem(hh, channel->getChannelID(), channel->getEpgID(), channel->getName(), -1, -1);
			result = hh->outArray("channel", result);
			hh->SendResult(result);
		}
		else
			hh->SendError(hh->ParamList["id"] + " seems wrong");
	}
	else
		hh->SendError();
}

//-------------------------------------------------------------------------
/** Return all bouquets
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getbouquets?[showhidden=true|false][&encode=true|false][&format=|xml|json]
 *
 * @endcode
 * @par
 * @e showhidden = true (default) | false -> show hidden bouquets
 * @n @e encode = true | false (default) use URLencode
 * @n @e Result: bouquet number, bouquet name *
 *
 * @par output (json)
 * @code
 * /control/getbouquets&format=json
 * @endcode
 * @code
 * {"success": "true", "data":{"bouquets": [{"number": "2",
 * "name": "ARD"
 * },
 * {"number": "12",
 * "name": "Digital Free"
 * },
 * [...snip...]
 * {"number": "23",
 * "name": "Other"
 * }
 * ]
 * }}
 * @endcode
 *
 * @par output (plain)
 * @code
 * /control/getbouquets
 * @endcode
 * @code
 * 2 ARD
 * 12 Digital Free
 * [...snip...]
 * 22 ZDFvision
 * 23 Other
 * @endcode
 *
 * @par output (xml)
 * @code
 * /control/getbouquets&format=xml
 * @endcode
 * @code
 * <bouquets>
 * <bouquet>
 * <number>2</number>
 * <name>ARD</name>
 * </bouquet>
 * [...snip...]
 * <number>23</number>
 * <name>Other</name>
 * </bouquet>
 * </bouquets>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::GetBouquetsCGI(CyhookHandler *hh)
{
	bool show_hidden = true;
	bool encode = false;
	std::string result = "";

	TOutType outType = hh->outStart();

	if (hh->ParamList["showhidden"] == "false")
		show_hidden = false;

	if (hh->ParamList["encode"] == "true")
		encode = true;

	bool fav = false;
	if (hh->ParamList["fav"] == "true")
		fav = true;

	int mode = NeutrinoAPI->Zapit->getMode();
	if (hh->ParamList["mode"].compare("all") == 0)
		mode = CZapitClient::MODE_ALL;
	else if (hh->ParamList["mode"].compare("TV") == 0)
		mode = CZapitClient::MODE_TV;
	else if (hh->ParamList["mode"].compare("RADIO") == 0)
		mode = CZapitClient::MODE_RADIO;
 
	coreapi::Result<coreapi::BouquetList> bl = coreapi::channels::bouquets();
	if (!bl.ok())
	{
		hh->SendError();
		return;
	}
	const coreapi::BouquetList &bouquets = bl.value();

	std::string bouquet;
	for (int i = 0, size = (int) bouquets.size(); i < size; i++) {
		std::string item = "";
		unsigned int channel_count = 0;
		switch (mode) {
			case CZapitClient::MODE_RADIO:
				channel_count = bouquets[i].radio_count;
				break;
			case CZapitClient::MODE_TV:
				channel_count = bouquets[i].tv_count;
				break;
			case CZapitClient::MODE_ALL:
				channel_count = bouquets[i].radio_count + bouquets[i].tv_count;
		}
		if (channel_count && (!bouquets[i].hidden || show_hidden) && (!fav || bouquets[i].user_bouquet)) {
			bouquet = bouquets[i].name;
			if (encode)
				bouquet = encodeString(bouquet); // encode (URLencode) the bouquetname
			if (outType == httpd::compat::plain)
				item = string_printf("%u", bouquets[i].id) + " " + bouquet + "\n";
			else
			{
				item = hh->outPair("number", string_printf("%u", bouquets[i].id), true);
				item += hh->outPair("name", bouquet, false);
			}
			result += hh->outArrayItem("bouquet", item, (i < size-1));
		}
	}
	result = hh->outArray("bouquets", result);

	hh->SendResult(result);
}
//-----------------------------------------------------------------------------
//	details EPG Information for channelid
//-----------------------------------------------------------------------------
std::string CControlAPI::channelEPGformated(CyhookHandler *hh, int bouquetnr, t_channel_id channel_id, int max, long stoptime)
{
	std::string result = "";
	std::string channelData = "";

	t_channel_id epg_id = channel_id;
	CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(channel_id);
	if (ch)
		epg_id = ch->getEpgID();

	channelData += hh->outPair("channel_name", hh->outValue(NeutrinoAPI->GetServiceName(channel_id)), true);
	channelData += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
	channelData += hh->outPair("channel_short_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id & 0xFFFFFFFFFFFFULL), true);
	channelData += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
	channelData += hh->outPair("short_epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id & 0xFFFFFFFFFFFFULL), (bouquetnr > -1));
	if (bouquetnr > -1)
		channelData += hh->outPair("bouquetnr", string_printf("%d", bouquetnr), false);
	if (hh->outType == httpd::compat::json)
		channelData = hh->outObject("channelData", channelData);
	bool isFirstLine = true;

	coreapi::Result<coreapi::EventList> events =
		coreapi::epg::forChannel((coreapi::ChannelId) epg_id, EPG_WHOLE_FROM, EPG_WHOLE_TO);
	// An EPG that cannot be read leaves the channel with no programme rows,
	// which is what an empty schedule already produced here.
	coreapi::EventList eList;
	if (events.ok())
		eList = std::move(events).value();

	for (size_t i = 0; i < eList.size(); i++) {
		const coreapi::EventInfo &event = eList[i];
		if ((max != -1 && (int) i >= max) || (stoptime != -1 && event.start >= stoptime))
			break;
		std::string prog = "";
		if (hh->outType == httpd::compat::plain)
			prog += hh->outSingle("");

		prog += hh->outPair("bouquetnr", string_printf("%d", bouquetnr), true);
		prog += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id), true);
		prog += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
		prog += hh->outPair("eventid", string_printf("%llu", event.event_id), true);
		prog += hh->outPair("eventid_hex", string_printf("%llx", event.event_id), true);
		prog += hh->outPair("start_sec", string_printf("%ld", event.start), true);
		char zbuffer[25] = { 0 };
		struct tm *mtime = localtime(&event.start);
		strftime(zbuffer, 20, "%H:%M", mtime);
		prog += hh->outPair("start_t", std::string(zbuffer), true);
		memset(zbuffer, 0, sizeof(zbuffer));
		strftime(zbuffer, 20, "%d.%m.%Y", mtime);
		prog += hh->outPair("date", std::string(zbuffer), true);
		prog += hh->outPair("stop_sec", string_printf("%ld", event.start + event.duration), true);
		time_t _stoptime = event.start + event.duration;
		mtime = localtime(&_stoptime);
		strftime(zbuffer, 20, "%H:%M", mtime);
		prog += hh->outPair("stop_t", std::string(zbuffer), true);
		prog += hh->outPair("duration_min", string_printf("%d", (int) (event.duration / 60)), true);

		if (!(hh->ParamList["details"].empty())) {
			CShortEPGData epg;
			if (CEitManager::getInstance()->getEPGidShort(event.event_id, &epg)) {
				prog += hh->outPair("info1", hh->outValue(epg.info1), true);
				prog += hh->outPair("info2", hh->outValue(epg.info2), true);
			}
		}
		// The title, because the event manager's description is the event's
		// name and that is what this field has always carried.
		prog += hh->outPair("description", hh->outValue(event.title), false);

		if(isFirstLine)
			isFirstLine = false;
		else
			result += hh->outNext();
		result += hh->outArrayItem("prog", prog, false);
	}
	if(hh->outType == httpd::compat::json)
		result = hh->outArray("progData", result);
	result = channelData + hh->outNext() + result;
	return result;
}


//-----------------------------------------------------------------------------
// Detailed EPG list in XML or JSON
//-----------------------------------------------------------------------------
void CControlAPI::epgDetailList(CyhookHandler *hh)
{
	// ------ get parameters -------
	// max = maximal output items
	int max = -1;
	if (!(hh->ParamList["max"].empty()))
		max = atoi(hh->ParamList["max"].c_str());

	// stoptime = maximal output items until starttime >= stoptime
	long stoptime = -1;
	if (!(hh->ParamList["stoptime"].empty()))
		stoptime = atol(hh->ParamList["stoptime"].c_str());

	// determine channelid
	t_channel_id channel_id = (t_channel_id) -1;
	if (!(hh->ParamList["channelid"].empty())) {
		sscanf(hh->ParamList["channelid"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
	}
	else if (!(hh->ParamList["channelname"].empty())) {
		channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["channelname"].c_str());
	}

	// or determine bouquetnr -> iterate the bouquet
	int bouquetnr = -1;
	bool all_bouquets = false;

	if (hh->ParamList["bouquetnr"] == "all")
		all_bouquets = true;
	else if (!(hh->ParamList["bouquetnr"].empty())) {
		bouquetnr = atoi(hh->ParamList["bouquetnr"].c_str());
		bouquetnr--;
	}

	// fallback
	if ((channel_id == (t_channel_id) -1) && (bouquetnr == -1))
	{
		channel_id = CZapit::getInstance()->GetCurrentChannelID();
		//all_bouquets = true; //better, but broken
	}

	// ------ generate output ------
	hh->outStart(true /*old mode*/);
	std::string result = "";

	if (bouquetnr >= 0 || all_bouquets) {
		int bouquet_size = (int) g_bouquetManager->Bouquets.size();
		int start_bouquet = 0;

		if(bouquetnr >= 0 && !all_bouquets) {
			start_bouquet = bouquetnr;
			// The number came out of the request and nothing above it has
			// been compared with the bouquet count, so one past the end
			// indexed the vector past its end. Left below the start when it
			// is out of range, which is the loop's own way of doing nothing.
			if (bouquetnr + 1 < bouquet_size)
				bouquet_size = bouquetnr + 1;
		}
		// list all bouquets			if(encode)
		ZapitChannelList channels;
		int mode = NeutrinoAPI->Zapit->getMode();

		for (int i = start_bouquet; i < bouquet_size; i++) {
			channels = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->Bouquets[i]->radioChannels : g_bouquetManager->Bouquets[i]->tvChannels;
			std::string bouquet = std::string(g_bouquetManager->Bouquets[i]->bName.c_str());
			bouquet = encodeString(bouquet); // encode (URLencode) the bouquetname
			std::string res_channels = "";

			for (int j = 0, csize = (int) channels.size(); j < csize; j++) {
				CZapitChannel * channel = channels[j];
				res_channels += hh->outArrayItem("channel", channelEPGformated(hh, j + 1, channel->getChannelID(), max, stoptime), j<csize-1);
			}
			if(all_bouquets) {
				res_channels = hh->outPair("number", string_printf("%d", i + 1), true) +
						hh->outPair("name", hh->outValue(bouquet), true) +
						res_channels;
				result += hh->outArrayItem("bouquet", res_channels, i < bouquet_size-1);
			}
			else
				result += res_channels;
		}
	}
	else
		// list one channel, no bouquetnr given
		result = channelEPGformated(hh, 0, channel_id, max, stoptime);

	result = hh->outObject("epglist", result);

	hh->SendResult(result);
}
//-------------------------------------------------------------------------------------------------
inline static bool sortByDateTime (const CChannelEvent& a, const CChannelEvent& b)
{
	return a.startTime < b.startTime;
}
extern const char * GetGenre(const unsigned char contentClassification); // UTF-8

void CControlAPI::EpgSearchXMLCGI(CyhookHandler *hh) 
{
	SendFoundEvents(hh, true);
}

//-------------------------------------------------------------------------
/** Return EPG search data
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/epgsearch?<keywords>
 * or
 * /control/epgsearch?search=<keywords>[&epginfo=true|false|search][&format=plain|xml|json]
 * @endcode
 */

//-------------------------------------------------------------------------
void CControlAPI::EpgSearchCGI(CyhookHandler *hh)
{
	SendFoundEvents(hh);
}

void CControlAPI::SendFoundEvents(CyhookHandler *hh, bool xml_format)
{
	if (hh->ParamList.empty())
	{
		hh->SendError();
		return;
	}

	std::string result ="";
	std::string item = "";
	t_channel_id channel_id;
	CChannelEventList evtlist;

	bool search_epginfo = (hh->ParamList["epginfo"] != "false");
	bool return_epginfo = (hh->ParamList["epginfo"] == "true" || hh->ParamList["epginfo"].empty());

	std::string search_keyword = (hh->ParamList["search"].empty()) ? hh->ParamList["1"] : hh->ParamList["search"];
	const int search_epg_item = search_epginfo ? 5 /*SEARCH_EPG_ALL*/ : 1 /*SEARCH_EPG_TITLE*/;

	if (xml_format) // to stay backward compatible :/
		hh->ParamList["format"] = "xml";
	hh->outStart(true /*old mode*/);

	/* TODO: maybe add following options as in tuxbox neutrino
		hh->ParamList["epgitem"]
		hh->ParamList["mode"]
		hh->ParamList["channelid"]
		hh->ParamList["channelname"]
		hh->ParamList["bouquet"]
	*/

	std::vector<t_channel_id> v;
	int channel_nr = CNeutrinoApp::getInstance()->channelList->getSize(); //unique channelList TV or Radio
	for (int channel = 0; channel < channel_nr; channel++)
	{
	    channel_id =  CNeutrinoApp::getInstance()->channelList->getChannelFromIndex(channel)->getChannelID();
	    v.push_back(channel_id);
	}
	std::map<t_channel_id, t_channel_id> ch_id_map;
	std::vector<t_channel_id>::iterator it;
	for (it = v.begin(); it != v.end(); ++it)
	{
		ch_id_map[*it & 0xFFFFFFFFFFFFULL] = *it;
	}

	CEitManager::getInstance()->getEventsServiceKey(0, evtlist, search_epg_item, search_keyword, true);

	if (!evtlist.empty())
	{
		std::map<t_channel_id, t_channel_id>::iterator map_it;
		CChannelEventList::iterator e;
		for (e = evtlist.begin(); e != evtlist.end(); ++e)
		{
			map_it = ch_id_map.find(e->channelID);
			if (map_it != ch_id_map.end())
			{
				e->channelID = map_it->second;//map channelID48 to channelID
			}
			else
			{
				evtlist.erase(e--);// remove event for not found channels in channelList
			}
		}
	}
	if (!evtlist.empty())
	{
		sort(evtlist.begin(), evtlist.end(), sortByDateTime);
	}

	time_t azeit=time(NULL);
	CShortEPGData epg;
	CEPGData longepg;
	char tmpstr[256] ={0};
	std::string genre;
	CChannelEventList::iterator eventIterator;
	unsigned int u_azeit = ( azeit > -1)? azeit:0;
	for (eventIterator = evtlist.begin(); eventIterator != evtlist.end(); ++eventIterator)
	{
		bool got_next = (eventIterator != (evtlist.end() - 1));
		if (CEitManager::getInstance()->getEPGidShort(eventIterator->eventID, &epg))
		{
			if( (eventIterator->startTime+eventIterator->duration) < u_azeit)
				continue;

			t_channel_id chan_id = eventIterator->channelID;
			t_channel_id epg_id = chan_id;
			CZapitChannel * ch = CServiceManager::getInstance()->FindChannel(chan_id);
			if (ch)
				epg_id = ch->getEpgID();

			struct tm *tmStartZeit = localtime(&eventIterator->startTime);
			item.clear();
			if (hh->outType == httpd::compat::json || hh->outType == httpd::compat::xml)
			{
				item += hh->outPair("channelname", NeutrinoAPI->GetServiceName(chan_id), true);
				item += hh->outPair("epgtitle", hh->outValue(epg.title), true);
				if (return_epginfo) {
					item += hh->outPair("info1", hh->outValue(epg.info1), true);
					item += hh->outPair("info2", hh->outValue(epg.info2), true);
				}
				if (CEitManager::getInstance()->getEPGid(eventIterator->eventID, eventIterator->startTime, &longepg))
					{
					item += hh->outPair("fsk", string_printf("%u", longepg.fsk), true);
					genre = "";
#ifdef FULL_CONTENT_CLASSIFICATION
					if (!longepg.contentClassification.empty())
						genre = GetGenre(longepg.contentClassification[0]);
#else
					if (longepg.contentClassification)
						genre = GetGenre(longepg.contentClassification);
#endif
					item += hh->outPair("genre", ZapitTools::UTF8_to_UTF8XML(genre.c_str()), true);
				}
				strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d", tmStartZeit );
				item += hh->outPair("date", tmpstr, true);
				strftime(tmpstr, sizeof(tmpstr), "%H:%M", tmStartZeit );
				item += hh->outPair("time", tmpstr, true);
				item += hh->outPair("duration", string_printf("%d", eventIterator->duration / 60), true);
				item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, chan_id), true);
				item += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, epg_id), true);
				item += hh->outPair("eventid", string_printf("%llu", eventIterator->eventID), false);

				result += hh->outArrayItem("item", item, got_next);
			}
			else // outType == plain
			{
				std::string datetimer_str ;
				strftime(tmpstr, sizeof(tmpstr), "%Y-%m-%d %H:%M", tmStartZeit );
				datetimer_str = tmpstr;
				datetimer_str += " ";
				datetimer_str += g_Locale->getText(CLocaleManager::getMonth(tmStartZeit));
				datetimer_str += " ";
				datetimer_str += g_Locale->getText(CLocaleManager::getWeekday(tmStartZeit));
				snprintf(tmpstr, sizeof(tmpstr)," [%d min]",eventIterator->duration / 60);
				datetimer_str += tmpstr;

				result += hh->outSingle(datetimer_str);
				result += hh->outSingle(NeutrinoAPI->GetServiceName(chan_id));
				result += hh->outSingle(epg.title);
				if (return_epginfo) {
					if(!epg.info1.empty())
						result += hh->outSingle(epg.info1);
					if(!epg.info2.empty())
						result += hh->outSingle(epg.info2);
				}
				if (CEitManager::getInstance()->getEPGid(eventIterator->eventID, eventIterator->startTime, &longepg)) {
					result += hh->outSingle(string_printf("fsk:%u", longepg.fsk));
					genre = "";
#ifdef FULL_CONTENT_CLASSIFICATION
					if (!longepg.contentClassification.empty())
						genre = GetGenre(longepg.contentClassification[0]);
#else
					if (longepg.contentClassification)
						genre = GetGenre(longepg.contentClassification);
#endif
					if(!genre.empty())
						result += hh->outSingle(ZapitTools::UTF8_to_UTF8XML(genre.c_str()));
				}
				result += hh->outSingle("----------------------------------------------------------");
			}
		}
	}
	result = hh->outArray("epgsearch", result);

	hh->SendResult(result);
}

//-------------------------------------------------------------------------
/** Return EPG data
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/epg
 * /control/epg?<channelid64> 64Bit, hex
 * /control/epg?id=<channelid>
 * /control/epg?search=<keywords>
 * /control/epg?eventid=<eventid>
 * /control/epg?ext
 * /control/epg?xml=true&channelid=<channelid>|channelname=<channel name>[&details=true][&max=<max items>][&stoptime=<long:stop time>]
 *  	details=true : Show EPG Info1 and info2
 *		stoptime : show only items until stoptime reached
 * @endcode
 */

//-------------------------------------------------------------------------
void CControlAPI::EpgCGI(CyhookHandler *hh)
{
	bool param_empty = hh->ParamList.empty();
	hh->SetHeader(httpd::compat::HTTP_OK, "text/plain; charset=UTF-8"); // default
	// Detailed EPG list in XML or JSON
	if (hh->getOutType() == httpd::compat::xml || hh->getOutType() == httpd::compat::json || !hh->ParamList["detaillist"].empty()) {
		epgDetailList(hh);
	}
	// Standard list normal or extended
	else if (param_empty || hh->ParamList["1"] == "ext") {
		hh->SetHeader(httpd::compat::HTTP_OK, "text/plain; charset=UTF-8");
		bool isExt = (hh->ParamList["1"] == "ext");
		CChannelEvent event;
		NeutrinoAPI->GetChannelEvents();

		int mode = NeutrinoAPI->Zapit->getMode();
		CBouquetManager::ChannelIterator cit = mode == CZapitClient::MODE_RADIO ? g_bouquetManager->radioChannelsBegin() : g_bouquetManager->tvChannelsBegin();
		for (; !(cit.EndOfChannels()); cit++) {
			CZapitChannel *channel = *cit;
			NeutrinoAPI->GetChannelEvent(channel->getChannelID(), event);
			if (event.eventID) {
				if (!isExt) {
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " %" PRIu64 " %s\n", channel->getChannelID(), event.eventID, event.description.c_str());
				}
				else { // ext output
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " %" PRIdMAX " %u %" PRIu64 " %s\n", channel->getChannelID(), static_cast<intmax_t>(event.startTime), event.duration, event.eventID, event.description.c_str());
				}
			}
		}
	}
	else if (!hh->ParamList["search"].empty())
	{
		SendFoundEvents(hh, (hh->getOutType() == httpd::compat::xml));
	}
	// query details for given eventid
	else if (!hh->ParamList["eventid"].empty()) {
		//special epg query
		uint64_t epgid = 0;
		sscanf(hh->ParamList["eventid"].c_str(), "%" SCNu64 "", &epgid);
		CShortEPGData epg;
		if (CEitManager::getInstance()->getEPGidShort(epgid, &epg)) {
			hh->WriteLn(epg.title);
			hh->WriteLn(epg.info1);
			hh->WriteLn(epg.info2);
		}
	}
	else if (!hh->ParamList["eventid2fsk"].empty()) {
		if (!hh->ParamList["starttime"].empty()) {
			uint64_t epgid = 0;
			time_t starttime = 0;
			sscanf(hh->ParamList["fskid"].c_str(), "%" SCNu64 "", &epgid);
			sscanf(hh->ParamList["starttime"].c_str(), "%lld", reinterpret_cast<long long*>(&starttime));
			CEPGData longepg;
			if (CEitManager::getInstance()->getEPGid(epgid, starttime, &longepg)) {
				hh->printf("%u\n", longepg.fsk);
				return;
			}
		}
		hh->SendError();
	}
	// list EPG for channel id
	else if (!(hh->ParamList["id"].empty())) {
		t_channel_id channel_id = 0;
		sscanf(hh->ParamList["id"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
		coreapi::Result<coreapi::EventList> events =
			coreapi::epg::forChannel((coreapi::ChannelId) channel_id, EPG_WHOLE_FROM, EPG_WHOLE_TO);
		coreapi::EventList eList;
		if (events.ok())
			eList = std::move(events).value();
		for (size_t i = 0; i < eList.size(); i++) {
			// The three lines below come from the long form of the event, which
			// carries texts the listing does not, so it stays a read of its own.
			CShortEPGData epg;
			if (CEitManager::getInstance()->getEPGidShort(eList[i].event_id, &epg)) {
				hh->printf("%" PRIu64 " %" PRIdMAX " %d\n", eList[i].event_id, static_cast<intmax_t>(eList[i].start), eList[i].duration);
				hh->printf("%s\n", epg.title.c_str());
				hh->printf("%s\n", epg.info1.c_str());
				hh->printf("%s\n\n", epg.info2.c_str());
			}
		}
	}
	// list EPG for channelID 64Bit
	else {
		//eventlist for a chan
		t_channel_id channel_id = 0;
		sscanf(hh->ParamList["1"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
		SendEventList(hh, channel_id);
	}
}

//-----------------------------------------------------------------------------
void CControlAPI::VersionCGI(CyhookHandler *hh)
{
	hh->SendFile(IMAGE_VERSION_FILE);
}
//-----------------------------------------------------------------------------
void CControlAPI::ReloadNeutrinoSetupCGI(CyhookHandler *hh)
{
	NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::RELOAD_SETUP, CEventServer::INITID_HTTPD);
	hh->SendOk();
}

void CControlAPI::ReloadPluginsCGI(CyhookHandler *hh)
{
	if (g_Plugins ==  NULL)
		return;

	g_Plugins->loadPlugins();
	hh->SendOk();
}

void CControlAPI::ReloadChannelsCGI(CyhookHandler *hh)
{
	if(hh->ParamList["hardreload"].empty())
		CServiceManager::getInstance()->SaveServices(true, true);
	if (g_bouquetManager)
		g_bouquetManager->setWebchannelsReloadReason(hh->ParamList["hardreload"].empty() ? "webif_reload" : "webif_hardreload");
	NeutrinoAPI->Zapit->reinitChannels();
	CNeutrinoApp::getInstance()->SDTreloadChannels = false;
	hh->SendOk();
}

#ifdef SCREENSHOT
void CControlAPI::ScreenshotCGI(CyhookHandler *hh)
{
	bool enableOSD = true;
	bool enableVideo = true;
	std::string filename = "screenshot";

	if(hh->ParamList["osd"] == "0")
		enableOSD = false;
	if(hh->ParamList["video"] == "0")
		enableVideo = false;
	if(!hh->ParamList["name"].empty())
		filename = hh->ParamList["name"];

	CScreenShot * screenshot = new CScreenShot("/tmp/" + filename + ".png", (CScreenShot::screenshot_format_t)0 /*PNG*/);
	if(screenshot){
		screenshot->EnableOSD(enableOSD);
		screenshot->EnableVideo(enableVideo);
		if (screenshot->StartSync())
			hh->SendOk();
		else
			hh->SendError();
		delete screenshot;
	}
}
#endif

#ifdef ENABLE_GRAPHLCD
void CControlAPI::GlcdScreenshotCGI(CyhookHandler *hh)
{
	std::string filename = "screenshot-glcd";

	if(!hh->ParamList["name"].empty())
		filename = hh->ParamList["name"];

	filename = "/tmp/" + filename + ".png";

	cGLCD *cglcd = cGLCD::getInstance();
	if (cglcd) {
		if (cglcd->dumpBuffer((uint32_t*)cglcd->bitmap->Data(), cGLCD::PNG, filename.c_str()))
			hh->SendOk();
		else
			hh->SendError();
	}
}
#endif

//-----------------------------------------------------------------------------
void CControlAPI::ZaptoCGI(CyhookHandler *hh)
{
	if (hh->ParamList.empty())
	{
		hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
			   "\n",
			   NeutrinoAPI->Zapit->getCurrentServiceID());
		return;
	}
	else
	{
		if (hh->ParamList["1"] == "getpids")		// getpids !
			SendcurrentVAPid(hh);
		else if (hh->ParamList["1"] == "getallpids")		// getpids !
			SendAllCurrentVAPid(hh);
		else if (hh->ParamList["1"] == "stopplayback")
		{
			if(NeutrinoAPI->Zapit->isPlayBackActive()){
				NeutrinoAPI->Zapit->stopPlayBack();
				NeutrinoAPI->Sectionsd->setPauseScanning(true);
			}
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "startplayback")
		{
			if(!NeutrinoAPI->Zapit->isPlayBackActive()){
				NeutrinoAPI->Zapit->startPlayBack();
				NeutrinoAPI->Sectionsd->setPauseScanning(false);
			}
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "statusplayback")
			hh->Write((char *) (NeutrinoAPI->Zapit->isPlayBackActive() ? "1" : "0"));
		else if (hh->ParamList["1"] == "stopsectionsd")
		{
			NeutrinoAPI->Sectionsd->setPauseScanning(true);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "startsectionsd")
		{
			NeutrinoAPI->Sectionsd->setPauseScanning(false);
			hh->SendOk();
		}
		else if (hh->ParamList["1"] == "statussectionsd")
			hh->Write((char *) (NeutrinoAPI->Sectionsd->getIsScanningActive() ? "1" : "0"));
		else if (hh->ParamList["1"] == "getallsubchannels")
		{
			t_channel_id current_channel = NeutrinoAPI->Zapit->getCurrentServiceID();
			CSectionsdClient::LinkageDescriptorList desc;
			CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
			CEitManager::getInstance()->getCurrentNextServiceKey(current_channel, currentNextInfo);

			if (currentNextInfo.flags & CSectionsdClient::epgflags::current_has_linkagedescriptors &&
			    CEitManager::getInstance()->getLinkageDescriptorsUniqueKey(currentNextInfo.current_uniqueKey, desc))
			{
				CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(current_channel);
				t_satellite_position satellitePosition = channel->getSatellitePosition();
				for(unsigned int i=0; i< desc.size(); i++)
				{
					t_channel_id sub_channel_id =
						      ((uint64_t) ( satellitePosition >= 0 ? satellitePosition : (uint64_t)(0xF000+ abs(satellitePosition))) << 48) |
						      (uint64_t) CREATE_CHANNEL_ID(desc[i].serviceId, desc[i].originalNetworkId, desc[i].transportStreamId);
					hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS
						   " %s\n",
						   sub_channel_id,
						   (desc[i].name).c_str());
				}
			}
		}
		else if (!hh->ParamList["subchannel"].empty())
		{
			if (!::g_RemoteControl->subChannels.empty())
			{
				NeutrinoAPI->ZapToSubService(hh->ParamList["subchannel"].c_str());
				hh->SendOk();
			}
			else
				hh->SendError();
		}
		else if (!hh->ParamList["name"].empty())
		{
			t_channel_id channel_id;
			channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["name"]);
			if(channel_id != (t_channel_id)-1)
			{
				NeutrinoAPI->ZapToChannelId(channel_id);
				hh->SendOk();
			}
			else
				hh->SendError();
		}
		else if (hh->ParamList["subchannel"] != "")
		{
			NeutrinoAPI->ZapToSubService(hh->ParamList["subchannel"].c_str());

			hh->SendOk();
		}
		else
		{
			NeutrinoAPI->ZapTo(hh->ParamList["1"].c_str());

			hh->SendOk();
		}
		return;
	}
	hh->SendError();
	return;
}

//-----------------------------------------------------------------------------
void CControlAPI::StartPluginCGI(CyhookHandler *hh)
{
	std::string pluginname;
	if (!(hh->ParamList.empty()))
	{
		if (!hh->ParamList["name"].empty())
		{
			pluginname = hh->ParamList["name"];
			//pluginname=decodeString(pluginname);
			NeutrinoAPI->EventServer->sendEvent(NeutrinoMessages::EVT_START_PLUGIN,
							    CEventServer::INITID_HTTPD,
							    (void *) pluginname.c_str(),
							    pluginname.length() + 1);

			hh->SendOk();
		}
		else
			hh->SendError();
	}
	else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::LCDAction(CyhookHandler *hh)
{
	hh->SendOk();
}

//-------------------------------------------------------------------------
// Send functions (for ExecuteCGI)
//-------------------------------------------------------------------------
void CControlAPI::SendEventList(CyhookHandler *hh, t_channel_id channel_id)
{
	coreapi::Result<coreapi::EventList> events =
		coreapi::epg::forChannel((coreapi::ChannelId) channel_id, EPG_WHOLE_FROM, EPG_WHOLE_TO);
	if (!events.ok())
		return;

	const coreapi::EventList &eList = events.value();
	// The title, because the event manager calls an event's name its
	// description and that is the name this listing has always printed.
	for (size_t i = 0; i < eList.size(); i++)
		hh->printf("%llu %ld %d %s\n", eList[i].event_id, eList[i].start, eList[i].duration, eList[i].title.c_str());
}

//-----------------------------------------------------------------------------
void CControlAPI::SendChannelList(CyhookHandler *hh, bool currentTP)
{
	t_channel_id current_channel = 0;
	std::vector<t_channel_id> v;

	if(currentTP){
		// The filter wants the transponder half of the running id, and that
		// half is still there when the channel list cannot resolve the id.
		current_channel = CZapit::getInstance()->GetCurrentChannelID() >> 16;
	}

	hh->SetHeader(httpd::compat::HTTP_OK, "text/plain; charset=UTF-8");
	coreapi::Result<coreapi::BouquetList> bl = coreapi::channels::bouquets();
	if (!bl.ok())
		return;
	// The listing follows the bouquet order rather than the channel list's own,
	// so a channel in two bouquets is printed once, where it first appears.
	bool radio = NeutrinoAPI->Zapit->getMode() == CZapitClient::MODE_RADIO;
	for (size_t i = 0, size = bl.value().size(); i < size; i++) {
		coreapi::Result<coreapi::ChannelList> cl = coreapi::channels::bouquetChannels((uint32_t) i + 1);
		if (!cl.ok())
			continue;
		const coreapi::ChannelList &channels = cl.value();
		for (size_t j = 0; j < channels.size(); j++) {
			t_channel_id id = (t_channel_id) channels[j].id;
			if (isRadioChannel(channels[j].kind) != radio)
				continue;
			if (currentTP && (id >> 16) != current_channel)
				continue;
			if (std::find(v.begin(), v.end(), id) != v.end())
				continue;
			v.push_back(id);

			hh->printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS " %s\n", id, channels[j].name.c_str());
		}
	}
}

//-----------------------------------------------------------------------------
void CControlAPI::SignalInfoCGI(CyhookHandler *hh)
{
	CFrontend *frontend = CFEManager::getInstance()->getLiveFE();
	if(frontend){
		bool parame_empty = false;

		if (hh->ParamList["1"].empty())
			parame_empty = true;

		if ( parame_empty || (hh->ParamList["1"] == "sig") ){
			unsigned int sig = frontend->getSignalStrength() & 0xFFFF;
			sig = (sig & 0xFFFF) * 100 / 65535;
			if (parame_empty)
				hh->printf("SIG: ");
			hh->printf("%3u\n", sig);
		}
		if ( parame_empty || (hh->ParamList["1"] == "snr") ){
			unsigned int snr = frontend->getSignalNoiseRatio() & 0xFFFF;
			snr = (snr & 0xFFFF) * 100 / 65535;
			if (parame_empty)
				hh->printf("SNR: ");
			hh->printf("%3u\n", snr);
		}
		if ( parame_empty || (hh->ParamList["1"] == "ber") ){
			unsigned int ber = frontend->getBitErrorRate();
			if (parame_empty)
				hh->printf("BER: ");
			hh->printf("%3u\n", ber);
		}
	}else
		hh->SendError();
}

//-----------------------------------------------------------------------------
void CControlAPI::SendStreamInfo(CyhookHandler *hh)
{
	hh->WriteLn(NeutrinoAPI->getVideoResolutionAsString());
	hh->WriteLn(NeutrinoAPI->getVideoAspectRatioAsString());
	hh->WriteLn(NeutrinoAPI->getVideoFramerateAsString());
	hh->WriteLn(NeutrinoAPI->getAudioInfoAsString());
}

//-----------------------------------------------------------------------------
void CControlAPI::SendcurrentVAPid(CyhookHandler *hh)
{
	CZapitClient::responseGetPIDs pids;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	hh->printf("%u\n", pids.PIDs.vpid);
	if(!pids.APIDs.empty())
		hh->printf("%u\n", pids.APIDs[0].pid);
	else
		hh->printf("0\n");
}

//-----------------------------------------------------------------------------
void CControlAPI::SendAllCurrentVAPid(CyhookHandler *hh)
{
	static bool init_iso=true;
	if(init_iso)
	{
		if(_initialize_iso639_map())
			init_iso=false;
	}
	bool eit_not_ok=true;
	CZapitClient::responseGetPIDs pids;

	CSectionsdClient::ComponentTagList tags;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	hh->printf("%05u\n", pids.PIDs.vpid);

	t_channel_id current_channel = NeutrinoAPI->Zapit->getCurrentServiceID();
	CSectionsdClient::responseGetCurrentNextInfoChannelID currentNextInfo;
	CEitManager::getInstance()->getCurrentNextServiceKey(current_channel, currentNextInfo);
	if (CEitManager::getInstance()->getComponentTagsUniqueKey(currentNextInfo.current_uniqueKey,tags))
	{
		for (unsigned int i=0; i< tags.size(); i++)
		{
			for (unsigned short j=0; j< pids.APIDs.size(); j++)
			{
				if ( pids.APIDs[j].component_tag == tags[i].componentTag )
				{
					if(!tags[i].component.empty())
					{
						if(!(isalnum(tags[i].component[0])))
							tags[i].component=tags[i].component.substr(1,tags[i].component.length()-1);
						hh->printf("%05u %s\n",pids.APIDs[j].pid,tags[i].component.c_str());
					}
					else
					{
						if(!(init_iso))
						{
							std::string tmp_desc = _getISO639Description( pids.APIDs[j].desc);
							strncpy(pids.APIDs[j].desc, tmp_desc.c_str(), DESC_MAX_LEN -1);
						}
						hh->printf("%05u %s %s\n",pids.APIDs[j].pid,pids.APIDs[j].desc,pids.APIDs[j].is_ac3 ? " (AC3)": pids.APIDs[j].desc,pids.APIDs[j].is_aac ? "(AAC)" : pids.APIDs[j].desc,pids.APIDs[j].is_eac3 ? "(EAC3)" : " ");
					}
					eit_not_ok=false;
					break;
				}
			}
		}
	}
	if(eit_not_ok)
	{
		unsigned short i = 0;
		for (CZapitClient::APIDList::iterator it = pids.APIDs.begin(); it!=pids.APIDs.end(); ++it)
		{
			if(!(init_iso))
			{
				strncpy( pids.APIDs[i].desc, _getISO639Description( pids.APIDs[i].desc ),DESC_MAX_LEN-1 );
			}
			hh->printf("%05u %s %s\n",it->pid,pids.APIDs[i].desc,pids.APIDs[i].is_ac3 ? " (AC3)": pids.APIDs[i].desc,pids.APIDs[i].is_aac ? "(AAC)" : pids.APIDs[i].desc,pids.APIDs[i].is_eac3 ? "(EAC3)" : " ");
			i++;
		}
	}

	if(pids.APIDs.empty())
		hh->printf("0\n"); // shouldnt happen, but print at least one apid
	if(pids.PIDs.vtxtpid)
		hh->printf("%05u vtxt\n",pids.PIDs.vtxtpid);
	if (pids.PIDs.pmtpid)
		hh->printf("%05u pmt\n",pids.PIDs.pmtpid);
	if (pids.PIDs.pcrpid)
		hh->printf("%05u pcr\n",pids.PIDs.pcrpid);
}
//-----------------------------------------------------------------------------
void CControlAPI::SendTimersPlain(CyhookHandler *hh)
{
	bool send_id = false;

	if (hh->ParamList["format"] == "id")
		send_id = true;

	// A list that could not be read prints nothing, which is what this printed
	// before whenever the daemon answered with nothing.
	coreapi::Result<coreapi::TimerList> read = coreapi::timers::list();
	if (!read.ok())
		return;
	const coreapi::TimerList &timerlist = read.value();

	for (size_t i = 0; i < timerlist.size(); i++)
	{
		const coreapi::TimerInfo &timer = timerlist[i];

		// Add Data
		char zAddData[22+1] = { 0 };
		if (send_id)
		{
			zAddData[0] = '0';
			zAddData[1] = 0;
		}

		switch((CTimerd::CTimerEventTypes) timer.type) {
		//case CTimerd::TIMER_NEXTPROGRAM:
		case CTimerd::TIMER_ZAPTO:
		case CTimerd::TIMER_RECORD:
			if (!send_id)
			{
				strncpy(zAddData, NeutrinoAPI->GetServiceName(timer.channel_id).c_str(), 22);
				if (zAddData[0] == 0)
					strcpy(zAddData, CServiceManager::getInstance()->IsChannelTVChannel(timer.channel_id) ?
							"Unknown TV-Channel" : "Unknown Radio-Channel");
			}
			else
				snprintf(zAddData,sizeof(zAddData), PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.channel_id);

			zAddData[22]=0;

			break;

		case CTimerd::TIMER_STANDBY:
			if (!send_id)
				snprintf(zAddData,sizeof(zAddData),"Standby: %s",(timer.standby_on ? "ON" : "OFF"));
			break;

		case CTimerd::TIMER_REMIND :
			if (!send_id)
				strncpy(zAddData, timer.title.c_str(), 22);
			zAddData[22]=0;
			break;

		default:
			break;
		}

		hh->printf("%d %d %d %d %d %d %d %s\n",
			   (int)timer.id,
			   timer.type,
			   timer.repeat,
			   (int)timer.repeat_count,
			   (int)timer.announce,
			   (int)timer.start,
			   (int)timer.stop,
			   zAddData);
	}
}

//-----------------------------------------------------------------------------
std::string CControlAPI::_SendTime(CyhookHandler *hh, struct tm *Time, int digits) {
	char zTime[25] = {0};
	char zDate[25] = {0};
	strftime(zTime,20,"%H:%M",Time);
	strftime(zDate,20,"%d.%m.%Y",Time);
	std::string result = "";

	result += hh->outPair("text", string_printf("%s %s", zDate, zTime), true);
	result += hh->outPair("date", string_printf("%s", zDate), true);
	result += hh->outPair("time", string_printf("%s", zTime), true);
	result += hh->outPair("digits", string_printf("%d", digits), true);
	result += hh->outPair("day", string_printf("%d", Time->tm_mday), true);
	result += hh->outPair("month", string_printf("%d", Time->tm_mon+1), true);
	result += hh->outPair("year", string_printf("%d", Time->tm_year+1900), true);
	result += hh->outPair("hour", string_printf("%d", Time->tm_hour), true);
	result += hh->outPair("min", string_printf("%d", Time->tm_min), false);

	return result;
}
//-----------------------------------------------------------------------------
// The text a label stands for, out of this program's own locale rather than
// out of the language files the removed server kept beside its pages. Every
// timerlist label the two helpers below produce is written in those locale
// files and in none of those language files, and the removed server's own
// lookup fell through to the locale for exactly that reason, so what a client
// reads here does not move.
//
// The seven weekday labels are the one exception: they were written only in
// the language files that go out with the pages, and this program writes the
// same seven days under names of its own. Mapped rather than let fall through,
// because a timer repeating on weekdays would otherwise name its days with the
// token nobody resolved. The abbreviation is then this program's and not
// always the one those files held.
static std::string translateLabel(const std::string &id)
{
	static const char *const kWeekdayLabels[7] =
	{
		"date.su", "date.mo", "date.tu", "date.we", "date.th", "date.fr", "date.sa"
	};
	for (int i = 0; i < 7; i++)
		if (id == kWeekdayLabels[i])
			return g_Locale->getText(CLocaleManager::getWeekday(i));

	const neutrino_locale_t locale = CLocaleManager::getLocale(id.c_str());
	if (locale != NONEXISTANT_LOCALE)
		return g_Locale->getText(locale);

	// What the removed server answered for a name none of its sources held.
	return "# " + id + " #";
}

// The templated pages are run through the parser before they are sent and the
// control API's answers are not, so a label left as a token reaches a client
// with no way to resolve it. Resolved here rather than where the token is made,
// because the two helpers that make them also feed the templated pages, where
// the token is what belongs. A token without an end is left alone: it is not a
// label, and dropping the rest of the text would lose more than it fixes.
static std::string resolveLabels(const std::string &text)
{
	static const char OPEN[] = "{=L:";
	static const char CLOSE[] = "=}";
	const size_t open_len = sizeof(OPEN) - 1;
	const size_t close_len = sizeof(CLOSE) - 1;

	std::string out;
	size_t pos = 0;
	for (;;)
	{
		size_t start = text.find(OPEN, pos);
		if (start == std::string::npos)
			break;
		size_t end = text.find(CLOSE, start + open_len);
		if (end == std::string::npos)
			break;
		out.append(text, pos, start - pos);
		out += translateLabel(text.substr(start + open_len, end - start - open_len));
		pos = end + close_len;
	}
	out.append(text, pos, std::string::npos);
	return out;
}

// The order the list has always been sent in. The daemon's own comparison is
// this one, so a list built from the same rows in the same order comes out in
// the same order as before.
static bool earlierAlarm(const coreapi::TimerInfo &a, const coreapi::TimerInfo &b)
{
	return a.start < b.start;
}

//-----------------------------------------------------------------------------
// build json/xml for all timer data (needed for yWeb 3)
//-----------------------------------------------------------------------------
void CControlAPI::SendTimers(CyhookHandler *hh)
{
	// A list that could not be read leaves an empty timer_list, which is what a
	// box with no timers left here before.
	coreapi::TimerList timerlist;
	coreapi::Result<coreapi::TimerList> read = coreapi::timers::list();
	if (read.ok())
		timerlist = std::move(read).value();
	sort(timerlist.begin(), timerlist.end(), earlierAlarm);		// sort timer
	std::string result = "";
	std::string config = "";
	std::string timer_list = "";

	// Look for Recording Safety Timers too
	int pre=0, post=0;
	NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
	config += hh->outPair("pre_delay", string_printf("%d", pre), true);
	config += hh->outPair("post_delay", string_printf("%d", post), false);

	result += hh->outObject("config", config, true);

	for(size_t i = 0; i < timerlist.size(); i++)
	{
		const coreapi::TimerInfo &timer = timerlist[i];

		if (i > 0)
			timer_list += hh->outNext();

		std::string timer_item = "";

		timer_item += hh->outPair("type", resolveLabels(NeutrinoAPI->timerEventType2Str((CTimerd::CTimerEventTypes) timer.type)), true);
		timer_item += hh->outPair("id", string_printf("%d", (int)timer.id), true);
		timer_item += hh->outPair("state", string_printf("%d", timer.state), true);
		timer_item += hh->outPair("type_number", string_printf("%d", timer.type), true);

		// alarmtime
		std::string alarm = "";

		struct tm *alarmTime = localtime(&timer.start);
		alarm += hh->outArrayItem("normal", _SendTime(hh, alarmTime, (int)timer.start), true);

		time_t real_alarmTimeT = timer.start - pre;
		struct tm *safetyAlarmTime = localtime(&real_alarmTimeT);
		alarm += hh->outArrayItem("safety", _SendTime(hh, safetyAlarmTime, (int)real_alarmTimeT), false);

		timer_item += hh->outArray("alarm", alarm, true);

		// announcetime
		std::string announce = "";

		struct tm *announceTime = localtime(&timer.announce);
		announce += hh->outArrayItem("normal", _SendTime(hh, announceTime, (int)timer.announce), true);

		time_t real_announceTimeT = timer.announce - pre;
		struct tm *safetyAnnounceTime = localtime(&real_announceTimeT);
		announce += hh->outArrayItem("safety", _SendTime(hh, safetyAnnounceTime, (int)real_announceTimeT), false);

		timer_item += hh->outArray("announce", announce, true);

		// stoptime
		if(timer.stop > 0) {
			std::string stop = "";

			struct tm *stopTime = localtime(&timer.stop);
			stop += hh->outArrayItem("normal", _SendTime(hh, stopTime, (int)timer.stop), true);

			time_t real_stopTimeT = timer.stop - post;
			struct tm *safetyStopTime = localtime(&real_stopTimeT);
			stop += hh->outArrayItem("safety", _SendTime(hh, safetyStopTime, (int)real_stopTimeT), false);

			timer_item += hh->outArray("stop", stop, true);
		}

		// epg_starttime
		std::string start = "";
		struct tm *startTime = localtime(&timer.epg_start);
		start += hh->outArrayItem("normal", _SendTime(hh, startTime, (int)timer.epg_start), false);

		timer_item += hh->outArray("start", start, true);

		// repeat
		std::string repeat = "";

		std::string zRep = resolveLabels(NeutrinoAPI->timerEventRepeat2Str((CTimerd::CTimerEventRepeat) timer.repeat));
		std::string zRepCount;
		if (timer.repeat == (int) CTimerd::TIMERREPEAT_ONCE)
			zRepCount = "-";
		else
			zRepCount = (timer.repeat_count == 0) ? "&#x221E;" : string_printf("%dx",timer.repeat_count);
		std::string weekdays;
		NeutrinoAPI->Timerd->setWeekdaysToStr((CTimerd::CTimerEventRepeat) timer.repeat, weekdays);

		repeat += hh->outPair("count", zRepCount, true);
		repeat += hh->outPair("number", string_printf("%d", timer.repeat), true);
		repeat += hh->outPair("text", zRep, true);
		repeat += hh->outPair("weekdays", weekdays, false);

		timer_item += hh->outObject("repeat", repeat, false);

		// channel infos
		std::string channel_name = NeutrinoAPI->GetServiceName(timer.channel_id);
		if (channel_name.empty())
			channel_name = CServiceManager::getInstance()->IsChannelTVChannel(timer.channel_id) ? "Unknown TV-Channel" : "Unknown Radio-Channel";

		// epg title
		std::string title = timer.title;
		if(timer.epg_id!=0) {
			CEPGData epgdata;
			if (CEitManager::getInstance()->getEPGid(timer.epg_id, timer.epg_start, &epgdata))
				title = epgdata.title;
		}

		// timer specific data
		switch((CTimerd::CTimerEventTypes) timer.type)
		{
#if 0
		case CTimerd::TIMER_NEXTPROGRAM : {
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, false);
		}
		break;
#endif

		case CTimerd::TIMER_ZAPTO : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, false);
		}
		break;

		case CTimerd::TIMER_RECORD : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("channel_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.channel_id), true);
			timer_item += hh->outPair("channel_name", channel_name, true);
			timer_item += hh->outPair("title", title, true);

			// audio
			std::string audio = "";
			std::string apids_conf = "true";
			std::string apids_std = "false";
			std::string apids_alt = "false";
			std::string apids_ac3 = "false";

			if (timer.apids != TIMERD_APIDS_CONF)
			{
				apids_conf = "false";
				if (timer.apids & TIMERD_APIDS_STD)
					apids_std = "true";
				if (timer.apids & TIMERD_APIDS_ALT)
					apids_alt = "true";
				if (timer.apids & TIMERD_APIDS_AC3)
					apids_ac3 = "true";
			}

			audio += hh->outPair("apids_conf", apids_conf, true);
			audio += hh->outPair("apids_std", apids_std, true);
			audio += hh->outPair("apids_alt", apids_alt, true);
			audio += hh->outPair("apids_ac3", apids_ac3, false);

			timer_item += hh->outObject("audio", audio, true);

			timer_item += hh->outPair("recording_dir", timer.recording_dir, true);
			timer_item += hh->outPair("epg_id", string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, timer.epg_id), false);
		}
		break;

		case CTimerd::TIMER_STANDBY : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("status", (timer.standby_on) ? "on" : "off", false);
		}
		break;

		case CTimerd::TIMER_REMIND : {
			std::string _message = timer.title.substr(0, 20);
			timer_item += hh->outNext();
			timer_item += hh->outPair("message", _message, false);
		}
		break;

		case CTimerd::TIMER_EXEC_PLUGIN : {
			timer_item += hh->outNext();
			timer_item += hh->outPair("plugin", timer.title, false);
		}
		break;

		case CTimerd::TIMER_SLEEPTIMER : {
		}
		break;

		case CTimerd::TIMER_IMMEDIATE_RECORD : {
		}
		break;

		default:
		{}
		}
		timer_list += hh->outArrayItem("timer", timer_item, false);
	}
	result += hh->outArray("timer_list", timer_list);
	if (hh->getOutType() == httpd::compat::json)
		result = hh->outArrayItem("timer", result, false);
	result = hh->outArray("timer", result);

	hh->SendResult(result);
}

//-----------------------------------------------------------------------------
// yweb : Extentions
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Dispatcher
//-----------------------------------------------------------------------------
void CControlAPI::YWebCGI(CyhookHandler *hh)
{
	bool status=true;
	if (!hh->ParamList["video_stream_pids"].empty())
	{
		int para=0;
		sscanf( hh->ParamList["video_stream_pids"].c_str(), "%d", &para);
		YWeb_SendVideoStreamingPids(hh, para);
	}
	else if (hh->ParamList["1"] == "radio_stream_pid")
		YWeb_SendRadioStreamingPid(hh);

	if(!status)
		hh->SendError();
}

//-----------------------------------------------------------------------------
// Get Streaming Pids 0x$pmt,0x$vpid,0x$apid with apid_no is the Number of Audio-Pid
//-----------------------------------------------------------------------------
void CControlAPI::YWeb_SendVideoStreamingPids(CyhookHandler *hh, int apid_no)
{
	CZapitClient::responseGetPIDs pids;
	int apid=0,apid_idx=0;
	pids.PIDs.vpid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	if( apid_no < (int)pids.APIDs.size())
		apid_idx=apid_no;
	if(!pids.APIDs.empty())
		apid = pids.APIDs[apid_idx].pid;
	if(!hh->ParamList["no_commas"].empty())
	{
		hh->printf("0x%04x 0x%04x 0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
		if (pids.PIDs.pcrpid != pids.PIDs.vpid)
			hh->printf(" 0x%04x", pids.PIDs.pcrpid);
	}
	else
	{
		hh->printf("0x%04x,0x%04x,0x%04x",pids.PIDs.pmtpid,pids.PIDs.vpid,apid);
		if (pids.PIDs.pcrpid != pids.PIDs.vpid)
			hh->printf(",0x%04x", pids.PIDs.pcrpid);
	}
}

//-----------------------------------------------------------------------------
// Get Streaming Pids 0x$pmt,0x$vpid,0x$apid with apid_no is the Number of Audio-Pid
//-----------------------------------------------------------------------------
void CControlAPI::YWeb_SendRadioStreamingPid(CyhookHandler *hh)
{
	CZapitClient::responseGetPIDs pids;
	int apid=0;
	NeutrinoAPI->Zapit->getPIDS(pids);

	if(!pids.APIDs.empty())
		apid = pids.APIDs[0].pid;
	hh->printf("0x%04x",apid);
}

//-----------------------------------------------------------------------------
void CControlAPI::doModifyTimer(CyhookHandler *hh)
{
	hh->ParamList["update"]="1";
	doNewTimer(hh);
}
//-------------------------------------------------------------------------
void CControlAPI::doNewTimer(CyhookHandler *hh)
{
	time_t	announceTimeT = 0,
		stopTimeT = 0,
		alarmTimeT = 0,
		tnull = 0;
	unsigned int repCount = 0;
	int alHour=0;

	// if alarm given then in parameters im time_t format
	if(!hh->ParamList["alarm"].empty())
	{
		alarmTimeT = atoi(hh->ParamList["alarm"].c_str());
		if(!hh->ParamList["stop"].empty())
			stopTimeT = atoi(hh->ParamList["stop"].c_str());
		if(!hh->ParamList["announce"].empty())
			announceTimeT = atoi(hh->ParamList["announce"].c_str());
		else
			announceTimeT = alarmTimeT;
	}
	else if(!hh->ParamList["alDate"].empty()) //given formatted
	{
		// Alarm Date - Format exact! DD.MM.YYYY
		tnull = time(NULL);
		struct tm *alarmTime=localtime(&tnull);
		alarmTime->tm_sec = 0;
		if(sscanf(hh->ParamList["alDate"].c_str(),"%2d.%2d.%4d",&(alarmTime->tm_mday), &(alarmTime->tm_mon), &(alarmTime->tm_year)) == 3)
		{
			alarmTime->tm_mon -= 1;
			alarmTime->tm_year -= 1900;
		}

		// Alarm Time - Format exact! HH:MM
		if(!hh->ParamList["alTime"].empty())
			sscanf(hh->ParamList["alTime"].c_str(),"%2d.%2d",&(alarmTime->tm_hour), &(alarmTime->tm_min));
		alHour = alarmTime->tm_hour;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;
		struct tm *stopTime = localtime(&alarmTimeT);
		stopTime->tm_sec = 0;
		// Stop Time - Format exact! HH:MM
		if(!hh->ParamList["stTime"].empty())
			sscanf(hh->ParamList["stTime"].c_str(),"%2d.%2d",&(stopTime->tm_hour), &(stopTime->tm_min));

		// Stop Date - Format exact! DD.MM.YYYY
		if(!hh->ParamList["stDate"].empty())
			if(sscanf(hh->ParamList["stDate"].c_str(),"%2d.%2d.%4d",&(stopTime->tm_mday), &(stopTime->tm_mon), &(stopTime->tm_year)) == 3)
			{
				stopTime->tm_mon -= 1;
				stopTime->tm_year -= 1900;
			}
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
		if(hh->ParamList["stDate"].empty() && alHour > stopTime->tm_hour)
			stopTimeT += 24* 60 * 60; // add 1 Day
	}
	else	// alarm/stop time given in pieces
	{
		// alarm time
		time_t now = time(NULL);
		struct tm *alarmTime=localtime(&now);
		if(!hh->ParamList["ad"].empty())
			alarmTime->tm_mday = atoi(hh->ParamList["ad"].c_str());
		if(!hh->ParamList["amo"].empty())
			alarmTime->tm_mon = atoi(hh->ParamList["amo"].c_str())-1;
		if(!hh->ParamList["ay"].empty())
			alarmTime->tm_year = atoi(hh->ParamList["ay"].c_str())-1900;
		if(!hh->ParamList["ah"].empty())
			alarmTime->tm_hour = atoi(hh->ParamList["ah"].c_str());
		if(!hh->ParamList["ami"].empty())
			alarmTime->tm_min = atoi(hh->ParamList["ami"].c_str());
		alarmTime->tm_sec = 0;
		correctTime(alarmTime);
		alarmTimeT = mktime(alarmTime);
		announceTimeT = alarmTimeT;

		// stop time
		struct tm *stopTime = alarmTime;
		if(!hh->ParamList["sd"].empty())
			stopTime->tm_mday = atoi(hh->ParamList["sd"].c_str());
		if(!hh->ParamList["smo"].empty())
			stopTime->tm_mon = atoi(hh->ParamList["smo"].c_str())-1;
		if(!hh->ParamList["sy"].empty())
			stopTime->tm_year = atoi(hh->ParamList["sy"].c_str())-1900;
		if(!hh->ParamList["sh"].empty())
			stopTime->tm_hour = atoi(hh->ParamList["sh"].c_str());
		if(!hh->ParamList["smi"].empty())
			stopTime->tm_min = atoi(hh->ParamList["smi"].c_str());
		stopTime->tm_sec = 0;
		correctTime(stopTime);
		stopTimeT = mktime(stopTime);
	}

	if(announceTimeT != 0)
		announceTimeT -= 60;

	CTimerd::CTimerEventTypes type;
	if(!hh->ParamList["type"].empty())
		type  = (CTimerd::CTimerEventTypes) atoi(hh->ParamList["type"].c_str());
	else // default is: record
		type = CTimerd::TIMER_RECORD;

	// repeat
	if(!hh->ParamList["repcount"].empty())
	{
		repCount = atoi(hh->ParamList["repcount"].c_str());
	}
	CTimerd::CTimerEventRepeat rep;
	if(!hh->ParamList["rep"].empty())
		rep = (CTimerd::CTimerEventRepeat) atoi(hh->ParamList["rep"].c_str());
	else // default: no repeat
		rep = (CTimerd::CTimerEventRepeat)0;
	if(((int)rep) >= ((int)CTimerd::TIMERREPEAT_WEEKDAYS) && !hh->ParamList["wd"].empty())
		NeutrinoAPI->Timerd->getWeekdaysFromStr(&rep, hh->ParamList["wd"]);

	// apids
	bool changeApids=false;
	unsigned char apids=0;
	if(hh->ParamList["apcf"] == "on")
	{
		changeApids=true;
		apids=0;
	}
	else
	{
		if(hh->ParamList["apst"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_STD;
		}
		if(hh->ParamList["apal"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_ALT;
		}
		if(hh->ParamList["apac"] == "on")
		{
			changeApids=true;
			apids |= TIMERD_APIDS_AC3;
		}
	}

	CTimerd::RecordingInfo recinfo;
	CTimerd::EventInfo eventinfo;
	eventinfo.epg_id = 0;
	eventinfo.epg_starttime = atoi(hh->ParamList["start"].c_str());
	eventinfo.apids = TIMERD_APIDS_CONF;
	eventinfo.recordingSafety = (hh->ParamList["rs"] == "1") || (hh->ParamList["rs"] == "on");
	eventinfo.autoAdjustToEPG = (hh->ParamList["aj"] == "1") || (hh->ParamList["aj"] == "on");

	// channel by Id or name
	if(!hh->ParamList["channel_id"].empty())
		sscanf(hh->ParamList["channel_id"].c_str(),
		SCANF_CHANNEL_ID_TYPE,
		&eventinfo.channel_id);
	else
		eventinfo.channel_id = NeutrinoAPI->ChannelNameToChannelId(hh->ParamList["channel_name"]);

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel48(eventinfo.channel_id);
	if(channel)
		eventinfo.channel_id = channel->getChannelID();

	std::string _rec_dir = hh->ParamList["rec_dir"];
	void *data=NULL;
	bool standby_on = false;
	char msg[256];
	static_assert(EXEC_PLUGIN_NAME_MAXLEN <= sizeof(msg) && REMINDER_MESSAGE_MAXLEN <= sizeof(msg), "Timer msg size mismatch");
	if(type == CTimerd::TIMER_RECORD)
		announceTimeT-=120;
	if(type == CTimerd::TIMER_STANDBY)
	{
		standby_on = (hh->ParamList["sbon"]=="1");
		data=&standby_on;
	}
	/* else if(type==CTimerd::TIMER_NEXTPROGRAM || type==CTimerd::TIMER_ZAPTO) */
	else if (type == CTimerd::TIMER_ZAPTO)
		data= &eventinfo;
	else if (type==CTimerd::TIMER_RECORD)
	{
		if(_rec_dir.empty())
		{
			// get Default Recordingdir
			CConfigFile *Config = new CConfigFile(',');
			Config->loadConfig(NEUTRINO_SETTINGS_FILE);
			_rec_dir = Config->getString("network_nfs_recordingdir", TARGET_ROOT "/media/sda1/movies");
			delete Config;
		}
		if(changeApids)
			eventinfo.apids = apids;
		recinfo = eventinfo;
		strncpy(recinfo.recordingDir, _rec_dir.c_str(), RECORD_DIR_MAXLEN-1);
		data = &recinfo;
	}
	else if(type==CTimerd::TIMER_REMIND)
	{
		memset(msg, 0, REMINDER_MESSAGE_MAXLEN);
		strncpy(msg, hh->ParamList["msg"].c_str(),REMINDER_MESSAGE_MAXLEN-1);
		data = msg;
	}
	else if(type==CTimerd::TIMER_EXEC_PLUGIN)
	{
		memset(msg, 0, EXEC_PLUGIN_NAME_MAXLEN);
		strncpy(msg, hh->ParamList["PluginName"].c_str(),EXEC_PLUGIN_NAME_MAXLEN-1);
		data = msg;
	}
	// update or add timer
	if(hh->ParamList["update"]=="1")
	{
		if(!hh->ParamList["id"].empty())
		{
			unsigned modyId = atoi(hh->ParamList["id"].c_str());
			if(type == CTimerd::TIMER_RECORD)
				NeutrinoAPI->Timerd->modifyRecordTimerEvent(modyId, announceTimeT, alarmTimeT, stopTimeT, rep,repCount,_rec_dir.c_str());
			else
				NeutrinoAPI->Timerd->modifyTimerEvent(modyId, announceTimeT, alarmTimeT, stopTimeT, rep,repCount);
//					NeutrinoAPI->Timerd->removeTimerEvent(modyId);
			if(changeApids)
				NeutrinoAPI->Timerd->modifyTimerAPid(modyId,apids);
		}
		else
		{
			CTimerd::TimerList timerlist;
			timerlist.clear();
			NeutrinoAPI->Timerd->getTimerList(timerlist);
			CTimerd::TimerList::iterator timer = timerlist.begin();

			// Look for Recording Safety Timers too
			time_t real_alarmTimeT = alarmTimeT;
			if(eventinfo.recordingSafety)
			{
				int pre,post;
				NeutrinoAPI->Timerd->getRecordingSafety(pre,post);
				real_alarmTimeT -= pre;
			}

			for(; timer != timerlist.end();++timer)
				if(timer->alarmTime == real_alarmTimeT)
				{
					NeutrinoAPI->Timerd->removeTimerEvent(timer->eventID);
					break;
				}
			NeutrinoAPI->Timerd->addTimerEvent(type,data,announceTimeT,alarmTimeT,stopTimeT,rep,repCount);
		}
	}
	else
		NeutrinoAPI->Timerd->addTimerEvent(type,data,announceTimeT,alarmTimeT,stopTimeT,rep,repCount);

	hh->SendOk();
}
//-------------------------------------------------------------------------
void CControlAPI::setBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["selected"].empty()) {
		int selected = atoi(hh->ParamList["selected"].c_str());
		if(hh->ParamList["action"].compare("hide") == 0)
			NeutrinoAPI->Zapit->setBouquetHidden(selected - 1,true);
		else if(hh->ParamList["action"].compare("show") == 0)
			NeutrinoAPI->Zapit->setBouquetHidden(selected - 1,false);
		else if(hh->ParamList["action"].compare("lock") == 0)
			NeutrinoAPI->Zapit->setBouquetLock(selected - 1,true);
		else if(hh->ParamList["action"].compare("unlock") == 0)
			NeutrinoAPI->Zapit->setBouquetLock(selected - 1,false);
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::saveBouquetCGI(CyhookHandler *hh)
{
	/* Thrown away on purpose. What this layer answers is held to the old
	   server's answer byte for byte, and that one is "ok" whatever became of
	   the files; a caller that wants to know whether they were written asks
	   the route that was built to say so. Named rather than cast to void
	   because a cast does not quiet the compiler's own complaint about a
	   discarded result. */
	const bool saved __attribute__((unused)) = NeutrinoAPI->Zapit->saveBouquets();
	NeutrinoAPI->UpdateBouquets();
	hh->SendOk();
}
//-------------------------------------------------------------------------
void CControlAPI::moveBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["selected"].empty() && (
				hh->ParamList["action"] == "up" ||
				hh->ParamList["action"] == "down"))
	{
		int selected = atoi(hh->ParamList["selected"].c_str());
		if (hh->ParamList["action"] == "up") {
			NeutrinoAPI->Zapit->moveBouquet(selected - 1, (selected - 1) - 1);
			selected--;
		} else {
			NeutrinoAPI->Zapit->moveBouquet(selected - 1, (selected + 1) - 1);
			selected++;
		}
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::deleteBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["selected"].empty()) {
		int selected = atoi(hh->ParamList["selected"].c_str());
		NeutrinoAPI->Zapit->deleteBouquet(selected - 1);
		hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::addBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["name"].empty())
	{
		std::string tmp = hh->ParamList["name"];
		if (NeutrinoAPI->Zapit->existsBouquet(tmp.c_str()) == -1)
		{
			NeutrinoAPI->Zapit->addBouquet(tmp.c_str());
			hh->SendOk();
		}
		else
			hh->SendError();
	}
}
//-------------------------------------------------------------------------
void CControlAPI::renameBouquetCGI(CyhookHandler *hh)
{
	if (!hh->ParamList["selected"].empty())
	{
		if (!hh->ParamList["nameto"].empty())
		{
			if (NeutrinoAPI->Zapit->existsBouquet((hh->ParamList["nameto"]).c_str()) == -1)
			{
				NeutrinoAPI->Zapit->renameBouquet(atoi(hh->ParamList["selected"].c_str()) - 1, hh->ParamList["nameto"].c_str());
				hh->SendOk();
				return;
			}
		}
	}
	hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::changeBouquetCGI(CyhookHandler *hh)
{
	if (!(hh->ParamList["selected"].empty()))
	{
		int selected = atoi(hh->ParamList["selected"].c_str());
		CZapitClient::BouquetChannelList BChannelList;
		NeutrinoAPI->Zapit->getBouquetChannels(selected - 1, BChannelList, CZapitClient::MODE_CURRENT, true);
		CZapitClient::BouquetChannelList::iterator channels = BChannelList.begin();
		for(; channels != BChannelList.end(); ++channels)
		{
			NeutrinoAPI->Zapit->removeChannelFromBouquet(selected - 1, channels->channel_id);
		}

		t_channel_id channel_id;
		int delta;
		const char * bchannels = hh->ParamList["bchannels"].c_str();
		while (sscanf(bchannels,
				SCANF_CHANNEL_ID_TYPE
				"%n",
				&channel_id,
				&delta) > 0)
		{
			NeutrinoAPI->Zapit->addChannelToBouquet(selected - 1, channel_id);
			bchannels += (delta + 1); // skip the separating ',', too
		}

		NeutrinoAPI->Zapit->renumChannellist();
		NeutrinoAPI->UpdateBouquets();
		if(!hh->ParamList["redirect"].empty())
			hh->SendRewrite(hh->ParamList["redirect"]);
		else
			hh->SendOk();
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::updateBouquetCGI(CyhookHandler *hh)
{
	NeutrinoAPI->UpdateBouquets();
	hh->SendOk();
}
//-----------------------------------------------------------------------------
//	details EPG Information in xmltv format from all user bouquets
//-----------------------------------------------------------------------------
void CControlAPI::xmltvepgCGI(CyhookHandler *hh)
{
	hh->ParamList["format"] = "xml";
	hh->outStart();

	int mode;
	if (hh->ParamList["mode"] == "tv")
		mode = CZapitClient::MODE_TV;
	else if (hh->ParamList["mode"] == "radio")
		mode = CZapitClient::MODE_RADIO;
	else
		mode = CZapitClient::MODE_ALL;

	bool xml_cdata = false;
	t_channel_id channel_id;
	std::string result = "";
	std::string channelTag = "", channelData = "";
	std::string programmeTag = "", programmeData = "";

	ZapitChannelList chanlist;
	CChannelEventList eList;
	CChannelEventList::iterator eventIterator;

	for (unsigned int i = 0; i < g_bouquetManager->Bouquets.size(); i++)
	{
		g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);

		for (int m = CZapitClient::MODE_TV; m < CZapitClient::MODE_ALL; m++)
		{
			if (mode == CZapitClient::MODE_RADIO || m == CZapitClient::MODE_RADIO)
				g_bouquetManager->Bouquets[i]->getRadioChannels(chanlist);
			else
				g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);

			if(!chanlist.empty() && !g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
			{
				for(unsigned int j = 0; j < chanlist.size(); j++)
				{
					CZapitChannel * channel = chanlist[j];
					channel_id = channel->getChannelID() & 0xFFFFFFFFFFFFULL;
					channelTag = "channel id=\""+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id)+"\"";
					channelData = hh->outPair("display-name", hh->outValue(channel->getName(), xml_cdata), true);
					result += hh->outObject(channelTag, channelData);

					eList.clear();

					CEitManager::getInstance()->getEventsServiceKey(channel_id, eList);

					if (eList.size() == 0)
						continue;

					if (eList.size() > 50)
						eList.erase(eList.begin()+50,eList.end());

					for (eventIterator = eList.begin(); eventIterator != eList.end(); ++eventIterator)
					{
						if (eventIterator->get_channel_id() == channel_id)
						{
							programmeTag  = "programme ";
							programmeTag += "channel=\""+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel_id)+"\" ";
							char zbuffer[25] = { 0 };
							struct tm *mtime = gmtime(&eventIterator->startTime);
							strftime(zbuffer, 21, "%Y%m%d%H%M%S %z", mtime);
							programmeTag += "start=\""+std::string(zbuffer)+"\" ";
							time_t _stoptime = eventIterator->startTime + eventIterator->duration;
							mtime = gmtime(&_stoptime);
							strftime(zbuffer, 21, "%Y%m%d%H%M%S %z", mtime);
							programmeTag += "stop=\""+std::string(zbuffer)+"\" ";

							programmeData  = hh->outPair("title lang=\"de\"", hh->outValue(eventIterator->description, xml_cdata), false);
							programmeData += hh->outPair("desc lang=\"de\"", hh->outValue(eventIterator->text, xml_cdata), true);

							result += hh->outArrayItem(programmeTag, programmeData, false);
						}
					}
				}
			}
		}
	}

	result = hh->outObject("tv generator-info-name=\"Neutrino XMLTV Generator v1.0\"", result);

	result = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n" + result;

	hh->SendResult(result);
}

void CControlAPI::xmltvm3uCGI(CyhookHandler *hh)
{
	//hh->outStart();

	int mode;
	if (hh->ParamList["mode"] == "tv")
		mode = CZapitClient::MODE_TV;
	else if (hh->ParamList["mode"] == "radio")
		mode = CZapitClient::MODE_RADIO;
	else
		mode = CZapitClient::MODE_ALL;

	std::string host = "";
	if (!hh->ParamList["host"].empty())
		host = "http://" + hh->ParamList["host"];
	else
		host = "http://" + hh->HeaderList["Host"];

	// get hostname
	char hostname[HOST_NAME_MAX];
	gethostname(hostname, HOST_NAME_MAX);

	std::string result("#EXTM3U");
	result += " tvg-url=\"" + host + "/control/xmltv.xml";

	if (mode == CZapitClient::MODE_TV)
		result += "?mode=tv";
	else if (mode == CZapitClient::MODE_RADIO)
		result += "?mode=radio";

	result += "\"\n";

	// build url
	std::string url = host;
	/* strip off optional custom port */
	if (url.rfind(":") != 4)
		url = url.substr(0, url.rfind(":")); // strip off optional custom port
	url += ":";
	url += to_string(g_settings.streaming_port);
	url += "/id=";

	for (unsigned int i = 0; i < g_bouquetManager->Bouquets.size(); i++)
	{
		ZapitChannelList chanlist;

		for (int m = CZapitClient::MODE_TV; m < CZapitClient::MODE_ALL; m++)
		{
			if (mode == CZapitClient::MODE_RADIO || m == CZapitClient::MODE_RADIO)
				g_bouquetManager->Bouquets[i]->getRadioChannels(chanlist);
			else
				g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);

			if (!chanlist.empty() && !g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
			{
				for (unsigned int j = 0; j < chanlist.size(); j++)
				{
					CZapitChannel *channel = chanlist[j];
					std::string bouq_name = g_bouquetManager->Bouquets[i]->bName;
					std::string chan_name = channel->getName();
					std::string chan_id_short = string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID() & 0xFFFFFFFFFFFFULL);
					result += "#EXTINF:-1 tvg-id=\"" + chan_id_short + "\"";
					result += " tvg-name=\"" + str_replace(",", ".", chan_name) + "\"";
					if (!NeutrinoAPI->getLogoFile(channel->getChannelID()).empty())
						result += " tvg-logo=\"" + host + NeutrinoAPI->getLogoFile(channel->getChannelID()) + "\"";
					else
						result += " tvg-logo=\"\"";
					if (mode == CZapitClient::MODE_RADIO || m == CZapitClient::MODE_RADIO)
						result += " radio=\"true\"";
					else
						result += " radio=\"\"";
					result += " group-prefix=\"" + std::string(hostname) + "\"";
					result += " group-title=\"" + bouq_name + "\",";
					result += channel->getName() + "\n";
					result += url + string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID()) + "\n";
				}
			}

			if (mode != CZapitClient::MODE_ALL)
				break;
		}
	}

	hh->SendResult(result);
}

void CControlAPI::xmltvlistCGI(CyhookHandler *hh)
{
	std::vector<std::string>::iterator it;
	std::vector<std::string> url_list;
	std::string tmp;

	if (!hh->ParamList["webtv"].empty())
	{
		std::string webtv_url = hh->ParamList["webtv"];
		g_settings.webtv_xml.clear();
		url_list = ::split(webtv_url, '\n');
		for (it = url_list.begin(); it != url_list.end(); it++)
		{
			tmp = (*it);
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\n'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\r'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\t'), tmp.end());
			if (!tmp.empty())
				g_settings.webtv_xml.push_back(tmp);
		}
	}
	else
		g_settings.webtv_xml.clear();

	if (!hh->ParamList["webradio"].empty())
	{
		std::string webradio_url = hh->ParamList["webradio"];
		g_settings.webradio_xml.clear();
		url_list = ::split(webradio_url, '\n');
		for (it = url_list.begin(); it != url_list.end(); it++)
		{
			tmp = (*it);
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\n'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\r'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\t'), tmp.end());
			if (!tmp.empty())
				g_settings.webradio_xml.push_back(tmp);
		}
	}
	else
		g_settings.webradio_xml.clear();

	if (!hh->ParamList["xmltv"].empty())
	{
		std::string xmltv_url = hh->ParamList["xmltv"];
		g_settings.xmltv_xml.clear();
		url_list = ::split(xmltv_url, '\n');
		for (it = url_list.begin(); it != url_list.end(); it++)
		{
			tmp = (*it);
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\n'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\r'), tmp.end());
			tmp.erase(std::remove(tmp.begin(), tmp.end(), '\t'), tmp.end());
			if (!tmp.empty())
				g_settings.xmltv_xml.push_back(tmp);
		}
	}
	else
		g_settings.xmltv_xml.clear();

	hh->SendOk();
}
//-------------------------------------------------------------------------
// host : (optional) ip of dbox
void CControlAPI::build_live_url(CyhookHandler *hh)
{
	int mode = NeutrinoAPI->Zapit->getMode();
	// build url
	std::string url = "";
	if(!hh->ParamList["host"].empty())
		url = "http://"+hh->ParamList["host"];
	else
		url = "http://"+hh->HeaderList["Host"];
	/* strip off optional custom port */
	if (url.rfind(":") != 4)
		url = url.substr(0, url.rfind(":"));

	url += ":";
	url += to_string(g_settings.streaming_port);
	url += "/id=";

	// response url
	if(!hh->ParamList["vlc_link"].empty())
	{
		// An external player (VLC, Kodi) fetches this URL directly and expects
		// the playlist itself, not a redirect to a file it has no way to
		// reach once sendfile stops serving arbitrary paths. The file at
		// /tmp/vlc.m3u still gets written line for line, in step with the
		// body, in case something else reads it there.
		std::string line = "#EXTM3U\n";
		write_to_file("/tmp/vlc.m3u", line);
		hh->Write(line);
		for (int i = 0; i < (int) g_bouquetManager->Bouquets.size(); i++)
		{
			ZapitChannelList chanlist;
			if (mode == CZapitClient::MODE_RADIO)
				g_bouquetManager->Bouquets[i]->getRadioChannels(chanlist);
			else
				g_bouquetManager->Bouquets[i]->getTvChannels(chanlist);
			if (!chanlist.empty() && !g_bouquetManager->Bouquets[i]->bHidden && g_bouquetManager->Bouquets[i]->bUser)
			{
				for (int j = 0; j < (int) chanlist.size(); j++)
				{
					CZapitChannel * channel = chanlist[j];
					//printf("---> %s/n",channel->getName().c_str());
					line = "#EXTINF:-1,"+channel->getName()+"\n";
					write_to_file("/tmp/vlc.m3u", line, true);
					hh->Write(line);
					line = url+string_printf(PRINTF_CHANNEL_ID_TYPE_NO_LEADING_ZEROS, channel->getChannelID())+"\n";
					write_to_file("/tmp/vlc.m3u", line, true);
					hh->Write(line);
				}
			}
		}
		hh->SetHeader(httpd::compat::HTTP_OK, "audio/x-mpegurl");
	} else
		hh->SendError();
}

//-------------------------------------------------------------------------
void CControlAPI::build_playlist(CyhookHandler *hh)
{
	// build url
	std::string url = "";
	if(!hh->ParamList["host"].empty())
		url = "http://"+hh->ParamList["host"];
	else
		url = "http://"+hh->HeaderList["Host"];
	/* strip off optional custom port */
	if (url.rfind(":") != 4)
		url = url.substr(0, url.rfind(":"));

	url += ":";
	url += to_string(g_settings.streaming_port);
	url += "/id=";

	if (!hh->ParamList["id"].empty())
	{
		url += hh->ParamList["id"];
		t_channel_id channel_id;
		sscanf(hh->ParamList["id"].c_str(), SCANF_CHANNEL_ID_TYPE, &channel_id);
		std::string chan_name = NeutrinoAPI->Zapit->getChannelName(channel_id);
		std::string illegalChars = "\\/:?\"<>|+ ";
		std::string::iterator it;
		for (it = chan_name.begin() ; it < chan_name.end() ; ++it)
		{
			bool found = illegalChars.find(*it) != std::string::npos;
			if (found)
			{
				*it = '_';
			}
		}
		std::string m3u = "/tmp/" + chan_name + ".m3u";
		// Same reasoning as build_live_url: the external client that asked
		// for this playlist gets the body directly, and the file stays only
		// for whatever else might read it there.
		std::string line = "#EXTM3U\n";
		write_to_file(m3u, line);
		hh->Write(line);
		line = "#EXTINF:-1," + NeutrinoAPI->Zapit->getChannelName(channel_id) + "\n";
		write_to_file(m3u, line, true);
		hh->Write(line);
		write_to_file(m3u, url, true);
		hh->Write(url);
		hh->SetHeader(httpd::compat::HTTP_OK, "audio/x-mpegurl");
	}
	else
		hh->SendError();
}
//-------------------------------------------------------------------------
void CControlAPI::logoCGI(CyhookHandler *hh)
{
	t_channel_id channel_id;
	sscanf(hh->ParamList["1"].c_str(),
	       SCANF_CHANNEL_ID_TYPE,
	       &channel_id);
	hh->Write(NeutrinoAPI->getLogoFile(channel_id));
}
//-------------------------------------------------------------------------
/** Get Config File or save values to given config file
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/config?config=neutrino|nhttpd|yweb[&action=submit&key1=value1&key2=value2&...][&format=|xml|json]
 * @endcode
 *
 * @par example:
 * @code
 * /control/config?config=neutrino
 * /control/config?config=neutrino&format=json
 * /control/config?config=neutrino&action=submit&epg_dir=/media/sda1/epg
 * @endcode
 *
 * @par output (json)
 * /control/config?config=neutrino&format=json
 * @code
 * {"success": "true", "data":{"analog_mode1": "16",
 * "analog_mode2": "1",
 * [...snip...]
 * "zap_cycle": "0",
 * "zapto_pre_time": "0",
 * }}
 * @endcode
 *
 * @par output (plain)
 * /control/config?config=neutrino
 * @code
 * analog_mode1=16
 * analog_mode2=1
 * [...snip...]
 * zap_cycle=0
 * zapto_pre_time=0
 * @endcode
 *
 * @par output (xml)
 * /control/config?config=neutrino&format=xml
 * @code
 * <config>
 * <analog_mode1>16</analog_mode1>
 * <analog_mode2>1</analog_mode2>
 * [...snip...]
 * <zap_cycle>0</zap_cycle>
 * <zapto_pre_time>0</zapto_pre_time>
 * </config>
 * @endcode
 */
//-------------------------------------------------------------------------
void CControlAPI::ConfigCGI(CyhookHandler *hh)
{
	bool load = true;
	CConfigFile *Config = new CConfigFile(',');
	ConfigDataMap conf;
	std::string config_filename = "";
	std::string error = "";
	std::string result = "";
	std::string configFileName = hh->ParamList["config"];

	hh->outStart();

	if (hh->ParamList["action"] == "submit")
		load = false;

	// Para "config" describes the config type
	if (configFileName == "neutrino")
		config_filename = NEUTRINO_SETTINGS_FILE;
	else if (configFileName == "moviebrowser")
		config_filename = MOVIEBROWSER_CONFIGFILE;
	else if (configFileName == "nhttpd")
		config_filename = NI_WEB_OLD_SETTINGS_FILE;
	else if (configFileName == "yweb")
		config_filename = YWEB_CONFIGFILE;

	if (!config_filename.empty()) {
		Config->loadConfig(config_filename);

		if (load) {	// get and output list
			conf = Config->getConfigDataMap();
			ConfigDataMap::iterator it, end, start;
			for (start = conf.begin(), it=start, end = conf.end(); it != end; ++it) {
				std::string key = it->first;
				replace(key, ".", "_dot_");
				replace(key, "-", "_bind_");
				if (!(hh->ParamList["config"] == "nhttpd" && it->first == "mod_auth.password")) {
					if(it != start)
						result += hh->outNext();
					result += hh->outPair(key, it->second, false);
				}
			}
		}
		else { // set values and save list
			for (CStringList::iterator it = hh->ParamList.begin(); it != hh->ParamList.end(); ++it) {
				std::string key = it->first;

				// parseParams (httpd/compat/query.cpp) adds a second entry for
				// every token, named or not: "<position>" -> "<name>". A submit
				// here would otherwise write that echo into the config file
				// too, as a phantom "<position>=<name>" line next to the real
				// setting.
				if (isPositionKey(key))
					continue;

				replace(key, "_dot_", ".");
				replace(key, "_bind_", "-");
				if (key != "_dc" && key != "action" && key != "format" && key != "config") {
					Config->setString(key, it->second);
				}
			}
			if (!config_filename.empty())
				Config->saveConfig(config_filename);
		}
	}
	else {
		if(!configFileName.empty())
			error = string_printf("no config defined for %s", (hh->ParamList["config"]).c_str());
	}

	hh->WriteLn(hh->outObject("config", result));

	if (error.empty())
		hh->SendResult(result);
	else
		hh->SendError(error);

	delete Config;
}

//-----------------------------------------------------------------------------
/** Get a list of files with attributtes for a given path
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/file?action=list&path={path}[&format=|xml|json][&sort=false]
 * @endcode
 *
 * @par example:
 * @code
 * /control/file?action=list&path=/media
 * /control/file?action=list&path=/media&format=json
 * /control/file?action=list&path=/media&format=json&sort=false
 * /control/file?action=new_folder&path=/media/sda1/movies/new
 * /control/file?action=delete&path=/media/sda1/movies/new
 * @endcode
 *
 * @par
 * A path has to resolve inside one of the permitted directories: the media and
 * the plugin directories, plus whatever this box was told to record into. A
 * path outside them is answered with 400, one that is not there with 404.
 *
 * @par output
 * @code
 * {"success": "true", "data":{"filelist": [{"name": "timeshift",
 * "type_str": "dir",
 * "type": "4",
 * "fullname": "/media/timeshift",
 * "mode": "41ffld",
 * "nlink": "2",
 * "user": "root",
 * "group": "root",
 * "size": "1024",
 * "time": "Sun Sep  4 07:38:10 2011",
 * "time_t": "1315114690"
 * },
 * @endcode
 * ... snip ...
 * @code
 * {"name": "recordings",
 * "type_str": "dir",
 * "type": "4",
 * "fullname": "/media/recordings",
 * "mode": "41edld",
 * "nlink": "2",
 * "user": "1000",
 * "group": "1000",
 * "size": "1024",
 * "time": "Wed Aug 31 12:09:29 2011",
 * "time_t": "1314785369"
 * },
 * ]
 * }}
 * @endcode
 *
 *  @par Not implemented now:
 *  action =read_file|write_file|set_properties
 */
//-----------------------------------------------------------------------------

// What a directory calls one of its names, in the words this endpoint has
// always used. Anything else keeps the empty name it has always had.
static const char *fileTypeName(coreapi::FileKind kind)
{
	switch (kind) {
		case coreapi::FileKind::Dir:		return "dir";
		case coreapi::FileKind::Link:		return "lnk";
		case coreapi::FileKind::Regular:	return "file";
		default:				return "";
	}
}

// The number a directory entry carries, for every kind one can have. This
// endpoint has always printed it straight off the directory, so every kind maps
// back to exactly the number it came from and none of them collapses into
// another. It cannot be read off the mode instead: that follows a link and
// would answer with what the link points at.
static int fileTypeNumber(coreapi::FileKind kind)
{
	switch (kind) {
		case coreapi::FileKind::Fifo:		return DT_FIFO;
		case coreapi::FileKind::CharDevice:	return DT_CHR;
		case coreapi::FileKind::Dir:		return DT_DIR;
		case coreapi::FileKind::BlockDevice:	return DT_BLK;
		case coreapi::FileKind::Regular:	return DT_REG;
		case coreapi::FileKind::Link:		return DT_LNK;
		case coreapi::FileKind::Socket:		return DT_SOCK;
		case coreapi::FileKind::Whiteout:	return DT_WHT;
		default:				return DT_UNKNOWN;
	}
}

// The listing has always come back in name order unless the caller said not to.
static bool byName(const coreapi::FileEntry &a, const coreapi::FileEntry &b)
{
	return a.name < b.name;
}

static httpd::compat::HttpResponseType httpStatusFor(coreapi::Status status)
{
	switch (status) {
		case coreapi::Status::NotFound:		return httpd::compat::HTTP_NOT_FOUND;
		case coreapi::Status::InvalidArgument:	return httpd::compat::HTTP_BAD_REQUEST;
		case coreapi::Status::Conflict:		return httpd::compat::HTTP_CONFLICT;
		case coreapi::Status::NotSupported:	return httpd::compat::HTTP_NOT_IMPLEMENTED;
		case coreapi::Status::Busy:		return httpd::compat::HTTP_SERVICE_UNAVAILABLE;
		default:				return httpd::compat::HTTP_INTERNAL_SERVER_ERROR;
	}
}

// The code and never the message: the message quotes the name the caller sent,
// and this body is written into json and xml without escaping anything.
static void sendCode(httpd::compat::CyhookHandler *hh, coreapi::Status status, const char *code)
{
	hh->httpStatus = httpStatusFor(status);
	// This server's header for a not found declares no body at all, so writing
	// one would put bytes behind a length of zero.
	if (hh->httpStatus != httpd::compat::HTTP_NOT_FOUND)
		hh->SendError(code);
}

static void sendStorageError(httpd::compat::CyhookHandler *hh, const coreapi::Error &error)
{
	sendCode(hh, error.status, coreapi::codeString(error.code));
}

void CControlAPI::FileCGI(CyhookHandler *hh)
{
	std::string result = "";
	const std::string path = hh->ParamList["path"];

	coreapi::storage::refreshRoots();

	if (hh->ParamList["action"] == "list") { // directory list: action=list&path=<path>
		hh->outStart();

		coreapi::Result<std::vector<coreapi::FileEntry> > read = coreapi::storage::list(path);
		if (!read.ok()) {
			// A directory that could not be opened used to answer an empty
			// filelist, which reads as a directory with nothing in it.
			sendStorageError(hh, read.error());
			return;
		}

		std::string dir = path;
		if (dir[dir.length() - 1] != '/')
			dir += "/";

		std::vector<coreapi::FileEntry> filelist = read.value();
		if (hh->ParamList["sort"] != "false")
			sort(filelist.begin(), filelist.end(), byName);

		for(std::vector<coreapi::FileEntry>::iterator f = filelist.begin(); f != filelist.end(); ++f)
		{
			bool got_next = (f != filelist.end()-1);

			std::string item = "";
			item += hh->outPair("name",	hh->outValue(f->name.c_str()), true);
			item += hh->outPair("type_str",	hh->outValue(fileTypeName(f->kind)), true);
			item += hh->outPair("type",	string_printf("%d", fileTypeNumber(f->kind)), true);
			item += hh->outPair("fullname",	hh->outValue(dir + f->name), true);

			// A name nothing could be read about leaves this whole block out,
			// because a zero here is a real owner, a real size and a real time.
			if (f->attributes_read) {
				item += hh->outPair("mode", string_printf("%xld", (long) f->mode), true);

				/* Print out type, permissions, and number of links. */
				//TODO:	hh->printf("\t\t<permission>%10.10s</permission>\n", sperm (f->mode));
				item += hh->outPair("nlink", string_printf("%d", (int) f->nlink), true);

				/* Print out owner's name if it is found using getpwuid(). */
				struct passwd *pwd;
				if ((pwd = getpwuid(f->uid)) != NULL)
					item += hh->outPair("user", pwd->pw_name, true);
				else
					item += hh->outPair("user", string_printf("%d", f->uid), true);

				/* Print out group name if it is found using getgrgid(). */
				struct group *grp;
				if ((grp = getgrgid(f->gid)) != NULL)
					item += hh->outPair("group", grp->gr_name, true);
				else
					item += hh->outPair("group", string_printf("%d", f->gid), true);

				/* Print size of file. */
				item += hh->outPair("size", string_printf("%jd", (intmax_t) f->size), true);

				struct tm *tm = localtime(&f->mtime);
				char datestring[256] = {0};
				/* Get localized date string. */
				strftime(datestring, sizeof(datestring), nl_langinfo(D_T_FMT), tm);
				item += hh->outPair("time", hh->outValue(datestring), true);
				item += hh->outPair("time_t", string_printf("%ld", (long) f->mtime), false);
			}
			result += hh->outArrayItem("item", item, got_next);
		}
		result = hh->outArray("filelist", result);

		hh->SendResult(result);
	}
	// create new folder
	else if (hh->ParamList["action"] == "new_folder") {
		hh->outStart();
		coreapi::Result<void> made = coreapi::storage::createDirectory(path);
		if (!made.ok()) {
			sendStorageError(hh, made.error());
			return;
		}
		hh->SendOk();
	}
	else if (hh->ParamList["action"] == "delete") {
		hh->outStart();
		coreapi::Result<void> removed = coreapi::storage::removePath(path);
		if (!removed.ok()) {
			sendStorageError(hh, removed.error());
			return;
		}
		hh->SendOk();
	}
	else {
		// Every other action used to reach the end of this with no header set
		// and nothing written, which the server answers as an empty 200. The
		// code is this endpoint's own: what action names it takes is a thing
		// only this endpoint has.
		hh->outStart();
		sendCode(hh, coreapi::Status::InvalidArgument, "unknown-action");
	}
}

//-----------------------------------------------------------------------------
/** Get a list of statfs output for a given path
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/statfs[?path={path}][&format=plain|xml|json]
 * @endcode
 *
 * @par example:
 * @code
 * /control/statfs
 * /control/statfs?path=/media/sda1/movies&format=json
 * @endcode
 *
 * @par output
 * @code
 * {"success": "true", "data":
 * {
 * 	"statfs": {
 * 		"path": "/media/sda1/movies",
 * 		"f_type": "0x4d44",
 * 		"f_bsize": "4096",
 * 		"f_blocks": "488444",
 * 		"f_bfree": "365874",
 * 		"f_bavail": "365874",
 * 		"f_files": "0",
 * 		"f_ffree": "0",
 * 		"f_fsid": "0x801, 0",
 * 		"f_namelen": "1530",
 * 		"f_frsize": "24"
 * 	}
 * }}
 * @endcode
 */
//-----------------------------------------------------------------------------
void CControlAPI::StatfsCGI(CyhookHandler *hh)
{
	std::string result = "";

	if (hh->ParamList["path"].empty())
		hh->ParamList["path"] = "/";

	hh->outStart();

	std::string path = hh->ParamList["path"];
	// The kernel read here rather than through the layer below. What this
	// answers is the filesystem's own eleven counters under their own names,
	// for any name the caller sends and for / by default, and that is a shape
	// and a reach the layer below deliberately does not have. Keeping the old
	// answer belongs on this side of the line, with the rest of what is here
	// only because it always has been.
	struct statfs s;
	if (::statfs(path.c_str(), &s) == 0)
	{
		std::string item = "";
		item += hh->outPair("path", path.c_str(), true);
		item += hh->outPair("f_type", string_printf("%#lx", (unsigned long) s.f_type), true);
		item += hh->outPair("f_bsize", string_printf("%lu", (unsigned long) s.f_bsize), true);
		item += hh->outPair("f_blocks", string_printf("%lu", (unsigned long) s.f_blocks), true);
		item += hh->outPair("f_bfree", string_printf("%lu", (unsigned long) s.f_bfree), true);
		item += hh->outPair("f_bavail", string_printf("%lu", (unsigned long) s.f_bavail), true);
		item += hh->outPair("f_files", string_printf("%lu", (unsigned long) s.f_files), true);
		item += hh->outPair("f_ffree", string_printf("%lu", (unsigned long) s.f_ffree), true);
		item += hh->outPair("f_fsid", string_printf("%#x, %#x", (unsigned) s.f_fsid.__val[0], (unsigned) s.f_fsid.__val[1]), true);
		item += hh->outPair("f_namelen", string_printf("%lu", (unsigned long) s.f_namelen), true);
		item += hh->outPair("f_frsize", string_printf("%lu", (unsigned long) s.f_frsize), false);

		result = hh->outObject("statfs", item);

		hh->SendResult(result);
	}
	else
		hh->SendError("statfs failed");
}

//-----------------------------------------------------------------------------
/** Get neutrino directories
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getdir?dir=allmoviedirs&[&subdirs=true][&format=|xml|json]
 * @endcode
 *
{"success": "true", "data":{"dirs": [{"dir": "/mnt/series/",
"used": "1"
}
,{"dir": "/mnt/movies/",
"used": "1"
}
,{"dir": "/mnt/movies/subdir"
}
{"dir": "/media/sda1/movie"
}
,]
}}
 * @endcode
 *
 */
//-----------------------------------------------------------------------------
void CControlAPI::getDirCGI(CyhookHandler *hh)
{
	std::string result = "";
	std::string item = "";
	bool isFirstLine = true;

	coreapi::storage::refreshRoots();
	hh->outStart(true /*old mode*/);

	//Shows all 7 directories stored in the moviebrowser.conf
	if (hh->ParamList["dir"] == "moviedir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(MOVIEBROWSER_CONFIGFILE);
		char index[21];
		std::string mb_dir;

		for(int i=0;i<8;i++) {
			snprintf(index, sizeof(index), "%d", i);
			mb_dir = "mb_dir_";
			mb_dir = mb_dir + index;
			mb_dir = Config->getString(mb_dir, "");

			if(!mb_dir.empty()) {
				item += hh->outPair("dir", hh->outValue(mb_dir), false);
				if(isFirstLine) {
					isFirstLine = false;
				}
				else {
					result += hh->outNext();
				}
				result += hh->outArrayItem("item", item, false);
				item = "";
				if (hh->ParamList["subdirs"] == "true") {
					result = getSubdirectories(hh, mb_dir, result);
				}
			}
		}
	}

	//Shows the neutrino recording dir
	if (hh->ParamList["dir"] == "recordingdir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		item += hh->outPair("dir", hh->outValue(settingsText(g_settings.network_nfs_recordingdir)), false);
		if(isFirstLine) {
			isFirstLine = false;
		}
		else {
			result += hh->outNext();
		}
		result += hh->outArrayItem("item", item, false);
		if (hh->ParamList["subdirs"] == "true") {
			result = getSubdirectories(hh, settingsText(g_settings.network_nfs_recordingdir), result);
		}
	}

	result = hh->outArray("dirs", result);

	hh->SendResult(result);
}

//Helpfunction to get subdirs of a dir
std::string CControlAPI::getSubdirectories(CyhookHandler *hh, std::string path, std::string result)
{
	std::string item = "";

	coreapi::Result<std::vector<coreapi::FileEntry> > read = coreapi::storage::list(path);
	// A configured directory that cannot be read is passed over, whatever the
	// reason, which is what an unopenable one has always been. Refusing the
	// whole answer over one of them would cost the caller the directories that
	// are perfectly readable, and would make this arm disagree with the one
	// that lists the same directories without descending into them.
	if (!read.ok())
		return result;

	// Directories only, and a link that points at one is not a directory here.
	// Following it would walk the same names twice, or forever.
	const std::vector<coreapi::FileEntry> &entries = read.value();
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].kind == coreapi::FileKind::Dir && entries[i].name[0] != '.') {
			if (path[path.length() - 1] != '/') {
				path += "/";
			}
			std::string fullname = path + entries[i].name;
			item += hh->outPair("dir", hh->outValue(fullname), false);
			result += hh->outNext();
			result += hh->outArrayItem("item", item, false);
			item = "";
			result = getSubdirectories(hh, fullname, result);
		}
	}
	return result;
}

//-----------------------------------------------------------------------------
/** Get neutrino movies
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/getmovies?ir=allmoviedirs&[&subdirs=true][&format=|xml|json]
 * @endcode
 *
{"success": "true", "data":{"movies": [{"title": "Sample.mkv",
"path": "/media/sda1/movies/Sample.mkv",
"size": "1136242099"
}
,{"title": "Aufnahme1.ts",
"path": "/media/sda1/recording/Aufnahme1.ts",
"size": "941"
}
,{"title": "Aufnahme2.ts",
"path": "/media/sda1/recording/Aufnahme2.ts",
"size": "941"
}
]
}}
 * @endcode
 *
 */
//-----------------------------------------------------------------------------
void CControlAPI::getMoviesCGI(CyhookHandler *hh) {
	std::string result = "";
	bool subdirs = true;
	coreapi::Result<void> refusal = coreapi::Result<void>::success();

	if(hh->ParamList["subdirs"] == "false") {
		subdirs = false;
	}

	coreapi::storage::refreshRoots();
	hh->outStart();

	//Shows all movies with path in moviebrowser.conf
	if (hh->ParamList["dir"] == "moviedir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		CConfigFile *Config = new CConfigFile(',');
		Config->loadConfig(MOVIEBROWSER_CONFIGFILE);
		char index[21];
		std::string mb_dir;

		for(int i=0;i<8;i++) {
			snprintf(index, sizeof(index), "%d", i);
			mb_dir = "mb_dir_";
			mb_dir = mb_dir + index;
			mb_dir = Config->getString(mb_dir, "");

			if(!mb_dir.empty()) {
				result = readMovies(hh, mb_dir, result, subdirs, NULL);
			}
		}
	}

	//Shows all movies in the recordingdir
	if (hh->ParamList["dir"] == "recordingdir" || hh->ParamList["dir"] == "allmoviedirs" ) {
		result = readMovies(hh, settingsText(g_settings.network_nfs_recordingdir), result, subdirs, NULL);
	}

	//Shows movie from a given path
	if (hh->ParamList["dir"][0] == '/') {
		result = readMovies(hh, hh->ParamList["dir"], result, subdirs, &refusal);
	}

	// Only the directory the caller named can refuse this answer, and it does
	// so with the error the layer below gave rather than with one made up here.
	if (!refusal.ok()) {
		sendStorageError(hh, refusal.error());
		return;
	}

	result = hh->outArray("movies", result);

	hh->SendResult(result);
}

//Helpfunction to get movies of a dir
std::string CControlAPI::readMovies(CyhookHandler *hh, std::string path, std::string result, bool subdirs,
				    coreapi::Result<void> *refusal) {
	std::string item = "";
	std::string fullname;

	coreapi::Result<std::vector<coreapi::FileEntry> > read = coreapi::storage::list(path);
	if (!read.ok()) {
		// A directory the caller named itself has to be told what was wrong
		// with it, in the words the layer below used, or the answer reads as a
		// directory holding no recordings. A configured one passes nothing here
		// and is skipped, as an unreadable one has always been.
		if (refusal != NULL)
			*refusal = coreapi::fail(read.error());
		return result;
	}

	if (path[path.length() - 1] != '/') {
		path += "/";
	}

	// Regular files only: a link is not one, whatever it points at, which is
	// what keeps a link back up the tree from being listed as a recording.
	const std::vector<coreapi::FileEntry> &entries = read.value();
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].kind == coreapi::FileKind::Regular) {
			fullname = path + entries[i].name;
			item += hh->outPair("title", hh->outValue(entries[i].name),true);
			item += hh->outPair("path", hh->outValue(fullname), true);
			if (entries[i].attributes_read) {
				/* Print size of file. */
				item += hh->outPair("size", string_printf("%jd", (intmax_t) entries[i].size), false);
			}
			if(!result.empty()) {
				result += hh->outNext();
			}
			result += hh->outArrayItem("item", item, false);
			item = "";
		}
	}
	if(subdirs)
	{
		for (size_t i = 0; i < entries.size(); i++) {
			if (entries[i].kind == coreapi::FileKind::Dir && entries[i].name[0] != '.') {
				fullname = path + entries[i].name;
				// Nothing below the directory the caller named can refuse
				// the answer: one unreadable subdirectory has always been
				// walked past, and the ones beside it still listed.
				result = readMovies(hh, fullname, result, subdirs, NULL);
			}
		}
	}
	return result;
}

//NI
//-----------------------------------------------------------------------------
void CControlAPI::InfoIconsCGI(CyhookHandler *hh)
{
	std::string result = "unknown";

	if (hh->ParamList.empty() || hh->ParamList["1"] == "status")
	{
		if (g_settings.mode_icons)
		{
			if (g_settings.mode_icons_skin == INFOICONS_STATIC)
				result = "static";
			else if (g_settings.mode_icons_skin == INFOICONS_POPUP)
				result = "popup";
		}
		else
		{
			if (g_settings.mode_icons_skin == INFOICONS_INFOVIEWER)
				result = "infoviewer";
			else
				result = "off";
		}
		return hh->WriteLn(result);
	}

	int remember_mode_icons = g_settings.mode_icons;

	if (hh->ParamList["1"] == "static")
	{
		g_settings.mode_icons=1;
		g_settings.mode_icons_skin=INFOICONS_STATIC;
	}
	else if (hh->ParamList["1"] == "popup")
	{
		g_settings.mode_icons=1;
		g_settings.mode_icons_skin=INFOICONS_POPUP;
	}
	else if (hh->ParamList["1"] == "infoviewer")
	{
		g_settings.mode_icons=0;
		g_settings.mode_icons_skin=INFOICONS_INFOVIEWER;
	}
	else if (hh->ParamList["1"] == "off")
	{
		g_settings.mode_icons=0;
		if (g_settings.mode_icons_skin == INFOICONS_INFOVIEWER)
			g_settings.mode_icons_skin=INFOICONS_STATIC;
	}
	else
	{
		hh->SendError();
		return;
	}

	CConfigFile *Config = new CConfigFile(',');
	Config->loadConfig(NEUTRINO_SETTINGS_FILE);
	Config->setInt32("mode_icons", g_settings.mode_icons);
	Config->setInt32("mode_icons_skin", g_settings.mode_icons_skin);
	Config->saveConfig(NEUTRINO_SETTINGS_FILE);
	delete Config;

	if (g_settings.mode_icons != remember_mode_icons)
	{
		if (remember_mode_icons)
			CInfoIcons::getInstance()->enableInfoIcons(false);
		else
			CInfoIcons::getInstance()->enableInfoIcons(true);
	}

	hh->SendOk();
}

//NI
//-------------------------------------------------------------------------
/** Display update info
 *
 * @param hh CyhookHandler
 *
 * @par nhttpd-usage
 * @code
 * /control/updateinfo[?format=plain|json|xml]
 * @endcode
 *
 */
//-----------------------------------------------------------------------------
void CControlAPI::UpdateInfoCGI(CyhookHandler *hh)
{
	hh->outStart();
	std::string result = "";

	CFlashUpdate update;
	if (update.checkOnlineVersion())
		result = hh->outPair("available", "yes", false);
	else
		result = hh->outPair("available", "no", false);

	result = hh->outObject("update", result);
	hh->SendResult(result);
}
