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

#include "bufferedOutPort.h"
#include "../switchModule.h"

bufferedOutPort::bufferedOutPort(int numVCs, int portNumber, int bufferNumber, int bufferCapacity, float delay,
		switchModule * sw, int reservedBufferCapacity) :
		outPort(numVCs, portNumber, sw), bufferedPort(1, bufferNumber, bufferCapacity, delay, reservedBufferCapacity) {
	outCredits = new int[numVCs];
	maxOutCredits = new int[numVCs];
	for (int i = 0; i < numVCs; i++) {
		outCredits[i] = 0;
		maxOutCredits[i] = 0;
	}
}

bufferedOutPort::~bufferedOutPort() {
	delete[] outCredits;
}

/*
 * Remove flit from port buffer. Every time a flit is extracted from
 * a buffer, we need to update occupancy status for local and shared
 * buffers statistics.
 */
bool bufferedOutPort::extract(int vc, flitModule* &flitExtracted, float length) {
	assert(vc >= 0 && vc < this->outPort::numVCs);
	m_sw->messagesInQueuesCounter -= 1; /*First we update sw track stats */
	outCredits[vc] -= length;
	assert(length == g_flit_size);
	assert(outCredits[vc] >= 0);
	if (label >= g_p_computing_nodes_per_router) {
		occupancyCredits[vc] += length;
		assert(occupancyCredits[vc] <= maxCredits[vc]);
	}
	return vcBuffers[0]->extract(flitExtracted, length);
}

/*
 * Every time a flit is inserted into a buffer, we need
 * to update occupancy status for local and shared buffers
 * statistics.
 */
void bufferedOutPort::insert(int vc, flitModule* flit, float txLength) {
	assert(vc >= 0 && vc < this->outPort::numVCs);
	m_sw->messagesInQueuesCounter += 1; /*First we update sw track stats */
	outCredits[vc] += g_flit_size;
	if (outCredits[vc] > maxOutCredits[vc])
		cerr << "problem with port " << this->label << " vc " << vc << ", outCredits: " << outCredits[vc] << " max: "
				<< maxOutCredits[vc] << endl;
	assert(outCredits[vc] <= maxOutCredits[vc]);
	vcBuffers[0]->insert(flit, txLength);
}

/* Returns occupancy status based on credits for an output
 * port; considering both next input buffer and current
 * output buffer*/
int bufferedOutPort::getTotalOccupancy(int vc) {
	assert(vc >= 0 && vc < this->outPort::numVCs);
	return occupancyCredits[vc] + outCredits[vc];
}

/* Updates maximum occupancy registers for output buffer.
 * Should only be called upon switch initialization phase */
void bufferedOutPort::setMaxOutOccupancy(int vc, int phits) {
	assert(vc >= 0 && vc < this->outPort::numVCs);
	maxOutCredits[vc] = phits;
}

/* Returns the value of maximum allowed occupancy for a given
 * vc in the output buffer, in phits. */
int bufferedOutPort::getMaxOutOccupancy(int vc) {
	assert(vc >= 0 && vc < this->outPort::numVCs);
	return maxOutCredits[vc];
}
