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

#include "pbFlit.h"

long long pbFlit::id = 0;

/* 
 * Creates a message with a PARTIAL COPY of the globalLinkCongested array
 * with g_dH elements (one per global link in the switch)
 */
pbFlit::pbFlit(int srcOffset, bool** globalLinkCongested, int arrivalCycle) :
		m_srcOffset(srcOffset), m_arrivalCycle(arrivalCycle), m_generatedCycle(g_cycle), m_id(pbFlit::id++) {
	//makes a local copy of the source globalLinkCongested info
	//(g_dH elements after the offset)
	m_globalLinkInfo = new bool*[g_dH];
	for (int i = 0; i < g_dH; i++) {
		m_globalLinkInfo[i] = new bool[g_channels_glob];
		for (int channel = 0; channel < g_channels_glob; channel++) {
			m_globalLinkInfo[i][channel] = globalLinkCongested[srcOffset + i][channel];
		}
		assert(m_globalLinkInfo[i] != NULL);
	}
}

pbFlit::pbFlit(const pbFlit& original) :
		m_srcOffset(original.m_srcOffset), m_arrivalCycle(original.m_arrivalCycle), m_generatedCycle(
				original.m_generatedCycle), m_id(original.m_id) {
	m_globalLinkInfo = new bool*[g_dH];
	assert(original.m_globalLinkInfo != NULL);
	for (int i = 0; i < g_dH; i++) {
		m_globalLinkInfo[i] = new bool[g_channels_glob];
		assert(original.m_globalLinkInfo[i] != NULL);
		for (int channel = 0; channel < g_channels_glob; channel++) {
			m_globalLinkInfo[i][channel] = original.m_globalLinkInfo[i][channel];
		}
		assert(m_globalLinkInfo[i] != NULL);
	}
}

pbFlit::~pbFlit() {
	for (int i = 0; i < g_dH; i++) {
		delete[] m_globalLinkInfo[i];
		m_globalLinkInfo[i] = NULL;
	}
	delete[] m_globalLinkInfo;
}

int pbFlit::GetArrivalCycle() const {
	return m_arrivalCycle;
}

/* 
 * Returns true if the n-th global link in the
 * current switch is congested
 */
bool pbFlit::GetGlobalLinkInfo(int n, int channel) const {
	return m_globalLinkInfo[n][channel];
}

int pbFlit::GetSrcOffset() const {
	return m_srcOffset;
}

/*
 *  Priority comparation: earlier flit has higher priority
 */
bool pbFlit::operator <(const pbFlit& flit) const {
	return flit.GetArrivalCycle() < GetArrivalCycle();
}

long long pbFlit::GetId() const {
	return m_id;
}

int pbFlit::GetGeneratedCycle() const {
	return m_generatedCycle;
}

