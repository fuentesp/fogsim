/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
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

#ifndef class_valAny
#define class_valAny

#include "routing.h"

template<class base>
class valAny: public base {
protected:

public:

	valAny(switchModule *switchM) :
			base(switchM) {
		assert(g_deadlock_avoidance == DALLY); /* Sanity check */
		assert(g_local_link_channels >= 4 && g_global_link_channels >= 2);
		int vc;
	}
	~valAny() {
	}
	candidate enroute(flitModule * flit, int inPort, int inVC) {
		candidate selectedRoute;
		int destination, valNode, minOutP, valOutP;

		/* Determine minimal output port (& VC) */
		destination = flit->destId;
		minOutP = baseRouting::minOutputPort(destination);

		/* Calculate Valiant nodevalAnytput port (&VC) */
		valNode = flit->valId;
		valOutP = baseRouting::minOutputPort(valNode);

		/* If flit is being injected into the network, set flit parameters */
		if (inPort < g_p_computing_nodes_per_router) {
			flit->setMisrouted(true, VALIANT);
			flit->setCurrentMisrouteType(VALIANT);
		}

		/* If Valiant node is in current switch, flag flit as already Valiant misrouted */
		if (baseRouting::switchM->label == int(valNode / g_p_computing_nodes_per_router)) {
			flit->valNodeReached = true;
		}

		/* If Valiant node has not been reached, next port is determined after Valiant non-minimal path. */
		if (!flit->valNodeReached)
			selectedRoute.port = valOutP;
		else
			selectedRoute.port = minOutP;

		selectedRoute.neighPort = baseRouting::neighPort[selectedRoute.port];
		selectedRoute.vc = this->nextChannel(inPort, selectedRoute.port, flit);

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
	int nextChannel(int inP, int outP, flitModule * flit) {
		char outType, inType;
		int next_channel, inVC = flit->channel;

		inType = baseRouting::portType(inP);
		outType = baseRouting::portType(outP);

		if ((inType == 'p'))
			next_channel = 0;
		else if (inType == 'a' && outType == 'h'
				&& (inVC == 2 || (inVC == 1 && this->switchM->hPos == flit->sourceGroup)))
			next_channel = inVC - 1; // Local->global hop after reaching Val node
		else if (inType == 'h' && outType == 'a' && baseRouting::switchM->hPos == flit->destGroup)
			next_channel = inVC + 2; // Global->local hop in destination group
		else if (outType == 'p' || (inType == 'a' && outType == 'h'))
			next_channel = inVC;
		else
			next_channel = inVC + 1;
		assert(next_channel < g_channels); // Sanity check
		return (next_channel);
	}
};

#endif
