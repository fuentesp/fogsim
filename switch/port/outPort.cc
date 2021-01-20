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

#include "outPort.h"
#include "../../dgflySimulator.h"
#include "../switchModule.h"

outPort::outPort(unsigned short cosLevels, int numVCs, int portNumber, switchModule * sw) :
		port(cosLevels, numVCs, portNumber, sw) {
	/* When using FLEXIBLE vcs, the # of VCs is updated to reflect the new range of vcs
	 * used (local & global vcs can not share a link to be more easily differentiated */
	if (g_vc_usage == FLEXIBLE || g_vc_usage == TBFLEX) this->numVCs = g_local_link_channels + g_global_link_channels;
	this->occupancyCredits = new int*[this->cosLevels];
	this->minOccupancyCredits = new int*[this->cosLevels];
	this->maxCredits = new int*[this->cosLevels];
	for (int cos = 0; cos < this->cosLevels; cos++) {
		this->occupancyCredits[cos] = new int[this->numVCs];
		this->minOccupancyCredits[cos] = new int[this->numVCs];
		this->maxCredits[cos] = new int[this->numVCs];
		for (int vc = 0; vc < this->numVCs; vc++) {
			this->occupancyCredits[cos][vc] = 0;
			this->minOccupancyCredits[cos][vc] = 0;
			this->maxCredits[cos][vc] = 0;
		}
	}
}

outPort::~outPort() {
	for (int cos = 0; cos < this->cosLevels; cos++) {
		delete[] occupancyCredits[cos];
		delete[] minOccupancyCredits[cos];
		delete[] maxCredits[cos];
	}
	delete[] occupancyCredits;
	delete[] minOccupancyCredits;
	delete[] maxCredits;
}

/*
 * For a bufferless output port, if we insert a message we
 * must write the packet in the next sw input buffer. Thus,
 * we call next sw insertFlit() function and increase occupancy
 * credits, since the packet is occupying space in next sw.
 */
void outPort::insert(int vc, flitModule* flit, float txLength, int buffer) {
	assert(flit->cos >= 0 && flit->cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	/* Increase occupancy statistics */
	occupancyCredits[flit->cos][vc] += txLength;
	assert(occupancyCredits[flit->cos][vc] <= maxCredits[flit->cos][vc]);
	if (!flit->getMisrouted())
		minOccupancyCredits[flit->cos][vc] += txLength;
	else
		flit->setMisrouted(true); /* Update misroute status */
	assert(minOccupancyCredits[flit->cos][vc] <= occupancyCredits[flit->cos][vc]);

	int nextP = m_sw->routing->neighPort[label];
	m_sw->routing->neighList[label]->insertFlit(nextP, vc, flit);
}

/* Returns occupancy status of neighbor router input buffers based on credits */
int outPort::getOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	return occupancyCredits[cos][vc];
}

/* Returns occupancy status for an output based on credits; in this case,
 * behavior is equivalent to getOccupancy() */
int outPort::getTotalOccupancy(unsigned short cos, int vc, int buffer) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	return occupancyCredits[cos][vc];
}

/* Updates maximum occupancy registers. Should only
 * be called upon switch initialization phase */
void outPort::setMaxOccupancy(unsigned short cos, int vc, int phits) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	maxCredits[cos][vc] = phits;
}

/* Returns the value of maximum occupancy registers,
 * in phits. */
int outPort::getMaxOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	return maxCredits[cos][vc];
}

int outPort::getMinOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	return minOccupancyCredits[cos][vc];
}

void outPort::decreaseMinOccupancy(unsigned short cos, int vc, int phits) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	minOccupancyCredits[cos][vc] -= phits;
	assert(minOccupancyCredits[cos][vc] >= 0);
}

/* Increases occupancy registers.
 * TODO: remove increaseOccupancy function?? it may be merged
 * with insert function. The decreaseOccupancy function, on the
 * other hand, may need to stay (credits from next sw are only
 * updated when sw rx. a credit pkt). */
void outPort::increaseOccupancy(unsigned short cos, int vc, int phits) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	occupancyCredits[cos][vc] += phits;
	assert(occupancyCredits[cos][vc] <= maxCredits[cos][vc]);
}

/* Decreases occupancy registers */
void outPort::decreaseOccupancy(unsigned short cos, int vc, int phits) {
	assert(cos >= 0 && cos < this->cosLevels);
	assert(vc >= 0 && vc < this->numVCs);
	occupancyCredits[cos][vc] -= phits;
	assert(occupancyCredits[cos][vc] >= 0);
}
