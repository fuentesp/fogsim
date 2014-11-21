/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2014 University of Cantabria

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

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <fstream>

#include "buffer.h"
#include "gModule.h"
#include "generatorModule.h"
#include "switchModule.h"
#include "flitModule.h"
#include "dgflySimulator.h"
#include "event.h"
#include "global.h"

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
	m_packets_sent_count = 0;
	m_packets_received_count = 0;
	m_flits_sent_count = 0;
	m_flits_received_count = 0;
	m_flitSeq = 0;
	m_injVC = 0;
	destLabel = -1;
	destSwitch = -1;
	m_dest_pointer = 0;
	m_flit_created = false;
	m_phase = 0;
	m_valiantLabel = 0;
	m_assignedRing = 0;
	m_dests_array = new int[g_generatorsCount - 1];
	int i, rand_num, var;
	for (i = 0; i < (g_generatorsCount - 1); i++) {
		if (i < this->sourceLabel) {
			m_dests_array[i] = i;
		} else {
			m_dests_array[i] = i + 1;
		}
	}
	//The array is permuted
	for (i = 0; i < (g_generatorsCount - 2); i++) {
		rand_num = (rand() % (g_generatorsCount - 1 - i)) + i;
		var = m_dests_array[rand_num];
		m_dests_array[rand_num] = m_dests_array[i];
		m_dests_array[i] = var;
	}
	/* TRACES */
	flit = NULL;
	saved_packet = NULL;
	pending_packet = 0;
	init_event(&events);
	init_occur(&occurs);
	m_packet_id = 0;
	m_packet_in_cycle = 0;
}

generatorModule::~generatorModule() {
}

void generatorModule::action() {
	int sourceA, sourceP, sourceH, destP, destA, destH, valP, valA, valH, rand_num;

	if (g_TRACE_SUPPORT != 0) {
		event e;
		if (!event_empty(&events) && head_event(&events).type == COMPUTATION) {
			e = head_event(&events);
			do_event(&events, &e);
			g_eventDeadlock = 0;
		}
		traceTrafficGeneration();
	} else {
		syntheticTrafficGeneration();
	}

	if (m_flit_created == true) {
		assert(flit->head == 1);
		do {
			m_valiantLabel = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_generatorsCount));
		} while ((m_valiantLabel == g_generatorsCount)
				|| (int(m_valiantLabel / (g_dP * g_dA)) == int(sourceLabel / (g_dP * g_dA))));

		flit->valId = m_valiantLabel;
		flit->valNodeReached = 0;

		sourceH = int(sourceLabel / (g_dA * g_dP));
		sourceP = module(sourceLabel, g_dP);
		sourceA = (sourceLabel - sourceP - sourceH * g_dP * g_dA) / g_dP;
		assert(sourceP < g_dP);
		assert(sourceA < g_dA);
		assert(sourceH < ((g_dA * g_dH) + 1));

		valH = int(m_valiantLabel / (g_dA * g_dP));
		valP = module(m_valiantLabel, g_dP);
		valA = (m_valiantLabel - valP - valH * g_dP * g_dA) / g_dP;
		assert(valP < g_dP);
		assert(valA < g_dA);
		assert(valH < ((g_dA * g_dH) + 1));

		destH = int(flit->destId / (g_dA * g_dP));
		destP = module(flit->destId, g_dP);
		destA = (flit->destId - destP - destH * g_dP * g_dA) / g_dP;
		assert(destP < g_dP);
		assert(destA < g_dA);
		assert(destH < ((g_dA * g_dH) + 1));

		if ((g_rings > 0) && (g_embeddedRing)) {
			rand_num = rand() % 2;
			if (g_onlyRing2) rand_num = 0; //If g_onlyRing2, I always assign packets to ring 2
			if ((g_rings > 1) && (rand_num == 0)) { //I assign Rings to packets randomly
				m_assignedRing = 2;
			} else {
				m_assignedRing = 1;
			}
			flit->assignedRing = m_assignedRing;
		}

		m_min_path = 0;
		m_val_path = 0;

		if (sourceH == destH) {
			if (sourceA != destA) {
				m_min_path++;
			}
		} else {
			if (sourceH > destH) {
				if (!((destH >= (sourceA * g_dH)) && (destH < (sourceA * g_dH + g_dH)))) {
					m_min_path++;
				}
				if (!((sourceH >= (destA * g_dH + 1)) && (sourceH < (destA * g_dH + g_dH + 1)))) {
					m_min_path++;
				}
			}
			if (sourceH < destH) {
				if (!((destH >= (sourceA * g_dH + 1)) && (destH < (sourceA * g_dH + g_dH + 1)))) {
					m_min_path++;
				}
				if (!((sourceH >= (destA * g_dH)) && (sourceH < (destA * g_dH + g_dH)))) {
					m_min_path++;
				}
			}
		}

		if (sourceH == valH) {
			if (sourceA != valA) {
				m_val_path++;
			}
		} else {
			if (sourceH > valH) {
				if (!((valH >= (sourceA * g_dH)) && (valH < (sourceA * g_dH + g_dH)))) {
					m_val_path++;
				}
			}
			if (sourceH < valH) {
				if (!((valH >= (sourceA * g_dH + 1)) && (valH < (sourceA * g_dH + g_dH + 1)))) {
					m_val_path++;
				}
			}
		}

		if (valH == destH) {
			if (valA != destA) {
				m_val_path++;
			}
		} else {
			if (valH > destH) {
				if (!((destH >= (valA * g_dH)) && (destH < (valA * g_dH + g_dH)))) {
					m_val_path++;
				}
				if (!((valH >= (destA * g_dH + 1)) && (valH < (destA * g_dH + g_dH + 1)))) {
					m_val_path++;
				}
			}
			if (valH < destH) {
				if (!((destH >= (valA * g_dH + 1)) && (destH < (valA * g_dH + g_dH + 1)))) {
					m_val_path++;
				}
				if (!((valH >= (destA * g_dH)) && (valH < (destA * g_dH + g_dH)))) {
					m_val_path++;
				}
			}
		}

		assert(m_min_path <= 3);
		assert(m_val_path <= 5);

		flit->minPath_length = m_min_path;
		flit->valPath_length = m_val_path;

		assert((flit->destId / g_generatorsCount) < 1);
		m_injVC = int(flit->destId * (g_channels_inj) / g_generatorsCount);

		if ((switchM->getCredits(this->pPos, m_injVC) > g_flitSize)) {
			switchM->injectFlit(this->pPos, m_injVC, flit);
			if (g_cycle >= g_warmCycles) switchM->packetsInj++;
			lastTimeSent = g_cycle;
			flit->inCycle = g_cycle;
			assert(flit->head == 1);
			g_packetCount++;
			flit->inCyclePacket = g_cycle;
			m_packet_in_cycle = flit->inCyclePacket;
			g_flitCount++;
			m_flitSeq++;
			if ((g_burst)) {
				burstFlitSent();
				if (flit->tail == true) burstPacketSent();
			}
			if ((g_AllToAll)) {
				AllToAllFlitSent();
				if (flit->tail == true) AllToAllPacketSent();
			}

		} else {
			if (g_TRACE_SUPPORT != 0) {
				saved_packet = flit;
				pending_packet = 1;
			} else {
				assert(false);
			}
		}

		while (m_flitSeq < (g_flits_per_packet)) {
			syntheticTrafficGeneration();
			flit->valId = m_valiantLabel;
			flit->valNodeReached = 0;
			if (g_rings > 0) flit->assignedRing = m_assignedRing;

			assert(m_min_path <= 3);
			assert(m_val_path <= 5);

			flit->minPath_length = m_min_path;
			flit->valPath_length = m_val_path;

			assert((flit->destId / g_generatorsCount) < 1);

			if ((switchM->getCredits(this->pPos, m_injVC) > g_flitSize)) {
				switchM->injectFlit(this->pPos, m_injVC, flit);
				lastTimeSent = g_cycle;
				flit->inCycle = g_cycle + m_flitSeq;
				flit->inCyclePacket = m_packet_in_cycle;
				g_flitCount++;
				m_flitSeq++;
				if ((g_burst)) {
					burstFlitSent();
					if (flit->tail == true) burstPacketSent();
				}
				if ((g_AllToAll)) {
					AllToAllFlitSent();
					if (flit->tail == true) AllToAllPacketSent();
				}
			} else {
				if (g_TRACE_SUPPORT != 0) {
					saved_packet = flit;
					pending_packet = 1;
				} else {
					assert(false);
				}
			}
		};
	}
}

void generatorModule::syntheticTrafficGeneration() {
	int p, a, next_group, groups, type, random;
	m_flit_created = false;
	if (m_flitSeq >= (g_packetSize / g_flitSize)) {
		assert(m_flitSeq == g_flits_per_packet);
		m_flitSeq = 0; /* Flit position within the packet. When it overflows
		 the number of flits per packet it is reset to 0 */
	}
	// BURST
	if (g_burst) {
		//if burst is over, return with NO ACTION
		if (isBurstSent()) {
			return;
		}
		// changes traffic type: suffles different traffic patterns
		random = rand() % 100 + 1;
		//randomly select type 0, 1 or 2
		if (random <= g_burst_traffic_type_percent[0]) {
			type = 0;
		} else if (random <= g_burst_traffic_type_percent[0] + g_burst_traffic_type_percent[1]) {
			type = 1;
		} else {
			type = 2;
		}
		//change global traffic parameters to the selected type
		g_traffic = g_burst_traffic_type[type];
		g_traffic_adv_dist = g_burst_traffic_adv_dist[type];
	}

	if (g_AllToAll) {
		g_traffic = 5;
		//if burst is over, return with NO ACTION
		if (isAllToAllSent()) {
			return;
		}
		if (isPhaseSent()) {
			if (isPhaseReceived()) {
				m_phase++;
				m_flits_received_count = 0;
				m_flits_sent_count = 0;
				if (g_naive_AllToAll) {
					destLabel = this->sourceLabel;
				} else {
					m_dest_pointer = 0;
				}
			} else {
				return;
			}
		}
	}

	if (g_mix_traffic) {
		random = rand() % 100 + 1;
		//randomly select type 0, 1 or 2
		if (random <= g_random_percent) {
			g_traffic = 1;
		} else if (random <= g_random_percent + g_adv_local_percent) {
			g_traffic = 4;
		} else {
			g_traffic = 3;
		}
	}

	assert((g_traffic == 1) || (g_traffic == 2) || (g_traffic == 3) || (g_traffic == 4) || (g_traffic == 5));

	if ((g_transient_traffic) && (g_cycle == g_maxCycles + g_transient_traffic_cycle)) {
		g_traffic = g_transient_traffic_next_type;
		g_traffic_adv_dist = g_transient_traffic_next_dist;
	}

	groups = g_dH * g_dA + 1;
	next_group = module((this->hPos + g_traffic_adv_dist), groups);

	if ((m_flitSeq > 0) || (g_cycle == 0) || ((g_cycle >= (lastTimeSent + interArrivalTime)))) {
		if ((m_flitSeq > 0)
				|| ((switchM->getCredits(this->pPos, m_injVC) > g_packetSize)
						&& (rand() / ((double) RAND_MAX + 1)) < ((double) g_probability / 100) / g_packetSize)) {
			if (m_flitSeq == 0) { //Header flit
				m_packet_id = g_packetCount;
				destSwitch = 0;
				do {
					if (g_traffic == 5) { //all-to-all traffic
						if (g_naive_AllToAll) {
							destLabel = module((destLabel + 1), g_generatorsCount); //next Switch
							assert(destLabel < g_generatorsCount);
							assert(destLabel != this->sourceLabel);
						} else {
							assert(g_random_AllToAll);
							assert(m_dest_pointer < (g_generatorsCount - 1));
							destLabel = m_dests_array[m_dest_pointer]; //random Switch
							assert(destLabel < g_generatorsCount);
							assert(destLabel != this->sourceLabel);
							m_dest_pointer++;
						}
						destSwitch = int(destLabel / g_dP);
					} else {
						if (g_traffic == 1) { // random uniform traffic
							do {
								destSwitch = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_switchesCount)); //random destination
							} while ((destSwitch == g_switchesCount));
						}
						if (g_traffic == 2) { //Marina's adversarial traffic: same switch, but a given number of groups further away
							destSwitch = next_group * g_dA + (this->aPos);
						}
						if (g_traffic == 3) { //Cristobal's adversarial traffic: random destination within specified group
							do {
								a = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_dA));
							} while (a >= g_dA);
							destSwitch = next_group * g_dA + a;
						}
						if (g_traffic == 4) { //local adversarial traffic
							a = module((this->aPos + g_traffic_adv_dist_local), g_dA); //switch times switches further away
							destSwitch = this->hPos * g_dA + a;
						}
						do {
							p = rand() / (int) (((unsigned) RAND_MAX + 1) / (g_dP)); //random destination
						} while (p >= g_dP);

						destLabel = destSwitch * g_dP + p;
					}
				} while (destLabel == sourceLabel);

				if (destSwitch >= g_switchesCount) {
					cout << "destSwitch=" << destSwitch << endl;
				}
				assert(destSwitch < g_switchesCount);
				if (destLabel >= g_generatorsCount) {
					cout << "destGenerator=" << destLabel << endl;
				}
				assert(destLabel < g_generatorsCount);
				if (destLabel == g_generatorsCount) {
					cout << "generatorsCount=" << g_generatorsCount << endl;
					cout << "Message" << g_flitCount << "  destLabel=" << destLabel << endl;
				}
				/* Insert in the module that corresponds a pending packet */
			}
			flit = new flitModule(m_packet_id, g_flitCount, m_flitSeq, sourceLabel, destLabel, destSwitch, 0, 0, 0);
			if (m_flitSeq == (g_flits_per_packet - 1)) //Tail flit
				flit->tail = 1;
			if (m_flitSeq == 0) //Head flit
				flit->head = 1;
			assert(m_flitSeq < g_flits_per_packet);
			m_flit_created = true;
		}
	}
}

void generatorModule::traceTrafficGeneration() {
	long d, dstSw;
	if (pending_packet > 0) {
		if (flit->flitId == 57938) {
			cout << "Pending packet" << endl;
		}
		flit = saved_packet;
		saved_packet = NULL;
		pending_packet = 0;
		g_packet_created = true;
	} else {
		if (!event_empty(&events)) {
			event e;
			while (!event_empty(&events) && head_event(&events).type == RECEPTION) {
				e = head_event(&events);
				if (occurred(&occurs, e)) {
					rem_head_event(&events);
				} else
					break;
			}
			if (!event_empty(&events) && head_event(&events).type == SENDING) {
				g_eventDeadlock = 0;
				do_event(&events, &e);
				d = e.pid;
				dstSw = floor(d / g_dP);
				flit = new flitModule(g_packetCount, g_flitCount, m_flitSeq, sourceLabel, d, dstSw, 0, 1, 1);
				flit->task = e.task;
				flit->length = e.length;
				flit->mpitype = e.mpitype;

				if (flit->destSwitch >= g_switchesCount) {
					cout << "destSwitch=" << flit->destSwitch << endl;
				}
				assert(flit->destSwitch < g_switchesCount);
				if (flit->destId >= g_generatorsCount) {
					cout << "destGenerator=" << flit->destId << endl;
				}
				assert(flit->destId < g_generatorsCount);
				g_packet_created = true;
				if (d == sourceLabel) {
					cout << "self sent packet" << endl;
					event re = e;
					re.type = RECEPTION;
					ins_occur(&occurs, re);
					return; //do not create a true packet
				}
			} else {
				return;
			}
		} else {
			return;
		}
	}
}

bool generatorModule::isBurstSent() {
	assert(g_burst);
	assert(m_flits_sent_count <= g_burst_flits_length);
	return m_flits_sent_count == g_burst_flits_length;
}

bool generatorModule::isAllToAllSent() {
	assert(g_AllToAll);
	assert(m_flits_sent_count <= (g_generatorsCount - 1) * g_phases);
	return m_flits_sent_count == (g_generatorsCount - 1) * g_phases;
}

bool generatorModule::isAllToAllReceived() {
	assert(g_AllToAll);
	assert(m_flits_received_count <= (g_generatorsCount - 1) * g_phases);
	return m_flits_received_count == (g_generatorsCount - 1) * g_phases;
}

bool generatorModule::isPhaseSent() {
	assert(g_AllToAll);
	assert(m_flits_sent_count <= (g_generatorsCount - 1) * g_phases);
	assert(m_flits_sent_count <= (g_generatorsCount - 1) * m_phase);
	return m_flits_sent_count == (g_generatorsCount - 1) * m_phase;
}

bool generatorModule::isPhaseReceived() {
	assert(g_AllToAll);
	assert(m_flits_received_count <= (g_generatorsCount - 1) * g_phases);
	assert(m_flits_received_count <= (g_generatorsCount - 1) * m_phase);
	return m_flits_received_count == (g_generatorsCount - 1) * m_phase;
}

void generatorModule::AllToAllFlitSent() {
	assert(g_AllToAll);
	assert(m_flits_sent_count <= (g_generatorsCount - 1) * g_phases * g_flits_per_packet);
	m_flits_sent_count++;
}
void generatorModule::AllToAllPacketSent() {
	assert(g_AllToAll);
	assert(m_packets_sent_count <= (g_generatorsCount - 1) * g_phases);
	m_packets_sent_count++;
}

void generatorModule::AllToAllFlitReceived() {
	assert(g_AllToAll);
	assert(m_flits_received_count <= (g_generatorsCount - 1) * g_phases * g_flits_per_packet);
	m_flits_received_count++;
}

void generatorModule::AllToAllPacketReceived() {
	assert(g_AllToAll);
	assert(m_packets_received_count <= (g_generatorsCount - 1) * g_phases);
	m_packets_received_count++;
}

bool generatorModule::isBurstReceived() {
	assert(g_burst);
	assert(m_flits_received_count <= g_burst_flits_length);
	return m_flits_received_count == g_burst_flits_length;
}

void generatorModule::burstFlitSent() {
	assert(g_burst);
	assert(m_flits_sent_count <= g_burst_flits_length);
	m_flits_sent_count++;
}
void generatorModule::burstPacketSent() {
	assert(g_burst);
	assert(m_packets_sent_count <= g_burst_length);
	m_packets_sent_count++;
}

void generatorModule::burstFlitReceived() {
	assert(g_burst);
	assert(m_flits_received_count <= g_burst_flits_length);
	m_flits_received_count++;
}

void generatorModule::burstPacketReceived() {
	assert(g_burst);
	assert(m_packets_received_count <= g_burst_length);
	m_packets_received_count++;
}

