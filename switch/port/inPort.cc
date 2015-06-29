/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
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

#include "inPort.h"
#include "../switchModule.h"

inPort::inPort(int numVCs, int portNumber, int bufferNumber, int bufferCapacity, float delay, switchModule * sw,
		int reservedBufferCapacity) :
		port(numVCs, portNumber, sw), bufferedPort(numVCs, bufferNumber, bufferCapacity, delay, reservedBufferCapacity) {
}

inPort::~inPort() {
}

/*
 * Remove flit from port buffer. Every time a flit is extracted from
 * a buffer, we need to update occupancy status for local and shared
 * buffers statistics.
 */
bool inPort::extract(int vc, flitModule* &flitExtracted, float length) {
	assert(vc >= 0 && vc < this->port::numVCs);
	m_sw->messagesInQueuesCounter -= 1; /*First we update sw track stats */
	return vcBuffers[vc]->extract(flitExtracted, length);
}

/*
 * Every time a flit is inserted into a buffer, we need
 * to update occupancy status for local and shared buffers
 * statistics.
 */
void inPort::insert(int vc, flitModule* flit, float txLength) {
	assert(vc >= 0 && vc < this->port::numVCs);
	m_sw->messagesInQueuesCounter += 1; /*First we update sw track stats */
	vcBuffers[vc]->insert(flit, txLength);
}
