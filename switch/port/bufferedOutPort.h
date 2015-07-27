/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
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

#ifndef BUFFERED_OUTPUT_PORT_H_
#define BUFFERED_OUTPUT_PORT_H_

#include "outPort.h"
#include "bufferedPort.h"

using namespace std;

class bufferedOutPort: public outPort, public bufferedPort {
protected:
	int *outCredits;

public:
	int *maxOutCredits;

	bufferedOutPort(int numVCs, int portNumber, int bufferNumber, int bufferCapacity, float delay, switchModule * sw,
			int reservedBufferCapacity = 0);
	~bufferedOutPort();
	bool extract(int vc, flitModule* &flitExtracted, float length);
	void insert(int vc, flitModule *flit, float txLength);
	int getTotalOccupancy(int vc);
	void setMaxOutOccupancy(int vc, int phits);
	int getMaxOutOccupancy(int vc);

	inline int getSpace(int vc) {
		return this->bufferedPort::getSpace(vc);
	}
	inline void checkFlit(int vc, flitModule* &nextFlit) {
		this->bufferedPort::checkFlit(vc, nextFlit);
	}
	inline int getBufferOccupancy(int vc) {
		return this->bufferedPort::getBufferOccupancy(vc);
	}
	inline bool canSendFlit(int vc) {
		return this->bufferedPort::canSendFlit(vc);
	}
	inline bool canReceiveFlit(int vc) {
		return this->bufferedPort::canReceiveFlit(vc);
	}
	inline void reorderBuffer(int vc) {
		this->bufferedPort::reorderBuffer(vc);
	}
};

#endif /* BUFFERED_PORT_H_ */
