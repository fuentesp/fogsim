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

#include "all2all.h"
#include "../generatorModule.h"

all2allTraffic::all2allTraffic(int sourceLabel, int pPos, int aPos, int hPos) :
		steadyTraffic(sourceLabel, pPos, aPos, hPos) {
	assert(g_traffic == ALL2ALL);
	flits_tx_count = 0;
	flits_rx_count = 0;
	phase_count = 0;
	dest_pointer = 0;
	prev_dest = -1;

	/* If traffic pattern is a random All-to-all, a destinations
	 * array will be initialized to ensure all possible nodes
	 * are sent a packet once in a random fashion.
	 */
	if (g_random_AllToAll) {
		dests_array = new int[g_number_generators - 1];
		/* Fill in all possible dests */
		int i, rand_num, var;
		for (i = 0; i < (g_number_generators - 1); i++) {
			if (i < this->sourceLabel) {
				dests_array[i] = i;
			} else {
				dests_array[i] = i + 1;
			}
		}
		/* Permute them to ensure randomness */
		for (i = 0; i < (g_number_generators - 2); i++) {
			rand_num = (rand() % (g_number_generators - 1 - i)) + i;
			var = dests_array[rand_num];
			dests_array[rand_num] = dests_array[i];
			dests_array[i] = var;
		}
	}
}

all2allTraffic::~all2allTraffic() {
	delete[] dests_array;
}

int all2allTraffic::setDestination(TrafficType type) {
	int destLabel, destSwitch, destNode;
	destLabel = prev_dest;

	/* If burst is over, return a flag destination (-1) */
	assert(flits_tx_count <= (g_number_generators - 1) * g_phases);
	if (flits_tx_count == (g_number_generators - 1) * g_phases) return -1;

	/* If phase has been completed, step on the next phase */
	if (this->isPhaseSent()) {
		if (this->isPhaseRx()) {
			phase_count++;
			flits_rx_count = 0;
			flits_tx_count = 0;
			if (g_random_AllToAll) {
				dest_pointer = 0;
			} else {
				destLabel = this->sourceLabel;
			}
		} else
			return -1;
	}

	do {
		if (g_random_AllToAll) {
			assert(dest_pointer < (g_number_generators - 1));
			destLabel = dests_array[dest_pointer];
			assert(destLabel < g_number_generators);
			assert(destLabel != this->sourceLabel);
			dest_pointer++;
		} else {
			destLabel = module((destLabel + 1), g_number_generators);
			prev_dest = destLabel;
			assert(destLabel < g_number_generators);
			assert(destLabel != this->sourceLabel);
		}
		destSwitch = int(destLabel / g_p_computing_nodes_per_router);

	} while (destLabel == sourceLabel);

	assert(destSwitch < g_number_switches);
	assert(destLabel < g_number_generators);

	return destLabel;
}

bool all2allTraffic::isPhaseSent() {
	assert(g_traffic == ALL2ALL);
	assert(flits_tx_count <= (g_number_generators - 1) * g_phases);
	assert(flits_tx_count <= (g_number_generators - 1) * phase_count);
	return flits_tx_count == (g_number_generators - 1) * phase_count;
}

bool all2allTraffic::isPhaseRx() {
	assert(g_traffic == ALL2ALL);
	assert(flits_rx_count <= (g_number_generators - 1) * g_phases);
	assert(flits_rx_count <= (g_number_generators - 1) * phase_count);
	return flits_rx_count == (g_number_generators - 1) * phase_count;
}

bool all2allTraffic::isGenerationFinished() {
	assert(g_traffic == ALL2ALL);
	assert(flits_rx_count <= (g_number_generators - 1) * g_phases);
	return flits_rx_count == (g_number_generators - 1) * g_phases;
}

void all2allTraffic::flitTx() {
	assert(g_traffic == ALL2ALL);
	assert(flits_tx_count <= (g_number_generators - 1) * g_phases * g_flits_per_packet);
	flits_tx_count++;
}

void all2allTraffic::flitRx() {
	assert(g_traffic == ALL2ALL);
	assert(flits_rx_count <= (g_number_generators - 1) * g_phases * g_flits_per_packet);
	flits_rx_count++;
}

