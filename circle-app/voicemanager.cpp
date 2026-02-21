//
// miniorgan.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2023  R. Stange <rsta2@o2online.de>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

#ifndef ARM_ALLOW_MULTI_CORE
#error "ARM_ALLOW_MULTI_CORE was not defined; should be under circle/Config.mk"
#endif

#include "voicemanager.h"
#include <assert.h>
#include <circle/devicenameservice.h>
#include <circle/logger.h>
#include <circle/multicore.h>
#include <circle/sysconfig.h>
#include <circle/util.h>

#include <circle/atomic.h>
#include <circle/string.h>

#include "../common/synth.h"

#if MAX_KEYS % CORES != 0
#error "MAX_KEYS % CORES != 0"
#endif

VoiceManager::VoiceManager(CMemorySystem* pMemorySystem)
    : CMultiCoreSupport(pMemorySystem)
{

	for (unsigned nCore = 0; nCore < CORES; nCore++) {
		m_CoreStatus[nCore] = CoreStatusInit;

		m_fOutputLevel[nCore] = static_cast<float*>(::operator new(CHUNK_SIZE * sizeof(float)));
	}
}

VoiceManager::~VoiceManager(void)
{
}

boolean VoiceManager::Initialize(struct key* keys)
{
	this->keys = keys;
	if (!CMultiCoreSupport::Initialize()) {
		return FALSE;
	}

	wait_for_idle_cores();

	return TRUE;
}

void VoiceManager::wait_for_idle_cores()
{
	for (unsigned nCore = 1; nCore < CORES; nCore++) {
		while (m_CoreStatus[nCore] != CoreStatusIdle) {
			// just wait
		}
	}
}

void VoiceManager::set_cores_busy()
{
	for (unsigned nCore = 1; nCore < CORES; nCore++) {
		m_CoreStatus[nCore] = CoreStatusBusy;
	}
}

void VoiceManager::Run(unsigned nCore)
{
	assert(1 <= nCore && nCore < CORES);
	while (1) {

		m_CoreStatus[nCore] = CoreStatusIdle; // ready to be kicked
		while (m_CoreStatus[nCore] == CoreStatusIdle) {
			// just wait
		}
		assert(m_CoreStatus[nCore] == CoreStatusBusy);

		produce_keys(nCore);

		// indicate thread is done
		m_CoreStatus[nCore] = CoreStatusIdle;
	}
	// CLogger::Get()->Write("VOICEMAN", LogNotice, "core %u called", nCore);
}

// float VoiceManager::Process()
//{
//		float output = 0.0f;
//		for (int i = 0; i < MAX_KEYS; i++) {
//			struct key* k = &keys[i];
//			bool done = true;
//			for (int j = 0; j < NUM_OSCS * NUM_OSC_TYPES; j++) {
//				struct osc* osc = &k->oscs[j];
//				osc_set_output(k, osc, t);
//				if (osc->osc_type == OSC_TYPE_VFO) {
//					output += osc->output * osc->output_volume * osc->output_volume_m;
//					if (osc->output_volume > 0.0 || k->released_at == 0.0) {
//						done = false;
//					}
//				}
//			}
//			if (done) {
//				k->pressed_at = 0.f;
//				k->released_at = 0.f;
//				k->freq = 0.f;
//			}
//		}
// }

void VoiceManager::produce_keys(unsigned nCore)
{
	DataSyncBarrier();
	unsigned long tick = this->tick;

	const int start = nCore * (MAX_KEYS / CORES);
	const int end = (nCore + 1) * (MAX_KEYS / CORES);

	for (int chunk_i = 0; chunk_i < CHUNK_SIZE; chunk_i++) {
		float t = ((float)tick) / SAMPLE_RATE;
		tick++;
		float output = 0.0f;
		for (int i = start; i < end; i++) {
			struct key* k = &keys[i];
			bool done = true;
			for (int j = 0; j < NUM_OSCS * NUM_OSC_TYPES; j++) {
				struct osc* osc = &k->oscs[j];
				osc_set_output(k, osc, t);
				if (osc->osc_type == OSC_TYPE_VFO) {
					// if( osc->output > 0.0f ) {
					//	CLogger::Get()->Write("VOICEMAN", LogNotice, "t=%f core=%u freq=%f index=%u output=%f", t, nCore, k->freq, i, osc->output);
					// }
					output += osc->output * osc->output_volume * osc->output_volume_m;
					if (osc->output_volume > 0.0 || k->released_at == 0.0) {
						done = false;
					}
				}
			}
			if (done) {
				// if (k->pressed_at > 0.f) {
				//	CLogger::Get()->Write("VOICEMAN", LogNotice, "t=%f core=%u freq=%f index=%u is done", t, nCore, k->freq, i);
				// }
				k->pressed_at = 0.f;
				k->released_at = 0.f;
				k->freq = 0.f;
			}
		}
		m_fOutputLevel[nCore][chunk_i] = output;
	}
	DataSyncBarrier();
}

void VoiceManager::ProduceOutput(unsigned long t)
{
	tick = t;
	set_cores_busy();
	produce_keys(0);
	wait_for_idle_cores();
}

float VoiceManager::GetOutput(int chunk_i)
{
	float output = m_fOutputLevel[0][chunk_i];
	for (unsigned nCore = 1; nCore < CORES; nCore++) {
		output += m_fOutputLevel[nCore][chunk_i];
	}
	return output;
}
