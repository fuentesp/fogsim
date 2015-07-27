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

#ifndef class_minimal
#define class_minimal

#include "routing.h"

template<class base>
class minimal: public base {
protected:

public:

	minimal(switchModule *switchM) :
			base(switchM) {
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= 2 && g_global_link_channels >= 1);
		int vc;
	}
	~minimal();

	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int destination;
		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		selectedRoute.port = baseRouting::minOutputPort(destination);
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);
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
