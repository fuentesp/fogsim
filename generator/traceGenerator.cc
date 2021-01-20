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

#include "traceGenerator.h"
#include <math.h>

traceGenerator::traceGenerator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
		switchModule *switchM) :
		generatorModule(interArrivalTime, name, sourceLabel, pPos, aPos, hPos, switchM) {
	saved_packet = NULL;
	pending_packet = 0;
	init_event(&events);
	init_occur(&occurs);
}

traceGenerator::~traceGenerator() {
	delete saved_packet;
}

void traceGenerator::action() {
	int sourceA, sourceP, sourceH, destP, destA, destH, valP, valA, valH, rand_num, instance;

	event e;
	if (!event_empty(&events) && head_event(&events).type == COMPUTATION) {
		do_event(&events, &e);
		TraceNodeId trace_node = g_gen_2_trace_map[sourceLabel];
		for (instance = 0; instance < g_trace_instances[trace_node.trace_id]; instance++) {
			if (g_trace_2_gen_map[trace_node.trace_id][trace_node.trace_node][instance] == sourceLabel) break;
		}
		assert(instance < g_trace_instances[trace_node.trace_id]);
		g_event_deadlock[trace_node.trace_id][instance] = 0;
	}

	flit = this->generateFlit();

	if (flit != NULL) {
		assert(flit->head == 1);
		assert((flit->destId / g_number_generators) < 1);

		/* Select a Valiant destination (not in source group) */
		switchM->routing->setValNode(flit);
		flit->valNodeReached = 0;

		if (g_deadlock_avoidance == EMBEDDED_RING) {
			/* Ring-to-packet assignment. If 2 rings in use and
			 * not forcing to use only ring 2, choose randomly.	*/
			rand_num = rand() % 2;
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

		m_injVC = this->getInjectionVC(flit->destId, RESPONSE);
		/* TODO: Currently CoS level feature is not exploited */
		if ((switchM->switchModule::getCredits(this->pPos, 0, m_injVC) >= g_flit_size)) {
			switchM->injectFlit(this->pPos, m_injVC, flit);
			if (g_cycle >= g_warmup_cycles) switchM->packetsInj++;
			lastTimeSent = g_cycle;
			flit->inCycle = g_cycle;
			assert(flit->head == 1);
			g_tx_packet_counter++;
			flit->inCyclePacket = g_cycle;
			m_packet_in_cycle = flit->inCyclePacket;
		} else {
			/* If flit can not be immediately injected, store for later */
			saved_packet = flit;
			pending_packet = 1;
		}
	}
}

/* Creates a new flit upon trace behavior. If a previous flit is pending
 * to be sent, attempt its injection; otherwise, follow trace behavior:
 *  - If a reception event has occurred, determine if corresponding
 *  	flit has been already received. All occurred reception events
 *  	can be cleared at once.
 *  - If a sending event, generate a new flit.
 */
flitModule* traceGenerator::generateFlit() {
	flitModule* genFlit = NULL;
	long destId, destSw;
	event e;

	if (/*!drop_packets && */pending_packet > 0) {
		genFlit = saved_packet;
		saved_packet = NULL;
		pending_packet = 0;
	} else {
		if (!event_empty(&events)) {
			while (!event_empty(&events) && head_event(&events).type == RECEPTION) {
				/* RECEPTION event */
				e = head_event(&events);
				if (occurred(&occurs, e))
					rem_head_event(&events);
				else
					break;
			}
			if (!event_empty(&events) && head_event(&events).type == SENDING) {
				/* SENDING event */
				int instance;
				TraceNodeId trace_node = g_gen_2_trace_map[sourceLabel];
				for (instance = 0; instance < g_trace_instances[trace_node.trace_id]; instance++) {
					if (g_trace_2_gen_map[trace_node.trace_id][trace_node.trace_node][instance] == sourceLabel) break;
				}
				assert(instance < g_trace_instances[trace_node.trace_id]);
				assert(g_trace_2_gen_map[trace_node.trace_id][trace_node.trace_node][instance] == sourceLabel);
				g_event_deadlock[trace_node.trace_id][instance] = 0;
				do_event(&events, &e);
				destId = e.pid;
				if (destId >= g_number_generators) cerr << "destGenerator=" << destId << endl;
				assert(destId < g_number_generators);
				if (destId == sourceLabel) { /* If trace flit is self-sent, do not create flit and confirm its reception */
					cout << "Self-sent flit" << endl;
					event re = e;
					re.type = RECEPTION;
					ins_occur(&occurs, re);
					return NULL;
				}

				destSw = floor(destId / g_p_computing_nodes_per_router);
				if (destSwitch >= g_number_switches) cerr << "destSwitch=" << destSwitch << endl;
				assert(destSwitch < g_number_switches);

				genFlit = new flitModule(g_tx_packet_counter, g_tx_flit_counter, m_flitSeq, sourceLabel, destId, destSw,
						0, 1, 1);
				flit->task = e.task;
				flit->length = e.length;
				flit->mpitype = e.mpitype;
			}
		}
	}
	return genFlit;
}

bool traceGenerator::isGenerationEnded() {
	return event_empty(&this->events);
}

void traceGenerator::printHeadEvent() {
	event e;
	if (event_empty(&this->events)) {
		cout << "[" << sourceLabel << "] empty" << endl;
	} else {
		e = head_event(&this->events);
		cout << "[" << sourceLabel << "] " << (char) e.type << " (" << e.pid << ") flag=" << e.task << " length="
				<< e.length << " count=" << e.count << " mpitype=" << e.mpitype << endl;
	}
}

/* Returns the number of cycles that can be skipped in
 * current generator list of events. Only computation
 * events can be skipped; reception events or empty
 * queues do not alter number of cycles that can be
 * skipped, and return a '-1' flag.
 */
int traceGenerator::numSkipCycles() {
	event e;
	int numCycles = -1;

	if (event_empty(&this->events)) return -1;

	e = head_event(&this->events);
	switch (e.type) {
		case RECEPTION:
			return -1;
		case COMPUTATION:
			return (e.length - e.count);
		default:
			return 0;
	}
}

void traceGenerator::consumeCycles(int numCycles) {
	if (!event_empty(&this->events) && head_event(&this->events).type != RECEPTION) {
		event e = head_event(&this->events);
		assert(e.type == COMPUTATION);
		do_multiple_events(&this->events, &e, numCycles);
	}
}

/* Removes all elements of the events queue.
 */
void traceGenerator::resetEventQueue() {
	while (this->events.head)
		rem_head_event(&this->events);
}

/* Inserts an event into the events queue.
 */
void traceGenerator::insertEvent(event i, int destId) {
	event_n *e;
	e = new event_n;
	e->ev = i;
	e->ev.pid = destId;	// Update destination PID to reflect trace distribution
	e->next = NULL;
	if (this->events.head == NULL) { // Empty Queue
		this->events.head = e;
		this->events.tail = e;
	} else {
		this->events.tail->next = e;
		this->events.tail = e;
	}
}

void traceGenerator::insertOccurredEvent(flitModule *flit) {
	event e;
	e.type = RECEPTION;
	e.pid = flit->sourceId;
	e.task = flit->task;
	e.length = flit->length;
	e.mpitype = (enum coll_ev_t) flit->mpitype;
	ins_occur(&this->occurs, e);
}
