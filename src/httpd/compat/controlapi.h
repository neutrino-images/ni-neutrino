//=============================================================================
// NHTTPD
// Neutrino ControlAPI
//=============================================================================
#ifndef __nhttpd_neutrinocontrolapi_hpp__
#define __nhttpd_neutrinocontrolapi_hpp__
// C++
#include <string>
// yhttpd
#include <zapit/types.h>
#include "httpd/compat/hook.h"
// what a helper below carries a refusal out in
#include <coreapi/base/result.h>

namespace httpd
{
namespace compat
{

// forward declaration
class CNeutrinoAPI;

//-----------------------------------------------------------------------------
class CControlAPI
{
private:
	// Dispatcher Array
	typedef void (CControlAPI::*TyFunc)(CyhookHandler *hh);
	std::string func_req;
	typedef struct
	{
		const char *func_name;
		TyFunc pfunc;
		const char *mime_type;
	} TyCgiCall;
	const static TyCgiCall yCgiCallList[];

	void rc_sync(int fd);
	int rc_send(int fd, unsigned int code, unsigned int value);

	// send functions for ExecuteCGI (controld api)
	void SendEventList(CyhookHandler *hh,t_channel_id channel_id);
	void SendFoundEvents(CyhookHandler *hh, bool xml_format = false);
	void SendcurrentVAPid(CyhookHandler *hh);
	void SendAllCurrentVAPid(CyhookHandler *hh);
	void SendStreamInfo(CyhookHandler *hh);
	void SendBouquets(CyhookHandler *hh);
	void SendBouquet(CyhookHandler *hh,int BouquetNr);
	void SendChannelList(CyhookHandler *hh,  bool currentTP = false);
	void SendTimersPlain(CyhookHandler *hh);
	void SendTimers(CyhookHandler *hh);
	void epgDetailList(CyhookHandler *hh);
	void EpgSearchXMLCGI(CyhookHandler *hh);
	void EpgSearchCGI(CyhookHandler *hh);
	// subs
	friend class CNeutrinoWebserver; // for timer /fb/ compatibility
	void doModifyTimer(CyhookHandler *hh);
	void doNewTimer(CyhookHandler *hh);
	std::string _SendTime(CyhookHandler *hh, struct tm *Time, int digits);
	std::string _GetBouquetWriteItem(CyhookHandler *hh, t_channel_id channel_id, t_channel_id epg_id, const std::string &name, int bouquetNr, int channelNr);
	std::string channelEPGformated(CyhookHandler *hh, int bouquetnr, t_channel_id channel_id, int max, long stoptime);
	std::string _GetBouquetActualEPGItem(CyhookHandler *hh, t_channel_id channel_id);

	//yweb
	void YWeb_SendVideoStreamingPids(CyhookHandler *hh, int apid_no);
	void YWeb_SendRadioStreamingPid(CyhookHandler *hh);

	// CGI functions for ExecuteCGI
	void TimerCGI(CyhookHandler *hh);
	void TimerSendCGI(CyhookHandler *hh);
	void SetModeCGI(CyhookHandler *hh);
	void GetModeCGI(CyhookHandler *hh);
	void ExecCGI(CyhookHandler *hh);
	void SystemCGI(CyhookHandler *hh);
	void StandbyCGI(CyhookHandler *hh);
	void RCCGI(CyhookHandler *hh);
	void GetDateCGI(CyhookHandler *hh);
	void GetTimeCGI(CyhookHandler *hh);
	void GetServicesxmlCGI(CyhookHandler *hh);
	void GetBouquetsxmlCGI(CyhookHandler *hh);
	void GetUBouquetsxmlCGI(CyhookHandler *hh);
	void GetChannelIDCGI(CyhookHandler *hh);
	void GetChannelInfoCGI(CyhookHandler *hh);
	void GetEpgIDCGI(CyhookHandler *hh);
	void GetTPChannel_IDCGI(CyhookHandler *hh);
	void MessageCGI(CyhookHandler *hh);
	void InfoCGI(CyhookHandler *hh);
	void BoxInfoCGI(CyhookHandler *hh);
	void HWInfoCGI(CyhookHandler *hh);
	void ShutdownCGI(CyhookHandler *hh);
	void VolumeCGI(CyhookHandler *hh);
	void ChannellistCGI(CyhookHandler *hh);
	void LogolistCGI(CyhookHandler *hh);
	void GetBouquetCGI(CyhookHandler *hh);
	void GetChannelCGI(CyhookHandler *hh);
	void GetBouquetsCGI(CyhookHandler *hh);
	void EpgCGI(CyhookHandler *hh);
	void VersionCGI(CyhookHandler *hh);
	void ReloadNeutrinoSetupCGI(CyhookHandler *hh);
	void ReloadPluginsCGI(CyhookHandler *hh);
	void ReloadChannelsCGI(CyhookHandler *hh);
	void ScreenshotCGI(CyhookHandler *hh);
	void GlcdScreenshotCGI(CyhookHandler *hh);
	void ZaptoCGI(CyhookHandler *hh);
	void StartPluginCGI(CyhookHandler *hh);
	void LCDAction(CyhookHandler *hh);
	void YWebCGI(CyhookHandler *hh);
	void RebootCGI(CyhookHandler *hh);
	void RestartCGI(CyhookHandler *hh);
	void RCEmCGI(CyhookHandler *hh);
	void AspectRatioCGI(CyhookHandler *hh);
	void VideoFormatCGI(CyhookHandler *hh);
	void VideoOutputCGI(CyhookHandler *hh);
	void VCROutputCGI(CyhookHandler *hh);
	void AVInputModeCGI(CyhookHandler *hh);
	void AudioCGI(CyhookHandler *hh);
	void CryptCGI(CyhookHandler *hh);
	void setBouquetCGI(CyhookHandler *hh);
	void saveBouquetCGI(CyhookHandler *hh);
	void moveBouquetCGI(CyhookHandler *hh);
	void deleteBouquetCGI(CyhookHandler *hh);
	void addBouquetCGI(CyhookHandler *hh);
	void renameBouquetCGI(CyhookHandler *hh);
	void changeBouquetCGI(CyhookHandler *hh);
	void updateBouquetCGI(CyhookHandler *hh);
	void xmltvepgCGI(CyhookHandler *hh);
	void xmltvm3uCGI(CyhookHandler *hh);
	void xmltvlistCGI(CyhookHandler *hh);
	void build_live_url(CyhookHandler *hh);
	void build_playlist(CyhookHandler *hh);
	void logoCGI(CyhookHandler *hh);
	void ConfigCGI(CyhookHandler *hh);
	void FileCGI(CyhookHandler *hh);
	void StatfsCGI(CyhookHandler *hh);
	void SignalInfoCGI(CyhookHandler *hh);
	void getDirCGI(CyhookHandler *hh);
	void getMoviesCGI(CyhookHandler *hh);
	// refusal carries out what a directory was turned down with, for the one
	// caller that names its directory itself and has to be told rather than
	// answered an empty list. The configured directories pass nothing here: one
	// of those that cannot be read has always been skipped, and skipping it is
	// what keeps the ones beside it readable.
	std::string readMovies(CyhookHandler *hh, std::string path, std::string result, bool subdirs,
			       coreapi::Result<void> *refusal);
	std::string getSubdirectories(CyhookHandler *hh, std::string path, std::string result);

	void InfoIconsCGI(CyhookHandler *hh); //NI
	void UpdateInfoCGI(CyhookHandler *hh); //NI

protected:
	CNeutrinoAPI	*NeutrinoAPI;

public:
	static const unsigned int PLUGIN_DIR_COUNT = 9;
	static std::string PLUGIN_DIRS[PLUGIN_DIR_COUNT];

	// constructor & deconstructor
	CControlAPI(CNeutrinoAPI *_NeutrinoAPI);

	// Used to sit above Execute, called only by the two hook methods the move
	// into compat/ dropped. Nothing inside this class calls it either, so the
	// mount point that now owns the one caller this has needs it from outside.
	void init(CyhookHandler *hh);

	void Execute(CyhookHandler *hh);

	// yCgiCallList stays private; a caller that needs the 74 names it carries
	// reads them through here rather than keeping its own copy that could drift.
	size_t endpointCount();
	const char *endpointName(size_t index);
};

} // namespace compat
} // namespace httpd

#endif /* __nhttpd_neutrinocontrolapi_hpp__ */
