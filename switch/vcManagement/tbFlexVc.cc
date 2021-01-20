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

#include "tbFlexVc.h"
#include "../switchModule.h"

/* Table-based FlexVC determines the VC through minimal and nonminimal tables set up at the start of the execution. */
tbFlexVc::tbFlexVc(vector<portClass> * hopSeq, switchModule * switchM) {
	this->switchM = switchM;
	typeVc = *hopSeq;

	/* Check if the routing has opportunistic hops or not in order to set up the tables */
	int minLocalVCs = 0, minGlobalVCs = 0, numOppLocHops = 0, numOppGlobHops = 0;
	vector<portClass>::iterator it;
	for (it = hopSeq->begin(); it != hopSeq->end(); ++it) {
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
	assert(g_local_link_channels >= minLocalVCs);
	assert(g_global_link_channels >= minGlobalVCs);

	/* Setup auxiliary arrays with the mandatory VCs */
	int petLocalVCs = g_local_link_channels - g_local_res_channels;
	int petGlobalVCs = g_global_link_channels - g_global_res_channels;
	int vc = minLocalVCs + minGlobalVCs;
	for (it = hopSeq->begin(); it != hopSeq->end(); ++it) {
		switch (*it) {
			case portClass::global:
			case portClass::oppglobal:
				globalVc.push_back(petLocalVCs + petGlobalVCs - vc);
				vc--;
				break;
			case portClass::local:
				localVcDest.push_back(petLocalVCs + petGlobalVCs - vc);
				vc--;
				break;
			case portClass::opplocal:
				break;
			default:
				assert(0);
		}
	}

	/* Set up the tables for minimal/nonminimal routes */
	int destH, destSw, destP;
	this->tableVcGroupMin = new short[g_a_routers_per_group * g_h_global_ports_per_router + 1];
	this->tableVcGroupNonmin = new short[g_a_routers_per_group * g_h_global_ports_per_router + 1];
	for (destH = 0; destH < (g_h_global_ports_per_router * g_a_routers_per_group + 1); destH++) {
		if (destH == switchM->hPos) {
			this->tableVcGroupMin[destH] = -1;
			continue;
		}
		/* Determine if the minimal path to the dest group is through a local or global link */
		int offsetH;
		if (g_palm_tree_configuration)
			offsetH = module((switchM->hPos - destH), (g_a_routers_per_group * g_h_global_ports_per_router + 1)) - 1;
		else
			offsetH = (destH > switchM->hPos) ? destH - 1 : destH;

		if (int(offsetH / g_h_global_ports_per_router) == switchM->aPos) { /* Global port */
			this->tableVcGroupMin[destH] = short(globalVc.back());
			this->tableVcGroupNonmin[destH] = short(globalVc.front());
		} else { /* Local port */
			this->tableVcGroupMin[destH] = short(localVcDest.at(localVcDest.size() - 2));
			this->tableVcGroupNonmin[destH] = short(localVcDest.front());
		}
	}
	this->tableVcSwMin = new short[g_a_routers_per_group];
	this->tableVcSwNonmin = new short[g_a_routers_per_group];
	for (destSw = 0; destSw < g_a_routers_per_group; destSw++) {
		this->tableVcSwMin[destSw] = localVcDest.back();
		this->tableVcSwNonmin[destSw] = numOppLocHops > 0 ? localVcDest.front() : localVcDest.at(1);
	}

	/* Add the additional (non-mandatory) VCs to the corresponding VC arrays */
	if (petGlobalVCs > minGlobalVCs) {
		for (vc = (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1; vc >= petLocalVCs - minLocalVCs;
				vc--)
			globalVc.insert(globalVc.begin(), vc);
	}
	if (petLocalVCs > minLocalVCs) {
		for (vc = petLocalVCs - minLocalVCs - 1; vc >= 0; vc--)
			localVcDest.insert(localVcDest.begin(), vc);
	}

	if (g_reactive_traffic) {
		/* The number of VCs required for responses can change from the petitions if there are opportunistic global
		 * hops. */
		int minResLocalVCs = 0, minResGlobalVCs = 0;
		for (it = hopSeq->begin(); it != hopSeq->end(); ++it) {
			if (*it == portClass::opplocal) {
				if (*(it + 1) == portClass::oppglobal) minLocalVCs++;
			} else if (*it == portClass::local)
				minResLocalVCs++;
			else if (*it == portClass::oppglobal)
				minResLocalVCs = 0;
			else if (*it == portClass::global) minResGlobalVCs++;
		}
		if (numOppGlobHops > 0 && numOppLocHops == 0) minResLocalVCs = minLocalVCs / 2;
		assert(g_local_res_channels >= minResLocalVCs && g_global_res_channels >= minResGlobalVCs);

		/* Setup auxiliary arrays with the mandatory VCs. Under Flexible VC usage, response messages can use both its
		 * reserved VCs plus those from the petitions, with a caveat: if there is a global opportunistic hop and there
		 * aren't any local opportunistic hops in the source group (such as in source-adaptive routing), responses
		 * cannot use all the VCs in the petitions in order to ensure an increasing sequence (escape path). */
		vc = minResLocalVCs + minResGlobalVCs;
		for (vector<portClass>::iterator it = hopSeq->begin(); it != hopSeq->end(); ++it) {
			switch (*it) {
				case portClass::global:
					globalResVc.push_back(g_local_link_channels + g_global_link_channels - vc);
					vc--;
					break;
				case portClass::oppglobal:
					globalResVc.push_back(globalVc.back());
					break;
				case portClass::local:
					if (numOppGlobHops > 0 && localResVcDest.size() < (minLocalVCs - minResLocalVCs))
						localResVcDest.push_back(
								localVcDest.at(
										localVcDest.size() - minLocalVCs + minResLocalVCs + localResVcDest.size()));
					else {
						localResVcDest.push_back(g_local_link_channels + g_global_link_channels - vc);
						vc--;
					}
					break;
				case portClass::opplocal:
					if (*(it + 1) == portClass::oppglobal)
						localResVcDest.push_back(
								localVcDest.at(
										localVcDest.size() - minLocalVCs + minResLocalVCs + localResVcDest.size()));
					break;
				default:
					assert(0);
			}
		}
		assert(globalResVc.size() == minGlobalVCs && localResVcDest.size() == minLocalVCs);

		/* Set up the tables for minimal/nonminimal routes */
		this->tableResVcGroupMin = new short[g_a_routers_per_group * g_h_global_ports_per_router + 1];
		this->tableResVcGroupNonmin = new short[g_a_routers_per_group * g_h_global_ports_per_router + 1];
		for (destH = 0; destH < ((g_h_global_ports_per_router * g_a_routers_per_group) + 1); destH++) {
			if (destH == switchM->hPos) {
				this->tableResVcGroupMin[destH] = -1;
				continue;
			}
			/* Determine if the minimal path to the dest group is through a local or global link */
			int offsetH;
			if (g_palm_tree_configuration)
				offsetH = module((switchM->hPos - destH), (g_a_routers_per_group * g_h_global_ports_per_router + 1))
						- 1;
			else
				offsetH = (destH > switchM->hPos) ? destH - 1 : destH;

			if (int(offsetH / g_h_global_ports_per_router) == switchM->aPos) { /* Global port */
				this->tableResVcGroupMin[destH] = globalResVc.back();
				this->tableResVcGroupNonmin[destH] = globalResVc.front();
			} else { /* Local port */
				this->tableResVcGroupMin[destH] = localResVcDest.at(localResVcDest.size() - 2);
				this->tableResVcGroupNonmin[destH] = localResVcDest.front();
			}
		}

		this->tableResVcSwMin = new short[g_a_routers_per_group];
		this->tableResVcSwNonmin = new short[g_a_routers_per_group];
		for (destSw = 0; destSw < g_a_routers_per_group; destSw++) {
			this->tableResVcSwMin[destSw] = localResVcDest.back();
			this->tableResVcSwNonmin[destSw] = localResVcDest.at(1);
		}

		/* Now set up the auxiliary VC arrays with all the VCs that can be used by the replies */
		for (vector<int>::iterator it = globalVc.end() - 1 - numOppGlobHops; it != globalVc.begin() - 1; --it) {
			if (globalResVc.front() != *it) globalResVc.insert(globalResVc.begin(), *it);
		}
		for (vector<int>::iterator it = localVcDest.end() - 1 - minLocalVCs + minResLocalVCs;
				it != localVcDest.begin() - 1; --it) {
			if (localResVcDest.front() != *it) localResVcDest.insert(localResVcDest.begin(), *it);
		}

		/* Assign additional VCs */
		if (g_global_res_channels > minResGlobalVCs) {
			for (vc = g_local_link_channels - minResLocalVCs + petGlobalVCs;
					vc <= g_local_link_channels - minResLocalVCs + g_global_link_channels - minResGlobalVCs - 1; vc++)
				globalResVc.insert(globalResVc.begin() + petGlobalVCs - minGlobalVCs, vc);
		}
		if (g_local_res_channels > minResLocalVCs) {
			for (vc = petLocalVCs + petGlobalVCs; vc <= g_local_link_channels - minResLocalVCs + petGlobalVCs - 1; vc++)
				localResVcDest.insert(localResVcDest.begin() + petLocalVCs, vc);
		}
	}
}

tbFlexVc::~tbFlexVc() {
	delete[] tableVcSwMin;
	delete[] tableVcGroupMin;
	delete[] tableVcSwNonmin;
	delete[] tableVcGroupNonmin;
	delete[] tableResVcSwMin;
	delete[] tableResVcGroupMin;
	delete[] tableResVcSwNonmin;
	delete[] tableResVcGroupNonmin;
	globalVc.clear();
	localVcDest.clear();
	globalResVc.clear();
	localResVcDest.clear();
}

/*
 * Auxiliary function to return the array of available VCs for a given port.
 */
vector<int> tbFlexVc::getVcArray(int port) {
	vector<int> vcArray;

	assert(portType(port) == portClass::local || portType(port) == portClass::global);
	if (g_reactive_traffic)
		vcArray = portType(port) == portClass::local ? localResVcDest : globalResVc;
	else
		vcArray = portType(port) == portClass::local ? localVcDest : globalVc;

	return vcArray;
}

/*
 * Determines next channel to be used, following a given policy to choose between the range of possible VCs for that
 * hop.
 */
int tbFlexVc::nextChannel(int inP, int outP, flitModule * flit) {
	if (portType(outP) == portClass::node) return (flit->channel);

	/* Determine the highest VC that can be used */
	short highestVc;
	vector<int> auxVc;
	if (g_reactive_traffic && flit->flitType == RESPONSE) {
		if (outP == switchM->routing->minOutputPort(flit->destId)
				&& (flit->getCurrentMisrouteType() != VALIANT || flit->valNodeReached)) {
			if (flit->destGroup != switchM->hPos) /* Dest in other group */
				highestVc = this->tableResVcGroupMin[flit->destGroup];
			else
				highestVc = this->tableResVcSwMin[flit->destSwitch % g_a_routers_per_group];
		} else {
			assert(outP == switchM->routing->minOutputPort(flit->valId));
			int valSw = int(flit->valId / g_p_computing_nodes_per_router);
			int valGroup = int(valSw / g_a_routers_per_group);
			if (valGroup != switchM->hPos) /* Valiant dest in other group */
				highestVc = this->tableResVcGroupNonmin[valGroup];
			else
				highestVc = this->tableResVcSwNonmin[valSw % g_a_routers_per_group];
		}
		auxVc = this->portType(outP) == portClass::global ? globalResVc : localResVcDest;
	} else {
		if (outP == switchM->routing->minOutputPort(flit->destId)
				&& (flit->getCurrentMisrouteType() != VALIANT || flit->valNodeReached)) {
			if (flit->destGroup != switchM->hPos) /* Dest in other group */
				highestVc = this->tableVcGroupMin[flit->destGroup];
			else
				highestVc = this->tableVcSwMin[flit->destSwitch % g_a_routers_per_group];
		} else {
			assert(outP == switchM->routing->minOutputPort(flit->valId));
			int valSw = int(flit->valId / g_p_computing_nodes_per_router);
			int valGroup = int(valSw / g_a_routers_per_group);
			if (valGroup != switchM->hPos) /* Valiant dest in other group */
				highestVc = this->tableVcGroupNonmin[valGroup];
			else
				highestVc = this->tableVcSwNonmin[valSw % g_a_routers_per_group];
		}
		auxVc = this->portType(outP) == portClass::global ? globalVc : localVcDest;
	}
	/* Remove from the range of possible VCs those higher than the highest VC allowed for the hop */
	assert(highestVc >= 0 && highestVc < g_channels);
	for (int i = auxVc.size() - 1; i >= 0; --i)
		if (auxVc.back() > highestVc) auxVc.pop_back();

	/* Select a VC from the available range, following the VC allocation policy */
	int next_channel = -1, check_channel, min_occupancy = switchM->getMaxCredits(outP, flit->cos, 0) + 1;
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
