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

#include "caFlit.h"

long long caFlit::id = 0;

caFlit::caFlit(int aPos, int * counter, float arrivalCycle) :
		m_aPos(aPos), m_arrivalCycle(arrivalCycle), m_generatedCycle(g_internal_cycle), m_id(caFlit::id++) {
	partial_counter = new int[g_a_routers_per_group * g_h_global_ports_per_router + 1];
	for (int i = 0; i < g_a_routers_per_group * g_h_global_ports_per_router + 1; i++) {
		partial_counter[i] = counter[i];
		assert(partial_counter[i] >= 0);
		assert(
				partial_counter[i]
						<= (g_flit_size
								* (g_p_computing_nodes_per_router * g_injection_channels
										+ g_h_global_ports_per_router * g_global_link_channels)));
	}
}

caFlit::caFlit(const caFlit& original) :
		m_aPos(original.m_aPos), m_arrivalCycle(original.m_arrivalCycle), m_generatedCycle(original.m_generatedCycle), m_id(
				original.m_id) {
	partial_counter = new int[g_a_routers_per_group * g_h_global_ports_per_router + 1];
	for (int i = 0; i < g_a_routers_per_group * g_h_global_ports_per_router + 1; i++) {
		partial_counter[i] = original.partial_counter[i];
		assert(partial_counter[i] >= 0);
		assert(
				partial_counter[i]
						<= (g_flit_size
								* (g_p_computing_nodes_per_router * g_injection_channels
										+ g_h_global_ports_per_router * g_global_link_channels)));
	}
}

caFlit::~caFlit() {
	delete[] partial_counter;
}

float caFlit::getArrivalCycle() const {
	return m_arrivalCycle;
}
