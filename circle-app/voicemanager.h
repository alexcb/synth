//
// miniorgan.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2017-2024  R. Stange <rsta2@o2online.de>
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
#ifndef _voicemanager_h
#define _voicemanager_h

#include <circle/sound/pwmsoundbasedevice.h>
#define SOUND_CLASS CPWMSoundBaseDevice
#define SAMPLE_RATE 48000
#define CHUNK_SIZE 2048

#include <circle/interrupt.h>
#include <circle/multicore.h>
#include <circle/serial.h>
#include <circle/types.h>

enum TCoreStatus {
	CoreStatusInit,
	CoreStatusIdle,
	CoreStatusBusy,
	CoreStatusExit,
	CoreStatusUnknown
};

class VoiceManager : public CMultiCoreSupport {
    public:
	VoiceManager(CMemorySystem* pMemorySystem);
	~VoiceManager(void);

	boolean Initialize(struct key* keys);
	void Run(unsigned nCore);
	void ProduceOutput(unsigned long t);
	float GetOutput(int chunk_i);

    protected:
	struct key* keys;
	void produce_keys(unsigned nCore);
	void wait_for_idle_cores();
	void set_cores_busy();

	unsigned long tick;
	float* m_fOutputLevel[CORES];
	volatile TCoreStatus m_CoreStatus[CORES];
};

#endif
