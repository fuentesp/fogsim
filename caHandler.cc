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

#include "caHandler.h"
#include "switch/switchModule.h"
#include "flit/flitModule.h"

caHandler::caHandler(switchModule * sw) {
	int i, port, group;

	m_sw = sw;
	m_contention_counter = new int[g_ports];
	m_potential_contention_counter = new int[g_ports];
	m_accumulated_contention = new float[g_ports];
	m_partial_counter = new int*[g_a_routers_per_group];
	for (i = 0; i < g_a_routers_per_group; i++) {
		m_partial_counter[i] = new int[g_a_routers_per_group * g_h_global_ports_per_router + 1];
		for (group = 0; group < g_a_routers_per_group * g_h_global_ports_per_router + 1; group++) {
			m_partial_counter[i][group] = 0;
		}
	}
	m_sending_end_cycle_queue = new queue<float> [g_ports];
	m_sending_end_cycle_globalport_queue = new queue<float> [g_a_routers_per_group * g_h_global_ports_per_router + 1];

	for (port = 0; port < g_ports; port++) {
		m_contention_counter[port] = 0;
		m_potential_contention_counter[port] = 0;
		m_accumulated_contention[port] = 0.0;
	}
}

caHandler::~caHandler() {
	delete[] m_contention_counter;
	delete[] m_potential_contention_counter;
	delete[] m_accumulated_contention;
	for (int sw = 0; sw < g_a_routers_per_group; sw++) {
		delete[] m_partial_counter[sw];
	}
	delete[] m_partial_counter;
	delete[] m_sending_end_cycle_queue;
	delete[] m_sending_end_cycle_globalport_queue;
}

/* 
 * Reads all buffer heads and the sending queue, 
 * and updates the contention counters.
 */
void caHandler::update() {

	int port, chan, group;
	flitModule * flit = NULL;
	caFlit* ca_flit;

	assert(g_contention_aware);

	/* Reset partial contention counters */
	for (port = 0; port < g_ports; port++) {
		updateContention(port);
		for (group = 0; group < g_a_routers_per_group * g_h_global_ports_per_router + 1; group++) {
			m_partial_counter[m_sw->aPos][group] = 0;
		}
	}

	//check buffer heads
	for (port = 0; port < g_ports; port++) {
		for (chan = 0; chan < g_channels; chan++) {

			/* If buffer is not 'sending' and has
			 * a flit to be sent, count it */
			if (m_sw->inPorts[port]->canSendFlit(chan)) {

				// look-up flit in the buffer head
				m_sw->inPorts[port]->checkFlit(chan, flit);

				// increment minimal path output port counter
				if (flit != NULL) {
					if (g_misrouting_trigger == DUAL) {
						// TODO: HACK! PENDING TO BE DONE DUE TO SOME PROBLEMS WITHIN MISROUTETYPE FUNCTION
						/* CHECK THIS CAREFULLY, AS 'misrouteType' HAS BEEN MOVED
						 * TO A VIRTUAL baseRouting FUNCTION. IT MAY HAPPEN RESULT
						 * IS NOT 'NONE' EVEN IF CONGESTED RESTRICTION FORBIDS IT!!
						 */
//						MisrouteType mis = m_sw->routing->misrouteType(port, flit, m_sw->routing->min_outport(flit), chan);
//						if (mis == NONE || mis == GLOBAL_MANDATORY)
//							m_contention_counter[m_sw->routing->min_outport(flit)] += g_flit_size;
//						else if (mis != GLOBAL_MANDATORY) {
//							m_potential_contention_counter[m_sw->routing->min_outport(flit)] += g_flit_size;
//						}
					} else
						m_contention_counter[m_sw->routing->minOutputPort(flit->destId)] += g_flit_size;
					if (port < g_p_computing_nodes_per_router || port >= g_global_router_links_offset) {
						/* If input port corresponds to an injection port or a global link, also account it for partial counter */
						if (flit->destGroup != m_sw->hPos) {
							m_partial_counter[m_sw->aPos][flit->destGroup] += g_flit_size;
							assert(
									m_partial_counter[m_sw->aPos][flit->destGroup]
											< (g_flit_size
													* (g_p_computing_nodes_per_router * g_injection_channels
															+ g_h_global_ports_per_router * g_global_link_channels)));
						}
					}
				}
			}

		}
	}

	/* Take packets that are currently being sent into
	 * account. If they're being sent through a global
	 * link, account them for partial counter. */
	update_sending_end_cycle_queues();
	for (port = 0; port < g_ports; port++) {
		m_contention_counter[port] += g_flit_size * (int) m_sending_end_cycle_queue[port].size();

#if DEBUG
		if ((int) m_sending_end_cycle_queue[port].size() > 0) {
			cout << "cycle " << g_cycle << " CA update: SW " << m_sw->label << " already transmitting "
			<< (int) m_sending_end_cycle_queue[port].size() << " (min would have been outPort " << port << " )"
			<< endl;
		}
#endif
	}

	for (group = 0; group < g_a_routers_per_group * g_h_global_ports_per_router + 1; group++) {
		if (group == m_sw->hPos) continue;
		m_partial_counter[m_sw->aPos][group] += g_flit_size * (int) m_sending_end_cycle_globalport_queue[group].size();
		assert(
				m_partial_counter[m_sw->aPos][group]
						< (g_flit_size
								* (g_p_computing_nodes_per_router * g_injection_channels
										+ g_h_global_ports_per_router * g_global_link_channels)));
	}

#if DEBUG
	cout << "cycle " << g_cycle << " CA update SUMMARY: SW " << m_sw->label;
	for (port = 0; port < g_ports; port++) {
		cout << " P" << port << "=" << m_contention_counter[port] << "-->" << isThereContention(port);
	}
	cout << endl;
#endif

	/* Share self partial counter with all other routers in same group every 100 cycles */
	if (g_cycle % 100 == 0) {
		for (port = g_local_router_links_offset; port < g_global_router_links_offset; port++) {
			ca_flit = new caFlit(m_sw->aPos, m_partial_counter[m_sw->aPos], g_cycle + g_local_link_transmission_delay);
			m_sw->routing->neighList[port]->receiveCaFlit(*ca_flit);
			delete ca_flit;
		}
	}

	/* Read all other router CA flits (and update correspondent partial counters) */
	this->readIncomingCAFlits();

}

/* Reads incoming CA flits from all other routers in group
 * (and update correspondent partial counters).
 */
void caHandler::readIncomingCAFlits() {
	while ((!m_sw->incomingCa.empty()) && (m_sw->incomingCa.front().getArrivalCycle() <= g_internal_cycle)) {
		for (int group = 0; group < g_a_routers_per_group * g_h_global_ports_per_router + 1; group++) {
			m_partial_counter[m_sw->incomingCa.front().m_aPos][group] = m_sw->incomingCa.front().partial_counter[group];
		}
		m_sw->incomingCa.pop();
	}
}

/* 
 * Deletes any entry in the queues corresponding to a flit
 * that has completely being sent.
 */
void caHandler::update_sending_end_cycle_queues() {

	int port, group;
	assert(g_contention_aware);

	for (port = 0; port < g_ports; port++) {
		/* Pop any element that has already expired */
		while (not (m_sending_end_cycle_queue[port].empty())
				&& m_sending_end_cycle_queue[port].front() <= g_internal_cycle) {
			m_sending_end_cycle_queue[port].pop();
		}
	}

	for (group = 0; group < g_a_routers_per_group * g_h_global_ports_per_router + 1; group++) {
		while (not m_sending_end_cycle_globalport_queue[group].empty()
				&& m_sending_end_cycle_globalport_queue[group].front() <= g_internal_cycle) {
			m_sending_end_cycle_globalport_queue[group].pop();
		}
	}
}

/* 
 * A flit has started to be transmitted, and needs to be
 * stored in the sending queue corresponding to the
 * minimal path.
 */
void caHandler::flitSent(flitModule * flit, int input, int length) {

	int min_output = m_sw->routing->minOutputPort(flit->destId);

	m_sending_end_cycle_queue[min_output].push(g_internal_cycle + length);
	if (flit->destGroup != m_sw->hPos
			&& (input < g_p_computing_nodes_per_router || input >= g_global_router_links_offset))
		m_sending_end_cycle_globalport_queue[flit->destGroup].push(g_internal_cycle + length);
}

/* 
 * Returns port contention, defined as the number of
 * PHITS that would be minimally routed through a
 * specified output port.
 */
int caHandler::getContention(int output) const {
	assert(output < g_ports);
	assert(output >= 0);
	if (g_misrouting_trigger == FILTERED)
		/* If misrouting trigger is filtered, it needs to
		 * account current and previous contention values */
		return m_accumulated_contention[output];
	else
		return m_contention_counter[output];
}

/*
 * Updates the accummulated contention value (in PHITS)
 * through a specified output port, to reflect its previous
 * value.
 */
void caHandler::updateContention(int output) {
	assert(output < g_ports);
	assert(output >= 0);
	if (g_misrouting_trigger == FILTERED)
		m_accumulated_contention[output] = (1 - g_filtered_contention_regressive_coefficient)
				* m_contention_counter[output]
				+ (g_filtered_contention_regressive_coefficient * m_accumulated_contention[output]);
	m_contention_counter[output] = 0;
	m_potential_contention_counter[output] = 0;
}

bool caHandler::isThereGlobalContention(flitModule * flit) {
	assert(g_misrouting_trigger == CA_REMOTE || g_misrouting_trigger == HYBRID_REMOTE);
	if (flit->sourceSW != m_sw->label) return false; /* Only check global contention for injection packets */
	if (flit->destGroup == m_sw->hPos) return false; /* Only allow misroute if destination is outside current group */
	int contention = 0;
	for (int sw = 0; sw < g_a_routers_per_group; sw++) {
		contention += m_partial_counter[sw][flit->destGroup];
	}
#if DEBUG
	if (contention >= g_contention_aware_global_th * g_flit_size && m_sw->label == 0)
	cout << "Contention in group " << flit->destGroup << " at cycle " << g_cycle << endl;
#endif

	return (contention >= g_contention_aware_global_th * g_flit_size);
}

/*
 * Returns true if there is contention in the link and therefore 
 * a misroute should be considered. Its behavior depends on the
 * contention aware misrouting trigger employed.
 */
bool caHandler::isThereContention(int output) {
	bool contention = false;
	contention = getContention(output) >= g_contention_aware_th * g_flit_size;
	return contention;
}

/*
 * Set of functions to increase contention when a packet
 * enters a queue, and to decrease it when the packet is
 * egressed. Implies NOT increasing contention at header.
 */
void caHandler::increaseContention(int port) {
	m_contention_counter[port] += g_flit_size;
	assert(m_contention_counter[port] >= 0);
	assert(!g_increaseContentionAtHeader);
}

void caHandler::decreaseContention(int port) {
	m_contention_counter[port] -= g_flit_size;
	assert(m_contention_counter[port] >= 0);
	assert(!g_increaseContentionAtHeader);
}
