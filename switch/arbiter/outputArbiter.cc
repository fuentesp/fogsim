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

#include "outputArbiter.h"
#include "../switchModule.h"
#include "../ioqSwitchModule.h"

using namespace std;

outputArbiter::outputArbiter(int outPortNumber, unsigned short cosLevels, int ports, switchModule *switchM,
		ArbiterType policy) {
	int i, j;
	this->switchM = switchM;
	this->label = outPortNumber;
	this->petitions = new int[ports];
	this->nextPorts = new int[ports];
	this->nextChannels = new int[ports];
	this->inputChannels = new int[ports];
	this->inputCos = new unsigned short[ports];
	for (int i = 0; i < ports; i++) {
		petitions[i] = 0;
		inputChannels[i] = 0;
		inputCos[i] = 0;
		nextPorts[i] = 0;
		nextChannels[i] = 0;
	}
	switch (policy) {
		case LRS:
			this->arbProtocol = new lrsArbiter(OUT, outPortNumber, 0, ports, switchM);
			break;
		case PrioLRS:
			this->arbProtocol = new priorityLrsArbiter(OUT, outPortNumber, 0, ports, g_local_router_links_offset,
					switchM);
			break;
		case RR:
			this->arbProtocol = new rrArbiter(OUT, outPortNumber, 0, ports, switchM);
			break;
		case PrioRR:
			this->arbProtocol = new priorityRrArbiter(OUT, outPortNumber, 0, ports, g_local_router_links_offset,
					switchM);
			break;
		case AGE:
			this->arbProtocol = new ageArbiter(OUT, outPortNumber, 0, ports, switchM);
			break;
		case PrioAGE:
			this->arbProtocol = new priorityAgeArbiter(OUT, outPortNumber, 0, ports, g_local_router_links_offset,
					switchM);
			break;
		default:
			cerr << "ERROR: undefined output arbiter policy." << endl;
			exit(-1);
	}
}

outputArbiter::~outputArbiter() {
	delete[] petitions;
	delete[] nextPorts;
	delete[] nextChannels;
	delete[] inputChannels;
	delete[] inputCos;
	delete arbProtocol;
}

void outputArbiter::updateStatistics(int port) {
	g_served_petitions++;
	if (port < g_p_computing_nodes_per_router) {
		g_served_injection_petitions++;
	}
	this->petitions[port] = 0;
}

bool outputArbiter::attendPetition(int port) {
	int output_port, nextP;
	bool port_served = false;
	output_port = this->label;

	if (this->petitions[port] == 1) {
		nextP = switchM->routing->neighPort[output_port];
#if DEBUG
		if (nextP != this->nextPorts[port])
		cerr << "output_port=" << output_port << "NextP=" << nextP << ",  nextPorts="
		<< this->nextPorts[port] << endl;
#endif
		assert(nextP == this->nextPorts[port]);

		port_served = true;
	}
	return port_served;
}

bool outputArbiter::checkPort() {
	bool canAttendFlit = false;
	int port = this->label, nextP = switchM->routing->neighPort[port];
	/* If port is still rx. a previous packet, it can not attend any requests */
	if (port < g_p_computing_nodes_per_router)
		canAttendFlit = g_generators_list[switchM->label * g_p_computing_nodes_per_router + port]->checkConsume(NULL);
	else {
		canAttendFlit = switchM->nextPortCanReceiveFlit(port);
		if (!canAttendFlit) switchM->increasePortContentionCount(port);
	}
	return canAttendFlit;
}

void outputArbiter::initPetitions() {
	for (int p = 0; p < arbProtocol->ports; p++) {
		this->petitions[p] = 0;
		this->inputChannels[p] = 0;
		this->inputCos[p] = 0;
		this->nextPorts[p] = 0;
		this->nextChannels[p] = 0;
	}
}

/*
 * Arbitration function: iterates through all arbiter inputs
 * and returns attended port (if any, -1 otherwise).
 */
int outputArbiter::action() {
	int input_port, port, attendedPort = -1;
	bool port_served;

	/* If resource can not be granted, return -1 */
	if (!this->checkPort()) return -1;

	for (port = 0; port < arbProtocol->ports; port++) {
		input_port = arbProtocol->getServingPort(port);

		port_served = this->attendPetition(input_port);
		if (port_served) {
			attendedPort = input_port;
			port = arbProtocol->ports;
			/* Reorder port list */
			arbProtocol->markServedPort(input_port);

			this->updateStatistics(input_port);
		}
	}
	return attendedPort;
}
