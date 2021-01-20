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

#include "dynBufBufferedOutPort.h"

dynamicBufferBufferedOutPort::dynamicBufferBufferedOutPort(unsigned short cosLevels, int numVCs, int portNumber,
		int bufferNumber, int bufferCapacity, float delay, switchModule * sw, int reservedBufferCapacity) :
		bufferedOutPort(cosLevels, numVCs, portNumber, bufferNumber, bufferCapacity, delay, sw) {
	this->reservedBufferCapacity = reservedBufferCapacity;
}

dynamicBufferBufferedOutPort::~dynamicBufferBufferedOutPort() {
}

/* Returns occupancy status for the next input buffer based on credits. Remember that
 * with shared buffers, occupancy must account the occupancy of other vcs in the same
 * port that are making use of aggregated buffer space (does not consider reserved
 * buffer space for vcs other than the given).
 */
int dynamicBufferBufferedOutPort::getOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->port::numVCs);
	int aggregatedOccupancy = 0;
	for (int cl = 0; cl < this->port::cosLevels; cl++)
		for (int c = 0; c < this->port::numVCs; c++) {
			if (c == vc && cl == cos) continue;
			if (occupancyCredits[cl][c] > g_flit_size * reservedBufferCapacity)
				aggregatedOccupancy += occupancyCredits[cl][c] - g_flit_size * reservedBufferCapacity;
		}
	aggregatedOccupancy += occupancyCredits[cos][vc];

	return aggregatedOccupancy;
}

/* Returns occupancy status based on credits for an output
 * port; considering both next input buffer and current
 * output buffer and the use of shared buffers in next input
 * buffer. This includes those flits in the same output
 * buffer that target a different VC but will use the
 * shared space of memory.
 */
int dynamicBufferBufferedOutPort::getTotalOccupancy(unsigned short cos, int vc, int buffer) {
	assert(this->label >= g_p_computing_nodes_per_router); /* Sanity assert, since this function is not updated to segregated traffic flows */
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->outPort::numVCs);
	assert(buffer >= 0 && buffer < this->numberSegregatedFlows);
	int aggregatedOccupancy = 0;
	for (int cl = 0; cl < this->port::cosLevels; cl++)
		for (int c = 0; c < this->port::numVCs; c++) {
			if (c == vc && cl == cos) continue;
			if ((occupancyCredits[cl][c] + outCredits[cl][c][buffer]) > g_flit_size * reservedBufferCapacity)
				aggregatedOccupancy += occupancyCredits[cl][c] + outCredits[cl][c][buffer]
						- g_flit_size * reservedBufferCapacity;
		}
	aggregatedOccupancy += occupancyCredits[cos][vc] + outCredits[cos][vc][buffer];
	return aggregatedOccupancy;
}
