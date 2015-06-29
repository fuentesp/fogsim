/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2015 University of Cantabria

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

#include "../../global.h"

using namespace std;
class flitModule;
class switchModule;

class buffer {
protected:
private:
	int bufferNumber;
	float *bufferEntryCycle;
	flitModule **bufferContent;
	float delay; /* Switch parameter */
	int head; /* Head of line flit */
	int tail; /* Last flit in queue */
	float lastExtractCycle;
	float txLength;

public:
	int m_portLock; /* port locked by current packet */
	int m_vcLock; /* vc locked by current packet */
	long long m_pktLock; /* id of current packet locking port/vc */
	bool m_unLocked;
	int m_outPort_currentPkt; /* next port for the current packet being sent */
	int m_nextVC_currentPkt; /* next vc for the current packet being sent */
	long long m_currentPkt; /* id of the current packet being sent */
	float lastReceiveCycle;
	bool escapeBuffer; /* Does this buffer belong to a escape subnetwork? */
	int bufferCapacity; /* number of FLITS the buffer can store */

	buffer(int bufferNumber, int bufferCapacity, float delay);
	~buffer();
	bool extract(flitModule* &flitExtracted, float length);
	void checkFlit(flitModule* &nextFlit);
	void insert(flitModule *flit, float txLength);
	int getSpace();
	bool unLocked();
	int getBufferOccupancy();
	bool emptyBuffer();
	bool canSendFlit();
	bool canReceiveFlit();
	bool isBufferSending();
	void reorderBuffer();
	float getDelay() const;
	float getHeadEntryCycle();
};

#endif
