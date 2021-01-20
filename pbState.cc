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

#include "pbState.h"

pbState::pbState(int switchApos) :
		m_switchApos(switchApos) {

	m_offset = m_switchApos * g_h_global_ports_per_router;
	m_globalLinkCongested = new bool**[g_global_links_per_group];

	for (int link = 0; link < g_global_links_per_group; link++) {
		m_globalLinkCongested[link] = new bool*[g_cos_levels];
		for (int cos = 0; cos < g_cos_levels; cos++) {
			m_globalLinkCongested[link][cos] = new bool[g_global_link_channels];
			for (int channel = 0; channel < g_global_link_channels; channel++) {
				m_globalLinkCongested[link][cos][channel] = false;
			}
		}
	}
}

pbState::~pbState() {
	for (int link = 0; link < g_global_links_per_group; link++) {
		for (int cos = 0; cos < g_cos_levels; cos++)
			delete[] m_globalLinkCongested[link][cos];
		delete m_globalLinkCongested[link];
	}

	delete[] m_globalLinkCongested;
}

void pbState::readFlit(const pbFlit& flit) {
	int srcOffset, i, cos, channel;

	srcOffset = flit.getSrcOffset();

	assert(srcOffset != m_offset);
	assert(srcOffset <= g_a_routers_per_group * g_h_global_ports_per_router - g_h_global_ports_per_router);

	for (i = 0; i < g_h_global_ports_per_router; i++)
		for (cos = 0; cos < g_cos_levels; cos++)
			for (channel = 0; channel < g_global_link_channels; channel++)
				m_globalLinkCongested[srcOffset + i][cos][channel] = flit.getGlobalLinkInfo(i, cos, channel);
}

/* 
 * Create a NEW pbFlit with its information. Needs the latency to
 * determine the arrival cycle of the flit.
 * 
 * This flit must be deleted after use.
 */
pbFlit* pbState::createFlit(int latency) {
	pbFlit* flit = new pbFlit(m_offset, m_globalLinkCongested, g_internal_cycle + latency);
	return flit;
}

void pbState::update(int port, unsigned short cos, int channel, bool linkCongested) {
	m_globalLinkCongested[port2groupGlobalLinkID(port, m_switchApos)][cos][channel] = linkCongested;
}

bool pbState::isCongested(int link, unsigned short cos, int channel) {
	return m_globalLinkCongested[link][cos][channel];
}

/* 
 * Maps the switch port number and switch id
 * into the global link id within the group
 * 
 * Must be a global link port.
 */
int port2groupGlobalLinkID(int port, int switchApos) {

	int numRingPorts;

	if (g_deadlock_avoidance == RING)
		numRingPorts = 2;
	else
		numRingPorts = 0;

	assert(port >= g_global_router_links_offset);
	assert(port < g_ports - numRingPorts);
	assert(switchApos < g_a_routers_per_group);

	return switchApos * g_h_global_ports_per_router + port - g_global_router_links_offset;
}

/* 
 * Maps the global link id within the group and switch id
 * into the (global) switch port number
 */
int groupGlobalLinkID2port(int id, int switchApos) {

	assert(id >= 0);
	assert(id < g_a_routers_per_group * g_h_global_ports_per_router);
	assert(switchApos < g_a_routers_per_group);

	return id - switchApos * g_h_global_ports_per_router + g_global_router_links_offset;
}
