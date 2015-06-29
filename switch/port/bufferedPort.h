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

#ifndef BUFFERED_PORT_H_
#define BUFFERED_PORT_H_

#include "port.h"
#include "../buffer/buffer.h"

using namespace std;

class bufferedPort {
protected:
	int numVCs;
	buffer **vcBuffers;
	int reservedBufferCapacity; /* Per each VC buffer */
	int aggregatedBufferCapacity; /* number of FLITS all the associated buffers can store altogether (free shared slots) */
public:
	bufferedPort(int numVCs, int bufferNumber, int bufferCapacity, float delay, int reservedBufferCapacity = 0);
	~bufferedPort();
	virtual int getSpace(int vc);
	void checkFlit(int vc, flitModule* &nextFlit);
	bool unLocked(int vc);
	int getBufferOccupancy(int vc);
	bool emptyBuffer(int vc);
	bool canSendFlit(int vc);
	bool canReceiveFlit(int vc);
	bool isBufferSending(int vc);
	void reorderBuffer(int vc);
	float getDelay(int vc) const;
	float getHeadEntryCycle(int vc);
	void setCurPkt(int vc, int id);
	int getCurPkt(int vc);
	void setOutCurPkt(int vc, int port);
	int getOutCurPkt(int vc);
	void setNextVcCurPkt(int vc, int nextVc);
	int getNextVcCurPkt(int vc);
	int getBufferCapacity(int vc);
	void setPktLock(int vc, int id);
	int getPktLock(int vc);
	void setPortPktLock(int vc, int port);
	void setVcPktLock(int vc, int prevVc);
	void setUnlocked(int vc, int unlocked);
};

#endif /* BUFFERED_PORT_H_ */
