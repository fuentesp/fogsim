/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2014 University of Cantabria

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

	m_offset = m_switchApos * g_dH;
	m_globalLinkCongested = new bool*[g_globalLinksPerGroup];

	for (int link = 0; link < g_globalLinksPerGroup; link++) {
		m_globalLinkCongested[link] = new bool[g_channels_glob];
		for (int channel = 0; channel < g_channels_glob; channel++) {
			m_globalLinkCongested[link][channel] = false;
		}
	}
}

pbState::~pbState() {
	for (int link = 0; link < g_globalLinksPerGroup; link++) {
		delete[] m_globalLinkCongested[link];
	}

	delete[] m_globalLinkCongested;
}

void pbState::readFlit(const pbFlit& flit) {
	int srcOffset, i, channel;

	srcOffset = flit.GetSrcOffset();

	assert(srcOffset != m_offset);
	assert(srcOffset <= g_dA * g_dH - g_dH);

	for (i = 0; i < g_dH; i++) {
		for (channel = 0; channel < g_channels_glob; channel++) {
			m_globalLinkCongested[srcOffset + i][channel] = flit.GetGlobalLinkInfo(i, channel);
		}
	}
}

/* 
 * Create a NEW pbFlit with its information. Needs the latency to
 * determine the arrival cycle of the flit.
 * 
 * This flit must be deleted after use.
 */
pbFlit* pbState::createFlit(int latency) {

	pbFlit* flit = new pbFlit(m_offset, m_globalLinkCongested, g_cycle + latency);

	return flit;
}

void pbState::update(int port, int channel, bool linkCongested) {

	m_globalLinkCongested[port2groupGlobalLinkID(port, m_switchApos)][channel] = linkCongested;
}

bool pbState::isCongested(int link, int channel) {
	return m_globalLinkCongested[link][channel];
}

/* 
 * Maps the switch port number and switch id
 * into the global link id within the group
 * 
 * Must be a global link port.
 */
int port2groupGlobalLinkID(int port, int switchApos) {

	int numRingPorts;

	if ((g_rings == 0) || (g_embeddedRing != 0))
		numRingPorts = 0;
	else
		numRingPorts = 2;

	assert(port >= g_offsetH);
	assert(port < g_ports - numRingPorts);
	assert(switchApos < g_dA);

	return switchApos * g_dH + port - g_offsetH;
}

/* 
 * Maps the global link id within the group and switch id
 * into the (global) switch port number
 */
int groupGlobalLinkID2port(int id, int switchApos) {

	assert(id >= 0);
	assert(id < g_dA * g_dH);
	assert(switchApos < g_dA);

	return id - switchApos * g_dH + g_offsetH;
}
