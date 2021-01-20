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

#include "dynBufInPort.h"
#include "../switchModule.h"

dynamicBufferInPort::dynamicBufferInPort(unsigned short cosLevels, int numVCs, int portNumber, int bufferNumber,
		int bufferCapacity, float delay, switchModule * sw, int reservedBufferCapacity) :
		inPort(cosLevels, numVCs, portNumber, bufferNumber, bufferCapacity, delay, sw, reservedBufferCapacity) {
	int bufferSize = (this->aggregatedBufferCapacity + this->reservedBufferCapacity) * g_flit_size;
	for (int cos = 0; cos < cosLevels; cos++)
		for (int vc = 0; vc < numVCs; vc++) {
			assert(this->vcBuffers[cos][vc] != NULL);
			delete this->vcBuffers[cos][vc];
			this->vcBuffers[cos][vc] = new buffer(bufferNumber + cos * numVCs + vc, bufferSize, delay);
		}
}

dynamicBufferInPort::~dynamicBufferInPort() {
}

/* Every time a flit is extracted from a buffer, we need
 * to update occupancy status for local and shared buffers
 * statistics.
 */
bool dynamicBufferInPort::extract(unsigned short cos, int vc, flitModule* &flitExtracted, float length) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	m_sw->messagesInQueuesCounter -= 1; /*First we update sw track stats */
	/* If packet was hosted in shared pool between vcs, we need to
	 * update the aggregated buffer capacity statistics for all vcs.
	 * Otherwise, it was hosted in the reserved part of the buffer,
	 * and does not affect any statistics.
	 */
	if (vcBuffers[cos][vc]->getBufferOccupancy() > g_flit_size * reservedBufferCapacity) {
		aggregatedBufferCapacity += g_packet_size / g_flit_size;
		assert(aggregatedBufferCapacity <= (vcBuffers[cos][vc]->bufferCapacity - reservedBufferCapacity));
	}
	return vcBuffers[cos][vc]->extract(flitExtracted, length);
}

/* Every time a flit is inserted into a buffer, we need
 * to update occupancy status for local and shared buffers
 * statistics.
 */
void dynamicBufferInPort::insert(int vc, flitModule* flit, float txLength) {
	assert(flit->cos >= 0 && flit->cos < this->port::cosLevels);
	m_sw->messagesInQueuesCounter += 1; /*First we update sw track stats */
	/* If packet is inserted into shared part of the buffer, we need
	 * to update the aggregated buffer capacity statistics for all
	 * the vcs hosted in the buffer. Otherwise, it will be hosted in
	 * the reserved vc memory of the buffer and will thus not affect
	 * any shared statistics.
	 */
	if ((vcBuffers[flit->cos][vc]->getBufferOccupancy() + g_packet_size) > g_flit_size * reservedBufferCapacity) {
		aggregatedBufferCapacity -= g_packet_size / g_flit_size;
		assert(aggregatedBufferCapacity >= 0);
	}
	vcBuffers[flit->cos][vc]->insert(flit, txLength);
}

/* Returns number of free slots in the buffer (in PHITS). For shared buffers, this
 * number is the addition of free reserved space for the channel, plus total free shared space
 * in the buffer.
 */
int dynamicBufferInPort::getSpace(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0);
	if (vc >= this->port::numVCs) return 0; /* Little hack to avoid errors when checking inexistent buffers */
	if (vcBuffers[cos][vc]->getBufferOccupancy() < g_flit_size * reservedBufferCapacity)
		return (g_flit_size * (aggregatedBufferCapacity + reservedBufferCapacity)
				- vcBuffers[cos][vc]->getBufferOccupancy());
	else
		return (g_flit_size * aggregatedBufferCapacity);
}

