/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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

bufferedOutPort::bufferedOutPort(unsigned short cosLevels, int numVCs, int portNumber, int bufferNumber,
		int bufferCapacity, float delay, switchModule * sw, int reservedBufferCapacity, int numberSegregatedFlows) :
		outPort(cosLevels, numVCs, portNumber, sw), bufferedPort(1, numberSegregatedFlows, bufferNumber, bufferCapacity,
				delay, reservedBufferCapacity) {
	/* When using FLEXIBLE vcs, the # of VCs is updated to reflect the new range of vcs
	 * used (local & global vcs can not share a link to be more easily differentiated) */
	if (g_vc_usage == FLEXIBLE) numVCs = g_local_link_channels + g_global_link_channels;
	this->numberSegregatedFlows = numberSegregatedFlows;
	outCredits = new int**[cosLevels];
	outMinCredits = new int**[cosLevels];
	maxOutCredits = new int**[cosLevels];
	for (int cos = 0; cos < cosLevels; cos++) {
		outCredits[cos] = new int*[numVCs];
		outMinCredits[cos] = new int*[numVCs];
		maxOutCredits[cos] = new int*[numVCs];
		for (int vc = 0; vc < numVCs; vc++) {
			outCredits[cos][vc] = new int[numberSegregatedFlows];
			outMinCredits[cos][vc] = new int[numberSegregatedFlows];
			maxOutCredits[cos][vc] = new int[numberSegregatedFlows];
			for (int flow = 0; flow < numberSegregatedFlows; flow++) {
				outCredits[cos][vc][flow] = 0;
				outMinCredits[cos][vc][flow] = 0;
				maxOutCredits[cos][vc][flow] = 0;
			}
		}
	}
	numConsumePetitions = 0;
	totalMaxOutCredits = 0;
}

bufferedOutPort::~bufferedOutPort() {
	for (int cos = 0; cos < this->port::cosLevels; cos++) {
		for (int vc = 0; vc < this->port::numVCs; vc++) {
			delete[] outCredits[cos][vc];
			delete[] outMinCredits[cos][vc];
			delete[] maxOutCredits[cos][vc];
		}
		delete[] outCredits[cos];
		delete[] outMinCredits[cos];
		delete[] maxOutCredits[cos];
	}
	delete[] outCredits;
	delete[] outMinCredits;
	delete[] maxOutCredits;
}

/*
 * Remove flit from port buffer. Every time a flit is extracted from
 * a buffer, we need to update occupancy status for local and shared
 * buffers statistics.
 */
bool bufferedOutPort::extract(unsigned short cos, int vc, flitModule* &flitExtracted, float length, int buffer) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->outPort::numVCs);
	assert(length == g_flit_size);
	assert(buffer >= 0 && buffer < this->numberSegregatedFlows);
	m_sw->messagesInQueuesCounter -= 1; /*First we update sw track stats */
	outCredits[cos][vc][buffer] -= length;
	assert(outCredits[cos][vc][buffer] >= 0);
	if (!flitExtracted->getMisrouted()) {
		outMinCredits[cos][vc][buffer] -= length;
		assert(outMinCredits[cos][vc][buffer] >= 0);
	} else
		flitExtracted->setMisrouted(true); /* Update misroute status */
	if (label >= g_p_computing_nodes_per_router) {
		occupancyCredits[cos][vc] += length;
		assert(occupancyCredits[cos][vc] <= maxCredits[cos][vc]);
		if (!flitExtracted->getMisrouted()) minOccupancyCredits[cos][vc] += length;
		assert(minOccupancyCredits[cos][vc] <= occupancyCredits[cos][vc]);
	}
	bool aux = vcBuffers[cos][buffer]->extract(flitExtracted, length);
	if (flitExtracted->flitType == PETITION) numConsumePetitions--; /* # of petitions for reactive traffic */
	assert(buffer == 0 || flitExtracted->flitType == RESPONSE);
	return aux;
}

/*
 * Every time a flit is inserted into a buffer, we need
 * to update occupancy status for local and shared buffers
 * statistics.
 */
void bufferedOutPort::insert(int vc, flitModule* flit, float txLength, int buffer) {
	assert(flit->cos >= 0 && flit->cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->outPort::numVCs);
	assert(buffer >= 0 && buffer < this->numberSegregatedFlows);
	assert(buffer == 0 || flit->flitType == RESPONSE);
	m_sw->messagesInQueuesCounter += 1; /*First we update sw track stats */
	if (flit->flitType == PETITION) numConsumePetitions++; /*# of petitions for reactive traffic*/
	outCredits[flit->cos][vc][buffer] += g_flit_size; //TODO: this should not be done with global var value, should rx. somehow instead (i.e., through flit "length" field)
	if (outCredits[flit->cos][vc][buffer] > maxOutCredits[flit->cos][vc][buffer])
		cerr << "problem with port " << this->label << " cos " << flit->cos << " vc " << vc << ", outCredits: "
				<< outCredits[flit->cos][vc] << " max: " << maxOutCredits[flit->cos][vc][buffer] << endl;
	assert(outCredits[flit->cos][vc][buffer] <= maxOutCredits[flit->cos][vc][buffer]);
	if (not flit->getMisrouted() && flit->getCurrentMisrouteType() == NONE) {
		outMinCredits[flit->cos][vc][buffer] += g_flit_size;
		assert(outMinCredits[flit->cos][vc][buffer] <= outCredits[flit->cos][vc][buffer]);
	}
	assert(vcBuffers[0][buffer]->getSpace() >= txLength);
	vcBuffers[0][buffer]->insert(flit, txLength);
}

/* Returns occupancy status based on credits for an output
 * port; considering both next input buffer and current
 * output buffer */
int bufferedOutPort::getTotalOccupancy(unsigned short cos, int vc, int buffer) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(this->label >= g_p_computing_nodes_per_router); /* Sanity assert, since this function is not updated to segregated traffic flows */
	assert(vc >= 0 && vc < this->outPort::numVCs);
	assert(buffer >= 0 && buffer < this->numberSegregatedFlows);
	return occupancyCredits[cos][vc] + outCredits[cos][vc][buffer];
}

int bufferedOutPort::getMinOccupancy(unsigned short cos, int vc) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(this->label >= g_p_computing_nodes_per_router); /* Sanity assert, since this function is not updated to segregated traffic flows */
	assert(vc >= 0 && vc < this->outPort::numVCs);
	return minOccupancyCredits[cos][vc] + outMinCredits[cos][vc][0];
}

/* Updates maximum occupancy registers for output buffer.
 * Should only be called upon switch initialization phase */
void bufferedOutPort::setMaxOutOccupancy(unsigned short cos, int vc, int phits) {
	assert(cos >= 0 && cos < this->port::cosLevels);
	assert(vc >= 0 && vc < this->outPort::numVCs);
	for (int buffer = 0; buffer < this->numberSegregatedFlows; buffer++) {
		maxOutCredits[cos][vc][buffer] = phits;
	}
	totalMaxOutCredits = 0;
	for (unsigned short cos = 0; cos < this->port::cosLevels; cos++)
		for (int vc = 0; vc < this->port::numVCs; vc++)
			totalMaxOutCredits += maxOutCredits[cos][vc][0];
}

/* Returns the number of petitions hold currently in the
 * output buffer, to handle reactive traffic properly */
int bufferedOutPort::getNumPetitions() {
	assert(numConsumePetitions >= 0 && numConsumePetitions <= totalMaxOutCredits);
	return numConsumePetitions;
}

int bufferedOutPort::getSpace(int vc, int buffer) {
	assert(buffer < numberSegregatedFlows);
	return this->bufferedPort::getSpace(0, buffer);
}

void bufferedOutPort::checkFlit(unsigned short cos, int vc, flitModule* &nextFlit, int buffer, int offset) {
	assert(buffer < numberSegregatedFlows);
	this->bufferedPort::checkFlit(cos, buffer, nextFlit, offset);
}

int bufferedOutPort::getBufferOccupancy(int vc, int buffer) {
	assert(buffer < numberSegregatedFlows);
	return this->bufferedPort::getBufferOccupancy(0, buffer);
}

bool bufferedOutPort::canSendFlit(unsigned short cos, int vc, int buffer) {
	assert(buffer < numberSegregatedFlows);
	return this->bufferedPort::canSendFlit(cos, buffer);
}

bool bufferedOutPort::canReceiveFlit(int vc) {
	bool can_receive_flit = true;
	for (int i = 0; i < this->numberSegregatedFlows; i++) {
		can_receive_flit &= this->bufferedPort::canReceiveFlit(0, i);
	}
	return can_receive_flit;
}

void bufferedOutPort::reorderBuffer(int vc) {
	for (int buffer = 0; buffer < this->numberSegregatedFlows; buffer++) {
		this->bufferedPort::reorderBuffer(0, buffer);
	}
}
