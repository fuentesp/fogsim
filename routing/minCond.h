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

#ifndef class_minimal_conditional
#define class_minimal_conditional

#include "routing.h"

class minCond: public baseRouting {
protected:

public:
	minCond(switchModule *switchM) :
			baseRouting(switchM) {
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_vc_usage == BASE);
	}
	~minCond();
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		assert(g_p_computing_nodes_per_router == g_h_global_ports_per_router);
		candidate selectedRoute;
		int destination;
		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		selectedRoute.port = baseRouting::minOutputPort(destination);
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

		/* Flit can be enroute multiple times */
		if (inPort < g_p_computing_nodes_per_router) flit->setMisrouted(false, NONE);

		/* Only trigger misrouting at injection */
		if ((inPort < g_p_computing_nodes_per_router) && (selectedRoute.port >= g_p_computing_nodes_per_router)
				&& flit->sourceGroup != flit->destGroup
				&& switchM->switchModule::getCredits(selectedRoute.port, flit->cos, selectedRoute.vc) < g_packet_size) {
			// If remaining # of credits for the minimal path outport can't hold an entire packet, misroute
			selectedRoute.port = inPort + g_global_router_links_offset;
			selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);
			flit->setMisrouted(true, VALIANT);
			flit->setCurrentMisrouteType(VALIANT);
		}
		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];

		return selectedRoute;
	}

private:
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		assert(0);
	}
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}
};

#endif
