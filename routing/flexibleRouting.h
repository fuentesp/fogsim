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

#ifndef class_newVcBaseRouting
#define class_newVcBaseRouting

#include "routing.h"

class flexibleVcRouting: public baseRouting {
protected:
	bool misrouteCandidate(flitModule * flit, int inPort, int inVC, int minOutPort, int minOutVC, int &selectedPort,
			int &selectedVC, MisrouteType &misroute_type);
	bool misrouteCondition(flitModule * flit, int outPort, int outVC);

public:
	flexibleVcRouting(switchModule *switchM);
	~flexibleVcRouting();
	virtual int nextChannel(int inP, int outP, flitModule * flit);
	virtual struct candidate enroute(flitModule * flit, int inPort, int inVC) = 0;
	bool validMisroutePort(flitModule * flit, int outP, int nextC, double threshold, MisrouteType misroute);

private:
	virtual MisrouteType misrouteType(int inPort, int inVC, flitModule * flit, int minOutPort, int minOutVC) = 0;
	virtual int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) = 0;
};

#endif
