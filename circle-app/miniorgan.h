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
#ifndef _miniorgan_h
#define _miniorgan_h

#include <circle/sound/pwmsoundbasedevice.h>
#define SOUND_CLASS CPWMSoundBaseDevice
#define SAMPLE_RATE 48000
#define CHUNK_SIZE 2048

#include <circle/i2cmaster.h>
#include <circle/interrupt.h>
#include <circle/serial.h>
#include <circle/types.h>
#include <circle/usb/usbmidi.h>

#include "voicemanager.h"

#define MAX_NOTES 10

struct TNoteInfo {
	char Key;
	u8 KeyNumber; // MIDI number
};

struct key;

class CMiniOrgan : public SOUND_CLASS {
    public:
	CMiniOrgan(CInterruptSystem* pInterrupt, CI2CMaster* pI2CMaster);
	~CMiniOrgan(void);

	boolean Initialize(void);

	void Run(unsigned nCore);
	void Process(boolean bPlugAndPlayUpdated);

	unsigned GetChunk(u32* pBuffer, unsigned nChunkSize);

    private:
	static void MIDIPacketHandler(unsigned nCable, u8* pPacket, unsigned nLength);

	static void KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]);

	static void USBDeviceRemovedHandler(CDevice* pDevice, void* pContext);

	VoiceManager voice_manager;

	void FillChunkBuff();

    private:
	CUSBMIDIDevice* volatile m_pMIDIDevice;

	int m_nLowLevel;
	int m_nNullLevel;
	int m_nDiffLevel;
	int m_nHighLevel;
	int m_nCurrentLevel;
	unsigned long m_nSampleCount;
	// unsigned m_nFrequency[MAX_NOTES];
	unsigned m_nPrevFrequency;
	unsigned m_nPitchBend;

	u16 chunkBuffReadIndex;
	u16 chunkBuffReadAvail;
	u32* chunkBuff;

	u8 m_ucKeyNumber[MAX_NOTES];

	boolean m_bSetVolume;
	u8 m_uchVolume;
	u8 m_modulation;
	u8 m_noise;
	u8 m_detune;

	volatile u8 islockingneeded;

	unsigned m_nRandSeed;

	struct key* keys;
	// unsigned tt; // TODO can I use uint32_t instead?

	static const float s_KeyFrequency[];
	static const TNoteInfo s_Keys[];

	static CMiniOrgan* s_pThis;
};

#endif
