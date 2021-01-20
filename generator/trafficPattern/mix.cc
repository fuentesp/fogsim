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

#include "mix.h"

mixTraffic::mixTraffic(int sourceLabel, int pPos, int aPos, int hPos) :
		steadyTraffic(sourceLabel, pPos, aPos, hPos) {
	assert(g_traffic == MIX);
}

mixTraffic::~mixTraffic() {

}

int mixTraffic::setDestination(TrafficType type) {
	int random;

	/* Shuffle randomly between the 3 possible
	 * different traffic patterns */
	random = rand() % 100 + 1;
	if (random <= g_phase_traffic_percent[0]) {
		return this->steadyTraffic::setDestination(UN);
	} else if (random <= g_phase_traffic_percent[0] + g_phase_traffic_percent[1]) {
		return this->steadyTraffic::setDestination(ADV_LOCAL);
	} else {
		return this->steadyTraffic::setDestination(ADV_RANDOM_NODE);
	}
}

