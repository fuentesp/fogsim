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

#include "obl.h"

oblivious::oblivious(switchModule *switchM) :
		baseRouting(switchM) {
	const int minLocalVCs = 4, minGlobalVCs = 2;
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	/* Set up the VC management specific for oblivious routing */
	portClass aux[] = { portClass::local, portClass::global, portClass::local, portClass::local, portClass::global,
			portClass::local };
	vector<portClass> typeVc(aux, aux + minLocalVCs + minGlobalVCs);
	if (g_vc_usage == TBFLEX)
		this->vcM = new tbFlexVc(&typeVc, switchM);
	else if (g_vc_usage == FLEXIBLE)
		this->vcM = new flexVc(&typeVc, switchM);
	else if (g_vc_usage == BASE) this->vcM = new vcMngmt(&typeVc, switchM);
	vcM->checkVcArrayLengths(minLocalVCs, minGlobalVCs);
}

oblivious::~oblivious() {
}

candidate oblivious::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int minOutP, minOutVC, nonMinOutP, intNode, intSW, intGroup;

	/* Determine minimal output port (& VC) */
	minOutP = this->minOutputPort(flit->destId);
	assert(minOutP >= 0 && minOutP < this->portCount);
	intNode = flit->valId;

	if (inPort < g_p_computing_nodes_per_router && (g_reset_val || !flit->getMisrouted())) {
		/* Calculate non-minimal output port: select between neighbor or remote intermediate group */
		minOutVC = -1; /* Since it is not used in the misrouteType() function, we introduce a non-valid value */
		MisrouteType misroute = this->misrouteType(inPort, inVC, flit, minOutP, minOutVC);
		switch (misroute) {
			case LOCAL_MM:
				/* Local misrouting */
				intGroup = flit->sourceGroup;
				break;
			case LOCAL:
				/* Global misrouting with a nonminimal local hop first */
				nonMinOutP = rand() % (g_a_routers_per_group - 1) + g_local_router_links_offset;
				assert(nonMinOutP >= 0 && nonMinOutP < g_global_router_links_offset);
				intGroup = neighList[nonMinOutP]->routing->neighList[g_global_router_links_offset
						+ rand() % (g_h_global_ports_per_router)]->hPos;
				assert(
						intGroup != this->switchM->hPos
								&& intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
				break;
			case GLOBAL:
				nonMinOutP = rand() % (g_h_global_ports_per_router) + g_global_router_links_offset;
				assert(nonMinOutP >= 0 && nonMinOutP < this->portCount);
				intGroup = this->neighList[nonMinOutP]->hPos;
				assert(
						intGroup != this->switchM->hPos
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
		assert(!flit->getMisrouted() || g_reset_val);
		flit->setMisrouted(true, VALIANT);
	}

	/* Determine if intermediate ("Valiant") node has been reached, and route consequently */
	nonMinOutP = this->minOutputPort(intNode);

    /* Determine if intermediate ("Valiant") node has been reached, and route consequently */
    /* ValiantMisroutingDestination == GROUP - Valiant node is in current group */
    if (g_valiant_misrouting_destination == GROUP &&
            this->switchM->hPos == int(intNode / (g_p_computing_nodes_per_router * g_a_routers_per_group)))
        flit->valNodeReached = true;

    /* ValiantMisroutingDestination == NODE  -  Valiant node is in current switch */
    if (g_valiant_misrouting_destination == NODE &&
            this->switchM->label == int(intNode / g_p_computing_nodes_per_router))
        flit->valNodeReached = true; 

	/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
	if (!flit->valNodeReached) {
		selectedRoute.port = nonMinOutP;
		flit->setCurrentMisrouteType(VALIANT);
	} else {
		selectedRoute.port = minOutP;
	}

	selectedRoute.neighPort = this->neighPort[selectedRoute.port];
	selectedRoute.vc = vcM->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < this->portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < this->portCount && selectedRoute.neighPort >= 0);
	assert(selectedRoute.vc < g_channels && selectedRoute.vc >= 0); /* Is not restrictive enough, as it might be a forbidden vc for that port, but will be checked after */

	return selectedRoute;
}

/*
 * Determine type of misrouting to be performed, upon global misrouting policy.
 */
MisrouteType oblivious::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
    MisrouteType result;
    assert(flit->sourceGroup == this->switchM->hPos);
    assert(not (flit->globalMisroutingDone) && not (flit->localMisroutingDone));
    // Source and destination group are the same and use CRG_L or RRL_L: only make local misrouting
    if ((g_global_misrouting == CRG_L || g_global_misrouting == RRG_L) && flit->sourceGroup == flit->destGroup)
        result = LOCAL_MM;
    else
        switch (g_valiant_type) {
            case FULL:
                /* If Valiant routing considers src group, with probability 1/(Number of groups) misrouting
                 * will be local because INT group is on SRC group */
                if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_switches)) < g_a_routers_per_group - 1) {
                    result = LOCAL_MM;
                    break;
                } // rand's() result is not on SRC group: continue with SRCEXC behaviour
            case SRCEXC:
                /* INT group is not source group:
                 *  - CRG and CRG_L: use global of this router
                 *  - RRG and RRG_L: use global of any swith within this group (including current) */
                switch (g_global_misrouting) {
                    case CRG:
                    case CRG_L:
                        result = GLOBAL;
                        break;
                    case RRG:
                    case RRG_L:
                        if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group))
                                < g_a_routers_per_group - 1)
                            result = LOCAL;
                        else
                            result = GLOBAL;
                        break;
                    default:
                        /* Other global misrouting policies do not make sense in oblivious routing */
                        assert(false);
                }
                break;
            default:
                assert(false);
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
        default:
            cout << "DEFAULT" << endl;
            break;
    }
#endif
    return result;
}
