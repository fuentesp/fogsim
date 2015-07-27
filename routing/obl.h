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

#ifndef class_oblivious
#define class_oblivious

#include "routing.h"

template<class base>
class oblivious: public base {
protected:

public:

	oblivious(switchModule *switchM) :
			base(switchM) {
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= 4 && g_global_link_channels >= 2);
		int vc;
	}
	~oblivious() {
	}
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int minOutP, minOutVC, nonMinOutP, intNode, intSW, intGroup;

		/* Determine minimal output port (& VC) */
		minOutP = baseRouting::minOutputPort(flit->destId);
		assert(minOutP >= 0 && minOutP < baseRouting::portCount);
		intNode = flit->valId;

		if (inPort < g_p_computing_nodes_per_router && flit->getCurrentMisrouteType() != VALIANT) {
			/* Calculate non-minimal output port: select between neighbor or remote intermediate group */
			minOutVC = -1; /* Since it is not used in the misrouteType() function, we introduce a non-valid value */
			MisrouteType misroute = this->misrouteType(inPort, inVC, flit, minOutP, minOutVC);
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
		}

		/* Determine if intermediate ("Valiant") node has been reached, and route consequently */
		nonMinOutP = baseRouting::minOutputPort(intNode);

		/* If Valiant node is in current switch, flag flit as already Valiant misrouted */
		if (baseRouting::switchM->label == int(intNode / g_p_computing_nodes_per_router)) {
			flit->valNodeReached = true;
		}

		/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
		if (!flit->valNodeReached) {
			selectedRoute.port = nonMinOutP;
			flit->setCurrentMisrouteType(VALIANT);
		} else {
			selectedRoute.port = minOutP;
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

		/* Source group may be susceptible to global misroute */
		if ((flit->sourceGroup == baseRouting::switchM->hPos)) {
			assert(not (flit->globalMisroutingDone));
			switch (g_global_misrouting) {
				case CRG:
					assert(not (flit->localMisroutingDone));
					result = GLOBAL;
					break;
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
