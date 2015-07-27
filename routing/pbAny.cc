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

#include "pbAny.h"

pbAny::pbAny(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
}

pbAny::~pbAny() {

}

candidate pbAny::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int destination, valNode, minOutP, valOutP;

	/* Determine minimal output port (& VC) */
	destination = flit->destId;
	minOutP = this->minOutputPort(destination);

	/* Calculate Valiant node output port (&VC) */
	valNode = flit->valId;
	valOutP = this->minOutputPort(valNode);

	/* By default, consider minimal route as the selected one */
	selectedRoute.port = minOutP;

	if (this->misrouteCondition(flit, inPort)) {
		flit->setMisrouted(true, VALIANT);

		/* If Valiant node is in current switch, update check within flit. */
		if (switchM->label == int(valNode / g_p_computing_nodes_per_router)) flit->valNodeReached = true;

		/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
		if (!flit->valNodeReached) selectedRoute.port = valOutP;
	}

	selectedRoute.neighPort = this->neighPort[selectedRoute.port];
	selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < portCount && selectedRoute.neighPort >= 0);

	return selectedRoute;
}

/*
 * Redefined to suit PIGGYBACKING misrouting condition. PB implies UGAL-L(ocal) conditions
 * (misroute if packet is being injected and its destination node is linked to a different
 * switch, & both local/global restrictions are fulfilled) and global link congested
 * condition.
 */
bool pbAny::misrouteCondition(flitModule * flit, int inPort) {
	int destination, valNode, minOutP, valOutP, minQueueLength, valQueueLength;
	bool result = false;
	destination = flit->destId;
	minOutP = this->minOutputPort(destination);
	valNode = flit->valId;
	valOutP = this->minOutputPort(valNode);

	if ((inPort < g_p_computing_nodes_per_router) && (minOutP >= g_p_computing_nodes_per_router)) {
		/* Calculate credit occupancy for the minimal and Valiant paths output port */
		minQueueLength = switchM->switchModule::getCreditsOccupancy(minOutP, this->nextChannel(inPort, minOutP, flit));
		valQueueLength = switchM->switchModule::getCreditsOccupancy(valOutP, this->nextChannel(inPort, valOutP, flit));
		/* UGAL-L(ocal) condition: if the product of minimal outport queue occupancy and the
		 * minimal path length is greater than the corresponding of Valiant path (plus a
		 * local threshold), then do Valiant misrouting. */
		if (((minQueueLength * flit->minPathLength)
				> (valQueueLength * flit->valPathLength) + g_flit_size * g_ugal_local_threshold)
				|| switchM->isGlobalLinkCongested(flit)) {
			result = true;
			flit->setCurrentMisrouteType(VALIANT);
		} else
			/* Reset current misroute type, in case a previous iteration attempted misrouting and failed*/
			flit->setCurrentMisrouteType(NONE);
	} else if (flit->getCurrentMisrouteType() == VALIANT && !flit->valNodeReached) {
		/* If flit has been previously marked as misrouted and Val node has not yet been
		 * reached, choose min path to Val node */
		result = true;
	}

	return result;
}

/*
 * Defined for coherence reasons. Should never be called.
 */
MisrouteType pbAny::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	assert(0);
}

/*
 * Defined for coherence reasons. Should never be called.
 */
int pbAny::nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
		int* &candidates_port, int* &candidates_VC) {
	assert(0);
}

int pbAny::nextChannel(int inP, int outP, flitModule * flit) {
	char outType, inType;
	int next_channel, i, index, inVC = flit->channel;

	inType = portType(inP);
	outType = portType(outP);
	char vcTypeOrder[] = { 'a', 'h', 'a', 'a', 'h', 'a' };
	int vcOrder[] = { 0, 0, 1, 2, 1, 3 };

	if (inType == 'p')
		index = -1;
	else {
		for (i = 0; i < g_local_link_channels + g_global_link_channels; i++) {
			if (inType == vcTypeOrder[i] && inVC == vcOrder[i]) {
				index = i;
				break;
			}
		}
	}
	for (i = index + 1; i < g_local_link_channels + g_global_link_channels; i++) {
		if (outType == vcTypeOrder[i]) {
			next_channel = vcOrder[i];
			break;
		}
	}

	if (outType == 'p') next_channel = inVC;
	assert(next_channel < g_channels);

	return (next_channel);
}
