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

#include "par.h"

par::par(switchModule *switchM) :
		baseRouting(switchM) {
	const int minLocalVCs = 5, minGlobalVCs = 2;
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
	/* Remove default VC and port type ordering to replace with specific for oblivious routing */
	baseRouting::typeVc.clear();
	baseRouting::petitionVc.clear();
	baseRouting::responseVc.clear();
	char aux[] = { 'a', 'a', 'h', 'a', 'a', 'h', 'a' };
	typeVc.insert(typeVc.begin(), aux, aux + minLocalVCs + minGlobalVCs);
	int aux2[] = { 0, 1, 0, 2, 3, 1, 4 };
	petitionVc.insert(petitionVc.begin(), aux2, aux2 + minLocalVCs + minGlobalVCs);
	assert(typeVc.size() == petitionVc.size());
	if (g_reactive_traffic) {
		int aux3[] = { 5, 6, 2, 7, 8, 3, 9 };
		responseVc.insert(responseVc.begin(), aux3, aux3 + minLocalVCs + minGlobalVCs);
		assert(typeVc.size() == responseVc.size());
	}
}

par::~par() {

}

candidate par::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int destination, minOutP, minOutVC;
	bool misrouteFlit = false;
	MisrouteType misroute = NONE;

	/* Determine minimal output port (& VC) */
	destination = flit->destId;
	minOutP = this->minOutputPort(destination);
	minOutVC = this->nextChannel(inPort, minOutP, flit);

	/* Calculate misroute output port (if any should be taken) */
	if (this->misrouteCondition(flit, minOutP, minOutVC)) {
		misrouteFlit = this->misrouteCandidate(flit, inPort, inVC, minOutP, minOutVC, selectedRoute.port,
				selectedRoute.vc, misroute);
	}
	if (!misrouteFlit) {
		/* Flit will be minimally routed */
		assert(misroute == NONE);
		selectedRoute.port = minOutP;
		selectedRoute.vc = minOutVC;
	}

	/* Update neighbor input port info */
	selectedRoute.neighPort = neighPort[selectedRoute.port];
	/* Update flit's current misrouting type */
	flit->setCurrentMisrouteType(misroute);

	/* Sanity check: ensure port and vc are within allowed ranges */
	assert(selectedRoute.port < portCount);
	assert(selectedRoute.vc < g_channels); /* Is not restrictive enough, as it might be a forbidden vc for that port, but will be checked after */

	return selectedRoute;
}

/*
 * Returns the misroute port type that
 * might be used, according to the
 * Opportunistic Local Misrouting configuration.
 *
 * @return MisrouteType
 */
MisrouteType par::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	MisrouteType result = NONE;

	/* Source group may be susceptible to global misroute */
	if ((flit->sourceGroup == switchM->hPos) && (flit->destGroup != switchM->hPos)) {
		assert(not (flit->globalMisroutingDone));
		switch (g_global_misrouting) {
			case CRG_L:
			case CRG:
				assert(not (flit->localMisroutingDone));
				result = GLOBAL;
				break;
			case MM_L:
			case MM:
				/* Global mandatory: if previous hop has been a local misroute within source group, flit needs to be
				 * redirected to any global link within current router */
				if (flit->mandatoryGlobalMisrouting_flag) {
					assert(flit->localMisroutingDone);
					assert((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset));
					result = GLOBAL_MANDATORY;
				}
				/* Injection: select global misrouting */
				else if (inport < g_p_computing_nodes_per_router)
					result = GLOBAL;
				/* If packet is transitting locally (inport port is a local port) */
				else if ((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset)
						&& not (flit->localMisroutingDone)) result = LOCAL_MM;
				break;
			default:
				break;
		}
	} /* Source group is not liable to global misroute, try local */
	else if ((g_global_misrouting == CRG_L || g_global_misrouting == MM_L) && not (flit->localMisroutingDone)
			&& not (minOutputPort(flit->destId) >= g_global_router_links_offset)
			&& this->switchM->hPos != flit->destGroup) {
		assert(not (flit->mandatoryGlobalMisrouting_flag));
		result = LOCAL;
	}
#if DEBUG
	cout << "Cycle " << g_cycle << "--> SW " << this->switchM->label << " inPort " << inport << " vc " << flit->channel
	<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min outPort " << minOutPort << " POSSIBLE misroute: ";
	switch (result) {
		case LOCAL:
		cout << "LOCAL" << endl;
		break;
		case LOCAL_MM:
		cout << "LOCAL_MM" << endl;
		break;
		case GLOBAL:
		cout << "GLOBAL" << endl;
		break;
		case GLOBAL_MANDATORY:
		cout << "GLOBAL_MANDATORY" << endl;
		break;
		case NONE:
		cout << "NONE" << endl;
		break;
		default:
		cout << "DEFAULT" << endl;
		break;
	}
#endif

	return result;
}

int par::nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
		int* &candidates_port, int* &candidates_VC) {
	/* Sanity checks for those parameters passed by address */
	assert(!candidates_port);
	assert(!candidates_VC);

	int outP, nextC, i, port_offset, port_limit, num_candidates = 0;
	bool valid_candidate;

	switch (misroute) {
		case LOCAL:
		case LOCAL_MM:
			port_offset = g_local_router_links_offset;
			port_limit = g_local_router_links_offset + g_a_routers_per_group - 1;
			break;
		case GLOBAL_MANDATORY:
			assert(flit->mandatoryGlobalMisrouting_flag);
		case GLOBAL:
			assert(switchM->hPos == flit->sourceGroup); /* Sanity check: global misrouting is only allowed in source group. */
			port_offset = g_global_router_links_offset;
			port_limit = g_global_router_links_offset + g_h_global_ports_per_router;
			break;
		default:
			// This point should never be reached
			assert(false);
			break;
	}

	candidates_port = new int[port_limit - port_offset];
	candidates_VC = new int[port_limit - port_offset];
	for (i = 0; i < port_limit - port_offset; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}

	/* Determine all valid candidates (GLOBAL LINKS) */
	for (outP = port_offset; outP < port_limit; outP++) {
		if (outP == minOutP) continue;

		nextC = this->nextChannel(inPort, outP, flit);
		assert(nextC <= g_channels);
		valid_candidate = validMisroutePort(flit, outP, nextC, threshold, misroute);

		/* If it's a valid candidate, store its info */
		if (valid_candidate) {
			candidates_port[num_candidates] = outP;
			candidates_VC[num_candidates] = nextC;
			num_candidates++;
		}
	}
	return num_candidates;
}
