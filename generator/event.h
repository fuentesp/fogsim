/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2015 University of Cantabria

 FSIN Functional Simulator of Interconnection Networks
 Copyright (2003-2011) J. Miguel-Alonso, J. Navaridas

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

#ifndef _event
#define _event

#include <iostream>
#include "../global.h"
#include "dimemas.h"

/** 
 * Types of event.
 *
 * It should be 'r' for a reception, 's' for a sent or 'c' for a computation event.
 */
typedef enum event_t {
	RECEPTION = 'r',	///< Reception
	SENDING = 's',		///< Sent
	COMPUTATION = 'c'	///< Computation
} event_t;

/**
 * Structure that defines an event.
 *
 * It contains all the needed atributes for distinguish it:
 * type (S/R/C), the second node id (destination/source), a task id,
 * the length in packets, and the packets sent/arrived.
 */
typedef struct event {
	event_t type;	///< Type of the event (Reception / Sent / Computation).
	long pid;		///< The other node (processor id).
	long task;		///< An id for distinguish messages.
	long length;	///< Length of the message in packets. Number of cycles in computation.
	long count;		///< The number of packets sent/arrived. Number of elapsed cycles when running.
	enum coll_ev_t mpitype;	///< MPI Collective Event Type
} event;

/**
 * Structure that defines a node for event queue & lists.
 * @see event_q
 * @see event_l
 */
typedef struct event_n {
	event ev;				///< The event in this position.
	struct event_n * next;	///< The next node in the list/queue.
} event_n;

/**
 * Structure that defines an event queue.
 */
typedef struct event_q {
	event_n *head;	///< A pointer to the first event node (for removing).
	event_n *tail;	///< A pointer to the last event node (for enqueuing).
} event_q;

/**
 * Structure that defines a list of events.
 */
typedef struct event_l {
	event_n * first;	///< A pointer to the first element of the list.
} event_l;

void init_event(event_q *q);

/**
 * Empties an event queue
 *
 * @param q a pointer to the queue to be initialized.
 */
void clear_event(generatorModule **generators_array, int trace_id, vector<int> instances, int task_id);

/**
 * Adds an event to a queue.
 *
 * @param q a pointer to a queue.
 * @param i the event to be added to q.
 */
void ins_event(generatorModule **generators_array, int trace_id, vector<int> instances, int task_id, event i);

/**
 * Uses the first event in the queue.
 *
 * Takes an event an increases its packet count.
 * When it reaches the length of the event, this is erased from the queue.
 *
 * @param q A pointer to a queue.
 * @param i A pointer to the event to do.
 */
void do_event(event_q *q, event *i);

/**
 * Uses the first event in the queue.
 *
 * Takes an event an increases its packet count counter times.
 * When it reaches the length of the event, this is erased from the queue.
 *
 * @param q A pointer to a queue.
 * @param i A pointer to the event to do.
 */
void do_multiple_events(event_q *q, event *i, long counter);

/**
 * Looks at the first event in a queue.
 *
 * @param q A pointer to the queue.
 * @return The first event in the queue (without using nor modifying it).
 */
event head_event(event_q *q);

/**
 * Deletes the first event in a queue.
 *
 * @param q A pointer to the queue.
 */
void rem_head_event(event_q *q);

/**
 * Is a queue empty?.
 *
 * @param q A pointer to the queue.
 * @return TRUE if the queue is empty FALSE in other case.
 */
bool event_empty(event_q *q);

void init_occur(event_l *l);

void ins_occur(event_l *l, event i);

bool occurred(event_l *l, event i);

#endif /* _event */

