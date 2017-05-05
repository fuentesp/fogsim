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

#include "pbAny.h"

pbAny::pbAny(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	assert(g_vc_usage == BASE);
	/* Remove default VC and port type ordering to replace with specific for PB-any routing */
	baseRouting::typeVc.clear();
	baseRouting::petitionVc.clear();
	baseRouting::responseVc.clear();
	char aux[] = { 'a', 'h', 'a', 'a', 'h', 'a' };
	typeVc.insert(typeVc.begin(), aux, aux + 6);
	int aux2[] = { 0, 0, 1, 2, 1, 3 };
	petitionVc.insert(petitionVc.begin(), aux2, aux2 + 6);
	if (g_reactive_traffic) {
		int aux3[] = { 4, 2, 5, 6, 3, 7 };
		responseVc.insert(responseVc.begin(), aux3, aux3 + 6);
	}
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
		minQueueLength = switchM->switchModule::getCreditsOccupancy(minOutP, flit->cos,
				this->nextChannel(inPort, minOutP, flit));
		valQueueLength = switchM->switchModule::getCreditsOccupancy(valOutP, flit->cos,
				this->nextChannel(inPort, valOutP, flit));
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
