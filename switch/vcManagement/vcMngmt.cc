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

#include "vcMngmt.h"
#include <iostream>

vcMngmt::vcMngmt(vector<portClass> * hopSeq, switchModule * switchM) {
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
			locVc++;
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
				globalResVc.push_back(globVc);
				globVc++;
			} else {
				responseVc.push_back(locVc);
				localVcDest.push_back(locVc);
				localResVcDest.push_back(locVc);
				locVc++;
				if (globVc == globalVc[-1]) localResVcSource.push_back(locVc);
			}
		}
		assert(responseVc.size() == hopSeq->size());
		assert(responseVc.size() + petitionVc.size() == localVcDest.size() + globalVc.size());
	}
}

vcMngmt::~vcMngmt() {
	petitionVc.clear();
	responseVc.clear();
}

/*
 * Auxiliary function to determine port type. Returns an enumerate to distinguish between compute node, local link,
 * global link, or physical ring link.
 */
portClass vcMngmt::portType(int port) {
	portClass type;
	if (port < g_p_computing_nodes_per_router)
		type = portClass::node;
	else if (port < g_global_router_links_offset)
		type = portClass::local;
	else if (port < g_global_router_links_offset + g_h_global_ports_per_router)
		type = portClass::global;
	else if (g_congestion_management == QCNSW && port == g_qcn_port)
		type = portClass::qcn;
	else {
		type = portClass::ring;
		assert(g_deadlock_avoidance == RING);
	}
	return type;
}

/*
 * Auxiliary function to return the array of available VCs for a given port.
 */
vector<int> vcMngmt::getVcArray(int port) {
	vector<int> vcArray;

	if (portType(port) == portClass::local)
		vcArray = localVcDest;
	else if (portType(port) == portClass::global)
		vcArray = globalVc;
	else {
		cerr << "ERROR: trying to get vc array for a non-transit port" << endl;
		assert(false);
	}

	return vcArray;
}

/*
 * Auxiliary function to return the array of available VCs at the source group.
 */
vector<int> vcMngmt::getSrcVcArray(int port) {
	vector<int> vcArray;

	if (portType(port) == portClass::local)
		vcArray = localVcSource;
	else if (portType(port) == portClass::global) /* Discard last global VC */
		vcArray = std::vector<int>(globalVc.begin(), globalVc.end() - 2);
	else {
		cerr << "ERROR: trying to get vc array for a non-transit port" << endl;
		assert(false);
	}

	return vcArray;
}

/*
 * Determines next channel to be used, following an increasing sequence to break cyclic dependencies.
 */
int vcMngmt::nextChannel(int inP, int outP, flitModule * flit) {
	portClass outType, inType;
	int next_channel = -1, i, index, inVC = flit->channel;

	inType = portType(inP);
	outType = portType(outP);
	vector<int> auxVc;
	/* Reactive traffic splits evenly the channels between petition and response
	 * flits, so each kind of traffic receives the same number of resources. */
	if (g_reactive_traffic && flit->flitType == RESPONSE)
		auxVc = this->responseVc;
	else
		auxVc = this->petitionVc;

	assert(auxVc.size() == typeVc.size());

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
	for (i = index + 1; i < auxVc.size(); i++) {
		if (outType == typeVc[i]) {
			next_channel = auxVc[i];
			break;
		}
	}

	if (outType == portClass::node) next_channel = inVC;
	assert(next_channel >= 0 && next_channel < g_channels); // Sanity check
	return next_channel;
}

/*
 * Returns highest local or global VC that is used by the VC management.
 */
int vcMngmt::getHighestVc(portClass portType) {
	int vc;
	if (g_reactive_traffic)
		vc = (portType == portClass::local) ? localResVcDest.back() : globalResVc.back();
	else
		vc = (portType == portClass::local) ? localVcDest.back() : globalVc.back();
	return vc;
}

void vcMngmt::checkVcArrayLengths(short minLocalVCs, short minGlobalVCs) {
	assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
	assert(g_local_link_channels >= localVcDest.size() && g_global_link_channels >= globalVc.size());
	if (g_reactive_traffic) assert(g_local_link_channels >= localResVcDest.size() && g_global_link_channels >= globalResVc.size());
}
