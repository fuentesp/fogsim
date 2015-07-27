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

#ifndef OUT_PORT_H_
#define OUT_PORT_H_

#include "port.h"

using namespace std;

class outPort: public port {
protected:
	int *occupancyCredits;
public:
	int *maxCredits;

	outPort(int numVCs, int portNumber, switchModule * sw);
	~outPort();
	virtual void insert(int vc, flitModule *flit, float txLength);
	virtual int getOccupancy(int vc);
	virtual int getTotalOccupancy(int vc);
	void setMaxOccupancy(int vc, int phits);
	int getMaxOccupancy(int vc);
	void increaseOccupancy(int vc, int phits);
	void decreaseOccupancy(int vc, int phits);

	/* Added for bufferedOutPort compatibility */
	virtual void setMaxOutOccupancy(int vc, int phits) {
	}
	virtual int getSpace(int vc) {
	}
	virtual void checkFlit(int vc, flitModule* &nextFlit) {
	}
	virtual int getBufferOccupancy(int vc) {
	}
	virtual bool canSendFlit(int vc) {
	}
	virtual bool canReceiveFlit(int vc) {
	}
	virtual void reorderBuffer(int vc) {
	}
};

#endif /* OUT_PORT_H_ */
