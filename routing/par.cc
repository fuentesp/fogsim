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

#include "par.h"

par::par(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
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
	minOutVC = this->nextChannel(inPort, minOutP, flit->channel);

	/* Calculate misroute output port (if any should be taken) */
	if (this->misrouteCondition(flit, minOutP, minOutVC)) {
		misrouteFlit = this->misrouteCandidate(flit, inPort, inVC, minOutP, minOutVC, selectedRoute.port,
				selectedRoute.vc, misroute);
	}
	if (!misrouteFlit) {
		/* Flit will be minimally routed */
		selectedRoute.port = minOutP;
		selectedRoute.vc = minOutVC;
	}

	/* Update neighbor input port info */
	selectedRoute.neighPort = neighPort[selectedRoute.port];
	/* Update flit's current misrouting type */
	flit->setCurrentMisrouteType(misroute);

	/* Sanity check: ensure port and vc are within allowed ranges */
	assert(selectedRoute.port < (portCount));
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
	}
	/* Source group is not liable to global misroute, try local */
	else if ((g_global_misrouting == CRG_L || g_global_misrouting == MM_L) && not (flit->localMisroutingDone)
			&& not (minOutputPort(flit->destId) >= g_global_router_links_offset)) {
		assert(not (flit->mandatoryGlobalMisrouting_flag));
		result = LOCAL;
	}
#if DEBUG
	cout << "cycle " << g_cycle << "--> SW " << this->switchM->label << " input Port " << inport << " CV " << flit->channel
	<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
	<< baseRouting::minOutputPort(flit) << " POSSIBLE misroute: ";
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
	if (!candidates_port) assert(0);
	if (!candidates_VC) assert(0);

	int outP, nextC, i, port_offset, port_limit, num_candidates = 0;
	bool valid_candidate;

	switch (misroute) {
		case LOCAL:
		case LOCAL_MM:
			port_offset = g_local_router_links_offset;
			port_limit = g_local_router_links_offset + g_a_routers_per_group - 1;
			/* Determine VC depending on the group we are at */
			if (switchM->hPos == flit->sourceGroup) {
				/* Source group */
				assert(flit->channel == 0);
				nextC = 1;
			} else if (switchM->hPos != flit->destGroup) {
				/* Intermediate group */
				assert(flit->channel == 0);
				nextC = 2;
			} else {
				/* Destination group */
				switch (flit->channel) {
					case 0:
						nextC = 2;
						break;
					case 1:
						nextC = 4;
						break;
					default:
						assert(0);
				}
			}
			break;
		case GLOBAL_MANDATORY:
			assert(flit->mandatoryGlobalMisrouting_flag);
		case GLOBAL:
			assert(switchM->hPos == flit->sourceGroup); /* Sanity check: global misrouting is only allowed in source group. */
			port_offset = g_global_router_links_offset;
			port_limit = g_global_router_links_offset + g_h_global_ports_per_router;
			nextC = 0;
			assert(nextC <= g_global_link_channels);
			break;
		default:
			assert(0);
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

		valid_candidate = validMisroutePort(flit, outP, nextC, threshold, misroute);

		/* If it's a valid candidate, store its info */
		if (valid_candidate) {
			candidates_port[num_candidates] = outP;
			candidates_VC[num_candidates] = nextC;
			num_candidates = num_candidates + 1;
		}
	}
	return num_candidates;
}

/*
 * Determines next channel, following an increasing VC
 * order upon hops, unless jumping from local to global
 * or vice-versa.
 */
int par::nextChannel(int inP, int outP, int inVC) {
	char outType, inType;
	int next_channel;

	inType = portType(inP);
	outType = portType(outP);

	if (inType == 'p')
		next_channel = 0;
	else if (inType == outType && (inType == 'a' || inType == 'h'))
		/* If both input and output are either local or global */
		next_channel = inVC + 1;
	else if (inType == 'a' && outType == 'h') {
		assert(inVC < 4);
		next_channel = (inVC < 2) ? 0 : 1;
	} else if (inType == 'h' && outType == 'a') {
		assert(inVC <= 1);
		next_channel = (inVC == 0) ? 2 : 4;
	} else
		next_channel = inVC;

	assert(next_channel < g_channels); /*  Sanity check */
	return (next_channel);
}
