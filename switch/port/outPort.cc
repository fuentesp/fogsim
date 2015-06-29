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

#include "outPort.h"
#include "../../dgflySimulator.h"
#include "../switchModule.h"

outPort::outPort(int numVCs, int portNumber, switchModule * sw) :
		port(numVCs, portNumber, sw) {
	this->occupancyCredits = new int[this->numVCs];
	this->maxCredits = new int[this->numVCs];
	for (int i = 0; i < numVCs; i++) {
		this->occupancyCredits[i] = 0;
		this->maxCredits[i] = 0;
	}
}

outPort::~outPort() {
	delete[] occupancyCredits;
	delete[] maxCredits;
}

/*
 * For a bufferless output port, if we insert a message we
 * must write the packet in the next sw input buffer. Thus,
 * we call next sw insertFlit() function and increase occupancy
 * credits, since the packet is occupying space in next sw.
 */
void outPort::insert(int vc, flitModule* flit, float txLength) {
	assert(vc >= 0 && vc < this->numVCs);
	/* Increase occupancy statistics */
	occupancyCredits[vc] += txLength;
	assert(occupancyCredits[vc] <= maxCredits[vc]);

	int nextP = m_sw->routing->neighPort[label];
	m_sw->routing->neighList[label]->insertFlit(nextP, vc, flit);
}

/* Returns occupancy status of neighbor router input buffers based on credits */
int outPort::getOccupancy(int vc) {
	assert(vc >= 0 && vc < this->numVCs);
	return occupancyCredits[vc];
}

/* Returns occupancy status for an output based on credits; in this case,
 * behavior is equivalent to getOccupancy() */
int outPort::getTotalOccupancy(int vc) {
	assert(vc >= 0 && vc < this->numVCs);
	return occupancyCredits[vc];
}

/* Updates maximum occupancy registers. Should only
 * be called upon switch initialization phase */
void outPort::setMaxOccupancy(int vc, int phits) {
	assert(vc >= 0 && vc < this->numVCs);
	maxCredits[vc] = phits;
}

/* Returns the value of maximum occupancy registers,
 * in phits. */
int outPort::getMaxOccupancy(int vc) {
	assert(vc >= 0 && vc < this->numVCs);
	return maxCredits[vc];
}

/* Increases occupancy registers */
void outPort::increaseOccupancy(int vc, int phits) {
	assert(vc >= 0 && vc < this->numVCs);
	occupancyCredits[vc] += phits;
	assert(occupancyCredits[vc] <= maxCredits[vc]);
}

/* Decreases occupancy registers */
void outPort::decreaseOccupancy(int vc, int phits) {
	assert(vc >= 0 && vc < this->numVCs);
	occupancyCredits[vc] -= phits;
	assert(occupancyCredits[vc] >= 0);
}
