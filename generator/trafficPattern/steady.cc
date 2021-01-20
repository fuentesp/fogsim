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

#include "steady.h"
#include <iostream>
#include <cmath>
#include "../generatorModule.h"

steadyTraffic::steadyTraffic(int sourceLabel, int pPos, int aPos, int hPos) {
	this->sourceLabel = sourceLabel;
	this->pPos = pPos;
	this->aPos = aPos;
	this->hPos = hPos;
	this->rpDestination = -1;
}

steadyTraffic::~steadyTraffic() {

}

/*
 * Determines destination upon specified steady traffic pattern.
 * It returns a node label different from source router (avoids
 * intra-node traffic that won't stress the network).
 */
int steadyTraffic::setDestination(TrafficType type) {
	int destSwitch, destLabel, destNodeOffset, destSwOffset, destGroup, groups;
	double aux;

	groups = g_h_global_ports_per_router * g_a_routers_per_group + 1;
	destGroup = module((this->hPos + g_adv_traffic_distance), groups);
	do {
		destNodeOffset = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_p_computing_nodes_per_router));
	} while (destNodeOffset >= g_p_computing_nodes_per_router);

	do {
		switch (type) {
			case UN:
				/* Uniform Random traffic pattern. */
				do {
					destSwitch = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_switches));
				} while ((destSwitch == g_number_switches));
				break;

			case ADV:
				/* Adversarial traffic, as defined by M. Garcia in [??]: destination has same
				 * position within its group as source in its own, but is placed
				 * g_adv_global groups ahead of current. */
				destSwitch = destGroup * g_a_routers_per_group + (this->aPos);
				break;

			case ADV_RANDOM_NODE:
				/* Adversarial traffic pattern, as defined by C. Camarero in [??]:
				 * destination is placed randomly within destination group, at
				 * g_adv_global group distance ahead of current. */
				do {
					destSwOffset = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group));
				} while (destSwOffset >= g_a_routers_per_group);
				destSwitch = destGroup * g_a_routers_per_group + destSwOffset;
				break;

			case ADV_LOCAL:
				/* Adversarial local traffic pattern: destination is in source group,
				 * but adv_traffic_local_distance routers ahead of current. */
				destSwOffset = module((this->aPos + g_adv_traffic_local_distance), g_a_routers_per_group);
				destSwitch = this->hPos * g_a_routers_per_group + destSwOffset;
				break;

			case ADVc:
				/* Adversarial consecutive group traffic pattern: destination group is randomly
				 * selected among those linked to source router (in the palmtree layout, these are
				 * the +1,+2...+h consecutive groups). Destination router is randomly chosen
				 * amidst destination group. */
				int group_dist;
				do {
					destSwOffset = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_a_routers_per_group));
				} while (destSwOffset >= g_a_routers_per_group);
				do {
					group_dist = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_h_global_ports_per_router - 1));
				} while (group_dist >= g_h_global_ports_per_router - 1);
				destGroup = module((this->hPos + group_dist + 1), groups);
				destSwitch = destGroup * g_a_routers_per_group + destSwOffset;
				break;

			case oADV:
				/* Oversubscribed adversarial traffic pattern: all the routers with the same offset within
				 * their group target the same destination node in the network, to evaluate the impact of
				 * endpoint congestion. The destination is computed as a given local distance from the current
				 * offset in the group and a given global distance from the first group; local and global
				 * distances are the same for ease of implementation. */
				destSwOffset = module((this->aPos + g_adv_traffic_distance), groups);
				destSwitch = destSwOffset * g_a_routers_per_group + destSwOffset;
				destNodeOffset = 0;
				break;

			case HOTREGION:
				aux = ((double) rand() / (double) (RAND_MAX));
				// If random is lower than the percentage of the traffic to send to hotregion
				if (aux <= (g_percent_traffic_to_congest / 100.0)) {
					destSwitch = (g_percent_nodes_into_region / 100) * rand()
							/ (int) (((unsigned) RAND_MAX + 1) / (g_number_switches));
					// Verification of destSwitch >=0 is below in the code
					assert(destSwitch <= ceil(g_number_generators * g_percent_nodes_into_region / 100) - 1);
				} else
					do {
						destSwitch = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_switches));
					} while ((destSwitch == g_number_switches));
				break;

			case HOTSPOT:
				aux = ((double) rand() / (double) (RAND_MAX));
				// If random is lower than the percentage of the traffic to send to hotspot
				if (aux <= (g_percent_traffic_to_congest / 100.0)) {
					destSwitch = g_hotspot_node / g_p_computing_nodes_per_router;
					destNodeOffset = g_hotspot_node - destSwitch * g_p_computing_nodes_per_router;
				} else
					do {
						destSwitch = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_switches));
					} while ((destSwitch == g_number_switches));
				break;
			case RANDOMPERMUTATION:
				// First execution, node is not selected for this generator
				if (rpDestination == -1) {
					do {
						aux = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_available_generators.size()));
						rpDestination = g_available_generators.at(aux);
					} while (rpDestination == sourceLabel);
					g_available_generators.erase(g_available_generators.begin() + aux);
				}
				destSwitch = rpDestination / g_p_computing_nodes_per_router;
				destNodeOffset = rpDestination - destSwitch * g_p_computing_nodes_per_router;
				break;
			default:
				cerr << "ERROR: Unrecognized steady traffic pattern!: " << type << endl;
				exit(0);
		}

		/* After destination router is selected, destination node offset within router is randomly picked. */
		destLabel = destSwitch * g_p_computing_nodes_per_router + destNodeOffset;
		if (type == HOTSPOT && aux <= (g_percent_traffic_to_congest / 100.0)) {
			assert(destLabel == g_hotspot_node);
		}

	} while (destLabel == sourceLabel);

	assert(destSwitch >= 0 && destSwitch < g_number_switches);
	assert(destLabel >= 0 && destLabel < g_number_generators);

	return destLabel;
}

/* For steady traffic patterns, generation is conducted
 * until simulation ends.
 */
bool steadyTraffic::isGenerationFinished() {
	return false;
}
