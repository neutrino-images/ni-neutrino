/*
 * (C) 2002 by Andreas Oberritter <obi@tuxbox.org>,
 *             thegoodguy         <thegoodguy@berlios.de>
 *
 * Copyright (C) 2011-2012 CoolStream International Ltd
 * Copyright (C) 2012-2014 Stefan Seyfried
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <stdio.h>
/* zapit */
#include <zapit/capmt.h>
#include <zapit/settings.h> /* CAMD_UDS_NAME         */
#include <zapit/getservices.h>
#include <zapit/femanager.h>
#include <zapit/debug.h>

#include <ca_cs.h>

#include <dvbsi++/program_map_section.h>
#include <dvbsi++/ca_program_map_section.h>

#include <global.h> //NI /* to get g_settings */

//#define DEBUG_CAPMT

CCam::CCam()
{
	camask = 0;
	demuxes = new int[MAX_DMX_UNITS];
	for(unsigned i = 0; i < MAX_DMX_UNITS; i++)
		demuxes[i] = 0;
	source_demux = -1;
	calen = 0;
}

CCam::~CCam()
{
	delete []demuxes;
}

unsigned char CCam::getVersion(void) const
{
	return 0x9F;
}

const char *CCam::getSocketName(void) const
{
	return CAMD_UDS_NAME;
}

bool CCam::sendMessage(const char * const data, const size_t length, bool update)
{
	/* send_data return false without trying, if no opened connection */
	if(update) {
		if(!send_data(data, length)) {
			if (!open_connection())
				return false;
			return send_data(data, length);
		}
		return true;
	}

	close_connection();

	if(!data || !length) {
		camask = 1;
		return false;
	}
	if (!open_connection())
		return false;

	return send_data(data, length);
}

bool CCam::makeCaPmt(CZapitChannel * channel, bool add_private, uint8_t list, const CaIdVector &caids)
{
        int len;
        unsigned char * buffer = channel->getRawPmt(len);

	DBG("cam %p source %d camask %d list %02x buffer\n", this, source_demux, camask, list);

	if(!buffer)
		return false;

	ProgramMapSection pmt(buffer);
	CaProgramMapSection capmt(&pmt, list, 0x01, caids);

	if (add_private) {
		uint8_t tmp[10];
		tmp[0] = 0x84;
		tmp[1] = 0x02;
		tmp[2] = channel->getPmtPid() >> 8;
		tmp[3] = channel->getPmtPid() & 0xFF;
		capmt.injectDescriptor(tmp, false);

		tmp[0] = 0x82;
		tmp[1] = 0x02;
		tmp[2] = camask;
		tmp[3] = source_demux;
		capmt.injectDescriptor(tmp, false);

		memset(tmp, 0, sizeof(tmp));
		tmp[0] = 0x81;
		tmp[1] = 0x08;
		tmp[2] = channel->getSatellitePosition() >> 8;
		tmp[3] = channel->getSatellitePosition() & 0xFF;
		tmp[4] = channel->getFreqId() >> 8;
		tmp[5] = channel->getFreqId() & 0xFF;
		tmp[6] = channel->getTransportStreamId() >> 8;
		tmp[7] = channel->getTransportStreamId() & 0xFF;
		tmp[8] = channel->getOriginalNetworkId() >> 8;
		tmp[9] = channel->getOriginalNetworkId() & 0xFF;

		capmt.injectDescriptor(tmp, false);
	}

	calen = capmt.writeToBuffer(cabuf);
#ifdef DEBUG_CAPMT
	printf("CAPMT: ");
	for(int i = 0; i < calen; i++)
		printf("%02X ", cabuf[i]);
	printf("\n");
#endif
	return true;
}

bool CCam::setCaPmt(bool update)
{
	return sendMessage((char *)cabuf, calen, update);
}

bool CCam::sendCaPmt(uint64_t tpid, uint8_t *rawpmt, int rawlen, uint8_t type)
{
	return cCA::GetInstance()->SendCAPMT(tpid, source_demux, camask,
			rawpmt ? cabuf : NULL, rawpmt ? calen : 0, rawpmt, rawpmt ? rawlen : 0, (CA_SLOT_TYPE) type);
}

int CCam::makeMask(int demux, bool add)
{
	int mask = 0;

	if(add)
		demuxes[demux]++;
	else if(demuxes[demux] > 0)
		demuxes[demux]--;

	for(unsigned i = 0; i < MAX_DMX_UNITS; i++) {
		if(demuxes[i] > 0)
			mask |= 1 << i;
	}
	//DBG("demuxes %d:%d:%d:%d old mask %d new mask %d", demuxes[0], demuxes[1], demuxes[2], demuxes[3], camask, mask);
	return mask;
}

CCamManager * CCamManager::manager = NULL;

CCamManager::CCamManager()
{
	channel_map.clear();
	tunerno = -1;
	filter_channels = false;
	//NI
	useCI = false;
	rmode = false;
	mp = false;
}

CCamManager::~CCamManager()
{
	for(cammap_iterator_t it = channel_map.begin(); it != channel_map.end(); it++)
		delete it->second;
	channel_map.clear();
}

CCamManager * CCamManager::getInstance(void)
{
	if(manager == NULL)
		manager = new CCamManager();

	return manager;
}

void CCamManager::StopCam(t_channel_id channel_id, CCam *cam)
{
	cam->sendMessage(NULL, 0, false);
	cam->sendCaPmt(channel_id, NULL, 0, CA_SLOT_TYPE_ALL);
	channel_map.erase(channel_id);
	delete cam;
}

bool CCamManager::SetMode(t_channel_id channel_id, enum runmode mode, bool start, bool force_update)
{
	useCI = false; //NI
	if (IS_WEBTV(channel_id))
		return false;

	CCam * cam;
	int oldmask, newmask;
	int demux = DEMUX_SOURCE_0;
	int source = DEMUX_SOURCE_0;

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	cammap_iterator_t it = channel_map.find(channel_id);
	if(it != channel_map.end()) {
		cam = it->second;
	} else if(start) {
		cam = new CCam();
		channel_map.insert(std::pair<t_channel_id, CCam*>(channel_id, cam));
	} else {
		return false;
	}
	if(channel == NULL) {
		printf("CCamManager: channel %" PRIx64 " not found\n", channel_id);
		StopCam(channel_id, cam);
		return false;
	}
	//INFO("channel %llx [%s] mode %d %s update %d", channel_id, channel->getName().c_str(), mode, start ? "START" : "STOP", force_update);

	/* FIXME until proper demux management */
#if ! HAVE_COOL_HARDWARE
	CFrontend *dfe = CFEManager::getInstance()->allocateFE(channel);
	int fenum = -1;
	if (dfe)
		fenum = dfe->getNumber();
#endif
	switch(mode) {
		case PLAY:
#if HAVE_COOL_HARDWARE
			source = DEMUX_SOURCE_0;
			demux = LIVE_DEMUX;
#else
			source = cDemux::GetSource(0);
			demux = cDemux::GetSource(0);
			INFO("PLAY: fe_num %d dmx_src %d", fenum, cDemux::GetSource(0));
#endif
			break;
		case STREAM:
		case RECORD:
#if HAVE_COOL_HARDWARE
			source = channel->getRecordDemux();
			demux = channel->getRecordDemux();
#else
			source = cDemux::GetSource(channel->getRecordDemux());
			demux = source;
			INFO("RECORD/STREAM(%d): fe_num %d rec_dmx %d dmx_src %d", mode, fenum, channel->getRecordDemux(), demux);
#endif
			break;
		case PIP:
			source = channel->getRecordDemux();
			demux = channel->getPipDemux();
			break;
	}

	oldmask = cam->getCaMask();
	if(force_update)
		newmask = oldmask;
	else
		newmask = cam->makeMask(demux, start);

	if(cam->getSource() > 0)
		source = cam->getSource();

	INFO("channel %" PRIx64 " [%s] mode %d %s src %d mask %d -> %d update %d rmode %d", channel_id, channel->getName().c_str(),
			mode, start ? "START" : "STOP", source, oldmask, newmask, force_update, rmode);

	//INFO("source %d old mask %d new mask %d force update %s", source, oldmask, newmask, force_update ? "yes" : "no");

	/* stop decoding if record stops unless it's the live channel. TODO:PIP? */
	if (mode == RECORD && start == false && source != cDemux::GetSource(0)) {
		INFO("MODE!=record(%d) start=false, src %d getsrc %d", mode, source, cDemux::GetSource(0));
		cam->sendMessage(NULL, 0, false);
		cam->sendCaPmt(channel->getChannelID(), NULL, 0, CA_SLOT_TYPE_ALL);
	}

	if((oldmask != newmask) || force_update) {
		//INFO("    ##NI: (oldmask != newmask) || force_update)");
		cam->setCaMask(newmask);
		cam->setSource(source);
		if(newmask != 0 && (!filter_channels || !channel->bUseCI)) {
			cam->makeCaPmt(channel, true);
			cam->setCaPmt(true);
		}
	}

	if(newmask == 0) {
		//INFO("    ##NI: (newmask == 0)");
		/* FIXME: back to live channel from playback dont parse pmt and call setCaPmt
		 * (see CMD_SB_LOCK / UNLOCK PLAYBACK */
		//channel->setRawPmt(NULL);//FIXME
		StopCam(channel_id, cam);
#ifdef BOXMODEL_CS_HD2
		CZapitChannel * chan = CServiceManager::getInstance()->GetCurrentChannel();

		//NI - this is a hack for rezaping to the recording channe
		//if commig from movieplayer, disable hack
		if(!mp && ( (!mode || (mode && !chan->scrambled)) && (!start && rmode)) ){
			INFO("    ##NI: HACK: disabling TS");
			cCA::GetInstance()->SetTS(CA_DVBCI_TS_INPUT_DISABLED);
		}
		mp = false;
#endif
	}

//NI
#ifdef BOXMODEL_CS_HD2
	// disable if option "usable CI channel while recording" set to "all channels"
	if(mode && g_settings.ci_mode != 0 /*all channels*/) {
		if(start) {
			if ((tunerno >= 0 && tunerno == cDemux::GetSource(cam->getSource())) || (filter_channels && channel->bUseCI)) {
				rmode = true;
				//INFO("    ##NI: rmode");
			}

		} else
			rmode = false;
	}
#endif

	CaIdVector caids;
	cCA::GetInstance()->GetCAIDS(caids);
	//uint8_t list = CCam::CAPMT_FIRST;
	uint8_t list = CCam::CAPMT_ONLY;
	if (channel_map.size() > 1)
		list = CCam::CAPMT_ADD;

#ifdef BOXMODEL_CS_HD2
	//INFO("    ##NI: channel_map.size() %d",channel_map.size());
	int ci_use_count = 0;
	for (it = channel_map.begin(); it != channel_map.end(); ++it)
	{
		cam = it->second;
		channel = CServiceManager::getInstance()->FindChannel(it->first);

		if (tunerno >= 0 && tunerno == cDemux::GetSource(cam->getSource())) {
			cCA::GetInstance()->SetTS((CA_DVBCI_TS_INPUT)tunerno);
			ci_use_count++;
			break;
		} else if (filter_channels) {
			if (channel && channel->bUseCI)
				ci_use_count++;
		} else
			ci_use_count++;
	}
	if (ci_use_count == 0) {
		INFO("CI: not used for [%s], disabling TS",channel->getName().c_str());
		cCA::GetInstance()->SetTS(CA_DVBCI_TS_INPUT_DISABLED);
	}
#endif

	for (it = channel_map.begin(); it != channel_map.end(); /*++it*/)
	{
		cam = it->second;
		channel = CServiceManager::getInstance()->FindChannel(it->first);
		++it;
		if(!channel)
			continue;

#if 0
		if (it == channel_map.end())
			list |= CCam::CAPMT_LAST; // FIRST->ONLY or MORE->LAST
#endif

		cam->makeCaPmt(channel, false, list, caids);
		int len;
		unsigned char * buffer = channel->getRawPmt(len);
		cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_SMARTCARD);

		if (tunerno >= 0 && tunerno != cDemux::GetSource(cam->getSource())) {
			INFO("CI: configured tuner %d do not match %d, skip [%s]", tunerno, cam->getSource(), channel->getName().c_str());
		} else if (filter_channels && !channel->bUseCI) {
			INFO("CI: filter enabled, CI not used for [%s]", channel->getName().c_str());
		} else {
			useCI = true; //NI
			INFO("CI: use CI for [%s]", channel->getName().c_str());
			cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI);
		}
		//list = CCam::CAPMT_MORE;
	}

	return true;
}

void CCamManager::SetCITuner(int tuner)
{
	tunerno = tuner;
#ifdef BOXMODEL_CS_HD2
	if (tunerno >= 0)
		cCA::GetInstance()->SetTS((CA_DVBCI_TS_INPUT)tunerno);
#endif
}
