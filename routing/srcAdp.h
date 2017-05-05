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

#ifndef class_sourceAdp
#define class_sourceAdp

#include "routing.h"
#include "flexibleRouting.h"

template<class base>
class sourceAdp: public base {
protected:
	int ***lastValNodeSet;

public:

	sourceAdp(switchModule *switchM) :
			base(switchM) {
		const int minLocalVCs = 4, minGlobalVCs = 2;
		int petLocalVCs = g_local_link_channels - g_local_res_channels;
		int petGlobalVCs = g_global_link_channels - g_global_res_channels;
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
		assert(petLocalVCs >= minLocalVCs && petGlobalVCs >= minGlobalVCs);
		if (g_vc_usage == FLEXIBLE) {
			int vc;
			/* Fill in VC arrays */
			if (g_global_link_channels > minGlobalVCs) {
				for (vc = petLocalVCs - minLocalVCs;
						vc <= (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1; vc++) {
					baseRouting::globalVc.push_back(vc);
				}
			}
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 2);
			if (petLocalVCs > minLocalVCs) {
				for (vc = 0; vc <= petLocalVCs - minLocalVCs - 1; vc++) {
					baseRouting::localVcSource.push_back(vc);
					baseRouting::localVcInter.push_back(vc);
					baseRouting::localVcDest.push_back(vc);
				}
			}
			/* We include an additional VC value in source array to fix 2 cases:
			 A) Inter group is src group, to distinguish between minimal and misroute hop;
			 B) Inter group is dest group, so local hop in source group is minimal. */
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 6);
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 6);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 4);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 6);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 4);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 1);

			assert(baseRouting::globalVc.size() == petGlobalVCs);
			assert(baseRouting::localVcSource.size() == petLocalVCs - 2);
			assert(baseRouting::localVcInter.size() == petLocalVCs - 1);
			assert(baseRouting::localVcDest.size() == petLocalVCs);

			if (g_reactive_traffic) {
				/* Under Flexible VC usage, response messages can use both its
				 * reserved VCs plus those of the petitions. Although for the
				 * petition path we need 4/2 VCs, for responses we assume a
				 * minimum of 2/1 additional VCs (to ensure upwards VC path) */
				const int minLocalResVCs = 2, minGlobalResVCs = 1;
				assert(g_local_res_channels >= minLocalResVCs && g_global_res_channels >= minGlobalResVCs);
				baseRouting::globalResVc = baseRouting::globalVc;
				if (g_global_res_channels > minGlobalResVCs) {
					for (vc = g_local_link_channels - minLocalResVCs + petGlobalVCs;
							vc <= g_local_link_channels - minLocalResVCs + g_global_link_channels - minGlobalResVCs - 1;
							vc++) {
						baseRouting::globalResVc.push_back(vc);
					}
				}
				baseRouting::globalResVc.push_back(g_local_link_channels + g_global_link_channels - 2);
				baseRouting::localResVcSource = baseRouting::localVcInter;
				baseRouting::localResVcInter = baseRouting::localVcDest;
				baseRouting::localResVcDest = baseRouting::localVcDest;
				if (g_local_res_channels > minLocalResVCs) {
					for (vc = petLocalVCs + petGlobalVCs;
							vc <= g_local_link_channels - minLocalResVCs + petGlobalVCs - 1; vc++) {
						baseRouting::localResVcSource.push_back(vc);
						baseRouting::localResVcInter.push_back(vc);
						baseRouting::localResVcDest.push_back(vc);
					}
				}
				baseRouting::localResVcSource.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcInter.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 1);

				assert(baseRouting::globalResVc.size() == g_global_link_channels);
				assert(baseRouting::localResVcSource.size() == g_local_link_channels - 2);
				assert(baseRouting::localResVcInter.size() == g_local_link_channels - 1);
				assert(baseRouting::localResVcDest.size() == g_local_link_channels);
			}
		} else if (g_vc_usage == BASE) {
			/* Remove default VC and port type ordering to replace with specific for oblivious routing */
			baseRouting::typeVc.clear();
			baseRouting::petitionVc.clear();
			baseRouting::responseVc.clear();
			char aux[] = { 'a', 'h', 'a', 'a', 'h', 'a' };
			baseRouting::typeVc.insert(baseRouting::typeVc.begin(), aux, aux + 6);
			int aux2[] = { 0, 0, 1, 2, 1, 3 };
			baseRouting::petitionVc.insert(baseRouting::petitionVc.begin(), aux2, aux2 + 6);
			if (g_reactive_traffic) {
				assert(g_local_link_channels >= 2 * minLocalVCs && g_global_link_channels >= 2 * minGlobalVCs);
				int aux3[] = { 4, 2, 5, 6, 3, 7 };
				baseRouting::responseVc.insert(baseRouting::responseVc.begin(), aux3, aux3 + 6);
			}
		}
		lastValNodeSet = new int **[baseRouting::portCount];
		for (int port = 0; port < baseRouting::portCount; port++) {
			lastValNodeSet[port] = new int*[g_cos_levels];
			for (int cos = 0; cos < g_cos_levels; cos++) {
				lastValNodeSet[port][cos] = new int[g_channels];
				for (int vc = 0; vc < g_channels; vc++)
					lastValNodeSet[port][cos][vc] = -1;
			}
		}
	}
	~sourceAdp() {
	}
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int minOutP, minOutVC, nonMinOutP, intNode, intSW, intGroup;
		MisrouteType misroute = NONE;

		/* Determine minimal output port (& VC) */
		minOutP = baseRouting::minOutputPort(flit->destId);
		assert(minOutP >= 0 && minOutP < baseRouting::portCount);
		intNode = flit->valId;

		/* When injecting determine the intermediate node to be used */
		if (inPort < g_p_computing_nodes_per_router
				&& (g_reset_val || lastValNodeSet[inPort][flit->cos][inVC] != flit->flitId)) {
			/* Calculate non-minimal output port: select between neighbor or remote intermediate group */
			minOutVC = -1; /* Since it is not used in the misrouteType() function, we introduce a non-valid value */
			misroute = this->misrouteType(inPort, inVC, flit, minOutP, minOutVC);
			switch (misroute) {
				case LOCAL:
					nonMinOutP = rand() % (g_a_routers_per_group - 1) + g_local_router_links_offset;
					assert(nonMinOutP >= 0 && nonMinOutP < g_global_router_links_offset);
					intGroup = base::neighList[nonMinOutP]->routing->neighList[g_global_router_links_offset
							+ rand() % (g_h_global_ports_per_router)]->hPos;
					assert(
							intGroup != baseRouting::switchM->hPos
									&& intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
					break;
				case GLOBAL:
					nonMinOutP = rand() % (g_h_global_ports_per_router) + g_global_router_links_offset;
					assert(nonMinOutP >= 0 && nonMinOutP < baseRouting::portCount);
					intGroup = baseRouting::neighList[nonMinOutP]->hPos;
					assert(
							intGroup != baseRouting::switchM->hPos
									&& intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
					break;
				default:
					// This point should never be reached
					assert(false);
					break;
			}
			intSW = rand() % g_a_routers_per_group + intGroup * g_a_routers_per_group;
			assert(intSW < g_number_switches);
			intNode = rand() % g_p_computing_nodes_per_router + intSW * g_p_computing_nodes_per_router;
			assert(0 < intNode < g_number_generators);
			flit->valId = intNode;
			lastValNodeSet[inPort][flit->cos][inVC] = flit->flitId;

			/* Calculate non-minimal path length */
			int nonMinPathLength = 1;
			switchModule *sw = base::neighList[nonMinOutP];
			while (sw->label != intSW) {
				sw = sw->routing->neighList[sw->routing->minOutputPort(intNode)];
				nonMinPathLength++;
			}
			assert(nonMinPathLength == baseRouting::hopsToDest(flit->valId));
			nonMinPathLength += sw->routing->hopsToDest(flit->destId);
			if (nonMinPathLength == 6) nonMinPathLength--;
			flit->valPathLength = nonMinPathLength;
		}

		nonMinOutP = baseRouting::minOutputPort(intNode);
		/* If Valiant node is in current switch, update check within flit. */
		if (baseRouting::switchM->label == int(intNode / g_p_computing_nodes_per_router)) flit->valNodeReached = true;

		if (not (this->misrouteCondition(flit, inPort))) {
			selectedRoute.port = minOutP;
			flit->setCurrentMisrouteType(NONE);
			if (inPort < g_p_computing_nodes_per_router) flit->setMisrouted(false, NONE);
		} else {
			selectedRoute.port = nonMinOutP;
			flit->setCurrentMisrouteType(VALIANT);
			flit->setMisrouted(true, VALIANT);
		}

		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

		/* Sanity checks */
		assert(selectedRoute.port < baseRouting::portCount && selectedRoute.port >= 0);
		assert(selectedRoute.neighPort < baseRouting::portCount && selectedRoute.neighPort >= 0);
		assert(selectedRoute.vc < g_channels && selectedRoute.vc >= 0);

		return selectedRoute;
	}

private:
	/*
	 * Determine type of misrouting to be performed, upon global misrouting policy.
	 */
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		MisrouteType result = NONE;

		/* Sanity assert to ensure not any flit is assigned misroute path more than once, unless specifically enforced*/
		assert(flit->getMisrouteCount(NONE) == 0 || g_reset_val);

		/* Source group may be susceptible to global misroute */
		if ((flit->sourceGroup == baseRouting::switchM->hPos)) {
			assert(not (flit->globalMisroutingDone));
			switch (g_global_misrouting) {
				case CRG_L:
				case CRG:
					assert(not (flit->localMisroutingDone));
					result = GLOBAL;
					break;
				case RRG_L:
				case RRG:
					/* Global mandatory: if previous hop has been a local misroute within source group, flit needs to be
					 * redirected to any global link within current router.
					 * If not, selects whether to do local or global misrouting evenly, based on number of ports for
					 * either misrouting. */
					if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group))
							< g_a_routers_per_group - 1)
						result = LOCAL;
					else
						result = GLOBAL;
					break;
				default:
					/* Other global misrouting policies do not make sense in oblivious routing */
					assert(false);
					break;
			}
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

	/*
	 * Redefined to suit PiggyBacking-alike misrouting condition. PB implies UGAL-L(ocal) conditions
	 * (misroute if packet is being injected and its destination node is linked to a different
	 * switch, & both local/global restrictions are fulfilled) and global link congested
	 * condition.
	 */
	bool misrouteCondition(flitModule * flit, int inPort) {
		int destination, minOutPort, nonMinOutPort, minQueueLength, nonMinQueueLength;
		bool result = false;

		destination = flit->destId;
		minOutPort = this->minOutputPort(destination);
		nonMinOutPort = this->minOutputPort(flit->valId);

		if ((inPort < g_p_computing_nodes_per_router) && (minOutPort >= g_p_computing_nodes_per_router)) {
			minQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
					this->nextChannel(inPort, minOutPort, flit)); //Credit occupancy for the minimal path outport
			nonMinQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort, flit->cos,
					this->nextChannel(inPort, nonMinOutPort, flit)); //Credit occupancy for the Valiant outport
			/* UGAL-L(ocal) condition: if the product of minimal outport queue occupancy and the
			 * minimal path length is greater than the corresponding of non-minimal path (plus a
			 * local threshold), then misroute. */
			if (((minQueueLength * flit->minPathLength)
					> (nonMinQueueLength * flit->valPathLength) + g_flit_size * g_ugal_local_threshold)
					|| baseRouting::switchM->isGlobalLinkCongested(flit)) {
				result = true;
			}
		} else if (flit->getCurrentMisrouteType() == VALIANT && !flit->valNodeReached) {
			/* If flit has been previously marked as misrouted and Val node has not yet been
			 * reached, choose min path to Val node */
			result = true;
		}

		if (minOutPort < g_p_computing_nodes_per_router) result = false;

		return result;
	}

	/*
	 * Defined for coherence reasons. Should never be called.
	 */
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}
};

#endif
