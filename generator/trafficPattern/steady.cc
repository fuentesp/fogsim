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

#include "steady.h"
#include <iostream>
#include "../generatorModule.h"

steadyTraffic::steadyTraffic(int sourceLabel, int pPos, int aPos, int hPos) {
	this->sourceLabel = sourceLabel;
	this->pPos = pPos;
	this->aPos = aPos;
	this->hPos = hPos;
}

steadyTraffic::~steadyTraffic() {

}

/*
 * Determines destination upon specified steady traffic pattern.
 * It returns a node label different from source router (avoids
 * intra-node traffic that won't stress the network).
 */
int steadyTraffic::setDestination(TrafficType type) {
	int destSwitch, destLabel, destNode, destSwOffset, destGroup, groups;

	groups = g_h_global_ports_per_router * g_a_routers_per_group + 1;
	destGroup = module((this->hPos + g_adv_traffic_distance), groups);

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

			default:
				cerr << "ERROR: Unrecognized steady traffic pattern!: " << type << endl;
				exit(0);
		}

		/* After destination router is selected, destination node offset within router is randomly picked. */
		do {
			destNode = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_p_computing_nodes_per_router));
		} while (destNode >= g_p_computing_nodes_per_router);
		destLabel = destSwitch * g_p_computing_nodes_per_router + destNode;

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
