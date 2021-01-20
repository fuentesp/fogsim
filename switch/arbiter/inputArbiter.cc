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

#include "inputArbiter.h"
#include "../switchModule.h"
#include "../../flit/flitModule.h"

using namespace std;

inputArbiter::inputArbiter(int inPortNumber, unsigned short cosLevels, switchModule *switchM, ArbiterType policy) {
	this->switchM = switchM;
	this->label = inPortNumber;
	this->cosLevels = cosLevels;
	this->cosArbiters = new cosArbiter*[cosLevels];
	this->curCos = -1;
	for (int i = 0; i < cosLevels; i++)
		this->cosArbiters[i] = new cosArbiter(inPortNumber, i, switchM, policy);
}

inputArbiter::~inputArbiter() {
	for (unsigned short i = 0; i < this->cosLevels; i++)
		delete cosArbiters[i];
	delete[] cosArbiters;
}

/*
 * Increase petition statistics.
 */
void inputArbiter::updateStatistics(int port) {
	g_petitions++;
	if (this->label < g_p_computing_nodes_per_router) g_injection_petitions++;
}

/*
 * Checks if port is ready to transmit. For a local arbiter,
 * this means that allocator speed-up is not exceeded.
 * SpeedUp in the allocator is the equivalent for number
 * of ports to the xbar for each router ingress.
 */
bool inputArbiter::checkPort() {
	int xbarPortsInUse = 0, port = this->label;
	for (unsigned short cos = 0; cos < this->cosLevels; cos++)
		for (int vc = 0; vc < g_channels; vc++)
			if (switchM->inPorts[port]->isBufferSending(cos, vc)) xbarPortsInUse++;

	if (xbarPortsInUse < g_local_arbiter_speedup)
		return true;
	else
		return false;
}

unsigned short inputArbiter::getCurCos() {
	return this->curCos;
}

/*
 * Arbitration function: iterates through all arbiter inputs
 * and returns attended port (if any, -1 otherwise).
 */
int inputArbiter::action() {
	int attendedPort = -1;
	/* If resource can not be granted, return -1 */
	if (!this->checkPort()) return -1;

	for (short cos = cosLevels - 1; cos >= 0; cos--) {
		attendedPort = cosArbiters[cos]->action();
		if (attendedPort != -1) {
			curCos = cos;
			this->updateStatistics(attendedPort);
			break;
		}
	}
	return attendedPort;
}
