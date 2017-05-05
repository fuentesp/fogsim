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

#include "arbiter.h"
#include "../switchModule.h"

using namespace std;

arbiter::arbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM) {
	this->type = type;
	this->label = portNumber;
	this->cos = cos;
	this->ports = numPorts;
	this->switchM = switchM;

	/* Default port order is to go in order */
	this->portList = new int[ports];
	for (int i = 0; i < ports; i++)
		this->portList[i] = i;
}

arbiter::~arbiter() {
	delete[] portList;
}

int arbiter::getServingPort(int offset) {
	assert(offset < ports);
	return portList[offset];
}

void arbiter::markServedPort(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
}
