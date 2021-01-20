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

#include "dynBufOutPort.h"
#include "../../global.h"

dynamicBufferOutPort::dynamicBufferOutPort(unsigned short cosLevels, int numVCs, int portNumber, switchModule * sw,
		int reservedBufferCapacity) :
		outPort(cosLevels, numVCs, portNumber, sw) {
	this->reservedBufferCapacity = reservedBufferCapacity;
}

dynamicBufferOutPort::~dynamicBufferOutPort() {
}

/* Returns occupancy status for the next input buffer based on credits. Remember that
 * with shared buffers, occupancy must account the occupancy of other vcs in the same
 * port that are making use of aggregated buffer space (does not consider reserved
 * buffer space for vcs other than the given).
 */
int dynamicBufferOutPort::getOccupancy(unsigned short cos, int vc) {
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

/* Returns occupancy status based on credits for the output port; this considers the total
 * aggregated + reserved consumed space and the occupancy in the output port.
 */
int dynamicBufferOutPort::getTotalOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->port::numVCs);
	return getOccupancy(cos, vc);
}

