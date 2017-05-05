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

#include "lrsArbiter.h"

lrsArbiter::lrsArbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM) :
		arbiter(type, portNumber, cos, numPorts, switchM) {
}

lrsArbiter::~lrsArbiter() {
}

void lrsArbiter::reorderPortList(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	for (int i = 0; i < ports; i++)
		if (portList[i] == servedPort) {
			for (int j = i; j < (ports - 1); j++)
				portList[j] = portList[j + 1];
			portList[ports - 1] = servedPort;
			break;
		}
}

void lrsArbiter::markServedPort(int servedPort) {
	this->reorderPortList(servedPort);
}

