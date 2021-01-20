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

//#include <iterator>
#include "car.h"

contAdpRouting::contAdpRouting(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	int minLocalVCs = 5, minGlobalVCs = 2;
	/* Set up the vc management,,considering the opportunistic misroute hop at the intermediate group */
	portClass aux[] = { portClass::local, portClass::opplocal, portClass::oppglobal, portClass::opplocal,
			portClass::local, portClass::global, portClass::local };
	vector<portClass> typeVc(aux, aux + minLocalVCs + minGlobalVCs);
	if (g_vc_usage == TBFLEX) {
		minLocalVCs = 3, minGlobalVCs = 2;
		vcM = new tbFlexVc(&typeVc, switchM);
	} else if (g_vc_usage == FLEXIBLE) {
		minLocalVCs = 3, minGlobalVCs = 2;
		this->vcM = new flexVc(&typeVc, switchM);
	} else if (g_vc_usage == BASE) {
		this->vcM = new vcMngmt(&typeVc, switchM);
	}
	this->vcM->checkVcArrayLengths(minLocalVCs, minGlobalVCs);
	lastValNodeSet = new int **[portCount];
	for (int port = 0; port < portCount; port++) {
		lastValNodeSet[port] = new int*[g_cos_levels];
		for (int cos = 0; cos < g_cos_levels; cos++) {
			lastValNodeSet[port][cos] = new int[g_channels];
			for (int vc = 0; vc < g_channels; vc++)
				lastValNodeSet[port][cos][vc] = -1;
		}
	}
	if (g_congestion_detection == HISTORY_WINDOW || g_congestion_detection == HISTORY_WINDOW_AVG) {
		congestionWindow = new vector<int> *[portCount];
		for (int port = 0; port < portCount; port++) {
			congestionWindow[port] = new vector<int> [g_cos_levels];
			for (int cos = 0; cos < g_cos_levels; cos++) {
				int winSize = (port < g_global_router_links_offset) ? g_local_window_size : g_global_window_size;
				for (int i = 0; i < winSize; i++)
					this->congestionWindow[port][cos].push_back(0);
			}
		}
	}
}

contAdpRouting::~contAdpRouting() {
}

candidate contAdpRouting::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int minOutP, minOutVC, nonMinOutP, intNode, intSW, intGroup;
	MisrouteType misroute = NONE;

	/* Determine minimal output port (& VC) */
	minOutP = minOutputPort(flit->destId);
	assert(minOutP >= 0 && minOutP < portCount);
	intNode = flit->valId;

	/* Determine intermediate Valiant node (when at injection) and its associated output port */
	if (inPort < g_p_computing_nodes_per_router
			&& (g_reset_val || lastValNodeSet[inPort][flit->cos][inVC] != flit->flitId)) {
		/* Calculate non-minimal output port: select between neighbor or remote intermediate group */
		minOutVC = -1; /* Since it is not used in the misrouteType() function, we introduce a non-valid value */
		misroute = this->misrouteType(inPort, inVC, flit, minOutP, minOutVC);
		switch (misroute) {
			case LOCAL:
				nonMinOutP = rand() % (g_a_routers_per_group - 1) + g_local_router_links_offset;
				assert(nonMinOutP >= 0 && nonMinOutP < g_global_router_links_offset);
				intGroup = neighList[nonMinOutP]->routing->neighList[g_global_router_links_offset
						+ rand() % (g_h_global_ports_per_router)]->hPos;
				assert(intGroup != switchM->hPos && intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
				break;
			case GLOBAL:
				nonMinOutP = rand() % (g_h_global_ports_per_router) + g_global_router_links_offset;
				assert(nonMinOutP >= 0 && nonMinOutP < portCount);
				intGroup = neighList[nonMinOutP]->hPos;
				assert(intGroup != switchM->hPos && intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
				break;
			default:
				/* Misroute type must determine if Valiant group is directly linked to current router (GLOBAL
				 * misroute) or to a different router (LOCAL misroute). Any other case should never happen. */
				assert(false);
				break;
		}
		intSW = rand() % g_a_routers_per_group + intGroup * g_a_routers_per_group;
		assert(intSW < g_number_switches);
		intNode = rand() % g_p_computing_nodes_per_router + intSW * g_p_computing_nodes_per_router;
		assert(intNode < g_number_generators);
		flit->valId = intNode;
		lastValNodeSet[inPort][flit->cos][inVC] = flit->flitId;

		/* Calculate non-minimal path length */
		int nonMinPathLength = 1;
		switchModule *sw = neighList[nonMinOutP];
		while (sw->label != intSW) {
			sw = sw->routing->neighList[sw->routing->minOutputPort(intNode)];
			nonMinPathLength++;
		}
		assert(nonMinPathLength == hopsToDest(flit->valId));
		nonMinPathLength += sw->routing->hopsToDest(flit->destId);
		if (nonMinPathLength == 6) nonMinPathLength--;
		flit->valPathLength = nonMinPathLength;
	}
	nonMinOutP = minOutputPort(intNode);

	/* If Valiant node is in current switch, update check within flit. */
	if (switchM->label == int(intNode / g_p_computing_nodes_per_router)) flit->valNodeReached = true;

	/* Decide between minimal and Valiant routing */
	flit->setCurrentMisrouteType(NONE);
	if (!(misrouteCondition(flit, inPort))) {
		selectedRoute.port = minOutP;
		if (inPort < g_p_computing_nodes_per_router) flit->setMisrouted(false, NONE);
	} else {
		selectedRoute.port = nonMinOutP;
		flit->setCurrentMisrouteType(VALIANT);
		flit->setMisrouted(true, VALIANT);
	}

	selectedRoute.neighPort = neighPort[selectedRoute.port];
	selectedRoute.vc = vcM->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < portCount && selectedRoute.neighPort >= 0);
	assert(selectedRoute.vc < g_channels && selectedRoute.vc >= 0);

	return selectedRoute;
}

/*
 * Determine type of misrouting to be performed, upon global misrouting policy.
 */
MisrouteType contAdpRouting::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	MisrouteType result = NONE;

	/* Sanity assert to ensure not any flit is assigned a misroute path more than once */
	assert(flit->getMisrouteCount(NONE) == 0 || g_reset_val);

	/* Source group may be susceptible to global misroute */
	if ((flit->sourceGroup == switchM->hPos)) {
		assert(not (flit->globalMisroutingDone));
		switch (g_global_misrouting) {
			case CRG_L:
			case CRG:
				assert(not (flit->localMisroutingDone));
				result = GLOBAL;
				break;
			case RRG_L:
			case RRG:
				/* Random decision: check if selected port would be a local or global port based on the number of
				 * local and global ports */
				if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group)) < g_a_routers_per_group - 1)
					result = LOCAL;
				else
					result = GLOBAL;
				break;
			case MM_L:
			case MM:
				/* Mixed-Mode: employ CRG at injection, and NRG after a local hop */
				if (inport < g_p_computing_nodes_per_router)
					result = GLOBAL;
				else
					result = LOCAL;
				break;
			default:
				/* Other global misrouting policies do not make sense in oblivious routing */
				assert(false);
				break;
		}
	}
#if DEBUG
	cout << "Cycle " << g_cycle << " Sw " << switchM->label << " destSw " << flit->destSwitch
	<< " misroute Valiant path type: ";
	switch (result) {
		case LOCAL:
		cout << " LOCAL" << endl;
		break;
		case GLOBAL:
		cout << " GLOBAL" << endl;
		break;
	}
#endif

	return result;
}

/*
 * Redefined to suit FlOp misrouting condition: decision between minimal and non-minimal
 * routing can be made at injection and at the source and intermediate paths. Decision
 * depends on the congestion of each port and two thresholds.
 */
bool contAdpRouting::misrouteCondition(flitModule * flit, int inPort) {
	int destination, minOutPort, nonMinOutPort;
	float minQueueLength, nonMinQueueLength;
	bool result = false;

	destination = flit->destId;
	minOutPort = this->minOutputPort(destination);
	nonMinOutPort = this->minOutputPort(flit->valId);

	/* If the minimal output is a consumption port, Valiant routing has already been performed,
	 * or both source and destination are located within the same group, route minimally. */
	if (minOutPort < g_p_computing_nodes_per_router || flit->valNodeReached || flit->sourceGroup == flit->destGroup)
		return false;

	/* Determine credit occupancy for both MIN and VAL ports to decide whether to misroute or not, if packet is:
	 * -at injection
	 * -at source group, after a minimal local hop
	 * -at intermediate group, at a router not directly attached to the destination group.
	 * */
	if (inPort < g_p_computing_nodes_per_router
			|| (switchM->hPos == flit->sourceGroup && flit->getCurrentMisrouteType() == NONE)
			|| (switchM->hPos == int(flit->valId / g_p_computing_nodes_per_router / g_a_routers_per_group)
					&& minOutPort < g_global_router_links_offset)) {
		int maxVc;
		vector<int> vc_array;
		switch (g_congestion_detection) {
			case PER_PORT:
				minQueueLength = 0;
				vc_array = vcM->getVcArray(minOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					minQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
							*it) * 100.0 / baseRouting::switchM->getMaxCredits(minOutPort, flit->cos, *it);
				}
				minQueueLength /= vc_array.size();
				nonMinQueueLength = 0;
				vc_array = vcM->getVcArray(nonMinOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort,
							flit->cos, *it) * 100.0
							/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos, *it);
				}
				nonMinQueueLength /= vc_array.size();
				break;
			case PER_VC:
				minQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
						baseRouting::vcM->nextChannel(inPort, minOutPort, flit)) * 100.0
						/ baseRouting::switchM->getMaxCredits(minOutPort, flit->cos,
								baseRouting::vcM->nextChannel(inPort, minOutPort, flit));
				nonMinQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort, flit->cos,
						baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit)) * 100.0
						/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos,
								baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit));
				break;
			case PER_PORT_MIN:
				minQueueLength = 0;
				vc_array = vcM->getVcArray(minOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					minQueueLength += baseRouting::switchM->switchModule::getCreditsMinOccupancy(minOutPort, flit->cos,
							*it) * 100.0 / baseRouting::switchM->getMaxCredits(minOutPort, flit->cos, *it);
				}
				minQueueLength /= vc_array.size();
				nonMinQueueLength = 0;
				vc_array = vcM->getVcArray(nonMinOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0);
					nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsMinOccupancy(nonMinOutPort,
							flit->cos, *it) * 100.0
							/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos, *it);
				}
				nonMinQueueLength /= vc_array.size();
				break;
			case PER_VC_MIN:
				minQueueLength = baseRouting::switchM->switchModule::getCreditsMinOccupancy(minOutPort, flit->cos,
						baseRouting::vcM->nextChannel(inPort, minOutPort, flit)) * 100.0
						/ baseRouting::switchM->getMaxCredits(minOutPort, flit->cos,
								baseRouting::vcM->nextChannel(inPort, minOutPort, flit));
				nonMinQueueLength = baseRouting::switchM->switchModule::getCreditsMinOccupancy(nonMinOutPort, flit->cos,
						baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit)) * 100.0
						/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos,
								baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit));
				break;
			case PER_GROUP:
				/* If flit is at its source group, check those VCs that can be used in the source group. Otherwise,
				 * check by its concrete VC. Currently does not support reactive traffic, it would require to
				 * distinguish between petition and response flits. */
				assert(!g_reactive_traffic);
				if (flit->sourceGroup == baseRouting::switchM->hPos) {
					minQueueLength = 0;
					vc_array = vcM->getSrcVcArray(minOutPort);
					for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
						assert(*it >= 0 && *it < g_channels);
						minQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
								*it) * 100.0 / baseRouting::switchM->getMaxCredits(minOutPort, flit->cos, *it);
					}
					minQueueLength /= vc_array.size();
					nonMinQueueLength = 0;
					vc_array = vcM->getSrcVcArray(nonMinOutPort);
					for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
						assert(*it >= 0 && *it < g_channels);
						nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort,
								flit->cos, *it) * 100.0
								/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos, *it);
					}
					nonMinQueueLength /= vc_array.size();
				} else {
					minQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
							baseRouting::vcM->nextChannel(inPort, minOutPort, flit)) * 100.0
							/ baseRouting::switchM->getMaxCredits(minOutPort, flit->cos,
									baseRouting::vcM->nextChannel(inPort, minOutPort, flit));
					nonMinQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort,
							flit->cos, baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit)) * 100.0
							/ baseRouting::switchM->getMaxCredits(nonMinOutPort, flit->cos,
									baseRouting::vcM->nextChannel(inPort, nonMinOutPort, flit));
				}
				break;
			case HISTORY_WINDOW:
				minQueueLength = 0;
				vc_array = vcM->getVcArray(minOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					minQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
							*it);
				}
				nonMinQueueLength = 0;
				vc_array = vcM->getVcArray(nonMinOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort,
							flit->cos, *it);
				}
				int winSize;
				winSize = (minOutPort < g_global_router_links_offset) ? g_local_window_size : g_global_window_size;
				assert(this->congestionWindow[minOutPort][flit->cos].size() == winSize);
				winSize = (nonMinOutPort < g_global_router_links_offset) ? g_local_window_size : g_global_window_size;
				assert(this->congestionWindow[nonMinOutPort][flit->cos].size() == winSize);

				for (int i = 0; i < this->congestionWindow[minOutPort][flit->cos].size(); i++)
					minQueueLength -= this->congestionWindow[minOutPort][flit->cos][i];
				for (int i = 0; i < this->congestionWindow[nonMinOutPort][flit->cos].size(); i++)
					nonMinQueueLength -= this->congestionWindow[nonMinOutPort][flit->cos][i];
				break;
			case HISTORY_WINDOW_AVG:
				maxVc = (minOutPort < g_global_router_links_offset) ? g_local_link_channels : g_global_link_channels;
				minQueueLength = 0;
				vc_array = vcM->getVcArray(minOutPort);
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					minQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
							*it);
				}
				for (int i = 0; i < this->congestionWindow[minOutPort][flit->cos].size(); i++)
					minQueueLength -= this->congestionWindow[minOutPort][flit->cos][i];
				nonMinQueueLength = 0;
				/* Check first the local ports */
				vc_array = vcM->getVcArray(g_local_router_links_offset);
				for (int i = g_local_router_links_offset; i < g_global_router_links_offset; i++) {
					if (i == minOutPort) continue;
					for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
						assert(*it >= 0 && *it < g_channels);
						nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(i, flit->cos, *it);
					}
					for (int j = 0; j < this->congestionWindow[i][flit->cos].size(); j++)
						nonMinQueueLength -= this->congestionWindow[i][flit->cos][j];
				}
				/* Then check the global ports */
				vc_array = vcM->getVcArray(g_global_router_links_offset);
				for (int i = g_global_router_links_offset; i < g_ports; i++) {
					if (i == minOutPort) continue;
					for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
						assert(*it >= 0 && *it < g_channels);
						nonMinQueueLength += baseRouting::switchM->switchModule::getCreditsOccupancy(i, flit->cos, *it);
					}
					for (int j = 0; j < this->congestionWindow[i][flit->cos].size(); j++)
						nonMinQueueLength -= this->congestionWindow[i][flit->cos][j];
				}
				nonMinQueueLength /= (g_ports - g_p_computing_nodes_per_router - 1);
				break;
		}
		if ((minQueueLength > (nonMinQueueLength * g_car_misroute_factor + g_car_misroute_th))
				&& (switchM->getCredits(nonMinOutPort, flit->cos, vcM->nextChannel(inPort, nonMinOutPort, flit))
						>= g_flit_size)) result = true;
	} else if (switchM->hPos == flit->sourceGroup && flit->getCurrentMisrouteType() != NONE) {
		/* Source group, after a non-minimal local hop */
		result = true;
	}

	return result;
}

