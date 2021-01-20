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

#include "rlmVcMngmt.h"
#include "../switchModule.h"

rlmVcMngmt::rlmVcMngmt(vector<portClass> * hopSeq, switchModule * switchM) {
	this->switchM = switchM;

	/* Set up the vc arrays; if reactive traffic is in use, it needs to halve available vc resources to separate
	 * petitions from responses. */
	typeVc = *hopSeq;
	for (vector<portClass>::iterator it = typeVc.begin(); it != typeVc.end(); ++it) {
		if (*it == portClass::oppglobal)
			*it = portClass::global;
		else if (*it == portClass::opplocal) *it = portClass::local;
	}

	int locVc = 0, globVc = 0;
	for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
		if (*it == portClass::global || *it == portClass::oppglobal) {
			petitionVc.push_back(globVc);
			globalVc.push_back(globVc);
			globVc++;
		} else {
			petitionVc.push_back(locVc);
			localVcDest.push_back(locVc);
			/* Local consecutive hops reuse the same VC because the cyclic dependency is avoided through route
			 * restrictions */
			if (it == hopSeq->end() - 1 || *it != *(it + 1)) locVc++;
			if (globVc == 0) localVcSource.push_back(locVc);
		}
	}
	assert(petitionVc.size() == hopSeq->size());
	assert(petitionVc.size() == localVcDest.size() + globalVc.size());

	if (g_reactive_traffic) {
		/* For reactive traffic VCs, the indices are not reset to start with the next vc to the highest used in the
		 * petitions. */
		for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
			if (*it == portClass::global || *it == portClass::oppglobal) {
				responseVc.push_back(globVc);
				globalVc.push_back(globVc);
				globVc++;
			} else {
				responseVc.push_back(locVc);
				localVcDest.push_back(locVc);
				/* Local consecutive hops reuse the same VC because the cyclic dependency is avoided through route
				 * restrictions */
				if (it == hopSeq->end() - 1 || *it != *(it + 1)) locVc++;
				if (globVc == globalVc[-1]) localResVcSource.push_back(locVc);
			}
		}
		assert(responseVc.size() == hopSeq->size());
		assert(responseVc.size() + petitionVc.size() == localVcDest.size() + globalVc.size());
	}
}

rlmVcMngmt::~rlmVcMngmt() {
}
