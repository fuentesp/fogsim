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

#include "burst.h"

burstTraffic::burstTraffic(int sourceLabel, int pPos, int aPos, int hPos) :
		steadyTraffic(sourceLabel, pPos, aPos, hPos) {
	assert(g_traffic == SINGLE_BURST);
	flits_tx_count = 0;
	flits_rx_count = 0;
}

burstTraffic::~burstTraffic() {

}

int burstTraffic::setDestination(TrafficType type) {
	int destLabel, random, aux;

	/* If burst is over, return a destination flag (-1) */
	assert(flits_tx_count <= g_single_burst_length);
	if (flits_tx_count == g_single_burst_length) return -1;

	/* Shuffle randomly between the 3 possible
	 * different traffic patterns */
	random = rand() % 100 + 1;
	if (random <= g_phase_traffic_percent[0]) {
		aux = 0;
	} else if (random <= g_phase_traffic_percent[0] + g_phase_traffic_percent[1]) {
		aux = 1;
	} else {
		aux = 2;
	}
	g_adv_traffic_distance = g_phase_traffic_adv_dist[aux];
	return this->steadyTraffic::setDestination(g_phase_traffic_type[aux]);
}

/* Burst traffic generation ends when the burst is over */
bool burstTraffic::isGenerationFinished() {
	assert(g_traffic == SINGLE_BURST);
	assert(flits_rx_count <= g_single_burst_length);
	return flits_rx_count == g_single_burst_length;
}

void burstTraffic::flitTx() {
	assert(g_traffic == SINGLE_BURST);
	flits_tx_count++;
	assert(flits_tx_count <= g_single_burst_length);
}

void burstTraffic::flitRx() {
	assert(g_traffic == SINGLE_BURST);
	assert(flits_rx_count <= g_single_burst_length);
	flits_rx_count++;
}
