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

#include "minCond.h"

minCond::minCond(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	assert(g_vc_usage == BASE);

	/* Set up the vc management */
	const int minLocalVCs = 3, minGlobalVCs = 2;
	portClass aux[] = { portClass::local, portClass::global, portClass::local, portClass::global, portClass::local };
	vector<portClass> typeVc(aux, aux + minLocalVCs + minGlobalVCs);
	if (g_vc_usage == TBFLEX)
		this->vcM = new tbFlexVc(&typeVc, switchM);
	else if (g_vc_usage == FLEXIBLE)
		this->vcM = new flexVc(&typeVc, switchM);
	else
		this->vcM = new vcMngmt(&typeVc, switchM);
	vcM->checkVcArrayLengths(minLocalVCs, minGlobalVCs);
}

minCond::~minCond() {
}

candidate minCond::enroute(flitModule * flit, int inPort, int inVC) {
	/* Misroute global out port is assigned by computing node position */
	assert(g_p_computing_nodes_per_router == g_h_global_ports_per_router);
	assert(flit->getMisrouteCount(NONE) <= 1);
	candidate selectedRoute;
	int minOutP, misOutP;
	MisrouteType misrouteType = NONE;

	/* Determine minimal output port*/
	minOutP = this->minOutputPort(flit->destId);

	/* Calculate misroute output port*/
	misOutP = inPort + g_global_router_links_offset;

	/* By default, consider minimal route as the selected one */
	selectedRoute.port = minOutP;

	if (this->misrouteCondition(flit, inPort)) {
		misrouteType = this->misrouteType(inPort, inVC, flit, -1, -1);
		flit->setCurrentMisrouteType(misrouteType);
		// Assign misroute port
		selectedRoute.port = misOutP;
	} else
		/* Reset current misroute type after first GLOBAL misrouting */
		flit->setCurrentMisrouteType(NONE);

	selectedRoute.neighPort = this->neighPort[selectedRoute.port];
	selectedRoute.vc = vcM->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < portCount && selectedRoute.neighPort >= 0);

	// Set flit as enrouted
	flit->currentlyEnrouted = true;

	return selectedRoute;
}

bool minCond::misrouteCondition(flitModule * flit, int inPort) {
	bool pauseDoMisroute = false;
	bool qcnswDoMisroute = false;

	/* Determine minimal output port & VC */
	int minOutP = this->minOutputPort(flit->destId);
	int minOutVC = vcM->nextChannel(inPort, minOutP, flit);

	/* Only eval misrouting ... */
	if (inPort < g_p_computing_nodes_per_router && /* ... at injection */
	flit->sourceGroup != flit->destGroup) { /* ... traffic to other groups */
		if (flit->currentlyEnrouted) return (flit->getCurrentMisrouteType() != NONE);
		switch (g_congestion_management) {
			case PAUSE: /* Do misroute if port is paused */
				if (switchM->getCredits(minOutP, flit->cos, minOutVC) < g_flit_size) pauseDoMisroute = true;
				break;
			case QCNSW: /* QCNSW: evaluate probability of MIN injection */
				qcnswDoMisroute = (rand() % 100 + 1) > switchM->getPortEnrouteMinimalProbability(minOutP);
				break;
			default:
				assert(false);
		}
	}
	return pauseDoMisroute || qcnswDoMisroute;
}

MisrouteType minCond::misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
	MisrouteType result = NONE;

	/* Sanity check to ensure not any flit is misrouted previously */
	assert(flit->getMisrouteCount(NONE) == 0);
	/* Try to misroute only is valid at injection and for traffic to other groups */
	assert(inport < g_p_computing_nodes_per_router && flit->sourceGroup != flit->destGroup);
	assert(not (flit->globalMisroutingDone));

	switch (g_global_misrouting) {
		case CRG:
			result = GLOBAL;
			break;
		default:
			assert(false);
			break;
	}

	return result;
}
