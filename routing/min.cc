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

#include "min.h"

minimal::minimal(switchModule *switchM) :
		baseRouting(switchM) {
	const int minLocalVCs = 2, minGlobalVCs = 1;
	assert(g_deadlock_avoidance == DALLY); // Sanity check
	/* Set up the VC management specific for minimal routing */
	portClass aux[] = { portClass::local, portClass::global, portClass::local };
	vector<portClass> typeVc(aux, aux + minLocalVCs + minGlobalVCs);
	if (g_vc_usage == TBFLEX)
		vcM = new tbFlexVc(&typeVc, switchM);
	else if (g_vc_usage == FLEXIBLE)
		vcM = new flexVc(&typeVc, switchM);
	else if (g_vc_usage == BASE) vcM = new vcMngmt(&typeVc, switchM);
	vcM->checkVcArrayLengths(minLocalVCs, minGlobalVCs);
}

minimal::~minimal() {
}

candidate minimal::enroute(flitModule * flit, int inPort, int inVC) {
	candidate selectedRoute;
	int destination;
	/* Determine minimal output port (& VC) */
	destination = flit->destId;
	selectedRoute.port = minOutputPort(destination);
	selectedRoute.vc = vcM->nextChannel(inPort, selectedRoute.port, flit);
	selectedRoute.neighPort = neighPort[selectedRoute.port];
	return selectedRoute;
}

