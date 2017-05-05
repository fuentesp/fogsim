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

#include "transient.h"

transientTraffic::transientTraffic(int sourceLabel, int pPos, int aPos, int hPos) :
		steadyTraffic(sourceLabel, pPos, aPos, hPos) {
	assert(g_traffic == TRANSIENT);
}

transientTraffic::~transientTraffic() {

}

int transientTraffic::setDestination(TrafficType type) {
	/* Determine the traffic pattern employed upon the number of simulated cycles */
	if ((g_cycle < g_warmup_cycles + g_transient_traffic_cycle)) {
		g_adv_traffic_distance = g_phase_traffic_adv_dist[0];
		return this->steadyTraffic::setDestination(g_phase_traffic_type[0]);
	} else {
		g_adv_traffic_distance = g_phase_traffic_adv_dist[1];
		return this->steadyTraffic::setDestination(g_phase_traffic_type[1]);
	}
}

