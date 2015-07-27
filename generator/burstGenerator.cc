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

#include "burstGenerator.h"
#include <math.h>

burstGenerator::burstGenerator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
		switchModule *switchM) :
		generatorModule(interArrivalTime, name, sourceLabel, pPos, aPos, hPos, switchM) {
	/* Currently this generator type is only aimed to uniform random
	 * traffic pattern; could be updated in future revisions. */
	assert(g_traffic == BURSTY_UN);
	pOn2Off = (double) 1 / g_bursty_avg_length;
	double pON = (double) g_injection_probability / (100 * g_packet_size);
	pOff2On = pON / (g_bursty_avg_length + pON * (1 - g_bursty_avg_length));
	assert(fabs(pOff2On) <= 1 && fabs(pOn2Off) <= 1);
	injecting = false; /* Initial state is 'Off' (not sending) */
	prevDest = -1; /* To prevent errors, initial destination is not valid */
	curBurstLength = 0;
}

burstGenerator::~burstGenerator() {

}

/* Generates a new flit if injection probability triggers it, and
 * the injection buffer has enough space. It has 2 different
 * status (injecting / not injecting) and can change destination
 * target without halting injection by allowing to transit from
 * ON to OFF and back to ON state in the same cycle [the opposite
 * does not apply, it can not go OFF > ON > OFF]. These transitions
 * are triggered through the On2Off and Off2On possibilities determined
 * by the average burst length and injection probability.
 */
void burstGenerator::generateFlit() {
	/* If there are no available credits, do not attempt to
	 * change state: OFF2ON would not enforce injection,
	 * and ON2OFF would reduce average burst size below
	 * specified value.
	 */
	m_flit_created = false;

	if ((g_cycle == 0 || g_cycle >= (lastTimeSent + interArrivalTime))) {
		/* Determine current cycle status */
		double random = rand() / ((double) RAND_MAX + 1);
		if (!injecting) { /* Current status: OFF */
			if (random < (double) pOff2On) {
				injecting = true;
				destLabel = pattern->setDestination(UN);
				prevDest = destLabel;
				g_injected_bursts_counter++;
				m_injVC = int(destLabel * (g_injection_channels) / g_number_generators);
			}
		} else if (switchM->switchModule::getCredits(this->pPos, m_injVC) >= g_packet_size) { /* Current status: ON */
			if (random < pOn2Off) {
				random = rand() / ((double) RAND_MAX + 1);
				if (random < pOff2On) {
					destLabel = pattern->setDestination(UN);
					prevDest = destLabel;
					g_injected_bursts_counter++;
					m_injVC = int(destLabel * (g_injection_channels) / g_number_generators);
				} else {
					injecting = false;
					curBurstLength = 0;
				}
			} else {
				destLabel = prevDest;
			}
		}
		assert(!injecting || (destLabel >= 0 && destLabel < g_number_generators));
	}

	/* Determine flit position within packet. When it overflows
	 the number of flits per packet, it is reset to 0. */
	if (m_flitSeq >= (g_packet_size / g_flit_size)) {
		assert(m_flitSeq == g_flits_per_packet);
		m_flitSeq = 0;
	}

	/* Generate packet */
	if ((m_flitSeq > 0) || (g_cycle == 0) || ((g_cycle >= (lastTimeSent + interArrivalTime)))) {
		if ((m_flitSeq > 0)
				|| (injecting && (switchM->switchModule::getCredits(this->pPos, m_injVC) >= g_packet_size))) {
			if (m_flitSeq == 0) {		// Flit is header of packet
				m_packet_id = g_tx_packet_counter;
				destSwitch = int(destLabel / g_p_computing_nodes_per_router);
			}
			flit = new flitModule(m_packet_id, g_tx_flit_counter, m_flitSeq, sourceLabel, destLabel, destSwitch, 0, 0,
					0);
			if (m_flitSeq == (g_flits_per_packet - 1)) flit->tail = 1;
			if (m_flitSeq == 0) flit->head = 1;
			assert(m_flitSeq < g_flits_per_packet);
			m_flit_created = true;
			curBurstLength++;
			g_injected_packet_counter++;
		}
	}
}

