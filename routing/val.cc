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

#include "val.h"

val::val(switchModule *switchM) :
		baseRouting(switchM) {
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	/* Set up the vc management. */
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

val::~val() {
}

candidate val::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int destination, valNode, minOutP, valOutP;

	/* Determine minimal output port (& VC) */
	destination = flit->destId;
	minOutP = this->minOutputPort(destination);

	/* Calculate Valiant node output port (&VC) */
	valNode = flit->valId;
	valOutP = this->minOutputPort(valNode);

	/* If flit is being injected into the network, set flit parameters */
	if (inPort < g_p_computing_nodes_per_router) {
		flit->setMisrouted(true, VALIANT);
		flit->setCurrentMisrouteType(VALIANT);
	}

	/* If Valiant node is in current group, flag flit as already Valiant misrouted */
	if (this->switchM->hPos == int(valNode / (g_p_computing_nodes_per_router * g_a_routers_per_group))) {
		flit->valNodeReached = true;
	}

	/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
	if (!flit->valNodeReached)
		selectedRoute.port = valOutP;
	else
		selectedRoute.port = minOutP;

	selectedRoute.neighPort = this->neighPort[selectedRoute.port];
	selectedRoute.vc = vcM->nextChannel(inPort, selectedRoute.port, flit);

	/* Sanity checks */
	assert(selectedRoute.port < this->portCount && selectedRoute.port >= 0);
	assert(selectedRoute.neighPort < this->portCount && selectedRoute.neighPort >= 0);

	return selectedRoute;
}
