/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2014-2021 University of Cantabria

 FSIN Functional Simulator of Interconnection Networks
 Copyright (2003-2011) J. Miguel-Alonso, A. Gonzalez, J. Navaridas

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

#include "event.h"
#include "generatorModule.h"

/**
 * Initializes an event queue.
 * 
 * @param q a pointer to the queue to be initialized.
 */
void init_event(event_q *q) {
	q->head = NULL;
	q->tail = NULL;
}

/**
 * Empties an event queue
 *
 * @param q a pointer to the queue to be initialized.
 */
void clear_event(generatorModule **generators_array, int trace_id, vector<int> instances, int task_id) {
	int iter, source_id;
	for (iter = 0; iter < int(instances.size()); iter++) {
		source_id = g_trace_2_gen_map[trace_id][task_id][instances[iter]];
		g_generators_list[source_id]->resetEventQueue();
	}
}

/**
 * Adds an event to a queue.
 * 
 * @param q a pointer to a queue.
 * @param i the event to be added to q.
 */
void ins_event(generatorModule **generators_array, int trace_id, vector<int> instances, int task_id, event i) {
	int iter, source_id, dest_id;
	for (iter = 0; iter < int(instances.size()); iter++) {
		source_id = g_trace_2_gen_map[trace_id][task_id][instances[iter]];
		dest_id = g_trace_2_gen_map[trace_id][i.pid][instances[iter]];
		g_generators_list[source_id]->insertEvent(i, dest_id);
	}
}

/**
 * Uses the first event in the queue.
 *
 * Takes an event an increases its packet count.
 * When it reaches the length of the event, this is erased from the queue.
 * 
 * @param q A pointer to a queue.
 * @param i A pointer to the event to do.
 */
void do_event(event_q *q, event *i) {
	event_n *e;
	if (q->head == NULL) cout << "Using event from an empty queue" << endl;
	e = q->head;
	e->ev.count++;
	*i = e->ev;
	if (i->count == i->length) {
		q->head = q->head->next;
		delete e;
		if (q->head == NULL) q->tail = NULL;
	}
}

void do_multiple_events(event_q *q, event *i, long counter) {
	event_n *e;
	if (q->head == NULL) cout << "Using event from an empty queue" << endl;
	e = q->head;
	e->ev.count += counter;
	assert(e->ev.count <= e->ev.length);
	*i = e->ev;
	if (i->count == i->length) {
		q->head = q->head->next;
		delete e;
		if (q->head == NULL) q->tail = NULL;
	}
}

/**
 * Looks at the first event in a queue.
 * 
 * @param q A pointer to the queue.
 * @return The first event in the queue (without using nor modifying it).
 */
event head_event(event_q *q) {
	if (q->head == NULL) cout << "Getting event from an empty queue" << endl;
	assert(q->head != NULL);
	return q->head->ev;
}

/**
 * Deletes the first event in a queue.
 * 
 * @param q A pointer to the queue.
 */
void rem_head_event(event_q *q) {
	if (q->head == NULL) cout << "Deleting event from an empty queue" << endl;
	assert(q->head != NULL);
	q->head = q->head->next;
	if (q->head == NULL) q->tail = NULL;
}

/**
 * Is a queue empty?.
 * 
 * @param q A pointer to the queue.
 * @return TRUE if the queue is empty FALSE in other case.
 */
bool event_empty(event_q *q) {
	return (q->head == NULL);
}

/** 
 * Initializes all ocurred events lists.
 *
 * There is only one list in each router.
 * 
 * @param l A pointer to the list to be initialized.
 */
void init_occur(event_l *l) {
	(*l).first = NULL;
}

/**
 * Inserts an event's occurrence in an event list.
 *
 * If the event is in the list, then its count is increased. Otherwise a new event is created
 * in the occurred event list.
 * 
 * @param l A pointer to a list.
 * @param i The event to be added.
 */
void ins_occur(event_l *l, event i) {
	event_n *e = (*l).first;
	event_n *aux = NULL;

	if (e == NULL) { // List is Empty
		// Create a new occurred event
		aux = new event_n;
		i.count = 1;
		aux->ev = i;
		aux->next = (*l).first;
		(*l).first = aux;
		return;
	}

	if (e->ev.type == i.type && e->ev.pid == i.pid && e->ev.task == i.task && e->ev.length == i.length
			&& e->ev.count < e->ev.length) {
		// The occurence is the first element.
		e->ev.count++;
		return;
	}

	while (e->next != NULL) {
		if (e->next->ev.type == i.type && e->next->ev.pid == i.pid && e->next->ev.task == i.task
				&& e->next->ev.length == i.length && e->next->ev.count < e->next->ev.length) {
			// Another element in the list
			e->next->ev.count++;
			return;
		}
		e = e->next;
	}

	// There is not in the list, so we create a new occurred event
	aux = new event_n;
	i.count = 1;
	aux->ev = i;
	aux->next = (*l).first;
	(*l).first = aux;
}

/**
 * Has an event completely occurred?.
 *
 * If it has totally occurred, this is, the event is in the list and its count is equal to
 * its length, then it is deleted from the list.
 * 
 * @param l a pointer to a list.
 * @param i the event we are seeking for.
 * @return TRUE if the event has been occurred, elseway FALSE
 */
bool occurred(event_l *l, event i) {
	event_n *e = (*l).first;
	event_n *aux;

	if (e == NULL) // List is Empty
		return false;
	if (e->ev.type == i.type && e->ev.count == e->ev.length && e->ev.task == i.task && e->ev.length == i.length
			&& e->ev.mpitype == i.mpitype && (i.mpitype > 0 || (e->ev.pid == i.pid))) {
		aux = e->next;
		delete e;
		(*l).first = aux;
		return true;
	}

	while (e->next != NULL) {
		if (e->next->ev.type == i.type && e->next->ev.count == e->next->ev.length && e->next->ev.task == i.task
				&& e->next->ev.length == i.length && e->next->ev.mpitype == i.mpitype
				&& (i.mpitype > 0 || (e->next->ev.pid == i.pid))) {
			aux = e->next->next;
			e->next = aux;
			return true;
		}
		e = e->next;
	}
	return false;
}
