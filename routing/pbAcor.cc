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

#include "pbAcor.h"

pbAcor::pbAcor(switchModule *switchM) :
baseRouting(switchM) {
    const int minLocalVCs = 4, minGlobalVCs = 2;
    /* Set up the VC management */
    portClass aux[] = {portClass::local, portClass::oppglobal, portClass::local, portClass::local, portClass::global,
        portClass::local};
    vector<portClass> typeVc(aux, aux + minLocalVCs + minGlobalVCs);
    if (g_vc_usage == TBFLEX)
        this->vcM = new tbFlexVc(&typeVc, switchM);
    else if (g_vc_usage == FLEXIBLE)
        this->vcM = new flexVc(&typeVc, switchM);
    else if (g_vc_usage == BASE) this->vcM = new vcMngmt(&typeVc, switchM);
    vcM->checkVcArrayLengths(minLocalVCs, minGlobalVCs);
}

pbAcor::~pbAcor() {
}

candidate pbAcor::enroute(flitModule * flit, int inPort, int inVC) {
    candidate selectedRoute;
    int minOutP, minOutVC, nonMinOutP, intNode, intSW, intGroup;
    MisrouteType misroute = NONE;

    /* Determine minimal output port (& VC) */
    minOutP = minOutputPort(flit->destId);
    assert(minOutP >= 0 && minOutP < portCount);
    intNode = flit->valId;

    /* ACOR status state machine management for packet & source routing (including recomputation) */
    if (inPort < g_p_computing_nodes_per_router) {
        switch (g_acor_state_management) { /* ACOR state management per packet or switch */
            case PACKETCGCSRS:
                switch (flit->acorFlitStatus) {
                    case None:
                        assert(!flit->getMisrouted());
                        flit->acorFlitStatus = CRGLGr;
                        break;
                    case CRGLGr:
                        flit->acorFlitStatus = CRGLSw;
                        break;
                    case CRGLSw:
                    case RRGLSw:
                        flit->acorFlitStatus = RRGLSw;
                        break;
                    default:
                        cerr << "ERROR: Status not defined" << endl;
                        assert(false);
                }
                break;
            case PACKETCGRS:
                switch (flit->acorFlitStatus) {
                    case None:
                        assert(!flit->getMisrouted());
                        flit->acorFlitStatus = CRGLGr;
                        break;
                    case CRGLGr:
                    case RRGLSw:
                        flit->acorFlitStatus = RRGLSw;
                        break;
                    default:
                        cerr << "ERROR: Status not defined" << endl;
                        assert(false);
                }
                break;
            case PACKETCSRS:
                switch (flit->acorFlitStatus) {
                    case None:
                        assert(!flit->getMisrouted());
                        flit->acorFlitStatus = CRGLSw;
                        break;
                    case CRGLSw:
                    case RRGLSw:
                        flit->acorFlitStatus = RRGLSw;
                        break;
                    default:
                        cerr << "ERROR: Status not defined" << endl;
                        assert(false);
                }
                break;
            case SWITCHCGCSRS:
            case SWITCHCGRS:
            case SWITCHCSRS:
                /* Update packets blocked when packet has been blocked on head of buffer
                 * using non-minimal path */
                if (flit->getMisrouted())
                    this->switchM->acor_packets_blocked_counter++;
                /* Use the acor state fixed by router */
                flit->acorFlitStatus = this->switchM->acorGetState();
                break;
            default:
                cerr << "ERROR: Switch state management not defined" << endl;
                assert(false);
        }
        /* ACOR group 0 switch statistics */
        if (this->switchM->label < g_a_routers_per_group && flit->getMisrouted())
            g_acor_group0_sws_packets_blocked[this->switchM->label][g_cycle]++;

        /* Calculate non-minimal output port */
        minOutVC = -1; /* Since it is not used in the misrouteType() function, we introduce a non-valid value */
        misroute = this->misrouteType(inPort, inVC, flit, minOutP, minOutVC);
        switch (misroute) {
            case LOCAL_MM:
                /* Local misrouting */
                intGroup = flit->sourceGroup;
                /* Prevent the detection of local misrouting as global to group */
                flit->acorFlitStatus = RRGLSw;
                break;
            case GLOBAL:
                /* Global misrouting directly doing a global hop */
                nonMinOutP = rand() % g_h_global_ports_per_router + g_global_router_links_offset;
                assert(nonMinOutP >= 0 && nonMinOutP < this->portCount);
                intGroup = this->neighList[nonMinOutP]->hPos;
                assert(
                        intGroup != this->switchM->hPos
                        && intGroup < g_a_routers_per_group * g_h_global_ports_per_router + 1);
                break;
            case LOCAL:
                /* Global misrouting with a non-minimal local hop first */
                nonMinOutP = rand() % (g_a_routers_per_group - 1) + g_local_router_links_offset;
                assert(nonMinOutP >= 0 && nonMinOutP < g_global_router_links_offset);
                intGroup = neighList[nonMinOutP]->routing->neighList[g_global_router_links_offset
                        + rand() % (g_h_global_ports_per_router)]->hPos;
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

        /* Calculate non-minimal path length */
        nonMinOutP = minOutputPort(intNode);
        int nonMinPathLength = 0;
        switchModule *sw;
        if (nonMinOutP >= g_p_computing_nodes_per_router) {
            sw = neighList[nonMinOutP];
            nonMinPathLength++;
            while (sw->label != intSW) {
                sw = sw->routing->neighList[sw->routing->minOutputPort(intNode)];
                nonMinPathLength++;
            }
        } else {
            sw = switchM;
        }
        assert(nonMinPathLength == hopsToDest(flit->valId));
        nonMinPathLength += sw->routing->hopsToDest(flit->destId);
        assert(nonMinPathLength <= 6);
        flit->valPathLength = nonMinPathLength;
    }

    /* Determine non-minimal output port */
    nonMinOutP = minOutputPort(intNode);

    /* Determine if intermediate ("Valiant") node has been reached, and route consequently */
    /* CRGLGr check - Valiant node is in current group */
    if (flit->acorFlitStatus == CRGLGr)
        if (this->switchM->hPos ==
                int(intNode / (g_p_computing_nodes_per_router * g_a_routers_per_group)))
            flit->valNodeReached = true;

    /* CRGLSw and RRGLSw check -  Valiant node is in current switch */
    if (flit->acorFlitStatus == CRGLSw || flit->acorFlitStatus == RRGLSw)
        if (this->switchM->label == int(intNode / g_p_computing_nodes_per_router))
            flit->valNodeReached = true;

    /* Eval misrouting condition */
    if (misrouteCondition(flit, inPort)) { /* Non-minimal path */
        selectedRoute.port = nonMinOutP;
        flit->setCurrentMisrouteType(VALIANT);
        flit->setMisrouted(true, VALIANT);
    } else { /* Minimal path */
        selectedRoute.port = minOutP;
        flit->setCurrentMisrouteType(NONE);
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
MisrouteType pbAcor::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
    MisrouteType result;
    assert(flit->sourceGroup == this->switchM->hPos);
    assert(not (flit->globalMisroutingDone) && not (flit->localMisroutingDone));
    // Source and destination group are the same: only make local misrouting - NOT GLOBAL
    if (flit->sourceGroup == flit->destGroup)
        result = LOCAL_MM;
    else {
        switch (g_valiant_type) {
            case FULL:
                /* If Valiant routing considers src group, with probability 1/(Number of groups) misrouting
                 *  will be local */
                if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_switches)) < g_a_routers_per_group - 1) {
                    result = LOCAL_MM;
                    break;
                } // On else is the same as INT is not on SRC group equal to SRCEXC policy
            case SRCEXC:
                /* INT group is not source group:
                 *  - CRGLGr and CRGLSw: use global of current (source) switch
                 *  - RRGLGr and RRGLSw: use global of any switch within this group (including current) */
                switch (flit->acorFlitStatus) {
                    case CRGLGr:
                    case CRGLSw:
                        result = GLOBAL;
                        break;
                    case RRGLSw:
                        if (rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group))
                                < g_a_routers_per_group - 1)
                            result = LOCAL;
                        else
                            result = GLOBAL;
                        break;
                    default:
                        /* Other global misrouting policies do not make sense in acor routing */
                        assert(false);
                }
                break;
            default:
                assert(false);
        }
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
bool pbAcor::misrouteCondition(flitModule * flit, int inPort) {
    int minOutPort, nonMinOutPort, minQueueLength, nonMinQueueLength;
    bool result = false;

    minOutPort = this->minOutputPort(flit->destId);
    nonMinOutPort = this->minOutputPort(flit->valId);

    if (inPort < g_p_computing_nodes_per_router && minOutPort >= g_p_computing_nodes_per_router &&
            !flit->valNodeReached) {
        minQueueLength = switchM->switchModule::getCreditsOccupancy(minOutPort, flit->cos,
                vcM->nextChannel(inPort, minOutPort, flit)); //Credit occupancy for the minimal path outport
        nonMinQueueLength = switchM->switchModule::getCreditsOccupancy(nonMinOutPort, flit->cos,
                vcM->nextChannel(inPort, nonMinOutPort, flit)); //Credit occupancy for the Valiant outport
        /* UGAL-L(ocal) condition: if the product of minimal outport queue occupancy and the
         * minimal path length is greater than the corresponding of non-minimal path (plus a
         * local threshold), then misroute. */
        if ((minQueueLength * flit->minPathLength
                > nonMinQueueLength * flit->valPathLength + g_flit_size * g_ugal_local_threshold) ||
                switchM->isGlobalLinkCongested(flit)) {
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
