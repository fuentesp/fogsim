/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
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

#ifndef CA_HANDLER_H
#define	CA_HANDLER_H

#include "global.h"
#include <queue>

class flitModule;

class caHandler {
public:
	caHandler(switchModule * sw);
	virtual ~caHandler();
	void update();
	void flitSent(flitModule * flit, int input, int length);
	int getContention(int output) const;
	bool isThereContention(int output);
	bool isThereGlobalContention(flitModule * flit);
	void increaseContention(int port);
	void decreaseContention(int port);
	void updateContention(int output);
	void readIncomingCAFlits();
private:
	/* Counts number of PHITS that would
	 * be minimally routed through each port
	 */
	int * m_contention_counter;
	/* Stores accumulated contention value
	 * (in PHITS) upon past cycles
	 */
	float * m_accumulated_contention;
	/* Series of partial counters */
	int ** m_partial_counter;
	/* Counts number of PHITS that would be
	 * minimally routed through each port,
	 * but that can be misrouted
	 */
	int * m_potential_contention_counter;
	/* For each output port, there is a
	 * queue that stores when a message
	 * that would have been minimally
	 * routed through that port, has
	 * completely left the buffer
	 * (g_packet_size cycles after
	 * starting to send)
	 */
	queue<float> * m_sending_end_cycle_queue;

	queue<float> * m_sending_end_cycle_globalport_queue;

	switchModule * m_sw;

	void update_sending_end_cycle_queues();

};

#endif	/* CA_HANDLER_H */

