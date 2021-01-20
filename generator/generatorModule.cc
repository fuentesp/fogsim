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

#include "generatorModule.h"
#include <math.h>
#include <string.h>

using namespace std;

generatorModule::generatorModule(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
		switchModule *switchM) {
	this->interArrivalTime = interArrivalTime;
	this->name = name;
	lastTimeSent = 0;
	this->switchM = switchM;
	this->sourceLabel = sourceLabel;
	this->pPos = pPos;
	this->aPos = aPos;
	this->hPos = hPos;
	m_flitSeq = 0;
	m_injVC = 0;
	destLabel = -1;
	destSwitch = -1;
	m_valiantLabel = 0;
	m_assignedRing = 0;
	injection_probability = g_injection_probability;
	sum_injection_probability = 0;
	pendingPetitions = 0;

	switch (g_traffic) {
		case SINGLE_BURST:
			this->pattern = new burstTraffic(this->sourceLabel, this->pPos, this->aPos, this->hPos);
			break;
		case ALL2ALL:
			this->pattern = new all2allTraffic(this->sourceLabel, this->pPos, this->aPos, this->hPos);
			break;
		case MIX:
			this->pattern = new mixTraffic(this->sourceLabel, this->pPos, this->aPos, this->hPos);
			break;
		case TRANSIENT:
			this->pattern = new transientTraffic(this->sourceLabel, this->pPos, this->aPos, this->hPos);
			break;
		default:
			this->pattern = new steadyTraffic(this->sourceLabel, this->pPos, this->aPos, this->hPos);
	}

	flit = NULL;
	m_packet_id = 0;
	m_packet_in_cycle = 0;
	int i;
	if (g_reactive_traffic) {
		for (i = 0; i < floor(float(g_injection_channels) / 2); i++) {
			injPetVcs.push_back(i);
		}
		for (i = floor(float(g_injection_channels) / 2); i < g_injection_channels; i++) {
			injResVcs.push_back(i);
		}
	} else {
		for (i = 0; i < g_injection_channels; i++) {
			injResVcs.push_back(i);
		}
	}
        /* Create the destinations for random permutation before any injection */
        if (g_traffic == RANDOMPERMUTATION || g_traffic == RANDOMPERMUTATION_RCTV)
            this->pattern->setDestination(g_traffic);
}

generatorModule::~generatorModule() {
	delete pattern;
}

/*
 * Triggered by action for inject flits.
 */
void generatorModule::inject() {
	/* TODO: Currently CoS level feature is not exploited */
	if ((switchM->switchModule::getCredits(this->pPos, 0, flit->channel) < g_flit_size)) {
		cerr << "ERROR: injection has been triggered although # of credits is insufficient." << endl;
		assert(false);
	}

	switchM->injectFlit(this->pPos, flit->channel, flit);
	if (g_cycle >= g_warmup_cycles) switchM->packetsInj++;
	lastTimeSent = g_cycle;
	flit->inCycle = g_cycle;
	assert(flit->head == 1);
	g_tx_packet_counter++;
	flit->inCyclePacket = g_cycle;
	m_packet_in_cycle = flit->inCyclePacket;
	m_flitSeq++;
	pattern->flitTx();
	pendingPetitions++;
	/* Wormhole routing: inject the rest of flits of packet */
	while (m_flitSeq < g_flits_per_packet) {
		m_valiantLabel = flit->valId;
		if (g_reactive_traffic)
			flit = this->generateFlit(PETITION);
		else
			flit = this->generateFlit(RESPONSE);
		assert((flit->destId / g_number_generators) < 1);

		flit->valId = m_valiantLabel;
		flit->valNodeReached = 0;
		if (g_rings > 0) flit->assignedRing = m_assignedRing;
		flit->minPathLength = m_min_path;
		flit->valPathLength = m_val_path;
		/* TODO: Currently CoS level feature is not exploited */
		if ((switchM->switchModule::getCredits(this->pPos, 0, flit->channel) < g_flit_size)) {
			cerr << "ERROR: injection has been triggered although # of credits is insufficient." << endl;
			assert(false);
		}

		switchM->injectFlit(this->pPos, flit->channel, flit);
		lastTimeSent = g_cycle;
		flit->inCycle = g_cycle + m_flitSeq;
		flit->inCyclePacket = m_packet_in_cycle;
		m_flitSeq++;
		pattern->flitTx();
	}
}

void generatorModule::action() {

	if (g_reactive_traffic)
		flit = this->generateFlit(PETITION);
	else
		flit = this->generateFlit(RESPONSE);

	if (flit != NULL) {
		assert(flit->head == 1);
		assert((flit->destId / g_number_generators) < 1);
		switchM->routing->setValNode(flit);
		if (g_deadlock_avoidance == EMBEDDED_RING) {
			/* Ring-to-packet assignment. If 2 rings in use and
			 * not forcing to use only ring 2, choose randomly.	*/
			int rand_num = rand() % 2;
			if (g_onlyRing2) rand_num = 0;
			if ((g_rings > 1) && (rand_num == 0)) {
				m_assignedRing = 2;
			} else {
				m_assignedRing = 1;
			}
			flit->assignedRing = m_assignedRing;
		}
		/* Set minimal and valiant paths length */
		this->determinePaths(flit);
		this->inject();
	}
	assert((ULONG_MAX - sum_injection_probability) >= injection_probability);
	sum_injection_probability += injection_probability;
}

/* If triggered by injection probability and the injection buffer
 * has enough credits, selects destination upon traffic pattern
 * and generates a new flit.
 */
flitModule* generatorModule::generateFlit(FlitType flitType, int destId) {
	flitModule * genFlit = NULL;

	/* Determine flit position within packet. When it overflows
	 the number of flits per packet, it is reset to 0. */
	if (m_flitSeq >= (g_packet_size / g_flit_size)) {
		assert(m_flitSeq == g_flits_per_packet);
		m_flitSeq = 0;
	}

	/* Generate packet */
	vector<int> vct;

	if (destId >= 0) { /* TODO: Currently CoS level feature is not exploited */
		assert(
				(g_cycle >= (lastTimeSent + interArrivalTime))
						&& switchM->switchModule::getPortCredits(this->pPos, 0, vct) >= g_packet_size);
		destLabel = destId;
		destSwitch = int(destLabel / g_p_computing_nodes_per_router);
		genFlit = new flitModule(m_packet_id, g_tx_flit_counter, 0, sourceLabel, destLabel, destSwitch, 0, true, true,
				0);
		genFlit->channel = this->getInjectionVC(destLabel, flitType);
		genFlit->flitType = flitType;
	} else {
		if ((m_flitSeq > 0) || (g_cycle == 0) || ((g_cycle >= (lastTimeSent + interArrivalTime)))) {
			if ((m_flitSeq > 0)
					|| (switchM->switchModule::getPortCredits(this->pPos, 0, vct) >= g_packet_size
							&& rand() / ((double) RAND_MAX + 1)
									< ((double) injection_probability / 100.0) / (1.0 * g_packet_size)
							&& (flitType == RESPONSE || g_max_petitions_on_flight < 0
									|| pendingPetitions < g_max_petitions_on_flight))) {
				if (m_flitSeq == 0) {		// Flit is header of packet
					m_packet_id = g_tx_packet_counter;
					destLabel = pattern->setDestination(g_traffic);
					if (destLabel >= 0) {
						m_injVC = this->getInjectionVC(destLabel, flitType);
					}
				} /* TODO: Currently CoS level feature is not exploited */
				if (destLabel >= 0 && m_injVC >= 0
						&& switchM->switchModule::getCredits(this->pPos, 0, m_injVC) >= g_packet_size) {
					destSwitch = int(destLabel / g_p_computing_nodes_per_router);
					genFlit = new flitModule(m_packet_id, g_tx_flit_counter, m_flitSeq, sourceLabel, destLabel,
							destSwitch, 0, false, false, 0);
					genFlit->channel = m_injVC;
					genFlit->flitType = flitType;
					if (m_flitSeq == (g_flits_per_packet - 1)) genFlit->tail = 1;
					if (m_flitSeq == 0) genFlit->head = 1;
					assert(m_flitSeq < g_flits_per_packet);
				}
			}
		}
	}

	return genFlit;
}

/* TODO: change this function to static and inline? */
void generatorModule::getNodeCoords(int nodeId, int &nodeP, int &nodeA, int &nodeH) {
	nodeH = int(nodeId / (g_a_routers_per_group * g_p_computing_nodes_per_router));
	nodeP = module(nodeId, g_p_computing_nodes_per_router);
	nodeA = (nodeId - nodeP - nodeH * g_p_computing_nodes_per_router * g_a_routers_per_group)
			/ g_p_computing_nodes_per_router;
	assert(nodeP < g_p_computing_nodes_per_router);
	assert(nodeA < g_a_routers_per_group);
	assert(nodeH < ((g_a_routers_per_group * g_h_global_ports_per_router) + 1));

}

/* Determines the length of minimal and Valiant paths,
 * and sets it in corresponding flit fields.
 */
void generatorModule::determinePaths(flitModule *injFlit) {
	int sourceP, sourceA, sourceH, destP, destA, destH, valP, valA, valH;

	assert(injFlit != NULL);
	assert(injFlit->sourceId == sourceLabel);

	getNodeCoords(sourceLabel, sourceP, sourceA, sourceH);
	getNodeCoords(injFlit->valId, valP, valA, valH);
	getNodeCoords(injFlit->destId, destP, destA, destH);

	m_min_path = 0;
	m_val_path = 0;

	/* Minimal path */
	if (sourceH == destH) {
		if (sourceA != destA) m_min_path++; /* 1 local hop path */
	} else {
		m_min_path++; /* If source and destination are in different groups, 1 global hop is required */
		if (sourceH > destH) {
			if (!((destH >= (sourceA * g_h_global_ports_per_router))
					&& (destH < (sourceA * g_h_global_ports_per_router + g_h_global_ports_per_router)))) {
				m_min_path++; /* Local hop in source group */
			}
			if (!((sourceH >= (destA * g_h_global_ports_per_router + 1))
					&& (sourceH < (destA * g_h_global_ports_per_router + g_h_global_ports_per_router + 1)))) {
				m_min_path++; /* Local hop in destination group */
			}
		}
		if (sourceH < destH) {
			if (!((destH >= (sourceA * g_h_global_ports_per_router + 1))
					&& (destH < (sourceA * g_h_global_ports_per_router + g_h_global_ports_per_router + 1)))) {
				m_min_path++; /* Local hop in source group */
			}
			if (!((sourceH >= (destA * g_h_global_ports_per_router))
					&& (sourceH < (destA * g_h_global_ports_per_router + g_h_global_ports_per_router)))) {
				m_min_path++; /* Local hop in destination group */
			}
		}
	}

	/* Valiant path */
	if (sourceH == valH) {
		if (sourceA != valA) m_val_path++; /* If Valiant dest is in source group, misrouting path is up to 1 local hop */
	} else {
		m_val_path++; /* If source and Valiant node are in different groups, 1 global hop is required */
		if (sourceH > valH) {
			if (!((valH >= (sourceA * g_h_global_ports_per_router))
					&& (valH < (sourceA * g_h_global_ports_per_router + g_h_global_ports_per_router)))) {
				m_val_path++; /* Local hop in source group */
			}
		}
		if (sourceH < valH) {
			if (!((valH >= (sourceA * g_h_global_ports_per_router + 1))
					&& (valH < (sourceA * g_h_global_ports_per_router + g_h_global_ports_per_router + 1)))) {
				m_val_path++; /* Local hop in source group */
			}
		}
	}
	if (valH == destH) {
		if (valA != destA) m_val_path++; /* If Valiant dest is in dest group, Valiant > dest path is up to 1 local hop */
	} else {
		m_val_path++; /* If destination and Valiant node are in different groups, 1 global hop is required */
		if (valH > destH) {
			if (!((destH >= (valA * g_h_global_ports_per_router))
					&& (destH < (valA * g_h_global_ports_per_router + g_h_global_ports_per_router)))) {
				m_val_path++; /* Local hop in intermediate group */
			}
			if (!((valH >= (destA * g_h_global_ports_per_router + 1))
					&& (valH < (destA * g_h_global_ports_per_router + g_h_global_ports_per_router + 1)))) {
				m_val_path++; /* Local hop in destination group */
			}
		}
		if (valH < destH) {
			if (!((destH >= (valA * g_h_global_ports_per_router + 1))
					&& (destH < (valA * g_h_global_ports_per_router + g_h_global_ports_per_router + 1)))) {
				m_val_path++; /* Local hop in intermediate group */
			}
			if (!((valH >= (destA * g_h_global_ports_per_router))
					&& (valH < (destA * g_h_global_ports_per_router + g_h_global_ports_per_router)))) {
				m_val_path++; /* Local hop in destination group */
			}
		}
	}
	assert(m_min_path <= 3);
	assert(m_val_path <= 5);

	injFlit->minPathLength = m_min_path;
	injFlit->valPathLength = m_val_path;
}

int generatorModule::getInjectionVC(int dest, FlitType flitType) {
	int vc, num_vcs = 0, aux;
	vector<int> aux_vec, aux_vec2;

	assert(dest >= 0 && dest < g_number_generators);

	if (flitType == RESPONSE || flitType == CNM) {
		aux_vec = injResVcs;
	} else
		aux_vec = injPetVcs;

	switch (g_vc_injection) {
		case RAND: /* TODO: Currently CoS level feature is not exploited */
			for (std::vector<int>::iterator it = aux_vec.begin(); it != aux_vec.end(); ++it) {
				if (switchM->switchModule::getCredits(this->pPos, 0, *it) >= g_packet_size) {
					aux_vec2.push_back(*it);
				}
			}
			if (aux_vec2.size() == 0) return -1;
			aux = (int) (aux_vec2.size() * rand() / ((unsigned) RAND_MAX + 1));
			assert (aux >= 0 && aux < aux_vec2.size());
			vc = aux_vec2[aux];
			break;
		case DEST:
			aux = int(dest * (aux_vec.size()) / g_number_generators);
			assert(aux >= 0 && aux < aux_vec.size());
			vc = aux_vec[aux];
			break;
		default:
			cerr << "ERROR: undefined VC injection policy" << endl;
			assert(0);
	}
	assert(vc >= 0 && vc < g_injection_channels);
	return vc;
}

vector<int> generatorModule::getArrayInjVC(int dest, bool response) {
	int aux;
	vector<int> aux_vec;

	if (response) {
		aux_vec = injResVcs;
	} else {
		aux_vec = injPetVcs;
	}

	if (g_vc_injection == DEST) {
		aux = int(dest * (aux_vec.size()) / g_number_generators);
		assert(aux >= 0 && aux < aux_vec.size());
		aux = aux_vec[aux];
		aux_vec.clear();
		aux_vec.push_back(aux);
	}

	return aux_vec;
}

/*
 * Change injection probability
 */
void generatorModule::setInjectionProbability(float newInjectionProbability) {
	this->injection_probability = newInjectionProbability;
}

/*
 * Decrease pending petition counter (when one or several responses
 * have been received).
 */
void generatorModule::decreasePendingPetitions(int numPetitions) {
	this->pendingPetitions -= numPetitions;
	assert(this->pendingPetitions >= 0);
}

/*
 * Check if consume port is available
 */
bool generatorModule::checkConsume(flitModule *flit) {
	bool availResponseSpace = true;
	if (flit != NULL) {
		assert(flit->destId == this->sourceLabel);
		if (flit->flitType == PETITION) {
			vector<int> vcArray = g_generators_list[flit->destId]->getArrayInjVC(flit->sourceId, true);
			availResponseSpace = g_flit_size <= this->switchM->getPortCredits(pPos, flit->cos, vcArray);
		}
	}
	return (availResponseSpace && g_internal_cycle >= (this->lastConsumeCycle + g_flit_size));
}

/*
 * Consume flit
 */
void generatorModule::consumeFlit(flitModule *flit, int input_port, int input_channel) {
#if DEBUG
	cout << "Tx pkt " << flit->flitId << " from source " << flit->sourceId << " (sw " << flit->sourceSW
	<< ", group " << flit->sourceGroup << ") to dest " << flit->destId << " (sw " << flit->destSwitch
	<< ", group " << flit->destGroup << "). Consuming pkt in node " << this->destLabel << endl;
#endif
	if (flit->flitType == CNM) {
		if (g_congestion_management != QCNSW)
			assert(false); /* CNM message type not exists without QCNSW congestion management mechanisms */
	}
	trackConsumptionStatistics(flit, input_port, input_channel, pPos);
	delete flit;
}

/*
 * Track statistics
 */
void generatorModule::trackConsumptionStatistics(flitModule *flit, int inP, int inC, int outP) {
	long double flitLatency = 0, packetLatency = 0;
	int k;
	float in_cycle;
	generatorModule * src_generator;
#if DEBUG
	if (flit->destSwitch != this->switchM->label) {
		cout << "ERROR in flit " << flit->flitId << " at sw " << this->switchM->label << ", rx from input " << inP
		<< " to out " << outP << "; sourceSw " << flit->sourceSW << ", destSw " << flit->destSwitch << endl;
	}
#endif
	assert(flit->destSwitch == this->switchM->label);
	assert(flit->destId == this->sourceLabel);

	/* Check if flit is a petition (reactive traffic) and, if so,
	 * trigger the response generation. */
	if (flit->flitType == PETITION) { //Generate flit
		flitModule * responseFlit = this->generateFlit(RESPONSE, flit->sourceId);
		this->switchM->routing->setValNode(responseFlit);
		this->determinePaths(responseFlit);
		responseFlit->petitionSrcId = flit->flitId;
		responseFlit->inCycle = g_internal_cycle;
		responseFlit->inCyclePacket = g_internal_cycle;
		responseFlit->petitionLatency = g_internal_cycle - flit->inCycle + g_flit_size;
		assert(this->switchM->switchModule::getCredits(outP, flit->cos, responseFlit->channel) >= g_flit_size);
		this->switchM->injectFlit(outP, responseFlit->channel, responseFlit);
	} else if (g_reactive_traffic) g_generators_list[flit->destId]->decreasePendingPetitions();

	if (inP < g_p_computing_nodes_per_router || (g_congestion_management == QCNSW && inP == g_qcn_port)) {
		assert(flit->injLatency < 0);
		flit->injLatency = g_internal_cycle - flit->inCycle;
		assert(flit->injLatency >= 0);
	}

	if (g_misrouting_trigger == CA || g_misrouting_trigger == HYBRID || g_misrouting_trigger == FILTERED
			|| g_misrouting_trigger == DUAL || g_misrouting_trigger == CA_REMOTE
			|| g_misrouting_trigger == HYBRID_REMOTE || g_misrouting_trigger == WEIGHTED_CA) {
		assert(flit->localContentionCount >= 0);
		assert(flit->globalContentionCount >= 0);
		assert(flit->localEscapeContentionCount >= 0);
		assert(flit->globalEscapeContentionCount >= 0);
	}

	if (g_internal_cycle >= g_warmup_cycles) {
		g_base_latency += flit->getBaseLatency();
	}

	this->lastConsumeCycle = g_internal_cycle;
	flitLatency = g_internal_cycle - flit->inCycle + g_flit_size;

	if (flit->tail == 1) {
		packetLatency = g_internal_cycle - flit->inCyclePacket + g_flit_size;
		assert(packetLatency >= 1);
	}

	long double lat = flit->injLatency
			+ (flit->localHopCount + flit->localEscapeHopCount) * g_local_link_transmission_delay
			+ (flit->globalHopCount + flit->globalEscapeHopCount) * g_global_link_transmission_delay
			+ flit->localContentionCount + flit->globalContentionCount + flit->localEscapeContentionCount
			+ flit->globalEscapeContentionCount;
	assert(flitLatency == lat + g_flit_size);

	if (flit->flitType == CNM) {
		g_rx_cnmFlit_counter++;
		return; /* The rest of statistics don't have interest for QCN messages */
	}

	if (flit->tail == 1) g_rx_packet_counter += 1;
	g_rx_flit_counter += 1;
	g_total_hop_counter += flit->hopCount;
	g_local_hop_counter += flit->localHopCount;
	g_global_hop_counter += flit->globalHopCount;
	g_local_ring_hop_counter += flit->localEscapeHopCount;
	g_global_ring_hop_counter += flit->globalEscapeHopCount;
	g_local_tree_hop_counter += flit->localEscapeHopCount;
	g_global_tree_hop_counter += flit->globalEscapeHopCount;
	g_subnetwork_injections_counter += flit->subnetworkInjectionsCount;
	g_root_subnetwork_injections_counter += flit->rootSubnetworkInjectionsCount;
	g_source_subnetwork_injections_counter += flit->sourceSubnetworkInjectionsCount;
	g_dest_subnetwork_injections_counter += flit->destSubnetworkInjectionsCount;
	g_local_contention_counter += flit->localContentionCount;
	g_global_contention_counter += flit->globalContentionCount;
	g_local_escape_contention_counter += flit->localEscapeContentionCount;
	g_global_escape_contention_counter += flit->globalEscapeContentionCount;
        g_rx_acorState_counter[flit->acorFlitStatus]++;

	/* Latency HISTOGRAM */
	if (g_internal_cycle >= g_warmup_cycles) {
		if (g_reactive_traffic && flit->flitType == RESPONSE) {
			g_response_latency += flitLatency + flit->petitionLatency;
			assert(g_response_latency >= 0);
			g_response_counter++;
		}
		if (flit->getMisrouted()) g_nonminimal_counter++;
		if (flit->head == 1) {
			/* If new latency value exceeds current max histogram value,
			 * enlarge vectors to fit new values. */
			if (flitLatency >= g_latency_histogram_maxLat) {
				g_latency_histogram_no_global_misroute.insert(g_latency_histogram_no_global_misroute.end(),
						flitLatency + 1 - g_latency_histogram_maxLat, 0);
				g_latency_histogram_global_misroute_at_injection.insert(
						g_latency_histogram_global_misroute_at_injection.end(),
						flitLatency + 1 - g_latency_histogram_maxLat, 0);
				g_latency_histogram_other_global_misroute.insert(g_latency_histogram_other_global_misroute.end(),
						flitLatency + 1 - g_latency_histogram_maxLat, 0);
				g_latency_histogram_maxLat = flitLatency + 1;
				assert(g_latency_histogram_no_global_misroute.size() == g_latency_histogram_maxLat);
				assert(g_latency_histogram_global_misroute_at_injection.size() == g_latency_histogram_maxLat);
				assert(g_latency_histogram_other_global_misroute.size() == g_latency_histogram_maxLat);
			}

			if (not (g_routing == PAR || g_routing == RLM || g_routing == OLM)) {
				/* If NOT using vc misrouting, store all latency values in a single histogram. */
				g_latency_histogram_other_global_misroute[int(flitLatency)]++;
			} /* Otherwise, split into three: */
			else if (flit->globalHopCount <= 1) {
				//1- NO GLOBAL MISROUTE
				assert(flit->hopCount <= 5);
				assert(flit->localHopCount <= 4);
				assert(flit->globalHopCount <= 1);
				assert(flit->getMisrouteCount(GLOBAL) == 0);
				assert(flit->getMisrouteCount(GLOBAL_MANDATORY) == 0);
				g_latency_histogram_no_global_misroute[int(flitLatency)]++;
			} else if (flit->isGlobalMisrouteAtInjection()) {
				//2- GLOBAL MISROUTING AT INJECTION
				assert(flit->getMisrouteCount(NONE) >= 1 && flit->getMisrouteCount(NONE) <= 3);
				assert(flit->getMisrouteCount(GLOBAL) == 1);
				assert(flit->hopCount <= 6);
				assert(flit->localHopCount <= 4);
				assert(flit->globalHopCount == 2);
				assert(flit->getMisrouteCount(GLOBAL_MANDATORY) == 0);
				g_latency_histogram_global_misroute_at_injection[int(flitLatency)]++;
			} else {
				//3- OTHER GLOBAL MISROUTE (after local hop in source group)
				assert(flit->getMisrouteCount(NONE) >= 1 && flit->getMisrouteCount(NONE) <= 4);
				assert(flit->getMisrouteCount(GLOBAL) == 1);
				assert(flit->hopCount <= 8);
				assert(flit->localHopCount <= 6);
				assert(flit->globalHopCount == 2);
				g_latency_histogram_other_global_misroute[int(flitLatency)]++;
			}
		}
	}

	if ((g_internal_cycle >= g_warmup_cycles) && (flit->hopCount < g_hops_histogram_maxHops) && (flit->head == 1)) {
		g_hops_histogram[flit->hopCount]++;
	}

	/* Group 0, per switch averaged latency */
	if ((g_internal_cycle >= g_warmup_cycles) && (flit->sourceGroup == 0)) {
		assert(flit->sourceSW < g_a_routers_per_group);
		int nodeP;
		int nodeA;
		int nodeH;
		g_generators_list[flit->sourceId]->getNodeCoords(flit->sourceId, nodeP, nodeA, nodeH);
		if (flit->getMisrouted())
			g_group0_numFlits[nodeA][nodeP][1]++;
		else
			g_group0_numFlits[nodeA][nodeP][0]++;
		g_group0_totalLatency[flit->sourceSW] += lat;
	}
	/* Group ROOT, per switch averaged latency */
	if (/*(g_trees>0)&&*/(g_internal_cycle >= g_warmup_cycles) && (flit->sourceGroup == g_tree_root_switch)) {
		assert(
				(flit->sourceSW >= g_a_routers_per_group * g_tree_root_switch)
						&& (flit->sourceSW < g_a_routers_per_group * (g_tree_root_switch + 1)));
		g_groupRoot_numFlits[flit->sourceSW - g_a_routers_per_group * g_tree_root_switch]++;
		g_groupRoot_totalLatency[flit->sourceSW - g_a_routers_per_group * g_tree_root_switch] += lat;
	}

	g_flit_latency += flitLatency;
	assert(g_flit_latency >= 0);

	if (flit->tail == 1) g_packet_latency += packetLatency;

	assert(flit->injLatency >= 0);
	g_injection_queue_latency += flit->injLatency;
	assert(g_injection_queue_latency >= 0);

	//Transient traffic recording
	if (g_transient_stats) {
		in_cycle = flit->inCycle;

		//cycle within recording range
		if ((in_cycle >= g_warmup_cycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles)
				&& (in_cycle < g_warmup_cycles + g_transient_traffic_cycle + g_transient_record_num_cycles)) {

			//calculate record index
			k = int(in_cycle - (g_warmup_cycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles));
			assert(k < g_transient_record_len);

			//record Transient traffic data
			g_transient_record_flits[k] += 1;
			g_transient_record_latency[k] += g_internal_cycle - flit->inCycle;
			g_transient_record_injection_latency[k] += flit->injLatency;
			if (flit->getMisrouteCount(GLOBAL) > 0 || flit->getCurrentMisrouteType() == VALIANT) {
				g_transient_record_misrouted_flits[k] += 1;
			}

			//Repeat transient record but considering injection to network time
			k = int(
					in_cycle + flit->injLatency
							- (g_warmup_cycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles));

			if (k < g_transient_record_len) {
				g_transient_net_injection_flits[k]++;
				g_transient_net_injection_latency[k] += g_internal_cycle - flit->inCycle;
				g_transient_net_injection_inj_latency[k] += flit->injLatency;
				if (flit->getMisrouteCount(GLOBAL) > 0 || flit->getCurrentMisrouteType() == VALIANT) {
					g_transient_net_injection_misrouted_flits[k] += 1;
				}
			}
		}
	}

	switch (g_traffic) {
		case SINGLE_BURST:
			/* BURST/ALL-TO-ALL traffic:
			 * -Locate source generator.
			 * -Count flit as received
			 * -If all generator messages have been
			 * received, count generator as finished
			 */
			src_generator = g_generators_list[flit->sourceId];
			src_generator->pattern->flitRx();
			if (src_generator->pattern->isGenerationFinished()) {
				g_burst_generators_finished_count++;
			}
			break;
		case ALL2ALL:
			src_generator = g_generators_list[flit->sourceId];
			src_generator->pattern->flitRx();
			if (src_generator->pattern->isGenerationFinished()) {
				g_AllToAll_generators_finished_count++;
			}
			break;
		case TRACE:
			/* TRACE support: add event to an occurred event's list */
			g_generators_list[flit->destId]->insertOccurredEvent(flit);
			break;
	}

	this->switchM->increasePortCount(outP);
}
