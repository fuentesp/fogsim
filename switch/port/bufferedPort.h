/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2014-2021 University of Cantabria

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
	unsigned short cosLevels;
	int numVCs;
	buffer ***vcBuffers; /* 2-Dimensional array: [Cos-Level,Vc] */
	int reservedBufferCapacity; /* Per each VC buffer */
	int aggregatedBufferCapacity; /* number of FLITS all the associated buffers can store altogether (free shared slots) */
public:
	bufferedPort(unsigned short cosLevels, int numVCs, int bufferNumber, int bufferCapacity, float delay,
			int reservedBufferCapacity = 0);
	virtual ~bufferedPort();
	virtual int getSpace(unsigned short cos, int vc);
	void checkFlit(unsigned short cos, int vc, flitModule* &nextFlit, int offset = 0);
	bool unLocked(unsigned short cos, int vc);
	int getBufferOccupancy(unsigned short cos, int vc);
	bool emptyBuffer(unsigned short cos, int vc);
	bool canSendFlit(unsigned short cos, int vc);
	bool canReceiveFlit(unsigned short cos, int vc);
	bool isBufferSending(unsigned short cos, int vc);
	void reorderBuffer(unsigned short cos, int vc);
	float getDelay(unsigned short cos, int vc) const;
	float getHeadEntryCycle(unsigned short cos, int vc);
	void setCurPkt(unsigned short cos, int vc, int id);
	int getCurPkt(unsigned short cos, int vc);
	void setOutCurPkt(unsigned short cos, int vc, int port);
	int getOutCurPkt(unsigned short cos, int vc);
	void setNextVcCurPkt(unsigned short cos, int vc, int nextVc);
	int getNextVcCurPkt(unsigned short cos, int vc);
	int getBufferCapacity(unsigned short cos, int vc);
	void setPktLock(unsigned short cos, int vc, int id);
	int getPktLock(unsigned short cos, int vc);
	void setPortPktLock(unsigned short cos, int vc, int port);
	void setVcPktLock(unsigned short cos, int vc, int prevVc);
	void setUnlocked(unsigned short cos, int vc, int unlocked);
};

#endif /* BUFFERED_PORT_H_ */
