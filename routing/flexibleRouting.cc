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

#include "flexibleRouting.h"

flexibleVcRouting::flexibleVcRouting(switchModule *switchM) :
		baseRouting(switchM) {

}

flexibleVcRouting::~flexibleVcRouting() {

}

int flexibleVcRouting::nextChannel(int inP, int outP, flitModule * flit) {
	char inType;
	int next_channel = -1, check_channel, min_occupancy;
	vector<int> vc_array;

	if (outP < g_p_computing_nodes_per_router) return (flit->channel);

	if (g_reactive_traffic && flit->flitType == RESPONSE) {
		if (portType(outP) == 'h') {
			/* Global hop: check if misrouting is used or not */
			vc_array = baseRouting::globalResVc;
		} else {
			assert(portType(outP) == 'a');
			/* Check what current group is */
			if (switchM->hPos == flit->destGroup) { /* Destination group */
				vc_array = baseRouting::localResVcDest;
			} else if ((switchM->hPos == int(flit->valId / (g_a_routers_per_group * g_p_computing_nodes_per_router)))
					|| flit->globalMisroutingDone) { /* Intermediate group */
				vc_array = baseRouting::localResVcInter;
			} else { /* Source group */
				assert(switchM->hPos == flit->sourceGroup);
				vc_array = baseRouting::localResVcSource;
			}
		}
	} else {
		if (portType(outP) == 'h') {
			/* Global hop: check if misrouting is used or not */
			vc_array = baseRouting::globalVc;
		} else {
			assert(portType(outP) == 'a');
			/* Check what current group is */
			if (switchM->hPos == flit->destGroup) { /* Destination group */
				vc_array = baseRouting::localVcDest;
			} else if ((switchM->hPos == int(flit->valId / (g_a_routers_per_group * g_p_computing_nodes_per_router)))
					|| flit->globalMisroutingDone) { /* Intermediate group */
				vc_array = baseRouting::localVcInter;
			} else { /* Source group */
				assert(switchM->hPos == flit->sourceGroup);
				vc_array = baseRouting::localVcSource;
			}
		}
	}
	if (outP != minOutputPort(flit->destId)
			|| (flit->getCurrentMisrouteType() == VALIANT && not (flit->valNodeReached))) vc_array.pop_back();

	/* Select VC with highest number of credits available, between all allowed VCs */
	min_occupancy = switchM->getMaxCredits(outP, flit->cos, 0) + 1;
	if (g_vc_alloc == LOWEST_VC) {
		for (vector<int>::iterator it = vc_array.begin(); it != vc_array.end(); ++it) {
			check_channel = *it;
			assert(check_channel >= 0);
			if (switchM->getCredits(outP, flit->cos, check_channel) >= g_packet_size || it == vc_array.end() - 1) {
				next_channel = check_channel;
				it = vc_array.end() - 1;
			}
		}
	} else {
		for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
			check_channel = *it;
			assert(check_channel >= 0 && check_channel < g_channels);
			switch (g_vc_alloc) {
				case LOWEST_OCCUPANCY:
					if (switchM->getCreditsOccupancy(outP, flit->cos, check_channel) < min_occupancy) {
						next_channel = check_channel;
						min_occupancy = switchM->getCreditsOccupancy(outP, flit->cos, check_channel);
					}
					break;
				case HIGHEST_VC:
					if (switchM->getCredits(outP, flit->cos, check_channel) >= g_packet_size
							|| it == vc_array.begin()) {
						next_channel = check_channel;
						it = vc_array.begin();
					}
					break;
				case RANDOM_VC:
					if (switchM->getCredits(outP, flit->cos, check_channel) < g_packet_size && vc_array.size() > 1) {
						vc_array.erase(it);
					}
					break;
			}
		}
	}

	assert(vc_array.size() > 0);
	if (g_vc_alloc == RANDOM_VC) {
		int random = rand() % vc_array.size();
		assert(random >= 0 && random < vc_array.size());
		next_channel = vc_array[random];
	}
	assert(next_channel >= 0 && next_channel < g_local_link_channels + g_global_link_channels); // Sanity check
	return (next_channel);
}

/*
 * Returns true if the flit should be misrouted.
 */
bool flexibleVcRouting::misrouteCondition(flitModule * flit, int outPort, int outVC) {
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
	int crd = switchM->getCredits(outPort, flit->cos, outVC);
	vector<int> vc_array;
	if (g_reactive_traffic)
		vc_array = outPort < g_global_router_links_offset ? baseRouting::localResVcDest : baseRouting::globalResVc;
	else
		vc_array = outPort < g_global_router_links_offset ? baseRouting::localVcDest : baseRouting::globalVc;
	if (g_misrouting_trigger == CGA || g_misrouting_trigger == HYBRID || g_misrouting_trigger == HYBRID_REMOTE) {
		switch (g_congestion_detection) {
			case PER_PORT:
				q_min = 0;
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					q_min += switchM->getCreditsOccupancy(outPort, flit->cos, *it) * 100.0
							/ switchM->getMaxCredits(outPort, flit->cos, outVC);
				}
				q_min /= vc_array.size();
				break;
			case PER_VC:
				q_min = switchM->getCreditsOccupancy(outPort, flit->cos, outVC) * 100.0
						/ switchM->getMaxCredits(outPort, flit->cos, outVC);
				break;
			case PER_PORT_MIN:
				q_min = 0;
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0);
					q_min += switchM->getCreditsMinOccupancy(outPort, flit->cos, *it) * 100.0
							/ switchM->getMaxCredits(outPort, flit->cos, outVC);
				}
				q_min /= vc_array.size();
				break;
			case PER_VC_MIN:
				q_min = switchM->getCreditsMinOccupancy(outPort, flit->cos, outVC) * 100.0
						/ switchM->getMaxCredits(outPort, flit->cos, outVC);
				break;
		}
	}

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
			result = q_min >= g_th_min && (crd < g_flit_size || not switchM->nextPortCanReceiveFlit(outPort));
			break;

		case HYBRID:
			/* Congestion OR Contention Aware: misroute if any of the conditions below are accomplished,
			 * 	-Contention Aware condition: link is saturated.
			 * 	-COMPULSARY min threshold is exceeded AND any of Congestion Aware conditions is true:
			 * 		link in use, or link out of credits
			 */
			result = switchM->m_ca_handler.isThereContention(outPort)
					|| (q_min >= g_th_min && (crd < g_flit_size || not switchM->nextPortCanReceiveFlit(outPort)));
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
			result = switchM->m_ca_handler.isThereContention(outPort)
					|| switchM->m_ca_handler.isThereGlobalContention(flit)
					|| (q_min >= g_th_min && (crd < g_flit_size || not switchM->nextPortCanReceiveFlit(outPort)));
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
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router) {
					result = result && globalLinkCongested[outVC][flit->cos][outPort - g_global_router_links_offset];
				}
				break;
			case HYBRID:
				if (outPort >= g_global_router_links_offset
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router
						&& not switchM->m_ca_handler.isThereContention(outPort)) {
					result = result && globalLinkCongested[outVC][flit->cos][outPort - g_global_router_links_offset];
				}
				break;
			case HYBRID_REMOTE:
				if (outPort >= g_global_router_links_offset
						&& outPort < g_global_router_links_offset + g_h_global_ports_per_router
						&& not (switchM->m_ca_handler.isThereContention(outPort)
								|| switchM->m_ca_handler.isThereGlobalContention(flit))) {
					result = result && globalLinkCongested[outVC][flit->cos][outPort - g_global_router_links_offset];
				}
				break;
			default:
				break;
		}
	}

	return result;
}

/*
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references.
 * This function will iterate through local/global ports
 * (regarding misrouteType elected) and will choose amongst
 * those candidates that are valid.
 *
 */
bool flexibleVcRouting::misrouteCandidate(flitModule * flit, int inPort, int inVC, int minOutPort, int minOutVC,
		int &selectedPort, int &selectedVC, MisrouteType &misroute_type) {
	int random, num_candidates = 0, *candidates_port = NULL, *candidates_VC = NULL;
	double q_min, th_non_min;
	bool result = false;
	vector<int> vc_array;
	if (g_reactive_traffic)
		vc_array = minOutPort < g_global_router_links_offset ? baseRouting::localResVcDest : baseRouting::globalResVc;
	else
		vc_array = minOutPort < g_global_router_links_offset ? baseRouting::localVcDest : baseRouting::globalVc;

	assert(minOutPort == minOutputPort(flit->destId)); /* Sanity check: given output port must belong to min path */
	switch (g_congestion_detection) {
		case PER_PORT:
			q_min = 0;
			for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
				assert(*it >= 0 && *it < g_channels);
				q_min += switchM->getCreditsOccupancy(minOutPort, flit->cos, *it) * 100.0
						/ switchM->getMaxCredits(minOutPort, flit->cos, *it);
			}
			q_min /= vc_array.size();
			break;
		case PER_VC:
			q_min = switchM->getCreditsOccupancy(minOutPort, flit->cos, minOutVC) * 100.0
					/ switchM->getMaxCredits(minOutPort, flit->cos, minOutVC);
			break;
		case PER_PORT_MIN:
			q_min = 0;
			for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
				assert(*it >= 0);
				q_min += switchM->getCreditsMinOccupancy(minOutPort, flit->cos, *it) * 100.0
						/ switchM->getMaxCredits(minOutPort, flit->cos, *it);
			}
			q_min /= vc_array.size();
			break;
		case PER_VC_MIN:
			q_min = switchM->getCreditsMinOccupancy(minOutPort, flit->cos, minOutVC) * 100.0
					/ switchM->getMaxCredits(minOutPort, flit->cos, minOutVC);
			break;
	}

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
		assert(selectedVC < g_local_link_channels + g_global_link_channels);
	} else {
		result = false;
		misroute_type = NONE;
	}

	delete[] candidates_port;
	delete[] candidates_VC;

	return result;
}

/*
 * Auxiliary function to check if a given port is eligible
 * for misrouting (only used in certain routing mechanisms)
 */
bool flexibleVcRouting::validMisroutePort(flitModule * flit, int outP, int nextC, double threshold,
		MisrouteType misroute) {
	int crd = switchM->getCredits(outP, flit->cos, nextC), minOutP = minOutputPort(flit->destId);
	bool can_rx_flit = switchM->nextPortCanReceiveFlit(outP);
	/* Discard those ports that are in use or out of credits */
	if (!can_rx_flit || crd < g_flit_size || minOutP == outP)
		return false;
	else if (g_forceMisrouting || (misroute == GLOBAL_MANDATORY)) return true;

	double q_non_min;
	bool valid_candidate = false;
	int nominations;
	vector<int> vc_array;
	if (g_reactive_traffic)
		vc_array = outP < g_global_router_links_offset ? baseRouting::localResVcDest : baseRouting::globalResVc;
	else
		vc_array = outP < g_global_router_links_offset ? baseRouting::localVcDest : baseRouting::globalVc;

	if (g_misrouting_trigger == CGA || g_misrouting_trigger == HYBRID || g_misrouting_trigger == HYBRID_REMOTE) {
		switch (g_congestion_detection) {
			case PER_PORT:
				q_non_min = 0;
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0 && *it < g_channels);
					q_non_min += switchM->getCreditsOccupancy(outP, flit->cos, *it) * 100.0
							/ switchM->getMaxCredits(outP, flit->cos, *it);
				}
				q_non_min /= vc_array.size();
				break;
			case PER_VC:
				q_non_min = switchM->getCreditsOccupancy(outP, flit->cos, nextC) * 100.0
						/ switchM->getMaxCredits(outP, flit->cos, nextC);
				break;
			case PER_PORT_MIN:
				q_non_min = 0;
				for (vector<int>::iterator it = vc_array.end() - 1; it != vc_array.begin() - 1; --it) {
					assert(*it >= 0);
					q_non_min += switchM->getCreditsMinOccupancy(outP, flit->cos, *it) * 100.0
							/ switchM->getMaxCredits(outP, flit->cos, *it);
				}
				q_non_min /= vc_array.size();
				break;
			case PER_VC_MIN:
				q_non_min = switchM->getCreditsMinOccupancy(outP, flit->cos, nextC) * 100.0
						/ switchM->getMaxCredits(outP, flit->cos, nextC);
				break;
		}
	}

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
