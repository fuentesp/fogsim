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

#include "oppVcMngmt.h"
#include "../switchModule.h"

oppVcMngmt::oppVcMngmt(vector<portClass> * hopSeq, switchModule * switchM) :
		vcMngmt(hopSeq, switchM) {
}

oppVcMngmt::~oppVcMngmt() {
}

/*
 * Determines next channel, following an increasing sequence to break cyclic dependencies, but addressing the
 * specific case of opportunistic misrouting hops (which will employ previous traversal VC).
 */
int oppVcMngmt::nextChannel(int inP, int outP, flitModule * flit) {
	portClass outType, inType;
	int next_channel = -1, i, index, inVC = flit->channel;

	inType = portType(inP);
	outType = portType(outP);
	vector<int> auxVc;
	/* Reactive traffic splits evenly the channels between petition and response flits, so each kind of traffic
	 * receives the same number of resources. */
	if (g_reactive_traffic && flit->flitType == RESPONSE)
		auxVc = this->responseVc;
	else
		auxVc = this->petitionVc;

	assert(auxVc.size() == this->typeVc.size());

	if (inType == portClass::node || inType == portClass::qcn)
		index = -1;
	else {
		for (i = 0; i < auxVc.size(); i++) {
			if (inType == typeVc[i] && inVC == auxVc[i]) {
				index = i;
				break;
			}
		}
	}
	assert(index < (int ) auxVc.size());
	/* If next hop is local misrouting, VC is picked from immediate previous local hop VC. */
	if (outP != switchM->routing->minOutputPort(flit->destId) && outType == portClass::local
			&& inType != portClass::node)
		for (i = index; i >= 0; i--) {
			if (typeVc[i] == portClass::local) {
				next_channel = auxVc[i];
				break;
			}
		}
	else
		for (i = index + 1; i < auxVc.size(); i++) {
			if (outType == typeVc[i]) {
				next_channel = auxVc[i];
				break;
			}
		}

	if (outType == portClass::node
			|| (switchM->hPos == flit->sourceGroup && flit->localMisroutingDone
					&& outType == portClass::local)) next_channel = inVC;
	assert(next_channel >= 0 && next_channel < g_channels); // Sanity check
	return (next_channel);
}
