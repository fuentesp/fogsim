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

#include "priorityAgeArbiter.h"
#include "../switchModule.h"

priorityAgeArbiter::priorityAgeArbiter(PortType type, int portNumber, unsigned short cos, int numPorts,
		int portPriorOffset, switchModule *switchM) :
		ageArbiter(type, portNumber, cos, numPorts, switchM) {
	assert(portPriorOffset <= g_p_computing_nodes_per_router);
	/* QCN is incompatible with assign more priority to injection ports, the qcn port is considered a transit port. */
	this->portPriorOffset = portPriorOffset;

	/* In priority age arbiter, we need to alter initial ageArbiter order */
	int i, j;
	for (i = 0; i < ports; i++) {
		j = (i + portPriorOffset) % ports;
		this->portList[i] = j;
		this->ageList[i] = -1;
	}
}

priorityAgeArbiter::~priorityAgeArbiter() {
}

void priorityAgeArbiter::reorderPortList() {
	int i, aux, limit, nextlimit;

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
				switchM->inPorts[portList[i]]->checkFlit(cos,
						switchM->outputArbiters[label]->inputChannels[portList[i]], flit);
				if (flit != NULL) ageList[i] = flit->inCycle;
			}
			break;
	}

	/* Perform reordering. If follows an optimized BubbleSort algorithm,
	 * respecting the order priority between transit and injection ports.
	 * Ordering is performed in two phases, one for transit ports, and
	 * another for injection. */
	for (int j = 0; j < 2; j++) {
		limit = (j == 0) ? ports - portPriorOffset : ports;
		i = (j == 0) ? 1 : 1 + ports - portPriorOffset;
		while (true) {
			nextlimit = 0;
			for (; i < limit; i++) {
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

	for (i = ports - 1; i >= ports - portPriorOffset; i--) {
		assert(this->portList[i] < portPriorOffset);
	}
	for (i = ports - portPriorOffset - 1; i >= 0; i--) {
		assert(this->portList[i] >= portPriorOffset);
	}
}
