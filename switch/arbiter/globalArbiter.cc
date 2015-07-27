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

#include "globalArbiter.h"
#include "../switchModule.h"
#include "../ioqSwitchModule.h"

using namespace std;

globalArbiter::globalArbiter(int outPortNumber, int ports, switchModule *switchM) :
		arbiter(switchM) {
	int i, j;
	this->ports = ports;
	this->label = outPortNumber;
	this->LRSPortList = new int[ports];
	for (i = 0; i < ports; i++) {
		/* Instead of storing ports in-order, give priority to
		 * transit over injection ports. */
		j = (i + g_local_router_links_offset) % ports;
		LRSPortList[i] = j;
	}

	this->petitions = new int[ports];
	this->nextPorts = new int[ports];
	this->nextChannels = new int[ports];
	this->inputChannels = new int[ports];
	for (int i = 0; i < ports; i++) {
		petitions[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		inputChannels[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		nextPorts[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		nextChannels[i] = 0;
	}
}

globalArbiter::~globalArbiter() {
	delete[] petitions;
	delete[] nextPorts;
	delete[] nextChannels;
	delete[] inputChannels;
}

void globalArbiter::updateStatistics(int port) {
	g_served_petitions++;
	if (port < g_p_computing_nodes_per_router) {
		g_served_injection_petitions++;
	}
	this->petitions[port] = 0;
}

bool globalArbiter::attendPetition(int port) {
	int output_port, nextP;
	bool port_served = false;
	output_port = this->label;

	if (this->petitions[port] == 1) {
		nextP = switchM->routing->neighPort[output_port];
#if DEBUG
		if (nextP != this->nextPorts[port])
		cout << "output_port=" << output_port << "NextP=" << nextP << ",  nextPorts="
		<< this->nextPorts[port] << endl;
#endif
		assert(nextP == this->nextPorts[port]);

		port_served = true;
	}
	return port_served;
}

bool globalArbiter::checkPort() {
	bool canAttendFlit = false;
	int port = this->label, nextP = switchM->routing->neighPort[port];
	/* If port is still rx. a previous packet, it can not attend any requests */
	canAttendFlit = switchM->nextPortCanReceiveFlit(port);
	if (port >= g_p_computing_nodes_per_router && !canAttendFlit) switchM->increasePortContentionCount(port);
	return canAttendFlit;
}

void globalArbiter::initPetitions() {
	for (int p = 0; p < this->ports; p++) {
		this->petitions[p] = 0;
		this->inputChannels[p] = 0;
		this->nextPorts[p] = 0;
		this->nextChannels[p] = 0;
	}
}
