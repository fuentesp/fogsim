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

#include "ugal.h"

ugal::ugal(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); /* Sanity check */
}

ugal::~ugal() {

}

candidate ugal::enroute(flitModule * flit, int inPort, int inVC) {
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

		/* If Valiant node is in current group, update check within flit. */
		if (switchM->hPos == int(valNode / (g_p_computing_nodes_per_router * g_a_routers_per_group)))
			flit->valNodeReached = 1;

		/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
		if (flit->valNodeReached == 0) selectedRoute.port = valOutP;

	}

	selectedRoute.neighPort = this->neighPort[selectedRoute.port];
	selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < portCount && selectedRoute.neighPort >= 0);

	return selectedRoute;
}

/*
 * Redefined to suit UGAL misrouting condition. Flit will be considered for misrouting if
 * packet is being injected and its destination node is linked to a different switch. To
 * route minimally, both (local and global) restrictions must be fulfilled. For that
 * purpose, corresponding VC for minimal and Valiant path must be calculated (remember that
 * using UGAL implies Dally usage of VCs, so we use 'nextChannel' function to that end).
 * Returns true if misrouting conditions are fulfilled.
 */
bool ugal::misrouteCondition(flitModule * flit, int inPort) {
	int destination, valNode, minOutP, valOutP, minQueueLength, valQueueLength;
	bool result = false;
	destination = flit->destId;
	minOutP = this->minOutputPort(destination);
	valNode = flit->valId;
	valOutP = this->minOutputPort(valNode);

	if (flit->getCurrentMisrouteType() == VALIANT && !flit->valNodeReached) {
		/* If flit has been previously marked as misrouted and Val node has not yet been
		 * reached, choose min path to Val node */
		result = true;
	} else if ((inPort < g_p_computing_nodes_per_router) && (minOutP >= g_p_computing_nodes_per_router)) {
		/* Determine credit occupancy for minimal and Valiant paths */
		minQueueLength = switchM->getCreditsOccupancy(minOutP, flit->cos, this->nextChannel(inPort, minOutP, flit));
		valQueueLength = switchM->getCreditsOccupancy(valOutP, flit->cos, this->nextChannel(inPort, valOutP, flit));

		/* UGAL-L(ocal) condition: if the product of minimal outport queue occupancy and the
		 * minimal path length is greater than the corresponding of Valiant path (plus a
		 * local threshold), then do Valiant misrouting. */
		if ((minQueueLength * (flit->minPathLength))
				> (valQueueLength * (flit->valPathLength) + g_flit_size * g_ugal_local_threshold)) {
			result = true;
			flit->setCurrentMisrouteType(VALIANT);
		}
	}

	return result;
}

/*
 * Defined for coherence reasons. Should never be called.
 */
MisrouteType ugal::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	assert(0);
}

/*
 * Defined for coherence reasons. Should never be called.
 */
int ugal::nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
		int* &candidates_port, int* &candidates_VC) {
	assert(0);
}

