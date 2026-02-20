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

VoiceManager::VoiceManager(CMemorySystem* pMemorySystem)
    : CMultiCoreSupport(pMemorySystem)
{

	for (unsigned nCore = 0; nCore < CORES; nCore++) {
		m_CoreStatus[nCore] = CoreStatusInit;
	}
}

VoiceManager::~VoiceManager(void)
{
}

boolean VoiceManager::Initialize(void)
{
	if (!CMultiCoreSupport::Initialize()) {
		return FALSE;
	}

	// wait for secondary cores to be ready
	for (unsigned nCore = 1; nCore < CORES; nCore++) {
		while (m_CoreStatus[nCore] != CoreStatusIdle) {
			// just wait
		}
	}

	return TRUE;
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

		// TODO do some work, in parallel

		// indicate thread is done
		m_CoreStatus[nCore] = CoreStatusIdle;
	}
	// CLogger::Get()->Write("VOICEMAN", LogNotice, "core %u called", nCore);
}
