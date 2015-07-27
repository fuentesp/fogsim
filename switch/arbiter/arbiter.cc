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

#include "arbiter.h"

arbiter::arbiter(switchModule *switchM) {
	this->switchM = switchM;
}

arbiter::~arbiter() {
	delete[] LRSPortList;
}

int arbiter::getLRSPort(int offset) {
	assert(offset < ports);
	return LRSPortList[offset];
}

void arbiter::reorderLRSPortList(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	for (int i = 0; i < ports; i++) {
		if (LRSPortList[i] == servedPort) {
			for (int j = i; j < (ports - 1); j++) {
				LRSPortList[j] = LRSPortList[j + 1];
			}
			LRSPortList[ports - 1] = servedPort;
		}
	}
}

/*
 * Arbitration function: iterates through all arbiter inputs
 * and returns attended port (if any, -1 otherwise).
 */
int arbiter::action() {
	int input_port, port, attendedPort = -1;
	bool port_served;

	/* If resource can not be granted, return -1 */
	if (!this->checkPort()) return -1;

	for (port = 0; port < this->ports; port++) {
		/* Check port within LRS order */
		input_port = this->getLRSPort(port);

		port_served = this->attendPetition(input_port);
		if (port_served) {
			attendedPort = input_port;
			port = this->ports;
			/* Reorder port list */
			this->reorderLRSPortList(input_port);

			this->updateStatistics(input_port);
		}
	}
	return attendedPort;
}
