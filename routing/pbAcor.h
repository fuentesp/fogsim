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

#ifndef class_pbAcor
#define class_pbAcor

#include "routing.h"

class pbAcor : public baseRouting {
protected:

public:
    pbAcor(switchModule *switchM);
    ~pbAcor();
    candidate enroute(flitModule * flit, int inPort, int inVC);

private:
    MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC);
    bool misrouteCondition(flitModule * flit, int inPort);

    /* Defined for coherence reasons. Should never be called. */
    int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
            int* &candidates_port, int* &candidates_VC) {
        assert(false);
    }
};

#endif
