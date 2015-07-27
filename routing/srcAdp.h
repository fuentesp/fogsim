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

#ifndef class_sourceAdp
#define class_sourceAdp

#include "routing.h"

template<class base>
class sourceAdp: public base {
protected:

public:

	sourceAdp(switchModule *switchM) :
			base(switchM) {
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= 4 && g_global_link_channels >= 2);
		int vc;
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
		if (inPort < g_p_computing_nodes_per_router && not (flit->getMisrouted())) {
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
			assert(intNode < g_number_generators);
			flit->valId = intNode;
			flit->setMisrouted(true, VALIANT);

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
			/* Sanity check to ensure flit's misroute path will not be recomputed */
			assert(flit->getMisrouteCount(NONE) > 0);
		}

		nonMinOutP = baseRouting::minOutputPort(intNode);
		/* If Valiant node is in current switch, update check within flit. */
		if (baseRouting::switchM->label == int(intNode / g_p_computing_nodes_per_router)) flit->valNodeReached = true;

		if (not (this->misrouteCondition(flit, inPort))) {
			selectedRoute.port = minOutP;
			flit->setCurrentMisrouteType(NONE);
		} else {
			selectedRoute.port = nonMinOutP;
			flit->setCurrentMisrouteType(VALIANT);
		}

		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

		/* Sanity checks */
		assert(selectedRoute.port < baseRouting::portCount && selectedRoute.port >= 0);
		assert(selectedRoute.neighPort < baseRouting::portCount && selectedRoute.neighPort >= 0);
		assert(selectedRoute.vc < g_local_link_channels && selectedRoute.vc >= 0);

		return selectedRoute;
	}

private:
	/*
	 * Determine type of misrouting to be performed, upon global misrouting policy.
	 */
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		MisrouteType result = NONE;

		/* Sanity assert to ensure not any flit is assigned misroute path twice */
		assert(flit->getMisrouteCount(NONE) == 0);

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
		int destination, minOutPort, nonMinOutPort, valNode, minQueueLength, nonMinQueueLength;
		bool result = false;

		destination = flit->destId;
		minOutPort = this->minOutputPort(destination);
		nonMinOutPort = this->minOutputPort(flit->valId);

		if ((inPort < g_p_computing_nodes_per_router) && (minOutPort >= g_p_computing_nodes_per_router)) {
			minQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(minOutPort,
					this->nextChannel(inPort, minOutPort, flit)); //Credit occupancy for the minimal path outport
			nonMinQueueLength = baseRouting::switchM->switchModule::getCreditsOccupancy(nonMinOutPort,
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

		return result;
	}

	/*
	 * Defined for coherence reasons. Should never be called.
	 */
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}

	int nextChannel(int inP, int outP, flitModule * flit) {
		char outType, inType;
		int next_channel, inVC = flit->channel;
		assert(inP >= 0 && inP < baseRouting::portCount);
		assert(outP >= 0 && outP < baseRouting::portCount);

		inType = baseRouting::portType(inP);
		outType = baseRouting::portType(outP);

		if ((inType == 'p'))
			next_channel = 0;
		else if (inType == 'a' && outType == 'h'
				&& (inVC == 2 || (inVC == 1 && this->switchM->hPos == flit->sourceGroup)))
			next_channel = inVC - 1; // Local->global hop after reaching Val node
		else if (inType == 'h' && outType == 'a' && baseRouting::switchM->hPos == flit->destGroup)
			next_channel = inVC + 2; // Global->local hop in destination group
		else if (outType == 'p' || (inType == 'a' && outType == 'h'))
			next_channel = inVC;
		else
			next_channel = inVC + 1;

		assert(next_channel < g_channels); // Sanity check
		return (next_channel);
	}
};

#endif
