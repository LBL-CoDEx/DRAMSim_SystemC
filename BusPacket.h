/****************************************************************************
*	 DRAMSim2: A Cycle Accurate DRAM simulator 
*	 
*	 Copyright (C) 2010   	Elliott Cooper-Balis
*									Paul Rosenfeld 
*									Bruce Jacob
*									University of Maryland
*
*	 This program is free software: you can redistribute it and/or modify
*	 it under the terms of the GNU General Public License as published by
*	 the Free Software Foundation, either version 3 of the License, or
*	 (at your option) any later version.
*
*	 This program is distributed in the hope that it will be useful,
*	 but WITHOUT ANY WARRANTY; without even the implied warranty of
*	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	 GNU General Public License for more details.
*
*	 You should have received a copy of the GNU General Public License
*	 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*****************************************************************************/






#ifndef BUSPACKET_H
#define BUSPACKET_H
//BusPacket.h
//
//Header file for bus packet object
//

#include "SystemConfiguration.h"

namespace DRAMSim
{
enum BusPacketType
{
	READ,
	READ_P,
	WRITE,
	WRITE_P,
	ACTIVATE,
	PRECHARGE,
	REFRESH,
	DATA
};


#if (USE_SCIC_MEMSYS_QUERY == 1)
#ifndef NULL_SIG64
#define NULL_SIG64			((1 << 64) - 1)
#endif
#endif


class BusPacket
{
public:
	//Fields
	BusPacketType busPacketType;
	uint column;
	uint row;
	uint bank;
	uint rank;
	uint64_t physicalAddress;
	void *data;
	uint64_t transID;

#if (USE_SCIC_MEMSYS_QUERY == 1)
	uint64_t _timeTracker;
	BusPacket(BusPacketType packtype, uint64_t physicalAddr, uint col, uint rw, uint r, uint b, void *dat, uint64_t txID, uint64_t timeTracker = NULL_SIG64);
#else
	//Functions
	BusPacket(BusPacketType packtype, uint64_t physicalAddr, uint col, uint rw, uint r, uint b, void *dat, uint64_t txID);
#endif
	BusPacket();

	void print();
	void print(uint64_t currentClockCycle, bool dataStart);
	static void printData(const void *data);
};
}

#endif

