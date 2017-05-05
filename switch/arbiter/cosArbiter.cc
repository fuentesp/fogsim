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

#include "cosArbiter.h"
#include "../switchModule.h"
#include "../../flit/flitModule.h"

using namespace std;

cosArbiter::cosArbiter(int portNumber, unsigned short cos, switchModule *switchM, ArbiterType policy) {
	this->switchM = switchM;
	this->label = cos;
	this->input_port = portNumber;
	switch (policy) {
		case LRS:
			this->arbProtocol = new lrsArbiter(IN, input_port, label, g_channels, switchM);
			break;
		case PrioLRS:
			cerr << "Warning: right now, there is no use in Priority LRS policy for cos arbiters" << endl;
			this->arbProtocol = new priorityLrsArbiter(IN, input_port, label, g_channels, 0, switchM);
			break;
		case RR:
			this->arbProtocol = new rrArbiter(IN, input_port, label, g_channels, switchM);
			break;
		case PrioRR:
			cerr << "Warning: right now, there is no use in Priority RoundRobin policy for input arbiters" << endl;
			this->arbProtocol = new priorityRrArbiter(IN, input_port, label, g_channels, 0, switchM);
			break;
		case AGE:
			this->arbProtocol = new ageArbiter(IN, input_port, label, g_channels, switchM);
			break;
		case PrioAGE:
			cerr << "Warning: right now, there is no use in Priority Age Arbiter policy for cos arbiters" << endl;
			this->arbProtocol = new priorityAgeArbiter(IN, input_port, label, g_channels, 0, switchM);
			break;
		default:
			cerr << "ERROR: undefined output arbiter policy." << endl;
			exit(-1);
	}
}

cosArbiter::~cosArbiter() {
	delete arbProtocol;
}

bool cosArbiter::attendPetition(int input_channel) {
	int outP, outVC, nextP;
	unsigned short cos = this->label;
	bool result;
	flitModule *flit = NULL;
	if (!portCanSendFlit(input_port, cos, input_channel) || switchM->reservedInPort[cos][input_channel]) return false;

	flit = switchM->getFlit(input_port, cos, input_channel);

	/* Reset misrouting petition tracking variables */
	if (flit->head == true) {
		/* First flit of a packet */
		candidate selectedPath;
		selectedPath = switchM->routing->enroute(flit, input_port, input_channel);
		outP = selectedPath.port;
		outVC = selectedPath.vc;
		nextP = selectedPath.neighPort;
	} else {
		/* Body flit of a packet: forward it to the assigned output
		 * port (remember routing is done in a per-packet basis) */
		outP = switchM->getCurrentOutPort(input_port, cos, input_channel);
		outVC = switchM->getCurrentOutVC(input_port, cos, input_channel);
		nextP = switchM->routing->neighPort[outP];
	}
	assert(outP >= 0 && outP < g_ports);

	/* Determine if petition can be released or not */
	if (petitionCondition(input_port, input_channel, outP, nextP, outVC, flit)) {
		/* Make petition to global arbiter */
		if (outP < g_p_computing_nodes_per_router) nextP = outVC = 0;
		flit->nextP = outP;
		flit->nextVC = outVC;
		result = true;
	} else {
		/* Petition can NOT be made */
		switchM->outputArbiters[outP]->petitions[input_port] = 0;
		result = false;
	}
	return result;
}

/*
 * Determines if a localArbiter can make petition for a given flit and route.
 */
bool cosArbiter::petitionCondition(int input_port, int input_channel, int outP, int nextP, int nextC,
		flitModule *flitEx) {
	assert(outP >= 0 && outP < g_ports);
	unsigned short cos = this->label;
	bool result;
	/* Sanity check: petition can only be made if input port is ready to send flit */
	assert(portCanSendFlit(input_port, cos, input_channel));

	/* Check if petition requires a consumption port (linked to a computing node) or a transit one */
	if (outP < g_p_computing_nodes_per_router) {
		/* Flit attempts consumption */
		result = switchM->checkConsumePort(outP, flitEx);
	} else {
		/* Flit attempts transit */
		bool can_receive_flit, output_emb_net, nextVC_unLocked;
		int bubble = 0, crd = switchM->getCredits(outP, cos, nextC);

		/* Check if neighbor port is ready to rx flit */
		can_receive_flit = switchM->nextPortCanReceiveFlit(outP);

		/* Determine if output belongs to an embedded escape subnetwork */
		output_emb_net = (g_local_link_channels <= nextC)
				&& (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == EMBEDDED_TREE)
				&& nextC < (g_local_link_channels + g_channels_escape);

		/* Check if VC is locked, either in buffer or in pkt.  */
		nextVC_unLocked = switchM->isVCUnlocked(flitEx, outP, nextC);

		/* Bubble value assignance.
		 * - By default, it will be zero.
		 * - If deadlock avoidance mechanism is a ring (either physical or embedded) and flit comes
		 * 		from outside the escape subnetwork, it will take 'g_ring_injection_bubble' value.
		 * - If there is a escape subnetwork (ring or tree) and base congestion control is active,
		 * 		it will take 'g_baseCongestionControl_bub' value.
		 */
		if (g_deadlock_avoidance == RING && input_port < (switchM->routing->portCount - 2)) bubble =
				g_ring_injection_bubble;
		if (g_deadlock_avoidance == EMBEDDED_RING && input_channel < g_local_link_channels) bubble =
				g_ring_injection_bubble;
		if ((input_port < g_p_computing_nodes_per_router) && (g_congestion_management == BCM)
				&& (g_deadlock_avoidance == RING || g_deadlock_avoidance == EMBEDDED_RING
						|| g_deadlock_avoidance == EMBEDDED_TREE)) bubble = g_baseCongestionControl_bub;

		if (flitEx->mandatoryGlobalMisrouting_flag && outP < g_global_router_links_offset && not output_emb_net) {
			/* If global mandatory flag is active and request is not for subescape network, no petition can be made. */
			assert(switchM->hPos != flitEx->destGroup);
			assert(switchM->hPos == flitEx->sourceGroup);
			result = false;
		} else if ((crd >= ((1 + bubble) * g_flit_size)) && can_receive_flit && nextVC_unLocked) {
			/* Petition can be made */
			result = true;
		} else {
			/* Petition can NOT be made */
			result = false;
		}
#if DEBUG
		if (not result) {
			cout << "TRANSIT flit: cycle " << g_cycle << "--> SW " << label << " inPort " << input_port
			<< " VC " << input_channel << " flit " << flitEx->flitId << " dest " << flitEx->destId
			<< " destSW " << flitEx->destSwitch << " outPort " << outP << " VC " << nextC
			<< " MIN outPort " << switchM->routing->minOutputPort(flitEx->destId) << " : can NOT make petition";
			if (not (crd >= ((1 + bubble) * g_flit_size))) cout << " (out of credits)";
			if (not (can_receive_flit == 1)) cout << " (busy link)";
			cout << "." << endl;
		}
#endif
	}
	return result;
}

/*
 * Checks whether port can send or not. It considers if
 * port is currently tx (or has speed-up) and that there's
 * a flit to be tx.
 */
bool cosArbiter::portCanSendFlit(int port, unsigned short cos, int vc) {
	return (switchM->inPorts[port]->canSendFlit(cos, vc) && !switchM->inPorts[port]->emptyBuffer(cos, vc));
}

/*
 * Arbitration function: iterates through all arbiter inputs
 * and returns attended port (if any, -1 otherwise).
 */
int cosArbiter::action() {
	int input_port, port, attendedPort = -1;
	bool port_served;
	for (port = 0; port < arbProtocol->ports; port++) {
		input_port = arbProtocol->getServingPort(port);
		port_served = this->attendPetition(input_port);
		if (port_served) {
			attendedPort = input_port;
			port = arbProtocol->ports;
			/* Reorder port list */
			arbProtocol->markServedPort(input_port);
		}
	}
	return attendedPort;
}
