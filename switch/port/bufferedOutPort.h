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

#ifndef BUFFERED_OUTPUT_PORT_H_
#define BUFFERED_OUTPUT_PORT_H_

#include "outPort.h"
#include "bufferedPort.h"

using namespace std;

class bufferedOutPort: public outPort, public bufferedPort {
private:
	const int cosOutPort = 0;
protected:
	int ***outCredits;
	int ***outMinCredits;
	int numConsumePetitions;
	int totalMaxOutCredits;

public:
	int ***maxOutCredits;
	int numberSegregatedFlows;

	bufferedOutPort(unsigned short cosLevels, int numVCs, int portNumber, int bufferNumber, int bufferCapacity,
			float delay, switchModule * sw, int reservedBufferCapacity = 0, int numberSegregatedFlows = 1);
	~bufferedOutPort();
	bool extract(unsigned short cos, int vc, flitModule* &flitExtracted, float length, int buffer = 0);
	void insert(int vc, flitModule *flit, float txLength, int buffer = 0);
	int getTotalOccupancy(unsigned short cos, int vc, int buffer = 0);
	int getMinOccupancy(unsigned short cos, int vc);
	void setMaxOutOccupancy(unsigned short cos, int vc, int phits);
	int getNumPetitions();
	int getSpace(int vc, int buffer = 0);
	void checkFlit(unsigned short cos, int vc, flitModule* &nextFlit, int buffer = 0, int offset = 0);
	int getBufferOccupancy(int vc, int buffer = 0);
	bool canSendFlit(unsigned short cos, int vc, int buffer = 0);
	bool canReceiveFlit(int vc);
	void reorderBuffer(int vc);

};

#endif /* BUFFERED_PORT_H_ */
