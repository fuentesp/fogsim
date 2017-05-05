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

#ifndef INPUT_PORT_H_
#define INPUT_PORT_H_

#include "port.h"
#include "bufferedPort.h"

using namespace std;

class inPort: public port, public bufferedPort {
public:
	inPort(unsigned short cosLevels, int numVCs, int portNumber, int bufferNumber, int bufferCapacity, float delay,
			switchModule * sw, int reservedBufferCapacity = 0);
	~inPort();
	virtual bool extract(unsigned short cos, int vc, flitModule* &flitExtracted, float length);
	virtual void insert(int vc, flitModule *flit, float txLength);
};

#endif /* BUFFERED_PORT_H_ */
