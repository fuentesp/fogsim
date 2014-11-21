/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2014 University of Cantabria

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef class_buffer
#define class_buffer

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "global.h"

using namespace std;
class flitModule;

class buffer {
protected:
private:
	int bufferNumber;
	int *bufferEntryCycle;
	flitModule **bufferContent;
	int busyUntil;
	int delay; // Switch parameter
	int head; //first valid message
	int tail; //First empty space
	int lastExtractCycle; // Port parameter
	switchModule * m_sw;

public:
	int m_portLock; //buffer lock by packet in port m_portLock
	int m_vcLock; //buffer lock by packet in vc m_vcLock
	long long m_pktLock; //buffer lock by packet m_pktLock
	bool m_unLocked; //buffer unlocked
	int m_outPort_currentPkt; // next port for the current packet being sent
	int m_nextVC_currentPkt; // next vc for the current packet being sent
	long long m_currentPkt; // current packet being sent
	int lastReceiveCycle; // Port parameter
	bool escapeBuffer; //Escape subnetwork buffer
	buffer(int bufferNumber, int bufferCapacity, int delay, switchModule * sw);
	~buffer();

	int bufferCapacity; /* number of FlitS the buffer can store*/
	bool extract(flitModule* &flitExtracted);
	void checkFlit(flitModule* &nextFlit);
	void insert(flitModule *flit);
	int getSpace();
	bool unLocked();
	int getBufferOccupancy();
	bool emptyBuffer();
	bool canSendFlit();
	bool canReceiveFlit();
	void reorderBuffer();
	int getDelay() const;
};

#endif
