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

#include "routing.h"
#include "../flit/flitModule.h"

baseRouting::baseRouting(switchModule *switchM) {
	this->switchM = switchM;
	portCount = switchM->getSwPortSize();
	neighList = new switchModule *[this->portCount];
	neighPort = new int[this->portCount];

	/* Create and fill in routing tables */
	this->tableSwOut = new int[g_a_routers_per_group];
	this->tableGroupOut = new int[g_a_routers_per_group * g_h_global_ports_per_router + 1];
	setMinTables();
	switch (g_deadlock_avoidance) {
		case RING:
		case EMBEDDED_RING:
			this->tableOutRing1 = new int[g_number_switches];
			this->tableInRing1 = new int[g_number_switches];
			this->tableOutRing2 = new int[g_number_switches];
			this->tableInRing2 = new int[g_number_switches];
			setRingTables();
			break;
		case EMBEDDED_TREE:
			this->tableOutTree = new int[g_number_switches];
			this->tableInTree = new int[g_number_switches];
			setTreeTables();
			break;
		default:
			break;
	}

	/* If misrouting trigger relays on congestion awareness, we need
	 * to initialize globalLinkCongested array to check congestion status
	 * for a given global link. */
	switch (g_misrouting_trigger) {
		case CGA:
		case HYBRID:
		case HYBRID_REMOTE:
			globalLinkCongested = new bool[g_h_global_ports_per_router];
			for (int i = 0; i < g_h_global_ports_per_router; i++) {
				globalLinkCongested[i] = false;
			}
			break;
		default:
			break;
	}
}

baseRouting::~baseRouting() {
	delete[] neighList;
	delete[] neighPort;
	delete[] tableSwOut;
	delete[] tableGroupOut;
	delete[] globalLinkCongested;
	switch (g_deadlock_avoidance) {
		case RING:
		case EMBEDDED_RING:
			delete[] tableInRing1;
			delete[] tableOutRing1;
			delete[] tableInRing2;
			delete[] tableOutRing2;
			break;
		case EMBEDDED_TREE:
			delete[] tableInTree;
			delete[] tableOutTree;
			break;
		default:
			break;
	}
}

void baseRouting::setMinTables() {
	int thisA, thisH, destP, destA, destH, destID, nextA, inPort, outPort, minPortCounter, offsetH, offsetHcmp, offsetA;
	thisA = switchM->aPos;
	thisH = switchM->hPos;
	minPortCounter = (g_deadlock_avoidance == RING) ? portCount - 2 : portCount;
	for (destH = 0; destH < ((g_h_global_ports_per_router * g_a_routers_per_group) + 1); destH++) {
		if (g_palm_tree_configuration) {
			offsetH = module((thisH - destH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1;
			offsetHcmp = module((destH - thisH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1;
		} else {
			if (destH > thisH) {
				offsetH = destH - 1;
				offsetHcmp = thisH;
			} else {
				offsetH = destH;
				offsetHcmp = thisH - 1;
			}
		}

		if (destH != thisH) {
			nextA = int(offsetH / g_h_global_ports_per_router);
			offsetA = nextA - thisA;
			if (offsetA != 0) {
				if (offsetA > 0) offsetA--;
				outPort = thisA + offsetA + g_local_router_links_offset;
			} else {
				outPort = module(offsetH, g_h_global_ports_per_router) + g_global_router_links_offset;
			}
			assert(outPort < minPortCounter && inPort < minPortCounter); /* Sanity check: in/out port can't be higher than router ports range */
			tableGroupOut[destH] = outPort;
			continue;
		} else
			tableGroupOut[destH] = -1;
		for (destA = 0; destA < g_a_routers_per_group; destA++) {
			if (destA != thisA) {
				offsetA = destA - thisA;
				if (offsetA > 0) offsetA--;
				outPort = thisA + offsetA + g_local_router_links_offset;
				assert(outPort < minPortCounter && inPort < minPortCounter); /* Sanity check: in/out port can't be higher than router ports range */
				tableSwOut[destA] = outPort;
			} else
				tableSwOut[destA] = -1;
		}
	}
}

/*
 * Returns output port to the minimal path
 * for a given destination node.
 */
int baseRouting::minOutputPort(int dest) {
	int destP, destA, destH, port;

	destP = dest % g_p_computing_nodes_per_router;
	assert(destP < g_p_computing_nodes_per_router);
	destA = int(dest / g_p_computing_nodes_per_router) % g_a_routers_per_group;
	assert(destA < g_a_routers_per_group);
	destH = int(int(dest / g_p_computing_nodes_per_router) / g_a_routers_per_group);
	assert(destH < g_a_routers_per_group * g_h_global_ports_per_router + 1);
	assert(
			dest
					== destP + destA * g_p_computing_nodes_per_router
							+ destH * g_a_routers_per_group * g_p_computing_nodes_per_router);

	if (destH != switchM->hPos)
		port = tableGroupOut[destH];
	else if (destA != switchM->aPos)
		port = tableSwOut[destA];
	else
		port = destP;

	assert(port >= 0 && port < portCount);
	return port;
}

/*
 * Returns input port in the neighbor router within the
 * minimal path for a given destination node.
 */
int baseRouting::minInputPort(int dest) {
	/* Calculate destID and determine which router from current group links to dest group */
	int inPort, destP, destA, destH, offsetA, offsetH, offsetHcmp, thisA, nextA, thisH;

	destP = dest % g_p_computing_nodes_per_router;
	assert(destP < g_p_computing_nodes_per_router);
	destA = int(dest / g_p_computing_nodes_per_router) % g_a_routers_per_group;
	assert(destA < g_a_routers_per_group);
	destH = int(int(dest / g_p_computing_nodes_per_router) / g_a_routers_per_group);
	assert(destH < g_a_routers_per_group * g_h_global_ports_per_router + 1);
	assert(
			dest
					== destP + destA * g_p_computing_nodes_per_router
							+ destH * g_a_routers_per_group * g_p_computing_nodes_per_router);

	thisA = switchM->aPos;
	thisH = switchM->hPos;

	/* Routing tables will depend on network distribution, which can be 'Palm Tree' or not.
	 * This will mainly affect port offset amongst groups */
	if (g_palm_tree_configuration) {
		offsetH = module((thisH - destH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1;
		offsetHcmp = module((destH - thisH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1;
	} else {
		offsetH = (destH > thisH) ? destH - 1 : destH;
		offsetHcmp = (destH > thisH) ? thisH : thisH - 1;
	}
	/* After this point, output port is calculated equally for any network distribution.
	 * If source and dest group, next hop will be for a local router linked to dest group */
	if (thisH != destH)
		nextA = int(offsetH / g_h_global_ports_per_router);
	else
		nextA = destA;

	/* If next local router is different from current, ports will be calculated upon the
	 * offset between them. Otherwise, next hop is either global or to a computing node. */
	offsetA = nextA - thisA;
	if (offsetA != 0) {
		if (offsetA > 0) offsetA--;
		inPort = nextA - offsetA - 1 + g_local_router_links_offset;
	} else {
		if (thisH != destH) {
			inPort = module(offsetHcmp, g_h_global_ports_per_router) + g_global_router_links_offset;
		} else {
			inPort = 0;
		}
	}
	return inPort;
}

void baseRouting::setRingTables() {
	int thisA, thisH, destP, destA, destH, destID, inPort, outPort, dist, neigh, ring, offsetA, offsetH, leftMargin,
			rightMargin, dir;
	thisA = switchM->aPos;
	thisH = switchM->hPos;

	for (destH = 0; destH < ((g_h_global_ports_per_router * g_a_routers_per_group) + 1); destH++) {
		for (destA = 0; destA < g_a_routers_per_group; destA++) {
			for (destP = 0; destP < g_p_computing_nodes_per_router; destP++) {
				leftMargin = 0;
				rightMargin = g_a_routers_per_group - 1;
				destID = destP + destA * g_p_computing_nodes_per_router
						+ destH * g_a_routers_per_group * g_p_computing_nodes_per_router;
				/* Determine distance to dest node through ring 1. If distance is equal going
				 * left or right, take a random decision among them */
				dist = module((int(destID / g_p_computing_nodes_per_router) - switchM->label), g_number_switches);
				if ((dist == g_number_switches / 2) && ((g_number_switches % 2) == 0))
					dist = (rand() % 2) * g_number_switches;
				/* Reduce dir to -1 if it should go left, 1 if it should go right, and 0 if dest sw is current */
				dir = dist > (g_number_switches / 2) ? -1 : 1;
				if (dist == 0) dir = 0;
				for (ring = 1; ring <= g_rings; ring++) {
					switch (ring) { /* Ring 1 or 2 */
						case 1:
							if (g_ringDirs == 1)
								offsetA = offsetH = 1; /* Unidirectional ring */
							else
								offsetA = offsetH = dir; /* Bidirectional ring */
							break;
						case 2:
							assert(g_ringDirs == 2);
							offsetA = dir * (g_h_global_ports_per_router + 1);
							offsetH = dir * (g_h_global_ports_per_router * g_h_global_ports_per_router / 2 + 2);
							leftMargin -= g_h_global_ports_per_router / 2;
							rightMargin += g_h_global_ports_per_router / 2;
							break;
					}
					if (thisA + offsetA < leftMargin || thisA + offsetA > rightMargin) {
						neigh = module((thisH + offsetH), ((g_h_global_ports_per_router * g_a_routers_per_group) + 1))
								* g_a_routers_per_group * g_p_computing_nodes_per_router;
						if (offsetH < 0 && ring == 1)
							neigh += (g_a_routers_per_group - 1) * g_p_computing_nodes_per_router;
					} else
						/* Jump within local ports is below range margins */
						neigh = thisH * g_a_routers_per_group * g_p_computing_nodes_per_router
								+ module((thisA + offsetA), g_a_routers_per_group) * g_p_computing_nodes_per_router;
					if (neigh == switchM->label) neigh = destID; /* If next router is the current, jump directly to the dest node */
					inPort = this->minInputPort(neigh);
					outPort = this->minOutputPort(neigh);
					assert(inPort < g_global_router_links_offset);
					assert(outPort < g_global_router_links_offset);
					switch (ring) {
						case 1:
							this->tableInRing1[destA + destH * g_a_routers_per_group] = inPort;
							this->tableOutRing1[destA + destH * g_a_routers_per_group] = outPort;
							break;
						case 2:
							this->tableInRing2[destA + destH * g_a_routers_per_group] = inPort;
							this->tableOutRing2[destA + destH * g_a_routers_per_group] = outPort;
							break;
					}
					assert(outPort < portCount);
					assert(inPort < portCount);
				}
			}
		}
	}

}

void baseRouting::setTreeTables() {
	int thisH, destP, destA, destH, destID, inPort, outPort, neigh;
	thisH = switchM->hPos;

	for (destH = 0; destH < ((g_h_global_ports_per_router * g_a_routers_per_group) + 1); destH++) {
		for (destA = 0; destA < g_a_routers_per_group; destA++) {
			for (destP = 0; destP < g_p_computing_nodes_per_router; destP++) {
				destID = destP + destA * g_p_computing_nodes_per_router
						+ destH * g_a_routers_per_group * g_p_computing_nodes_per_router;
				if ((switchM->label == int(g_tree_root_node / g_p_computing_nodes_per_router))
						|| ((thisH == destH) && (minOutputPort(g_tree_root_node) >= g_global_router_links_offset))
						|| ((thisH == g_tree_root_switch) && (minOutputPort(destID) >= g_global_router_links_offset)))
					neigh = destID;
				else
					neigh = g_tree_root_switch;
				inPort = this->minInputPort(neigh);
				outPort = this->minOutputPort(neigh);
				this->tableInTree[int(destID / g_p_computing_nodes_per_router)] = inPort;
				this->tableOutTree[int(destID / g_p_computing_nodes_per_router)] = outPort;
				assert(outPort < portCount);
				assert(inPort < portCount);
			}
		}
	}
}

/* Wrap-around function to call initializing
 * functions for routing tables.
 */
void baseRouting::initialize(switchModule* swList[]) {
	findNeighbors(swList);
	visitNeighbors();
}

void baseRouting::findNeighbors(switchModule* swList[]) {
	int thisPort, thisA, thisH, prt, neighA, neighH;
	char portType;
	thisA = switchM->aPos;
	thisH = switchM->hPos;

	for (thisPort = g_p_computing_nodes_per_router; thisPort < portCount; thisPort++) {
		/* First determine which kind of port is it */
		portType = this->portType(thisPort);

		/* Then determine neighbors depending on port type */
		switch (portType) {
			case 'a': /* Local port */
				neighH = thisH;
				neighA = thisPort - g_local_router_links_offset;
				/* We need to account router does not have any port linking to itself. Thereby,
				 * neighbour offset will be one position larger. */
				if (neighA >= thisA) neighA++;
				break;
			case 'h': /* Global port */
				/* Global links are differently distributed if groups have a "palm tree" disposal */
				prt = thisPort - g_global_router_links_offset;
				if (g_palm_tree_configuration == 1) {
					neighH = module(thisH - (thisA * g_h_global_ports_per_router + prt + 1),
							(g_a_routers_per_group * g_h_global_ports_per_router + 1));
					assert(neighH != thisH);
					assert(neighH < (g_a_routers_per_group * g_h_global_ports_per_router + 1));
					neighA = int(
							(module((neighH - thisH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1)
									/ g_h_global_ports_per_router);
				} else {
					neighH = prt + (thisA) * g_h_global_ports_per_router;
					if ((prt + thisA * g_h_global_ports_per_router) >= thisH) neighH++;
					assert(neighH != thisH);
					if (thisH < neighH)
						neighA = int(thisH / g_h_global_ports_per_router);
					else
						neighA = int((thisH - 1) / g_h_global_ports_per_router);
				}
				break;
			case 'r': /* Escape port (physical cycle - ring escape subnetwork) */
				if (thisPort
						- (g_p_computing_nodes_per_router + g_a_routers_per_group - 1 + g_h_global_ports_per_router)
						== 0)
					/* Physical cycle port 0 links to previous router */
					neighA = module(thisA - 1, g_a_routers_per_group);
				else
					/* Physical cycle port 1 links to following router */
					neighA = module(thisA + 1, g_a_routers_per_group);
				assert(neighA < g_a_routers_per_group);
				if ((neighA == 0) && (thisA == (g_a_routers_per_group - 1)))
					/* If following router has A = 0, it belongs to next H group */
					neighH = module(thisH + 1, ((g_h_global_ports_per_router * g_a_routers_per_group) + 1));
				else if ((neighA == (g_a_routers_per_group - 1)) && (thisA == 0))
					/* If following router has A = a - 1, it belongs to previous H group */
					neighH = module(thisH - 1, ((g_h_global_ports_per_router * g_a_routers_per_group) + 1));
				else
					neighH = thisH;
				break;
		}
		assert((neighA + (neighH * g_a_routers_per_group)) < (g_number_switches));
		assert(thisPort < portCount);
		neighList[thisPort] = swList[neighA + (neighH * g_a_routers_per_group)];
	}
}

void baseRouting::visitNeighbors() {
	int port, numRingPorts = 0, aux = -1;

	/* There are no ring ports unless a physical (not embedded) subescape
	 * ring is being used as deadlock avoidance mechanism */
	if (g_deadlock_avoidance == RING) {
		numRingPorts = 2;
		assert((g_ringDirs == 1) || (g_ringDirs == 2));
	}

	for (port = 0; port < portCount; port++) {
		if (port < g_local_router_links_offset)
			/* Computing node port */
			neighPort[port] = 0;
		else if (port < (portCount - numRingPorts)) {
			/* Local & Global link port */
			assert(neighList[port]->label * g_p_computing_nodes_per_router < g_number_generators);
			neighPort[port] = minInputPort(neighList[port]->label * g_p_computing_nodes_per_router);
		} else
			/* Ring port; it either links to next group (one port less)
			 * or to previous (one port more) */
			neighPort[port] = (port == portCount - 1) ? port - 1 : port + 1;
	}
}

/*
 * Checks if a given port is used within a escape
 * subnetwork.
 */
bool baseRouting::escapePort(int port) {
	int g;
	bool result = false;
	switch (g_deadlock_avoidance) {
		case EMBEDDED_TREE:
			for (g = 0; g < g_number_switches; g++) {
				if (port == tableOutTree[g]) {
					result = true; /* Embedded tree port */
					break;
				}
			}
			break;
		case RING:
		case EMBEDDED_RING:
			for (g = 0; g < g_number_switches; g++) {
				if (port == tableOutRing1[g] && !g_onlyRing2) {
					result = true; /* Ring 1 port */
				}
				if ((g_rings > 1) && (port == tableOutRing2[g])) {
					assert(g_ringDirs > 1);
					assert(port != tableOutRing1[g]);
					result = true; /* Ring 2 port */
				}
				if (result) break;
			}
			break;
		default:
			break;
	}
	return result;
}

/*
 * Returns true if the flit should be misrouted.
 */
bool baseRouting::misrouteCondition(flitModule * flit, int outPort, int outVC) {
	assert(outPort == minOutputPort(flit->destId)); /* Sanity check: determine that given output port is effectively minimal. */
	bool result = false;
	double q_min;

	/* There are 2 cases in which misrouting trigger is bypassed:
	 * 1) If Force Misrouting flag is active, it always tries to misroute
	 * 2) If next hop is global mandatory: we have made a local minimal
	 * 		hop and a local misroute in the source group. Consequently,
	 * 		minimal path would be to return to previous router. To avoid
	 * 		the creation of a cycle, we need to enforce a non-minimal
	 * 		jump to a random global link to avoid coming back to previous
	 * 		router (provoking a cycle).
	 */
	if ((g_forceMisrouting == 1) || (flit->mandatoryGlobalMisrouting_flag == 1)) return true;

	/* If output port is a consumption port (destination is within current router),
	 * misrouting is forbidden.
	 */
	if (outPort < g_p_computing_nodes_per_router) return false;

	/* Misrouting condition depends on the misrouting trigger
	 * mechanism employed.
	 */
	int crd = switchM->getCredits(outPort, outVC);
	switch (g_misrouting_trigger) {
		case FILTERED: /* Currently it is treated as contention trigger (with internal differences) */
		case WEIGTHED_CA: /* For misrouting triggering purposes, it works like CA */
		case CA:
			/* Contention Aware: misroute if minimal path next link is found to have contention. */
			if (switchM->m_ca_handler.isThereContention(outPort)) result = true;
			break;

		case CGA:
			/* Congestion Aware: misroute if minimal outport occupancy exceeds min threshold and
			 * 		- Link has run out of credits OR
			 * 		- Link under use
			 */
			q_min = switchM->getCreditsOccupancy(outPort, outVC) * 100.0 / switchM->getMaxCredits(outPort, outVC);
			result = (q_min >= g_th_min) && ((crd < g_flit_size) || not (switchM->nextPortCanReceiveFlit(outPort)));
			break;

		case HYBRID:
			/* Congestion OR Contention Aware: misroute if any of the conditions below are accomplished,
			 * 	-Contention Aware condition: link is saturated.
			 * 	-COMPULSARY min threshold is exceeded AND any of Congestion Aware conditions is true:
			 * 		link in use, or link out of credits
			 */
			q_min = switchM->getCreditsOccupancy(outPort, outVC) * 100.0 / switchM->getMaxCredits(outPort, outVC);
			result = switchM->m_ca_handler.isThereContention(outPort)
					|| ((q_min >= g_th_min) && ((crd < g_flit_size) || not (switchM->nextPortCanReceiveFlit(outPort))));
			break;

		case CA_REMOTE:
			/* Remote Contention Aware: discriminates between local and global contention, but considering
			 * all global ports within the group. For that purpose, routers periodically tx partial
			 * counters with their inner contention statistics for every destination group. Doing so,
			 * misrouting trigger can prevent collapsing local links due to a global hotspot.
			 */
			if (switchM->m_ca_handler.isThereContention(outPort) || switchM->m_ca_handler.isThereGlobalContention(flit))
				result = true;
			break;

		case HYBRID_REMOTE:
			/* HYBRID (Congestion or Contention Aware) & REMOTE: triggers misrouting if there is contention
			 * (sending partial counters across routers within each group in that regard) or if link
			 * is saturated.
			 */
			q_min = switchM->getCreditsOccupancy(outPort, outVC) * 100.0 / switchM->getMaxCredits(outPort, outVC);
			result = switchM->m_ca_handler.isThereContention(outPort)
					|| switchM->m_ca_handler.isThereGlobalContention(flit)
					|| ((q_min >= g_th_min) && ((crd < g_flit_size) || not (switchM->nextPortCanReceiveFlit(outPort))));
			break;

		default:
			break;
	}

	/* Misrouting restriction applies if misrouting trigger is:
	 * 	-CGA (Congestion Aware)
	 * 	-HYBRID (Contention/Congestion Aware) & there is NO contention
	 * 	-HYBRID_REMOTE & there is NO contention (either local or global)
	 *
	 * 	In case so, it checks if output link is global and is saturated, and otherwise cancels
	 * 	misrouting.
	 */
	if (g_vc_misrouting_congested_restriction) {
		switch (g_misrouting_trigger) {
			case CGA:
				if (outPort >= g_global_router_links_offset
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router && outVC == 0) {
					result = result && globalLinkCongested[outPort - g_global_router_links_offset];
				}
				break;
			case HYBRID:
				if (outPort >= g_global_router_links_offset
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router && outVC == 0
						&& not switchM->m_ca_handler.isThereContention(outPort)) {
					result = result && globalLinkCongested[outPort - g_global_router_links_offset];
				}
				break;
			case HYBRID_REMOTE:
				if (outPort >= g_global_router_links_offset
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router && outVC == 0
						&& not (switchM->m_ca_handler.isThereContention(outPort)
								|| switchM->m_ca_handler.isThereGlobalContention(flit))) {
					result = result && globalLinkCongested[outPort - g_global_router_links_offset];
				}
				break;
			default:
				break;
		}
	}

	return result;
}

/*
 * Updates the value of globalLinkCongested (only if vc = 0) for
 * misrouting restriction applied under certain cases with CGA
 * or HYBRID misrouting triggers;
 */
void baseRouting::updateCongestionStatusGlobalLinks() {
	double qMean = 0.0;	//mean global queue occupancy
	int port, channel = 0;
	double threshold;

	/* Global links are considered to be saturated if their occupancy exceeds a percentage
	 * of the average of global queues in current switch, plus a minimum amount of flits. */
	for (port = g_global_router_links_offset; port < g_global_router_links_offset + g_h_global_ports_per_router;
			port++) {
		qMean += switchM->getCreditsOccupancy(port, channel);
	}
	qMean = qMean / g_h_global_ports_per_router;
	threshold = g_vc_misrouting_congested_restriction_coef_percent / 100.0 * qMean
			+ g_vc_misrouting_congested_restriction_th * g_flit_size;

	for (port = g_global_router_links_offset; port < g_global_router_links_offset + g_h_global_ports_per_router;
			port++) {
		globalLinkCongested[port - g_global_router_links_offset] = (switchM->getCreditsOccupancy(port, channel)
				> threshold);
	}

#if DEBUG
	cout << "Cycle " << g_cycle << "--> SW " << this->switchM->label << "VC "<< channel<< " Global Ports qMEAN = " << qMean << " Threshold = "
	<< threshold << endl;
#endif
}

/*
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references.
 * This function will iterate through local/global ports
 * (regarding misrouteType elected) and will choose amongst
 * those candidates that are valid.
 *
 */
bool baseRouting::misrouteCandidate(flitModule * flit, int inPort, int inVC, int minOutPort, int minOutVC,
		int &selectedPort, int &selectedVC, MisrouteType &misroute_type) {
	int random, num_candidates = 0, *candidates_port = NULL, *candidates_VC = NULL;
	double q_min, th_non_min;
	bool result = false;

	assert(minOutPort == minOutputPort(flit->destId)); /* Sanity check: given output port must belong to min path */
	q_min = switchM->getCreditsOccupancy(minOutPort, minOutVC) * 100.0 / switchM->getMaxCredits(minOutPort, minOutVC);

	misroute_type = this->misrouteType(inPort, inVC, flit, minOutPort, minOutVC);

	switch (misroute_type) {
		case NONE:
			result = false;
			break;

		case LOCAL:
		case LOCAL_MM:
			th_non_min = g_relative_threshold ? g_percent_local_threshold * q_min / 100.0 : g_percent_local_threshold;
			num_candidates = this->nominateCandidates(flit, inPort, minOutPort, th_non_min, misroute_type,
					candidates_port, candidates_VC);
			assert(num_candidates <= g_a_routers_per_group - 1);
			break;

		case GLOBAL:
		case GLOBAL_MANDATORY:
			assert(switchM->hPos == flit->sourceGroup); /* Sanity check: global misrouting is only allowed from source group. */
			th_non_min = g_relative_threshold ? g_percent_global_threshold * q_min / 100.0 : g_percent_global_threshold;
			num_candidates = this->nominateCandidates(flit, inPort, minOutPort, th_non_min, misroute_type,
					candidates_port, candidates_VC);
			assert(num_candidates <= g_h_global_ports_per_router);
			break;

		default:
			assert(false);
			break;
	}

	/* Select a random candidate among all valid ones: this will only
	 * happen if LOCAL or GLOBAL misrouting has been selected */
	if (num_candidates > 0) {
		random = rand() % num_candidates;
		result = true;
		selectedPort = candidates_port[random];
		selectedVC = candidates_VC[random];
		assert(selectedVC >= 0);
		assert(selectedVC < g_local_link_channels);
	} else {
		result = false;
		misroute_type = NONE;
	}

	delete[] candidates_port;
	delete[] candidates_VC;

	return result;
}

/*
 * Determines next channel, following Dally's
 * usage of Virtual Channels (if Dally's
 * convention is not being used, this function
 * will be overwritten by a specific one).
 */
int baseRouting::nextChannel(int inP, int outP, flitModule * flit) {
	char outType, inType;
	int next_channel, inVC = flit->channel;

	inType = portType(inP);
	outType = portType(outP);

	if ((inType == 'p'))
		next_channel = 0;
	else if (outP != minOutputPort(flit->destId) && outType == 'a')
		next_channel = inVC;
	else if (outType == 'p' || (inType == 'a' && outType == 'h'))
		next_channel = inVC;
	else
		next_channel = inVC + 1;

	assert(next_channel < g_channels);
	return (next_channel);
}

/*
 * Auxiliary function to determine port type. Returns a
 * char distinguishing between 'p' (compute node),
 * 'a' (local link), 'h' (global link) or 'r' (physical
 * ring link).
 */
char baseRouting::portType(int port) {
	char type;
	if (port < g_p_computing_nodes_per_router)
		type = 'p';
	else if (port < g_global_router_links_offset)
		type = 'a';
	else if (port < g_global_router_links_offset + g_h_global_ports_per_router)
		type = 'h';
	else {
		type = 'r';
		assert(port >= (portCount - 2));
		assert(g_deadlock_avoidance == RING);
	}
	return type;
}

/*
 * Auxiliary function to check if a given port is eligible
 * for misrouting (only used in certain routing mechanisms)
 */
bool baseRouting::validMisroutePort(flitModule * flit, int outP, int nextC, double threshold, MisrouteType misroute) {
	int crd = switchM->getCredits(outP, nextC);
	bool can_rx_flit = switchM->nextPortCanReceiveFlit(outP);
	int minOutP = minOutputPort(flit->destId);
	/* Discard those ports that are in use or out of credits */
	if (!can_rx_flit || crd < g_flit_size || minOutP == outP)
		return false;
	else if (g_forceMisrouting || (misroute == GLOBAL_MANDATORY)) return true;

	double q_non_min = switchM->getCreditsOccupancy(outP, nextC) * 100.0 / switchM->getMaxCredits(outP, nextC);
	bool valid_candidate = false;
	int nominations;
	switch (g_misrouting_trigger) {
		case CGA:
			valid_candidate = (q_non_min <= threshold);
			break;
		case CA_REMOTE:
		case FILTERED:
		case DUAL:
		case CA:
			if (not switchM->m_ca_handler.isThereContention(outP)) valid_candidate = true;
			break;
		case WEIGTHED_CA:
			nominations = int(g_contention_aware_th - switchM->m_ca_handler.getContention(outP) / g_flit_size);
			if ((rand() / ((double) RAND_MAX + 1)) < ((double) nominations / g_contention_aware_th)) valid_candidate =
					true;
			break;
		case HYBRID:
			if (switchM->m_ca_handler.isThereContention(minOutP) || q_non_min <= threshold) valid_candidate = true;
			break;
		case HYBRID_REMOTE:
			if (switchM->m_ca_handler.isThereContention(minOutP) || switchM->m_ca_handler.isThereGlobalContention(flit)
					|| q_non_min <= threshold) valid_candidate = true;
			break;

	}
	return valid_candidate;
}

/*
 * Auxiliary function to determine number of hops towards destination
 * (under a MINIMAL path).
 */
int baseRouting::hopsToDest(flitModule * flit, int outP) {
	if (outP < g_p_computing_nodes_per_router) return 0;

	switchModule* nextSw = neighList[outP];
	int path = 0, nextOutP = nextSw->routing->minOutputPort(flit->destId);

	while (nextSw->label != flit->destSwitch) {
		nextOutP = nextSw->routing->baseRouting::minOutputPort(flit->destId);
		nextSw = nextSw->routing->neighList[nextOutP];
		path++;
	}
	assert(path < 4);
	if (path == 3 && outP == minOutputPort(flit->destId)) {
		cerr << "ERROR computing path length from sw " << switchM->label << " (group " << switchM->hPos << ")"
				<< " towards dest " << flit->destSwitch << " (group " << flit->destGroup << ")" << endl;
		nextSw = neighList[outP];
		path = 0;
		nextOutP = nextSw->routing->baseRouting::minOutputPort(flit->destId);
		while (nextSw->label != flit->destSwitch) {
			cerr << "Hop to out " << nextOutP << ", sw " << nextSw->label << " --> path= " << path << endl;
			nextOutP = nextSw->routing->baseRouting::minOutputPort(flit->destId);
			nextSw = nextSw->routing->neighList[nextOutP];
			path++;
		}
	}
	assert(path < 3 || outP != minOutputPort(flit->destId));

	return path;
}

/*
 * Auxiliary function to determine number of hops towards destination
 * (under a MINIMAL path) for a given destination.
 */
int baseRouting::hopsToDest(int destination) {
	int destSw, destGroup, outP;

	destSw = int(destination / g_p_computing_nodes_per_router);
	destGroup = int(destSw / g_a_routers_per_group);

	outP = minOutputPort(destination);

	if (outP < g_p_computing_nodes_per_router) return 0;

	switchModule* nextSw = neighList[outP];
	int path = 1, nextOutP = nextSw->routing->minOutputPort(destination);

	while (nextSw->label != destSw) {
		nextOutP = nextSw->routing->baseRouting::minOutputPort(destination);
		nextSw = nextSw->routing->neighList[nextOutP];
		path++;
	}
	assert(path <= 3);

	return path;
}

