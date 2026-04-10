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

#include <hardware/ca.h>

#include <dvbsi++/program_map_section.h>
#include <dvbsi++/ca_program_map_section.h>

#ifdef HAVE_SOFTCSA
#include "dvbapi_client.h"
#include <driver/softcsa/softcsa_manager.h>

/* CDvbApiClient singleton, initialized from zapit.cpp */
static CDvbApiClient *dvbapi_client = NULL;

void CCamManager_SetDvbApiClient(CDvbApiClient *client)
{
	dvbapi_client = client;
}
#endif

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

#ifdef HAVE_SOFTCSA
		/* DVBAPI protocol v3 descriptors */
		tmp[0] = 0x83; /* adapter device */
		tmp[1] = 0x01;
		tmp[2] = 0x00; /* adapter_index */
		capmt.injectDescriptor(tmp, false);

		tmp[0] = 0x86; /* demux device */
		tmp[1] = 0x01;
		tmp[2] = (source_demux >= 0) ? (uint8_t)source_demux : 0x00;
		capmt.injectDescriptor(tmp, false);

		tmp[0] = 0x87; /* CA device */
		tmp[1] = 0x01;
		tmp[2] = 0x00; /* ca_device_index */
		capmt.injectDescriptor(tmp, false);
#endif
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

#if ! HAVE_CST_HARDWARE
bool CCam::sendCaPmt(uint64_t tpid, uint8_t *rawpmt, int rawlen, uint8_t type, unsigned char scrambled, casys_map_t camap, int mode, bool enable)
{
	return cCA::GetInstance()->SendCAPMT(tpid, source_demux, camask,
			rawpmt ? cabuf : NULL, rawpmt ? calen : 0, rawpmt, rawpmt ? rawlen : 0, (CA_SLOT_TYPE) type, scrambled, camap, mode, enable);
}
#else
bool CCam::sendCaPmt(uint64_t tpid, uint8_t *rawpmt, int rawlen, uint8_t type)
{
	return cCA::GetInstance()->SendCAPMT(tpid, source_demux, camask,
			rawpmt ? cabuf : NULL, rawpmt ? calen : 0, rawpmt, rawpmt ? rawlen : 0, (CA_SLOT_TYPE) type);
}
#endif

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
	useCI = false;
	if (IS_WEBCHAN(channel_id))
		return false;

	CCam * cam;
	int oldmask, newmask;
	int demux = DEMUX_SOURCE_0;
	int source = DEMUX_SOURCE_0;

	CZapitChannel * channel = CServiceManager::getInstance()->FindChannel(channel_id);

	OpenThreads::ScopedLock<OpenThreads::Mutex> m_lock(mutex);

	cammap_iterator_t it = channel_map.find(channel_id);
	if (it != channel_map.end()) {
		cam = it->second;
	} else if(start) {
		cam = new CCam();
		channel_map.insert(std::pair<t_channel_id, CCam*>(channel_id, cam));
	} else {
		return false;
	}
	if (channel == NULL) {
		printf("CCamManager: channel %" PRIx64 " not found\n", channel_id);
		StopCam(channel_id, cam);
		return false;
	}
	//INFO("channel %llx [%s] mode %d %s update %d", channel_id, channel->getName().c_str(), mode, start ? "START" : "STOP", force_update);

	/* FIXME until proper demux management */
#if ! HAVE_CST_HARDWARE && ! HAVE_GENERIC_HARDWARE
	CFrontend *frontend = CFEManager::getInstance()->getFrontend(channel);
#endif
	switch (mode) {
		case PLAY:
#if HAVE_CST_HARDWARE
			source = DEMUX_SOURCE_0;
			demux = LIVE_DEMUX;
#else
			source = cDemux::GetSource(0);
			demux = cDemux::GetSource(0);
//			INFO("PLAY: fe_num %d dmx_src %d", frontend ? frontend->getNumber() : -1, cDemux::GetSource(0));
#endif
			INFO("PLAY: fe_num %d dmx_src %d", source, demux);
			break;
		case STREAM:
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
//			INFO("STREAM(%d): fe_num %d stream_dmx %d", mode, frontend ? frontend->getNumber() : -1, channel->getStreamDemux());
#ifdef DYNAMIC_DEMUX
			source = channel->getStreamDemux();
			demux = channel->getStreamDemux();
#else
			if(frontend)
				source = frontend->getNumber();
			demux = source;
#endif // DYNAMIC_DEMUX
#else // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			source = channel->getStreamDemux();
			demux = channel->getStreamDemux();
#endif
			INFO("STREAM(%d): fe_num %d stream_dmx %d", mode, source, demux);
			break;
		case RECORD:
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
//			INFO("RECORD(%d): fe_num %d rec_dmx %d", mode, frontend ? frontend->getNumber() : -1, channel->getRecordDemux());
#ifdef DYNAMIC_DEMUX
			source = channel->getRecordDemux();
			demux = channel->getRecordDemux();
#else
			if (frontend)
				source = frontend->getNumber();
			demux = source;
#endif // DYNAMIC_DEMUX
#else // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			source = channel->getRecordDemux();
			demux = channel->getRecordDemux();
#endif
			INFO("RECORD(%d): fe_num %d rec_dmx %d", mode, source, demux);
			break;
		case PIP:
#if ENABLE_PIP
#if HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
#ifdef DYNAMIC_DEMUX
			source = channel->getPipDemux();
			demux = channel->getPipDemux();
#else
			if (frontend)
			{
				if (frontend->sameTsidOnid(channel->getTransponderId()))
					source = frontend->getNumber();
				else
					source = frontend->getNumber() + 1;
			}
			demux = source;
#endif // DYNAMIC_DEMUX
#else
			source = channel->getRecordDemux();
			demux = channel->getPipDemux();
#endif // HAVE_ARM_HARDWARE || HAVE_MIPS_HARDWARE
			INFO("PIP: fe_num %d dmx_src %d", source, demux);
#endif // ENABLE_PIP
			break;
	}

	oldmask = cam->getCaMask();
	if (force_update)
		newmask = oldmask;
	else
		newmask = cam->makeMask(demux, start);

	if (cam->getSource() > 0)
		source = cam->getSource();

	INFO("channel %" PRIx64 " [%s] mode %d %s src %d mask %d -> %d update %d rmode %d mp %d", channel_id, channel->getName().c_str(),
			mode, start ? "START" : "STOP", source, oldmask, newmask, force_update, rmode, mp);

	//INFO("source %d old mask %d new mask %d force update %s", source, oldmask, newmask, force_update ? "yes" : "no");

#if ! HAVE_CST_HARDWARE
	/* stop decoding if record stops unless it's the live channel. TODO:PIP? */
	/* all the modes: RECORD, STREAM, PIP except PLAY now stopping here !! */
	if (mode && start == false && source != cDemux::GetSource(0)) {
		INFO("MODE not PLAY:(%d) start=false, src %d getsrc %d", mode, source, cDemux::GetSource(0));
		/* Possibly beware stopping cam in case of overlapping timers on same channel */
		if (newmask != oldmask)
		{
			cam->sendMessage(NULL, 0, false);
		}
		/* clean up channel_map with stopped record/stream/pip services NOT live-tv */
		it = channel_map.find(channel_id);
		if (it != channel_map.end() && newmask != 0 && it->second != cam)
		{
			delete it->second;
			channel_map.erase(channel_id);
		}
	}
#endif

	if((oldmask != newmask) || force_update) {
		cam->setCaMask(newmask);
		cam->setSource(source);
		if(newmask != 0 && (!filter_channels || !channel->bUseCI)) {
			INFO("\033[33m socket only\033[0m");
			/* CAPMT_ONLY makes OSCam stop every other demuxer on the
			 * same connection — would clobber a concurrent recording */
			uint8_t softcsa_list = (channel_map.size() > 1) ? CCam::CAPMT_ADD : CCam::CAPMT_ONLY;
			cam->makeCaPmt(channel, true, softcsa_list);
#ifdef HAVE_SOFTCSA
			if ((mode == PLAY || mode == RECORD || mode == PIP) && dvbapi_client && dvbapi_client->ensureConnected()) {
				uint32_t demux_index;
				if (mode == RECORD)
					demux_index = (uint32_t)channel->getRecordDemux();
				else
					demux_index = (source >= 0) ? (uint32_t)source : 0;

				/* Rebuild CA-PMT with recording demux in descriptor 0x86.
				 * makeCaPmt above used source_demux (=LIVE). For RECORD
				 * we need getRecordDemux() so OSCam sends CW to demux 1. */
				if (mode == RECORD) {
					int saved_source = cam->getSource();
					cam->setSource(demux_index);
					cam->makeCaPmt(channel, true, softcsa_list);
					cam->setSource(saved_source);
				}
				SoftCSASessionType stype = (mode == RECORD) ? SOFTCSA_SESSION_RECORD :
				                           (mode == PIP) ? SOFTCSA_SESSION_PIP : SOFTCSA_SESSION_LIVE;

				CSoftCSAManager::getInstance()->registerDemux(
					demux_index, channel->getChannelID(),
					stype, 0 /* adapter */, demux_index, source);

				if (channel->getVideoPid())
					CSoftCSAManager::getInstance()->addPid(demux_index, channel->getVideoPid());
				/* Add ALL audio PIDs so audio-language switch needs no runtime
				 * cDemux filter mutation — only routing in ts_demuxer changes. */
				for (unsigned char ai = 0; ai < channel->getAudioChannelCount(); ai++) {
					CZapitAudioChannel *ac = channel->getAudioChannel(ai);
					if (ac && ac->pid)
						CSoftCSAManager::getInstance()->addPid(demux_index, ac->pid);
				}
				if (channel->getPcrPid() && channel->getPcrPid() != channel->getVideoPid())
					CSoftCSAManager::getInstance()->addPid(demux_index, channel->getPcrPid());
				if (channel->getPmtPid())
					CSoftCSAManager::getInstance()->addPid(demux_index, channel->getPmtPid());

				CSoftCSAManager::getInstance()->setDecoderPids(demux_index,
					channel->getVideoPid(),
					channel->getAudioPid(),
					channel->getPcrPid());

				CSoftCSAManager::getInstance()->setDecoderTypes(demux_index,
					channel->type,
					channel->getAudioChannel() ? channel->getAudioChannel()->audioChannelType : 0);

				if (!dvbapi_client->sendCaPmt(cam->getBuffer(), cam->getLength(),
				                              channel->getServiceId()))
					printf("[softcsa] sendCaPmt failed for channel %llx\n",
					       (unsigned long long)channel->getChannelID());
			} else
#endif
			cam->setCaPmt(true);
#if 0
			// CI
			CaIdVector caids;
			cCA::GetInstance()->GetCAIDS(caids);
			uint8_t list = CCam::CAPMT_ONLY;
			cam->makeCaPmt(channel, false, list, caids);
			int len;
			unsigned char * buffer = channel->getRawPmt(len);
#if HAVE_CST_HARDWARE
			cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI);
#else
			cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, mode, start);
#endif
#endif
		}
	}

#if ! HAVE_CST_HARDWARE
	// CI
	if(oldmask == newmask) {
		INFO("\033[33m (oldmask == newmask)\033[0m");
#ifdef HAVE_SOFTCSA
		if (mode == RECORD && start && channel->scrambled && !channel->bUseCI
		    && dvbapi_client && dvbapi_client->ensureConnected()) {
			uint32_t rec_demux = (uint32_t)channel->getRecordDemux();
			CSoftCSAManager::getInstance()->registerDemux(
				rec_demux, channel->getChannelID(),
				SOFTCSA_SESSION_RECORD, 0 /* adapter */, rec_demux, source);

			if (channel->getVideoPid())
				CSoftCSAManager::getInstance()->addPid(rec_demux, channel->getVideoPid());
			/* Record all audio PIDs so language track choice remains flexible
			 * during playback and matches the LIVE session setup. */
			for (unsigned char ai = 0; ai < channel->getAudioChannelCount(); ai++) {
				CZapitAudioChannel *ac = channel->getAudioChannel(ai);
				if (ac && ac->pid)
					CSoftCSAManager::getInstance()->addPid(rec_demux, ac->pid);
			}
			if (channel->getPcrPid() && channel->getPcrPid() != channel->getVideoPid())
				CSoftCSAManager::getInstance()->addPid(rec_demux, channel->getPcrPid());
			if (channel->getPmtPid())
				CSoftCSAManager::getInstance()->addPid(rec_demux, channel->getPmtPid());

			CSoftCSAManager::getInstance()->setDecoderPids(rec_demux,
				channel->getVideoPid(),
				channel->getAudioPid(),
				channel->getPcrPid());

			/* no LIVE to key-copy from (e.g. record-from-standby) →
			 * subscribe RECORD demux with OSCam directly. Always ADD:
			 * an in-flight recording on another SID must not be clobbered
			 * even when this is the only entry in channel_map. */
			if (!CSoftCSAManager::getInstance()->hasRunningLiveSession(channel->getChannelID())) {
				int saved_source = cam->getSource();
				cam->setSource(rec_demux);
				cam->makeCaPmt(channel, true, CCam::CAPMT_ADD);
				cam->setSource(saved_source);
				if (!dvbapi_client->sendCaPmt(cam->getBuffer(), cam->getLength(),
				                              channel->getServiceId())) {
					printf("[softcsa] sendCaPmt (record-only) failed for channel %llx\n",
					       (unsigned long long)channel->getChannelID());
					/* roll back the RECORD state so the caller's cRecord
					 * fallback sees a clean slate */
					CSoftCSAManager::getInstance()->stopSession(
						channel->getChannelID(), SOFTCSA_SESSION_RECORD);
				}
			}
		}
#endif
		if (mode) {
			if(start) {
#if 0
				CaIdVector caids;
				cCA::GetInstance()->GetCAIDS(caids);
				uint8_t list = CCam::CAPMT_ONLY;
				cam->makeCaPmt(channel, false, list, caids);
				int len;
				unsigned char * buffer = channel->getRawPmt(len);
				cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, mode, start);
#endif
			} else {
				cam->sendCaPmt(channel->getChannelID(), NULL, 0, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, mode, start);
			}
		}
		else if (!start) {
			/* condition: STREAM or RECORD and LIVE-TV are running on the same channel
			 * now when zap to another channel, tell the CI here that former LIVE-TV has stopped */
			cam->sendCaPmt(channel->getChannelID(), NULL, 0, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, mode, start);
		}
	}
#endif

	if(newmask == 0) {
		INFO("\033[33m (newmask == 0)\033[0m");
		/* FIXME: back to live channel from playback dont parse pmt and call setCaPmt
		 * (see CMD_SB_LOCK / UNLOCK PLAYBACK */
		//channel->setRawPmt(NULL);//FIXME
#if HAVE_CST_HARDWARE
		StopCam(channel_id, cam);
#ifdef BOXMODEL_CST_HD2
		// hack for rezaping to the recording channel
		CZapitChannel * chan = CServiceManager::getInstance()->GetCurrentChannel();

		//if coming from movieplayer, disable hack
		if(!mp && ( (!mode || (mode && !chan->scrambled)) && (!start && rmode)) ){
			INFO("\033[33m HACK: disabling TS\033[0m");
			cCA::GetInstance()->SetTS(CA_DVBCI_TS_INPUT_DISABLED);
		}
#endif
#else
		mp = false;

		/* don't use StopCam() here: ci-cam needs the real mode stop */
		cam->sendCaPmt(channel->getChannelID(), NULL, 0, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, mode, start);
		cam->sendMessage(NULL, 0, false);
		channel_map.erase(channel_id);
		delete cam;
#endif
	}

	// CI
	if (mode && !start) {
		INFO("\033[33m (mode && !start) do we really need this?\033[0m");
	}

		CaIdVector caids;
		cCA::GetInstance()->GetCAIDS(caids);
		//uint8_t list = CCam::CAPMT_FIRST;
		uint8_t list = CCam::CAPMT_ONLY;
		if (channel_map.size() > 1)
			list = CCam::CAPMT_ADD;

#ifdef BOXMODEL_CST_HD2
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
			INFO("CI: not used, disabling TS");
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
#if HAVE_CST_HARDWARE
			cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_SMARTCARD);
#endif
			if (tunerno >= 0 && tunerno != cDemux::GetSource(cam->getSource())) {
				INFO("CI: configured tuner %d do not match %d, skip [%s]", tunerno, cam->getSource(), channel->getName().c_str());
			} else if (filter_channels && !channel->bUseCI) {
				INFO("CI: filter enabled, CI not used for [%s]", channel->getName().c_str());
			} else if(channel->scrambled) {
				useCI = true;
				INFO("CI: use CI for [%s]", channel->getName().c_str());
#if HAVE_CST_HARDWARE
				cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI);
#endif
			}
			//list = CCam::CAPMT_MORE;
#if ! HAVE_CST_HARDWARE
			if((oldmask != newmask) || force_update || (oldmask == newmask && mode && start))
			{
				//temp debug output
				if((oldmask != newmask) || force_update)
					INFO("\033[33m (oldmask != newmask) || force_update)\033[0m");
				else
					INFO("\033[33m (oldmask == newmask && mode && start)\033[0m");

				if(useCI)
				{
					cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI, channel->scrambled, channel->camap, 0, true);
				}
				else
				{
					cam->sendCaPmt(channel->getChannelID(), buffer, len, CA_SLOT_TYPE_CI, false /*channel->scrambled*/, channel->camap, mode, start);
				}
			}
#endif
		}

	return true;
}

void CCamManager::SetCITuner(int tuner)
{
	tunerno = tuner;
#ifdef BOXMODEL_CST_HD2
	if (tunerno >= 0)
		cCA::GetInstance()->SetTS((CA_DVBCI_TS_INPUT)tunerno);
#endif
}
