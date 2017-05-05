/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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

#ifndef class_val
#define class_val

#include "routing.h"
#include "flexibleRouting.h"

template<class base>
class val: public base {
protected:

public:

	val(switchModule *switchM) :
			base(switchM) {
		const int minLocalVCs = 3, minGlobalVCs = 2;
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
		if (g_vc_usage == FLEXIBLE) {
			int petLocalVCs = g_local_link_channels - g_local_res_channels;
			int petGlobalVCs = g_global_link_channels - g_global_res_channels;
			int vc;
			/* Fill in VC arrays */
			if (g_global_link_channels > minGlobalVCs) {
				for (vc = petLocalVCs - minLocalVCs;
						vc <= (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1; vc++) {
					baseRouting::globalVc.push_back(vc);
				}
			}
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 4);
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 2);
			if (petLocalVCs > minLocalVCs) {
				for (vc = 0; vc <= petLocalVCs - minLocalVCs - 1; vc++) {
					baseRouting::localVcSource.push_back(vc);
					baseRouting::localVcInter.push_back(vc);
					baseRouting::localVcDest.push_back(vc);
				}
			}

			/* We include one additional VC value in local source array because flexible
			 * VC usage discards highest VC when performing misrouting, and first Valiant
			 * hop is always non-minimal. */
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcInter.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 5);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 1);

			assert(baseRouting::globalVc.size() == petGlobalVCs);
			assert(baseRouting::localVcSource.size() == petLocalVCs - 1);
			assert(baseRouting::localVcInter.size() == petLocalVCs - 1);
			assert(baseRouting::localVcDest.size() == petLocalVCs);

			if (g_reactive_traffic) {
				/* Under Flexible VC usage, response messages can use both its
				 * reserved VCs plus those of the petitions. */
				assert(g_local_res_channels >= minLocalVCs && g_global_res_channels >= minGlobalVCs);
				baseRouting::globalResVc = baseRouting::globalVc;
				if (g_global_res_channels > minGlobalVCs) {
					for (vc = g_local_link_channels - minLocalVCs + petGlobalVCs;
							vc <= g_local_link_channels - minLocalVCs + g_global_link_channels - minGlobalVCs - 1;
							vc++) {
						baseRouting::globalResVc.push_back(vc);
					}
				}
				baseRouting::globalResVc.push_back(g_local_link_channels + g_global_link_channels - 4);
				baseRouting::globalResVc.push_back(g_local_link_channels + g_global_link_channels - 2);
				baseRouting::localResVcSource = baseRouting::localVcDest;
				baseRouting::localResVcInter = baseRouting::localVcDest;
				baseRouting::localResVcDest = baseRouting::localVcDest;
				if (g_local_res_channels > minLocalVCs) {
					for (vc = petLocalVCs + petGlobalVCs; vc <= g_local_link_channels - minLocalVCs + petGlobalVCs - 1;
							vc++) {
						baseRouting::localResVcSource.push_back(vc);
						baseRouting::localResVcInter.push_back(vc);
						baseRouting::localResVcDest.push_back(vc);
					}
				}
				/* We include an additional VC value in source array to fix 2 cases:
				 A) Inter group is src group, to distinguish between minimal and misroute hop;
				 B) Inter group is dest group, so local hop in source group is minimal. */
				baseRouting::localResVcSource.push_back(g_local_link_channels + g_global_link_channels - 5);
				baseRouting::localResVcSource.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcInter.push_back(g_local_link_channels + g_global_link_channels - 5);
				baseRouting::localResVcInter.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 5);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 1);

				assert(baseRouting::globalResVc.size() == g_global_link_channels);
				assert(baseRouting::localResVcSource.size() == g_local_link_channels - 1);
				assert(baseRouting::localResVcInter.size() == g_local_link_channels - 1);
				assert(baseRouting::localResVcDest.size() == g_local_link_channels);
			}
		}
	}
	~val() {
	}
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int destination, valNode, minOutP, valOutP;

		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		minOutP = baseRouting::minOutputPort(destination);

		/* Calculate Valiant node output port (&VC) */
		valNode = flit->valId;
		valOutP = baseRouting::minOutputPort(valNode);

		/* If flit is being injected into the network, set flit parameters */
		if (inPort < g_p_computing_nodes_per_router) {
			flit->setMisrouted(true, VALIANT);
			flit->setCurrentMisrouteType(VALIANT);
		}

		/* If Valiant node is in current group, flag flit as already Valiant misrouted */
		if (baseRouting::switchM->hPos == int(valNode / (g_p_computing_nodes_per_router * g_a_routers_per_group))) {
			flit->valNodeReached = true;
		}

		/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
		if (!flit->valNodeReached)
			selectedRoute.port = valOutP;
		else
			selectedRoute.port = minOutP;

		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		selectedRoute.vc = base::nextChannel(inPort, selectedRoute.port, flit);

		/* Sanity checks */
		assert(selectedRoute.port < baseRouting::portCount && selectedRoute.port >= 0);
		assert(selectedRoute.neighPort < baseRouting::portCount && selectedRoute.neighPort >= 0);

		return selectedRoute;
	}

private:
	/*
	 * Defined for coherence reasons. Should never be called.
	 */
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		assert(0);
	}
	/*
	 * Defined for coherence reasons. Should never be called.
	 */
	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}

}
;

#endif
