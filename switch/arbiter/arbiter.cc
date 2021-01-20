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
	this->qcnList = new short[ports];
	for (int i = 0; i < ports; i++) {
		this->portList[i] = i;
		this->qcnList[i] = -1;
	}
}

arbiter::~arbiter() {
	delete[] portList;
	delete[] qcnList;
}

void arbiter::reorderListQcn() {
	int i, qcnIdx = 0, port, newTop;

	/* First, find out if there are any new packets to be accounted.
	 * If it is an input port, it checks the information for all the
	 * buffers in that input, and it only needs to update those vcs
	 * for which there was not a recorded age in the list.
	 * For output ports is a bit trickier. Since assigned vc can have
	 * changed, it needs to update all the age list.  */
	flitModule *flit;
	switch (type) {
		case IN:
			for (i = 0; i < ports; i++) { /* i equal to vc */
				if (qcnList[i] == -1) {
					switchM->inPorts[this->label]->checkFlit(cos, i, flit);
					if (flit != NULL) qcnList[i] = flit->flitType == CNM;
				}
			}
			break;
		case OUT:
			for (i = 0; i < ports; i++) { /* i is input port */
				switchM->inPorts[portList[i]]->checkFlit(switchM->outputArbiters[label]->inputCos[portList[i]],
						switchM->outputArbiters[label]->inputChannels[portList[i]], flit);
				if (flit != NULL) qcnList[i] = flit->flitType == CNM;
			}
			break;
	}

	/* Perform reordering. */
	for (i = 0; i < ports; i++) {
		port = portList[i];
		if (qcnList[port] == 1) { /* If it uses qcn it must be at the beggining */
			newTop = portList[i];
			for (int j = i; j > qcnIdx; j--) {
				portList[j] = portList[j - 1];
			}
			portList[qcnIdx++] = newTop;
		}
	}
}

void arbiter::updateQcn(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	/* Output ports need to update all the ports in the list
	 * for every list traversal, so we skip the update. */
	if (type == OUT) return;
	qcnList[servedPort] = -1;
}

int arbiter::getServingPort(int offset) {
	assert(offset < ports);
	if (offset == 0) reorderListQcn();
	return portList[offset];
}

/* This function updates the qcn information for the
 * input ports. Since the other reorder list
 * function already performs the update for those
 * ports which do not hold a packet at the head of
 * their input, this only sets the age to -1.
 */
void arbiter::markServedPort(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	/* Output ports need to update all the ports in the list
	 * for every list traversal, so we skip the update. */
	if (type == OUT) return;
	qcnList[servedPort] = -1;
}
