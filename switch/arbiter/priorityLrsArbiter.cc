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

#include "priorityLrsArbiter.h"

priorityLrsArbiter::priorityLrsArbiter(PortType type, int portNumber, unsigned short cos, int numPorts,
		int portPriorOffset, switchModule *switchM) :
		lrsArbiter(type, portNumber, cos, numPorts, switchM) {
	assert(portPriorOffset <= g_p_computing_nodes_per_router);
	/* QCN is incompatible with assign more priority to injection ports, the qcn port is considered a transit port. */
	this->portPriorOffset = portPriorOffset;

	/* Instead of storing ports in-order, store first those
	 * ports with less priority (typically injection ports). */
	int i, j;
	for (i = 0; i < ports; i++) {
		j = (i + portPriorOffset) % ports;
		this->portList[i] = j;
	}
}

priorityLrsArbiter::~priorityLrsArbiter() {
}

/*
 * Reorders port list when one port has been attended.
 * This function is redefined after arbiter class to
 * preserve transit over injection ports preference.
 */
void priorityLrsArbiter::reorderPortList(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	int i, j, reorder_range_init = 0, reorder_range_end = ports;
	if (servedPort < portPriorOffset)
		/* Injection port */
		reorder_range_init = ports - portPriorOffset;
	else
		/* Transit port */
		reorder_range_end = ports - portPriorOffset;

	for (i = reorder_range_init; i < reorder_range_end; i++) {
		if (portList[i] == servedPort) {
			for (j = i; j < (reorder_range_end - 1); j++) {
				portList[j] = portList[j + 1];
			}
			portList[reorder_range_end - 1] = servedPort;
			break;
		}
	}
}
