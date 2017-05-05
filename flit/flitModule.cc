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

#include "flitModule.h"

using namespace std;

flitModule::flitModule(int packetId, int flitId, int flitSeq, int sourceId, int destId, int destSwitch, int valId,
		bool head, bool tail, unsigned short cos) {
	assert(destId < g_number_generators);
	this->flitId = flitId;
	this->packetId = packetId;
	this->flitSeq = flitSeq; /* Flit offset within packet */
	this->sourceId = sourceId;
	this->destId = destId;
	this->destSwitch = destSwitch;
	this->head = head;
	this->tail = tail;
	this->valId = valId;
	sourceGroup = (int) (sourceId / (g_a_routers_per_group * g_p_computing_nodes_per_router));
	sourceSW = (int) (sourceId / (g_p_computing_nodes_per_router));
	destGroup = (int) (destId / (g_a_routers_per_group * g_p_computing_nodes_per_router));
	valNodeReached = 0;
	channel = 0;
	injLatency = -1;
	m_base_latency = 0;
	inCycle = 0;
	inCyclePacket = 0;
	m_misrouted = false;
	m_misrouted_prev = false;
	m_current_misroute_type = NONE;
	mandatoryGlobalMisrouting_flag = 0;
	localMisroutingDone = 0;
	globalMisroutingDone = 0;
	m_local_misroute_count = 0;
	m_local_mm_misroute_count = 0;
	m_global_misroute_count = 0;
	m_mandatory_global_misroute_count = 0;
	m_global_misroute_at_injection_flag = false;
	hopCount = 0;
	localHopCount = 0;
	globalHopCount = 0;
	localContentionCount = 0;
	globalContentionCount = 0;
	assignedRing = 0;
	localEscapeHopCount = 0;
	globalEscapeHopCount = 0;
	subnetworkInjectionsCount = 0;
	rootSubnetworkInjectionsCount = 0;
	destSubnetworkInjectionsCount = 0;
	sourceSubnetworkInjectionsCount = 0;
	localEscapeContentionCount = 0;
	globalEscapeContentionCount = 0;
	flitType = RESPONSE;
	petitionSrcId = -1;
	petitionLatency = 0;
	this->length = g_flit_size;
	assert(cos < g_cos_levels);
	this->cos = cos;
	graph_queries = -1;

	this->nextP = this->nextVC = this->prevP = this->prevVC - 1;
}

/*
 * Adds hop to flit counter when transitting from a switch
 * to another. Besides main counter, also tracks if hop
 * belongs to escape subnetwork and whether the hop is through
 * a local or a global link.
 */
void flitModule::addHop(int outP, int swId) {
	hopCount++;
	if (g_max_hops < hopCount) g_max_hops = hopCount;

	assert(this->channel >= 0 && this->channel < g_channels);

	/* Update local/global hop counters */
	int thisA = swId % (g_a_routers_per_group);
	/* First check if flit is transitting through escape subnetwork */
	if (g_vc_usage == BASE) {
		if ((g_deadlock_avoidance == EMBEDDED_TREE || g_deadlock_avoidance == EMBEDDED_RING)
				&& this->channel >= g_local_link_channels) {
			/* Embedded network */
			if (outP < g_global_router_links_offset) {
				localEscapeHopCount++;
				if (g_max_local_subnetwork_hops < localEscapeHopCount) g_max_local_subnetwork_hops =
						localEscapeHopCount;
			} else {
				globalEscapeHopCount++;
				if (g_max_global_subnetwork_hops < globalEscapeHopCount) g_max_local_subnetwork_hops =
						globalEscapeHopCount;
			}
			return;
		} else if (outP >= g_global_router_links_offset + g_h_global_ports_per_router) {
			/* Physical ring */
			assert(g_deadlock_avoidance == RING && g_ring_ports != 0);
			if ((outP == g_ports - 2 && thisA == 0) || (outP == g_ports - 1 && thisA == g_a_routers_per_group - 1)) {
				globalEscapeHopCount++;
				if (g_max_global_subnetwork_hops < globalEscapeHopCount) g_max_local_subnetwork_hops =
						globalEscapeHopCount;
			} else {
				localEscapeHopCount++;
				if (g_max_local_subnetwork_hops < localEscapeHopCount) g_max_local_subnetwork_hops =
						localEscapeHopCount;
			}
			return;
		}
	}

	/* Flit is being tx. through main net */
	if (outP < g_global_router_links_offset) {
		/* Local link */
		localHopCount++;
		if (g_max_local_hops < localHopCount) g_max_local_hops = localHopCount;
	} else {
		/* Global link */
		globalHopCount++;
		if (g_max_global_hops < globalHopCount) g_max_global_hops = globalHopCount;
	}

}

void flitModule::addSubnetworkInjection() {
	subnetworkInjectionsCount++;
	if (g_max_subnetwork_injections < subnetworkInjectionsCount) {
		g_max_subnetwork_injections = subnetworkInjectionsCount;
		g_max_root_subnetwork_injections = rootSubnetworkInjectionsCount;
		g_max_source_subnetwork_injections = sourceSubnetworkInjectionsCount;
		g_max_dest_subnetwork_injections = destSubnetworkInjectionsCount;
	}
}

/*
 * Adds cycle to contention counter when a flit is rx. in a
 * switch. Distinguishes among normal and escape subnetwork
 * counters.
 */
void flitModule::addContention(int inP, int swId) {
	/* If input port is a computing node, do not track contention */
	if (inP < g_p_computing_nodes_per_router) return;

	assert(this->channel >= 0 && this->channel < g_channels);

	int thisA = swId % (g_a_routers_per_group);
	/* First check if flit is transitting through escape subnetwork */
	if (g_vc_usage == BASE) {
		if ((g_deadlock_avoidance == EMBEDDED_TREE || g_deadlock_avoidance == EMBEDDED_RING)
				&& this->channel >= g_local_link_channels) {
			/* Embedded network */
			if (inP < g_global_router_links_offset) {
				localEscapeContentionCount += g_internal_cycle;
			} else {
				globalEscapeContentionCount += g_internal_cycle;
			}
			return;
		} else if (inP >= g_global_router_links_offset + g_h_global_ports_per_router) {
			/* Physical ring */
			assert(g_deadlock_avoidance == RING && g_ring_ports != 0);
			if ((inP == g_ports - 2 && thisA == 0) || (inP == g_ports - 1 && thisA == g_a_routers_per_group - 1)) {
				globalEscapeContentionCount += g_internal_cycle;
			} else {
				localEscapeContentionCount += g_internal_cycle;
			}
			return;
		}
	}

	/* Flit is being tx through main net */
	if (inP < g_global_router_links_offset) {
		/* Local link */
		localContentionCount += g_internal_cycle;
	} else if (inP < g_global_router_links_offset + g_h_global_ports_per_router) {
		/* Global link */
		globalContentionCount += g_internal_cycle;
	}
}

/*
 * Subtracts cycle to contention counter when a flit is tx. from
 * a switch. Distinguishes among normal and escape subnetwork
 * counters.
 */
void flitModule::subsContention(int outP, int swId) {
	int thisA = swId % (g_a_routers_per_group);

	assert(this->channel >= 0 && this->channel < g_channels);

	/* First check if flit is transitting through escape subnetwork */
	if (g_vc_usage == BASE) {
		if ((g_deadlock_avoidance == EMBEDDED_TREE || g_deadlock_avoidance == EMBEDDED_RING)
				&& this->channel >= g_local_link_channels) {
			/* Embedded network */
			if (outP < g_global_router_links_offset) {
				localEscapeContentionCount -= (g_internal_cycle + g_local_link_transmission_delay);
			} else {
				globalEscapeContentionCount -= (g_internal_cycle + g_global_link_transmission_delay);
			}
			return;
		} else if (outP >= g_global_router_links_offset + g_h_global_ports_per_router) {
			/* Physical ring */
			assert(g_deadlock_avoidance == RING && g_ring_ports != 0);
			if ((outP == g_ports - 2 && thisA == 0) || (outP == g_ports - 1 && thisA == g_a_routers_per_group - 1)) {
				globalEscapeContentionCount -= (g_internal_cycle + g_global_link_transmission_delay);
			} else {
				localEscapeContentionCount -= (g_internal_cycle + g_local_link_transmission_delay);
			}
			return;
		}
	}

	if (outP < g_global_router_links_offset) {
		/* Local link */
		localContentionCount -= (g_internal_cycle + g_local_link_transmission_delay);
	} else {
		/* Global link */
		globalContentionCount -= (g_internal_cycle + g_global_link_transmission_delay);
	}
}

void flitModule::setChannel(int nextVC) {
	this->channel = nextVC;
}

void flitModule::setMisrouted(bool misrouted) {
	this->m_misrouted_prev = this->m_misrouted;
	this->m_misrouted = misrouted;
}

void flitModule::setMisrouted(bool misrouted, MisrouteType misroute_type) {
	if (misrouted) assert(misroute_type != NONE);
	if (misroute_type == NONE) assert(not (misrouted));

	this->m_misrouted = misrouted;

	switch (misroute_type) {
		case NONE:
			break;

		case LOCAL:
			m_local_misroute_count++;
			break;

		case LOCAL_MM:
			m_local_misroute_count++;
			m_local_mm_misroute_count++;
			break;

		case GLOBAL_MANDATORY:
			m_global_misroute_count++;
			m_mandatory_global_misroute_count++;
			break;

		case VALIANT:
		case GLOBAL:
			m_global_misroute_count++;
			break;

		default:
			assert(false);
			break;
	}
}

void flitModule::setBaseLatency(double base_latency) {
	this->m_base_latency = base_latency;
}

double flitModule::getBaseLatency() const {
	return m_base_latency;
}

void flitModule::setCurrentMisrouteType(MisrouteType current_misroute_type) {
	this->m_current_misroute_type = current_misroute_type;
}

bool flitModule::getMisrouted() const {
	return m_misrouted;
}

bool flitModule::getPrevMisrouted() const {
	return m_misrouted_prev;
}

MisrouteType flitModule::getCurrentMisrouteType() const {
	return m_current_misroute_type;
}

void flitModule::setGlobalMisrouteAtInjection(bool global_misroute_at_injection_flag) {
	this->m_global_misroute_at_injection_flag = global_misroute_at_injection_flag;
}

bool flitModule::isGlobalMisrouteAtInjection() const {
	return m_global_misroute_at_injection_flag;
}

/*
 * Returns misroute counter, specifying which particular type
 * is required. If 'NONE', returns total misroute count.
 */
int flitModule::getMisrouteCount(MisrouteType type) const {
	switch (type) {
		case NONE: /* Give back total misroute count */
			return m_local_misroute_count + m_global_misroute_count;
		case LOCAL: /* Give back LOCAL misroute count only */
			return m_local_misroute_count;
		case LOCAL_MM: /* Give back LOCAL_MM misroute count only */
			return m_local_mm_misroute_count;
		case GLOBAL: /* Give back GLOBAL misroute count only */
			return m_global_misroute_count;
		case GLOBAL_MANDATORY: /* Give back GLOBAL_MANDATORY misroute count only */
			return m_mandatory_global_misroute_count;
		default:
			assert(0);
	}
	return -1;
}
