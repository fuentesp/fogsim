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

#include "rrArbiter.h"

rrArbiter::rrArbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM) 
	: arbiter(type, portNumber, cos, numPorts, switchM) {
	int startPort = rand();

	/* Begin with a random port and increase in order */
	this->portList = new int[ports];
	this->qcnList = new short[ports];
	for (int i = 0; i < ports; i++) {
		this->portList[i] = (i + startPort) % ports;
		this->qcnList[i] = -1;
	}
}

rrArbiter::~rrArbiter() {
}

void rrArbiter::reorderPortList(int servedPort) {
	for (int i = 0; i < ports; i++)
		portList[i] = (servedPort + 1 + i) % ports;
}

void rrArbiter::markServedPort(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	this->reorderPortList(servedPort);
}
