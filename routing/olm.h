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

#ifndef class_olm
#define class_olm

#include "routing.h"
#include "flexibleRouting.h"

template<class base>
class olm: public base {
protected:

public:
	olm(switchModule *switchM) :
			base(switchM) {
		const int minLocalVCs = 3, minGlobalVCs = 2;
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
		if (g_vc_usage == FLEXIBLE) {
			int petLocalVCs = g_local_link_channels - g_local_res_channels;
			int petGlobalVCs = g_global_link_channels - g_global_res_channels;
			int vc;
			/* Fill in VC arrays */
			if (g_global_link_channels > minGlobalVCs) {
				for (vc = petLocalVCs - minLocalVCs;
						vc <= (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1; vc++) {
					baseRouting::globalVc.push_back(vc);
				}
			}
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 4);
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 2);
			if (petLocalVCs > minLocalVCs) {
				for (vc = 0; vc <= petLocalVCs - minLocalVCs - 1; vc++) {
					baseRouting::localVcSource.push_back(vc);
					baseRouting::localVcInter.push_back(vc);
					baseRouting::localVcDest.push_back(vc);
				}
			}
			/* We include one additional VC value in local source array to fix 2 cases:
			 * A) Inter group is src group, to distinguish between minimal and misroute hop;
			 * B) Inter group is dest group, so local hop in source group is minimal. */
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 1);

			assert(baseRouting::globalVc.size() == petGlobalVCs);
			assert(baseRouting::localVcSource.size() == petLocalVCs - 1);
			assert(baseRouting::localVcInter.size() == petLocalVCs - 1);
			assert(baseRouting::localVcDest.size() == petLocalVCs);
			if (g_reactive_traffic) {
				/* Under Flexible VC usage, response messages can use both its
				 * reserved VCs plus those of the petitions. Although for the
				 * petition path we need 3/2 VCs, for responses we assume a
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
				baseRouting::localResVcSource = baseRouting::localVcDest;
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
				/* Source array could be added an extra VC, but it would further complicate
				 * VC calculation for local misrouting at source group (need to discard not
				 * only the highest VC but also the second highest, in order to ensure an
				 * upwards VC path -considering the global misrouting get its VC reused
				 * from the petition path-). If # of VCs is minimum allowed, it would mean
				 * to change from VC 5 to VC 2 because global hop would use VC 3. */
				//baseRouting::localResVcSource.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcInter.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 1);

				assert(baseRouting::globalResVc.size() == g_global_link_channels);
				assert(baseRouting::localResVcSource.size() == g_local_link_channels - 2);
				assert(baseRouting::localResVcInter.size() == g_local_link_channels - 1);
				assert(baseRouting::localResVcDest.size() == g_local_link_channels);
			}
		}

	}
	~olm() {
	}
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int destination, minOutP, minOutVC;
		bool misrouteFlit = false;
		MisrouteType misroute = NONE;

		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		minOutP = baseRouting::minOutputPort(destination);
		if (g_vc_usage == FLEXIBLE)
			minOutVC = base::nextChannel(inPort, minOutP, flit);
		else
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
		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		/* Update flit's current misrouting type */
		flit->setCurrentMisrouteType(misroute);

		/* Sanity check: ensure port and vc are within allowed ranges */
		assert(selectedRoute.port < (baseRouting::portCount));
		assert(selectedRoute.vc < g_channels); /* Is not restrictive enough, as it might be a forbidden vc for that port, but will be checked after */

		return selectedRoute;
	}

private:
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		MisrouteType result = NONE;

		/* Source group may be susceptible to global misroute */
		if ((flit->sourceGroup == baseRouting::switchM->hPos) && (flit->destGroup != baseRouting::switchM->hPos)) {
			assert(not (flit->globalMisroutingDone));
			switch (g_global_misrouting) {
				case CRG_L:
				case CRG:
					assert(not (flit->localMisroutingDone));
					result = GLOBAL;
					break;
				case NRG_L:
				case NRG:
					if (flit->mandatoryGlobalMisrouting_flag) {
						assert(flit->localMisroutingDone);
						assert((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset));
						result = GLOBAL_MANDATORY;
					} else if (inport < g_global_router_links_offset) result = LOCAL;
					break;
				case RRG_L:
				case RRG:
					/* Global mandatory: if previous hop has been a local misroute within source group, flit needs to be
					 * redirected to any global link within current router.
					 * If not, selects whether to do local or global misrouting evenly, based on number of ports for
					 * either misrouting. */
					if (flit->mandatoryGlobalMisrouting_flag) {
						assert(flit->localMisroutingDone);
						assert((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset));
						result = GLOBAL_MANDATORY;
					} else if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group))
							< g_a_routers_per_group - 1)
						result = LOCAL;
					else
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
					}/* Injection: select global misrouting */
					else if (inport < g_p_computing_nodes_per_router)
						result = GLOBAL;
					/* If packet is transitting locally (inport port is a local port) */
					else if ((inport >= g_local_router_links_offset) && (inport < g_global_router_links_offset)
							&& not (flit->localMisroutingDone)) result = LOCAL_MM;
					break;
				default:
					break;
			}
		}/* Source group is not liable to global misroute, try local */
		else if ((g_global_misrouting == CRG_L || g_global_misrouting == RRG_L || g_global_misrouting == MM_L
				|| g_global_misrouting == NRG_L) && not (flit->localMisroutingDone)
				&& not (baseRouting::minOutputPort(flit->destId) >= g_global_router_links_offset)) {
			assert(not (flit->mandatoryGlobalMisrouting_flag));
			result = LOCAL;
		}
#if DEBUG
		cout << "cycle " << g_cycle << "--> SW " << this->switchM->label << " input Port " << inport << " CV " << flit->channel
		<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
		<< baseRouting::minOutputPort(flit->destId) << " POSSIBLE misroute: ";
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
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		// Sanity checks for those parameters passed by address
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
				assert(baseRouting::switchM->hPos == flit->sourceGroup); // Sanity check: global misrouting is only allowed in source group.
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

		/* Determine all valid candidates */
		for (outP = port_offset; outP < port_limit; outP++) {
			if (outP == minOutP) continue;

			if (g_vc_usage == FLEXIBLE)
				nextC = base::nextChannel(inPort, outP, flit);
			else
				nextC = this->nextChannel(inPort, outP, flit);
			assert(nextC <= g_channels);
			valid_candidate = this->validMisroutePort(flit, outP, nextC, threshold, misroute);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;
			}
		}
		return num_candidates;
	}
	/*
	 * Determines next channel, following Dally's usage of Virtual Channels
	 * but addressing the specific case of misrouting ports (which will
	 * employ previous traversal VC).
	 */
	int nextChannel(int inP, int outP, flitModule * flit) {
		char outType, inType;
		int next_channel = -1, i, index, inVC = flit->channel;

		inType = baseRouting::portType(inP);
		outType = baseRouting::portType(outP);

		vector<int> auxVc;
		/* Reactive traffic splits evenly the channels between petition and response
		 * flits, so each kind of traffic receives the same number of resources. */
		if (g_reactive_traffic && flit->flitType == RESPONSE)
			auxVc = baseRouting::responseVc;
		else
			auxVc = baseRouting::petitionVc;

		assert(auxVc.size() == baseRouting::typeVc.size());

		if (inType == 'p')
			index = -1;
		else {
			for (i = 0; i < auxVc.size(); i++) {
				if (inType == baseRouting::typeVc[i] && inVC == auxVc[i]) {
					index = i;
					break;
				}
			}
		}
		assert(index < (int) auxVc.size());
		/* If next hop is local misrouting, VC is picked from immediate previous
		 * local hop VC. */
		if (outP != baseRouting::minOutputPort(flit->destId) && outType == 'a' && inType != 'p') {
			for (i = index; i >= 0; i--) {
				if (baseRouting::typeVc[i] == 'a') {
					next_channel = auxVc[i];
					break;
				}
			}
		} else {
			for (i = index + 1; i < auxVc.size(); i++) {
				if (outType == baseRouting::typeVc[i]) {
					next_channel = auxVc[i];
					break;
				}
			}
		}

		if (outType == 'p'
				|| (baseRouting::switchM->hPos == flit->sourceGroup && flit->localMisroutingDone && outType == 'a'))
			next_channel = inVC;
		assert(next_channel >= 0 && next_channel < g_channels); // Sanity check
		return (next_channel);
	}
};

#endif
