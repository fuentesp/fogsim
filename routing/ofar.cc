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

#include "ofar.h"

ofar::ofar(switchModule *switchM) :
		baseRouting(switchM) {
	assert(
			g_deadlock_avoidance == RING || g_deadlock_avoidance == EMBEDDED_RING
					|| g_deadlock_avoidance == EMBEDDED_TREE); // Sanity check
}

ofar::~ofar() {

}

candidate ofar::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute, escapeRoute;
	int destination, minOutP, minOutVC, outP = -1, outVC = -1;
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
		selectedRoute.port = minOutP;
		selectedRoute.vc = minOutVC;
	}

	/* Update neighbor input port info */
	selectedRoute.neighPort = neighPort[selectedRoute.port];
	/* Update flit's current misrouting type */
	flit->setCurrentMisrouteType(misroute);

	/* Sanity check: ensure port and vc are within allowed ranges */
	if (g_deadlock_avoidance == RING)
		assert(selectedRoute.port < (portCount - 2));
	else
		assert(selectedRoute.port < (portCount));
	assert(selectedRoute.vc < g_channels); /* Is not restrictive enough, as it might be a forbidden vc for that port, but will be checked after */

	/* Calculate escape path */
	escapeRoute = escapeNetworkCandidate(flit, inPort, inVC, outP, outVC);
	if (escapeRoute.port != -1) selectedRoute = escapeRoute;

	return selectedRoute;
}

/*
 * Returns the misroute port type that
 * might be used, according to the
 * OFAR routing configuration.
 *
 * @return MisrouteType
 */
MisrouteType ofar::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	/* Determine if input port is a physical or embedded ring, or an embedded tree */
	bool input_emb_net = (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == EMBEDDED_TREE)
			&& (g_local_link_channels <= inchannel) && (inchannel < g_local_link_channels + g_channels_escape);
	bool input_phy_ring = g_deadlock_avoidance == RING && inport >= (this->portCount - 2);

	MisrouteType result = NONE;

	/* Source group may be susceptible to global misroute */
	if ((flit->sourceGroup == switchM->hPos) && (flit->destGroup != switchM->hPos)
			&& (not (flit->globalMisroutingDone))) {
		switch (g_global_misrouting) {
			case CRG_L:
			case CRG:
				assert(not (flit->localMisroutingDone));
				result = GLOBAL;
				break;
			case MM_L:
			case MM:
				/* Global mandatory: if previous hop has been a local misroute within source group,
				 * flit needs to be redirected to any global link within current router */
				if (flit->mandatoryGlobalMisrouting_flag) {
					assert(flit->localMisroutingDone);
					assert(
							((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset))
									|| input_phy_ring);
					result = GLOBAL_MANDATORY;
				}
				/* Injection: select global misrouting */
				else if (inport < g_p_computing_nodes_per_router)
					result = GLOBAL;
				/* Local transit */
				else if ((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset)
						&& (not input_emb_net) && not (flit->localMisroutingDone)) result = LOCAL_MM;
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
	<< minOutputPort(flit) << " POSSIBLE misroute: ";
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

int ofar::nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
		int* &candidates_port, int* &candidates_VC) {
	/* Sanity checks for those parameters passed by address */
	if (!candidates_port) assert(0);
	if (!candidates_VC) assert(0);

	int crd, outP, nextC, i, port_offset, port_limit, num_candidates = 0;
	bool can_rx_flit, valid_candidate;
	double q_non_min;

	switch (misroute) {
		case LOCAL:
		case LOCAL_MM:
			port_offset = g_local_router_links_offset;
			port_limit = g_local_router_links_offset + g_a_routers_per_group - 1;
			break;
		case GLOBAL_MANDATORY:
			assert(flit->mandatoryGlobalMisrouting_flag);
		case GLOBAL:
			port_offset = g_global_router_links_offset;
			port_limit = g_global_router_links_offset + g_h_global_ports_per_router;
			break;
		default:
			/* This function can only be entered if a misrouting type has been selected */
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

		/* Select VC with more credits available */
		crd = nextC = 0;
		for (i = 0; i < g_local_link_channels; i++) {
			if (crd < switchM->getCredits(outP, i)) {
				crd = switchM->getCredits(outP, i);
				nextC = i;
			}
		}
		can_rx_flit = switchM->nextPortCanReceiveFlit(outP);
		q_non_min = switchM->getCreditsOccupancy(outP, nextC) * 100.0 / switchM->getMaxCredits(outP, nextC);

		valid_candidate = ((q_non_min <= threshold) || g_forceMisrouting || misroute == GLOBAL_MANDATORY)
				&& (crd >= g_flit_size) && can_rx_flit && (outP != minOutP);

		//if it's a valid candidate, store its info
		if (valid_candidate) {
			candidates_port[num_candidates] = outP;
			candidates_VC[num_candidates] = nextC;
			num_candidates = num_candidates + 1;
		}
	}
	return num_candidates;
}

/* Check if misroute path is available, or flit has to go through escape subnetwork.
 * 3 cases are considered:
 * - By default, try escape subnetwork.
 * - Flit comes from an injection port, neither minimal or misroute port have any
 * 		remaining credits, and it's not forbidden to inject to escape subnetwork.
 * - Local cycles are restricted, flit comes from escape subnetwork, and output is
 * 		not a global link.
 */
candidate ofar::escapeNetworkCandidate(flitModule *flit, int inPort, int inVC, int outP, int outVC) {
	bool input_escape_network;
	int vc_offset, vc_range, i, dist;
	candidate escapePath = { -1, -1, -1 };

	int crd = switchM->getCredits(outP, outVC);
	/* Determine if flit comes from a escape network port */
	switch (g_deadlock_avoidance) {
		case RING:
			input_escape_network = inPort >= (portCount - 2);
			break;
		case EMBEDDED_RING:
		case EMBEDDED_TREE:
			input_escape_network = (g_local_link_channels <= inVC)
					&& (inVC < g_local_link_channels + g_channels_escape);
			break;
		default:
			assert(0);
	}

	if ((g_try_just_escape)
			|| ((not ((g_forbid_from_inj_queues_to_ring) && (inPort < g_p_computing_nodes_per_router)) && (crd == 0))
					|| ((g_restrictLocalCycles) && (input_escape_network) && (outP < g_global_router_links_offset)))) {
		switch (g_deadlock_avoidance) {
			case RING:
				dist = module((flit->destId - switchM->label), g_number_switches); // Distance to the destination node going through the ring
				assert(dist != 0); // If dist=0, we have reached dest node and shouldn't be trying to escape through ring
				if (g_ringDirs == 1) {
					/* UNIDIRECTIONAL ring */
					escapePath.port = this->portCount - 1;
					escapePath.neighPort = this->portCount - 2;
				} else {
					/* BIDIRECTIONAL ring */
					/* If even number of switches, and distance is same through left or right side of the ring, take random */
					if ((dist == g_number_switches / 2) && ((g_number_switches % 2) == 0)) dist = rand() % 2;
					if ((dist > int(g_number_switches / 2)) || (dist == 0)) {
						escapePath.port = this->portCount - 2; // Left side of the ring
						escapePath.neighPort = this->portCount - 1;
					} else {
						escapePath.port = this->portCount - 1; // Right side of the ring
						escapePath.neighPort = this->portCount - 2;
					}
				}
				vc_offset = 0;
				vc_range = g_local_link_channels;
				break;
			case EMBEDDED_RING:
				if (flit->assignedRing == 1) {
					assert(g_rings > 0);
					escapePath.port = this->tableOutRing1[flit->destSwitch]; // Minimal path
					escapePath.neighPort = this->tableInRing1[flit->destSwitch];
				} else if (flit->assignedRing == 2) {
					assert(g_rings > 1);
					escapePath.port = this->tableOutRing2[flit->destSwitch]; // Minimal path
					escapePath.neighPort = this->tableInRing2[flit->destSwitch];
				} else {
					assert(g_rings == 0);
				}
				vc_offset = g_local_link_channels;
				vc_range = g_local_link_channels + g_channels_escape;
				break;
			case EMBEDDED_TREE:
				escapePath.port = this->tableOutTree[flit->destId]; // Minimal path
				escapePath.neighPort = this->tableInTree[flit->destSwitch];
				vc_offset = g_local_link_channels;
				vc_range = g_local_link_channels + g_channels_escape;
				assert(escapePath.port >= g_local_router_links_offset);
				assert(escapePath.port < portCount);
				assert(escapePath.neighPort >= g_local_router_links_offset);
				assert(escapePath.neighPort < portCount);
				break;
			default:
				assert(0);
		}

		/* Select VC with max number of credits. */
		crd = 0;
		for (i = vc_offset; i < vc_range; i++) {
			if (switchM->getCredits(escapePath.port, i)) {
				crd = switchM->getCredits(escapePath.port, i);
				escapePath.vc = i;
			}
		}
	}
	return escapePath;
}
