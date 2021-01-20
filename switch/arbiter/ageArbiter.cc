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

#include "ageArbiter.h"
#include "../switchModule.h"
#include "../../flit/flitModule.h"

ageArbiter::ageArbiter(PortType type, int portNumber, unsigned short cos, int numPorts, switchModule *switchM) :
		arbiter(type, portNumber, cos, numPorts, switchM) {
	this->ageList = new float[ports];
	for (int i = 0; i < ports; i++)
		this->ageList[i] = -1;
}

ageArbiter::~ageArbiter() {
	delete[] ageList;
}

/* Under Age Arbitration, when the port list is
 * started to be traversed to determine the oldest
 * packet that can be granted access to the resource,
 * the list is first reordered. This allows to
 * have an up-to-date list without checking all the
 * packets every time the function is called. In order
 * for this function to operate properly, list traversal
 * must always start by the head of list (which makes
 * sense anyway).
 */
int ageArbiter::getServingPort(int offset) {
	assert(offset >= 0 && offset < ports);
	if (offset == 0) {
		this->reorderPortList();
		this->arbiter::getServingPort(offset);
	}
	return portList[offset];
}

void ageArbiter::reorderPortList() {
	int i, aux, limit = ports, nextlimit;

	/* First, find out if there are any new packets to be accounted.
	 * If it is an input port, it checks the information for all the
	 * buffers in that input, and it only needs to update those vcs
	 * for which there was not a recorded age in the list.
	 * For output ports is a bit trickier. Since assigned vc can have
	 * changed, it needs to update all the age list.  */
	flitModule *flit;
	switch (type) {
		case IN:
			for (i = 0; i < ports; i++) {
				if (ageList[i] == -1) {
					switchM->inPorts[this->label]->checkFlit(cos, portList[i], flit);
					if (flit != NULL) ageList[i] = flit->inCycle;
				}
			}
			break;
		case OUT:
			for (i = 0; i < ports; i++) {
				switchM->inPorts[portList[i]]->checkFlit(switchM->outputArbiters[label]->inputCos[portList[i]],
						switchM->outputArbiters[label]->inputChannels[portList[i]], flit);
				if (flit != NULL) ageList[i] = flit->inCycle;
			}
			break;
	}

	/* Perform reordering. If follows an optimized BubbleSort algorithm. */
	while (true) {
		nextlimit = 0;
		for (i = 1; i < limit; i++) {
			if (ageList[i - 1] > ageList[i]) {
				aux = ageList[i - 1];
				ageList[i - 1] = ageList[i];
				ageList[i] = aux;
				aux = portList[i - 1];
				portList[i - 1] = portList[i];
				portList[i] = aux;
				nextlimit = i;
			}
		}
		limit = nextlimit;
		if (nextlimit == 0) break;
	}
}

/* This function updates the age information for the
 * input ports. Since the other reorder list
 * function already performs the update for those
 * ports which do not hold a packet at the head of
 * their input, this only sets the age to -1.
 */
void ageArbiter::markServedPort(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	/* Output ports need to update all the ports in the list
	 * for every list traversal, so we skip the update. */
	if (type == OUT) return;
	for (int i = 0; i < ports; i++) {
		if (portList[i] == servedPort) {
			ageList[i] = -1;
			break;
		}
	}
	this->arbiter::markServedPort(servedPort);
}

