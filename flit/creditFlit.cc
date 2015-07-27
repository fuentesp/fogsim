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

#include "creditFlit.h"

creditFlit::creditFlit(float arrivalCycle, int numCreds, int vc, int flitId) :
		m_arrivalCycle(arrivalCycle), m_numCreds(numCreds), m_vc(vc), m_flitId(flitId) {
}

creditFlit::~creditFlit() {
}

int creditFlit::getVc() const {
	return m_vc;
}

int creditFlit::getNumCreds() const {
	return m_numCreds;
}

float creditFlit::getArrivalCycle() const {
	return m_arrivalCycle;
}

/*
 *  Priority comparation: earlier flit has higher priority
 */
bool creditFlit::operator <(const creditFlit& flit) const {
	return flit.getArrivalCycle() < getArrivalCycle();
}

int creditFlit::getFlitId() const {
	return m_flitId;
}

