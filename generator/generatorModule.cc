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
	m_flit_created = false;
	m_valiantLabel = 0;
	m_assignedRing = 0;

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
}

generatorModule::~generatorModule() {
	delete pattern;
}

void generatorModule::action() {

	this->generateFlit();

	if (m_flit_created == true) {
		assert(flit->head == 1);
		assert((flit->destId / g_number_generators) < 1);

		/* Select a Valiant destination (not in source group) */
		do {
			m_valiantLabel = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_number_generators));
		} while ((m_valiantLabel == g_number_generators)
				|| (int(m_valiantLabel / (g_p_computing_nodes_per_router * g_a_routers_per_group))
						== int(sourceLabel / (g_p_computing_nodes_per_router * g_a_routers_per_group))));
		flit->valId = m_valiantLabel;
		flit->valNodeReached = 0;

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
		this->determinePaths(sourceLabel, flit->destId, m_valiantLabel);

		m_injVC = int(flit->destId * (g_injection_channels) / g_number_generators);

		if ((switchM->switchModule::getCredits(this->pPos, m_injVC) >= g_flit_size)) {
			switchM->injectFlit(this->pPos, m_injVC, flit);
			if (g_cycle >= g_warmup_cycles) switchM->packetsInj++;
			lastTimeSent = g_cycle;
			flit->inCycle = g_cycle;
			assert(flit->head == 1);
			g_tx_packet_counter++;
			flit->inCyclePacket = g_cycle;
			m_packet_in_cycle = flit->inCyclePacket;
			g_tx_flit_counter++;
			m_flitSeq++;
			pattern->flitTx();
		} else {
			cerr << "ERROR: injection has been triggered although # of credits is insufficient." << endl;
			assert(false);
		}

		while (m_flitSeq < (g_flits_per_packet)) {
			this->generateFlit();
			assert((flit->destId / g_number_generators) < 1);

			flit->valId = m_valiantLabel;
			flit->valNodeReached = 0;
			if (g_rings > 0) flit->assignedRing = m_assignedRing;
			flit->minPathLength = m_min_path;
			flit->valPathLength = m_val_path;

			if ((switchM->switchModule::getCredits(this->pPos, m_injVC) >= g_flit_size)) {
				switchM->injectFlit(this->pPos, m_injVC, flit);
				lastTimeSent = g_cycle;
				flit->inCycle = g_cycle + m_flitSeq;
				flit->inCyclePacket = m_packet_in_cycle;
				g_tx_flit_counter++;
				m_flitSeq++;
				pattern->flitTx();
			} else {
				cerr << "ERROR: injection has been triggered although # of credits is insufficient." << endl;
				assert(false);
			}
		}
	}
}

/* If triggered by injection probability and the injection buffer
 * has enough credits, selects destination upon traffic pattern
 * and generates a new flit.
 */
void generatorModule::generateFlit() {
	m_flit_created = false;

	/* Determine flit position within packet. When it overflows
	 the number of flits per packet, it is reset to 0. */
	if (m_flitSeq >= (g_packet_size / g_flit_size)) {
		assert(m_flitSeq == g_flits_per_packet);
		m_flitSeq = 0;
	}

	/* Generate packet */
	if ((m_flitSeq > 0) || (g_cycle == 0) || ((g_cycle >= (lastTimeSent + interArrivalTime)))) {
		if ((m_flitSeq > 0)
				|| ((switchM->switchModule::getCredits(this->pPos, m_injVC) >= g_packet_size)
						&& (rand() / ((double) RAND_MAX + 1)) < ((double) g_injection_probability / 100) / g_packet_size)) {
			if (m_flitSeq == 0) {		// Flit is header of packet
				m_packet_id = g_tx_packet_counter;
				destLabel = pattern->setDestination(g_traffic);
				if (destLabel == -1) return;
				destSwitch = int(destLabel / g_p_computing_nodes_per_router);
			}
			flit = new flitModule(m_packet_id, g_tx_flit_counter, m_flitSeq, sourceLabel, destLabel, destSwitch, 0, 0,
					0);
			if (m_flitSeq == (g_flits_per_packet - 1)) flit->tail = 1;
			if (m_flitSeq == 0) flit->head = 1;
			assert(m_flitSeq < g_flits_per_packet);
			m_flit_created = true;
		}
	}
}

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
void generatorModule::determinePaths(int source, int dest, int val) {
	int sourceP, sourceA, sourceH, destP, destA, destH, valP, valA, valH;

	getNodeCoords(sourceLabel, sourceP, sourceA, sourceH);
	getNodeCoords(m_valiantLabel, valP, valA, valH);
	getNodeCoords(flit->destId, destP, destA, destH);

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

	flit->minPathLength = m_min_path;
	flit->valPathLength = m_val_path;
}
