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

#ifndef class_car
#define class_car

#include "routing.h"

class contAdpRouting: public baseRouting {
protected:
	int ***lastValNodeSet;

public:
	vector<int> **congestionWindow;

	contAdpRouting(switchModule *switchM);
	~contAdpRouting();
	candidate enroute(flitModule * flit, int inPort, int inVC);

private:
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC);
	bool misrouteCondition(flitModule * flit, int inPort);
	/* Defined for coherence reasons. Should never be called. */
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}
};

#endif