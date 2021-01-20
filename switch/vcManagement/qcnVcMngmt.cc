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

#include "qcnVcMngmt.h"
#include "../switchModule.h"

qcnVcMngmt::qcnVcMngmt(vector<portClass> * hopSeq, switchModule * switchM, int localVcOffset, int globalVcOffset) :
		vcMngmt(hopSeq, switchM) {
	/* Set up the vc arrays, increasing the vcs with the vc offsets */
	for (vector<int>::iterator it = globalVc.begin(); it != globalVc.end(); ++it) {
		*it += globalVcOffset;
	}
	for (vector<int>::iterator it = localVcDest.begin(); it != localVcDest.end(); ++it) {
		*it += localVcOffset;
	}
	for (vector<int>::iterator it = localVcSource.begin(); it != localVcSource.end(); ++it) {
		*it += localVcOffset;
	}
	int aux = 0;
	for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it, aux++) {
		if (*it == portClass::global || *it == portClass::oppglobal)
			petitionVc[aux] += globalVcOffset;
		else
			petitionVc[aux] += localVcOffset;
	}

	if (g_reactive_traffic) {
		aux = 0;
		for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it, aux++) {
			if (*it == portClass::global || *it == portClass::oppglobal)
				responseVc[aux] += globalVcOffset;
			else
				responseVc[aux] += localVcOffset;
		}
	}
}

qcnVcMngmt::~qcnVcMngmt() {
}

void qcnVcMngmt::checkVcArrayLengths(short minLocalVCs, short minGlobalVCs) {
	assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
	assert(g_local_link_channels >= localVcDest.size() && g_global_link_channels >= globalVc.size());
	assert(g_congestion_management == QCNSW);
	assert(g_local_link_channels >= minLocalVCs + localVcDest.size() && g_global_link_channels >= minGlobalVCs + globalVc.size());
}
