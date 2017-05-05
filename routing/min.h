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

#ifndef class_minimal
#define class_minimal

#include "routing.h"
#include "flexibleRouting.h"

template<class base>
class minimal: public base {
protected:

public:

	minimal(switchModule *switchM) :
			base(switchM) {
		const int minLocalVCs = 2, minGlobalVCs = 1;
		assert(g_deadlock_avoidance == DALLY); // Sanity check
		assert(g_local_link_channels >= minLocalVCs && g_global_link_channels >= minGlobalVCs);
		if (g_vc_usage == FLEXIBLE) {
			int petLocalVCs = g_local_link_channels - g_local_res_channels;
			int petGlobalVCs = g_global_link_channels - g_global_res_channels;
			int vc;
			/* Fill in VC arrays */
			if (petGlobalVCs > minGlobalVCs) {
				for (vc = petLocalVCs - minLocalVCs;
						vc <= (petLocalVCs - minLocalVCs) + (petGlobalVCs - minGlobalVCs) - 1; vc++) {
					baseRouting::globalVc.push_back(vc);
				}
			}
			baseRouting::globalVc.push_back(petLocalVCs + petGlobalVCs - 2);
			if (petLocalVCs > minLocalVCs) {
				for (vc = 0; vc <= petLocalVCs - minLocalVCs - 1; vc++) {
					baseRouting::localVcSource.push_back(vc);
					baseRouting::localVcDest.push_back(vc);
				}
			}
			baseRouting::localVcSource.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 3);
			baseRouting::localVcDest.push_back(petLocalVCs + petGlobalVCs - 1);

			assert(baseRouting::globalVc.size() == petGlobalVCs);
			assert(baseRouting::localVcSource.size() == petLocalVCs - 1);
			assert(baseRouting::localVcInter.size() == 0);
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
				baseRouting::globalResVc.push_back(g_local_link_channels + g_global_link_channels - 2);
				baseRouting::localResVcSource = baseRouting::localVcDest;
				baseRouting::localResVcDest = baseRouting::localVcDest;
				if (g_local_res_channels > minLocalVCs) {
					for (vc = petLocalVCs + petGlobalVCs; vc <= g_local_link_channels - minLocalVCs + petGlobalVCs - 1;
							vc++) {
						baseRouting::localResVcSource.push_back(vc);
						baseRouting::localResVcDest.push_back(vc);
					}
				}
				baseRouting::localResVcSource.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 3);
				baseRouting::localResVcDest.push_back(g_local_link_channels + g_global_link_channels - 1);

				assert(baseRouting::globalResVc.size() == g_global_link_channels);
				assert(baseRouting::localResVcSource.size() == g_local_link_channels - 1);
				assert(baseRouting::localResVcInter.size() == 0);
				assert(baseRouting::localResVcDest.size() == g_local_link_channels);
			}
		} else if (g_vc_usage == BASE) {
			/* Remove default VC and port type ordering to replace with specific for oblivious routing */
			baseRouting::typeVc.clear();
			baseRouting::petitionVc.clear();
			baseRouting::responseVc.clear();
			char aux[] = { 'a', 'h', 'a' };
			baseRouting::typeVc.insert(baseRouting::typeVc.begin(), aux, aux + 3);
			int aux2[] = { 0, 0, 1 };
			baseRouting::petitionVc.insert(baseRouting::petitionVc.begin(), aux2, aux2 + 3);
			if (g_reactive_traffic) {
				int aux3[] = { 2, 1, 3 };
				baseRouting::responseVc.insert(baseRouting::responseVc.begin(), aux3, aux3 + 3);
			}
		}
	}
	~minimal();

	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int destination;
		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		selectedRoute.port = baseRouting::minOutputPort(destination);
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);
		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		return selectedRoute;
	}

private:
	MisrouteType misrouteType(int inport, int inchannel, flitModule * flit, int minOutPort, int minOutVC) {
		assert(0);
	}

	int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) {
		assert(0);
	}
};

#endif
