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

#include "priorityRrArbiter.h"
#include "../switchModule.h"

priorityRrArbiter::priorityRrArbiter(PortType type, int portNumber, unsigned short cos, int numPorts,
		int portPriorOffset, switchModule *switchM) :
		rrArbiter(type, portNumber, cos, numPorts, switchM) {
	this->portPriorOffset = portPriorOffset;

	/* Instead of storing ports in-order, store last those
	 * ports with less priority (typically injection ports). */
	int i, j;
	for (i = 0; i < ports; i++) {
		j = (i + portPriorOffset) % ports;
		this->portList[i] = j;
	}
}

priorityRrArbiter::~priorityRrArbiter() {
}

/*
 * Reorders port list when one port has been attended.
 * This function is redefined after RR arbiter class to
 * preserve transit over injection ports preference.
 */
void priorityRrArbiter::reorderPortList(int servedPort) {
	assert(servedPort >= 0 && servedPort < ports);
	if (servedPort < portPriorOffset)
		/* Served port did not have priority (was injection port) */
		for (int i = 0; i < portPriorOffset; i++)
			portList[i + ports - portPriorOffset] = (servedPort + 1 + i) % portPriorOffset;
	else
		/* Served port had priority; reorder priority ports */
		for (int i = 0; i < ports - portPriorOffset; i++)
			portList[i] = (servedPort - portPriorOffset + 1 + i) % (ports - portPriorOffset) + portPriorOffset;
}
