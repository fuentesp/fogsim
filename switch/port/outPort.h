/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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
	int **occupancyCredits;
	int **minOccupancyCredits;
public:
	int **maxCredits;

	outPort(unsigned short cosLevels, int numVCs, int portNumber, switchModule * sw);
	virtual ~outPort() override;
	virtual void insert(int vc, flitModule *flit, float txLength, int buffer = 0);
	virtual int getOccupancy(unsigned short cos, int vc);
	virtual int getTotalOccupancy(unsigned short cos, int vc, int buffer = 0);
	void setMaxOccupancy(unsigned short cos, int vc, int phits);
	int getMaxOccupancy(unsigned short cos, int vc);
	virtual int getMinOccupancy(unsigned short cos, int vc);
	void increaseOccupancy(unsigned short cos, int vc, int phits);
	void decreaseOccupancy(unsigned short cos, int vc, int phits);
	void decreaseMinOccupancy(unsigned short cos, int vc, int phits);

	/* Added for bufferedOutPort compatibility */
	virtual void setMaxOutOccupancy(unsigned short cos, int vc, int phits) {
	}
	virtual int getSpace(int vc, int buffer = 0) {
	}
	virtual void checkFlit(unsigned short cos, int vc, flitModule* &nextFlit, int buffer = 0, int offset = 0) {
	}
	virtual int getBufferOccupancy(int vc, int buffer = 0) {
	}
	virtual bool canSendFlit(unsigned short cos, int vc, int buffer = 0) {
	}
	virtual bool canReceiveFlit(int vc) {
	}
	virtual void reorderBuffer(int vc) {
	}
	virtual int getNumPetitions() {
	}
};

#endif /* OUT_PORT_H_ */
