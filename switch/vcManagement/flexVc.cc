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

#include "flexVc.h"
#include "../switchModule.h"

flexVc::flexVc(vector<portClass> * hopSeq, switchModule * switchM) {
	this->switchM = switchM;
	typeVc = *hopSeq;

	/* Set up the vc arrays */
	int petLocalVCs = g_local_link_channels - g_local_res_channels;
	int petGlobalVCs = g_global_link_channels - g_global_res_channels;
	int vc, locVc = -1, minLocalVCs = 0, minGlobalVCs = 0, numOppLocHops = 0, numOppGlobHops = 0;

	for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
		switch (*it) {
			case portClass::opplocal:
				numOppLocHops++;
				break;
			case portClass::local:
				minLocalVCs++;
				break;
			case portClass::oppglobal:
				numOppGlobHops++;
				minGlobalVCs++;
				break;
			case portClass::global:
				minGlobalVCs++;
				break;
		}
	}
	/* Assign additional VCs */
	if (petGlobalVCs > minGlobalVCs) {
		for (vc = petLocalVCs - minLocalVCs; vc <= (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1;
				vc++)
			globalVc.push_back(vc);
	}
	if (petLocalVCs > minLocalVCs) {
		for (vc = 0; vc <= petLocalVCs - minLocalVCs - 1; vc++) {
			localVcSource.push_back(vc);
			localVcInter.push_back(vc);
			localVcDest.push_back(vc);
		}
	}
	/* Assign mandatory VCs */
	vc = minLocalVCs + minGlobalVCs;
	for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
		switch (*it) {
			case portClass::global:
			case portClass::oppglobal:
				globalVc.push_back(petLocalVCs + petGlobalVCs - vc);
				vc--;
				break;
			case portClass::local:
				locVc = petLocalVCs + petGlobalVCs - vc;
				if (globalVc.size() == 0) {
					localVcSource.push_back(locVc);
					localVcInter.push_back(locVc);
					localVcDest.push_back(locVc);
				} else if (globalVc.size() == 1) {
					localVcInter.push_back(locVc);
					localVcDest.push_back(locVc);
				} else {
					localVcDest.push_back(locVc);
				}
				vc--;
				break;
			case portClass::opplocal:
				assert(locVc != -1);
				if (globalVc.size() == 0)
					localVcSource.push_back(locVc);
				else if (globalVc.size() == 1)
					localVcInter.push_back(locVc);
				else
					localVcDest.push_back(locVc);
				break;
			default:
				assert(0);
		}
	}

	assert(globalVc.size() == petGlobalVCs);
	assert(localVcDest.size() == petLocalVCs);

	if (g_reactive_traffic) {
		/* The number of VCs required for responses can change from the petitions if there are opportunistic global
		 * hops. */
		minLocalVCs = minGlobalVCs = 0;
		for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
			if (*it == portClass::opplocal) {
				if (*(it + 1) == portClass::oppglobal) minLocalVCs++;
			} else if (*it == portClass::local)
				minLocalVCs++;
			else if (*it == portClass::oppglobal)
				minLocalVCs = 0;
			else if (*it == portClass::global) minGlobalVCs++;
		}
		if (numOppGlobHops > 0 && numOppLocHops == 0) minLocalVCs = minLocalVCs / 2;
		assert(g_local_res_channels >= minLocalVCs && g_global_res_channels >= minGlobalVCs);

		/* Under Flexible VC usage, response messages can use both its reserved VCs plus those of the petitions, with a
		 * caveat: if there is a global opportunistic hop and there aren't any local opportunistic hops in the source
		 * group, the responses cannot use all the VCs in the petitions in order to ensure an increasing sequence
		 * (escape path). */
		globalResVc = globalVc;
		bool reuseAllVcs = true;
		for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
			if (*it == portClass::oppglobal) {
				reuseAllVcs = false;
				for (vector<portClass>::iterator it2 = it - 1; it2 >= hopSeq->begin(); --it2) {
					if (*it2 == portClass::opplocal) reuseAllVcs = true;
				}
				break;
			}
		}
		localResVcSource = reuseAllVcs ? localVcDest : localVcInter;
		localResVcInter = localVcDest;
		localResVcDest = localVcDest;
		/* Assign additional VCs */
		if (g_global_res_channels > minGlobalVCs) {
			for (vc = g_local_link_channels - minLocalVCs + petGlobalVCs;
					vc <= g_local_link_channels - minLocalVCs + g_global_link_channels - minGlobalVCs - 1; vc++)
				globalResVc.push_back(vc);
		}
		if (g_local_res_channels > minLocalVCs) {
			for (vc = petLocalVCs + petGlobalVCs; vc <= g_local_link_channels - minLocalVCs + petGlobalVCs - 1; vc++) {
				localResVcSource.push_back(vc);
				localResVcInter.push_back(vc);
				localResVcDest.push_back(vc);
			}
		}
		/* Assign mandatory VCs */
		int i, numGlobHops = 0;
		vector<portClass>::iterator it = hopSeq->end() - 1;
		vector<int>::iterator it2 = localResVcDest.end(), it3 = localResVcInter.end(), it4 = localResVcSource.end();
		for (vc = g_local_link_channels + g_global_link_channels - 1;
				vc >= g_local_link_channels + g_global_link_channels - minLocalVCs - minGlobalVCs; it--) {
			switch (*it) {
				case portClass::global:
					globalResVc.push_back(vc);
					vc--;
				case portClass::oppglobal:
					numGlobHops++;
					break;
				case portClass::local:
					if (numGlobHops == 0) {
						it2 = localResVcDest.insert(it2, vc);
					} else if (numGlobHops == 1) {
						it2 = localResVcDest.insert(it2, vc);
						it3 = localResVcInter.insert(it3, vc);
					} else {
						it2 = localResVcDest.insert(it2, vc);
						it3 = localResVcInter.insert(it3, vc);
						it4 = localResVcSource.insert(it4, vc);
					}
					vc--;
					break;
				case portClass::opplocal:
					break;
				default:
					assert(0);
			}
		}
		/* We include an additional VC value in source array to fix the problem of discarding highest VC when a
		 * nonminimal port is used. */
		localVcSource.push_back(localVcInter[localVcInter.size() - 1]);
		assert(globalResVc.size() == g_global_link_channels);
		assert(localResVcDest.size() == g_local_link_channels);
	}
}

flexVc::~flexVc() {
	petitionVc.clear();
	responseVc.clear();
}

/*
 * Auxiliary function to return the array of available VCs for a given port.
 */
vector<int> flexVc::getVcArray(int port) {
	vector<int> vcArray;

	assert(portType(port) == portClass::local || portType(port) == portClass::global);
	if (g_reactive_traffic)
		vcArray = portType(port) == portClass::local ? localResVcDest : globalResVc;
	else
		vcArray = portType(port) == portClass::local ? localVcDest : globalVc;

	return vcArray;
}

/*
 * Determines next channel to be used, following an increasing sequence to break cyclic dependencies.
 */
int flexVc::nextChannel(int inP, int outP, flitModule * flit) {
	portClass outType, inType;
	int next_channel = -1, check_channel, min_occupancy;

	inType = portType(inP);
	outType = portType(outP);
	vector<int> auxVc;

	if (outType == portClass::node) return (flit->channel);

	if (g_reactive_traffic && flit->flitType == RESPONSE) {
		if (outType == portClass::global) {
			auxVc = globalResVc;
		} else {
			assert(outType == portClass::local);
			/* Check what current group is */
			if (switchM->hPos == flit->destGroup) { /* Destination group */
				auxVc = localResVcDest;
			} else if ((switchM->hPos == int(flit->valId / (g_a_routers_per_group * g_p_computing_nodes_per_router)))
					|| flit->globalMisroutingDone) { /* Intermediate group */
				auxVc = localResVcInter;
			} else { /* Source group */
				assert(switchM->hPos == flit->sourceGroup);
				auxVc = localResVcSource;
			}
		}
	} else {
		if (outType == portClass::global) {
			auxVc = globalVc;
		} else {
			assert(outType == portClass::local);
			/* Check what current group is */
			if (switchM->hPos == flit->destGroup) { /* Destination group */
				auxVc = localVcDest;
			} else if ((switchM->hPos == int(flit->valId / (g_a_routers_per_group * g_p_computing_nodes_per_router)))
					|| flit->globalMisroutingDone) { /* Intermediate group */
				auxVc = localVcInter;
			} else { /* Source group */
				assert(switchM->hPos == flit->sourceGroup);
				auxVc = localVcSource;
			}
		}
	}

	/* If misrouting is used, the highest VC in the set cannot be used, to ensure an increasing sequence */
	if (outP != switchM->routing->minOutputPort(flit->destId)
			|| (flit->getCurrentMisrouteType() == VALIANT && not (flit->valNodeReached))) auxVc.pop_back();

	/* Select VC with highest number of credits available, between all allowed VCs */
	min_occupancy = switchM->getMaxCredits(outP, flit->cos, 0) + 1;
	if (g_vc_alloc == LOWEST_VC) {
		for (vector<int>::iterator it = auxVc.begin(); it != auxVc.end(); ++it) {
			check_channel = *it;
			assert(check_channel >= 0);
			if (switchM->getCredits(outP, flit->cos, check_channel) >= g_packet_size || it == auxVc.end() - 1) {
				next_channel = check_channel;
				it = auxVc.end() - 1;
			}
		}
	} else {
		for (vector<int>::iterator it = auxVc.end() - 1; it != auxVc.begin() - 1; --it) {
			check_channel = *it;
			assert(check_channel >= 0 && check_channel < g_channels);
			switch (g_vc_alloc) {
				case LOWEST_OCCUPANCY:
					if (switchM->getCreditsOccupancy(outP, flit->cos, check_channel) < min_occupancy) {
						next_channel = check_channel;
						min_occupancy = switchM->getCreditsOccupancy(outP, flit->cos, check_channel);
					}
					break;
				case HIGHEST_VC:
					if (switchM->getCredits(outP, flit->cos, check_channel) >= g_packet_size || it == auxVc.begin()) {
						next_channel = check_channel;
						it = auxVc.begin();
					}
					break;
				case RANDOM_VC:
					if (switchM->getCredits(outP, flit->cos, check_channel) < g_packet_size && auxVc.size() > 1) {
						auxVc.erase(it);
					}
					break;
			}
		}
	}

	assert(auxVc.size() > 0);
	if (g_vc_alloc == RANDOM_VC) {
		int random = rand() % auxVc.size();
		assert(random >= 0 && random < auxVc.size());
		next_channel = auxVc[random];
	}
	assert(next_channel >= 0 && next_channel < g_local_link_channels + g_global_link_channels); // Sanity check
	return (next_channel);
}
