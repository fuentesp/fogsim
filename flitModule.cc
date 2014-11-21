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
#include <fstream>

#include "buffer.h"
#include "gModule.h"
#include "generatorModule.h"
#include "switchModule.h"
#include "flitModule.h"
#include "dgflySimulator.h"

using namespace std;

flitModule::flitModule(int packetId, int flitId, int flitSeq, int sourceId, int destId, int destSwitch, int valId,
		bool head, bool tail) {
	this->flitId = flitId; //Flit number
	this->packetId = packetId; //Packet number
	this->flitSeq = flitSeq; //Flit position within this packet
	this->destId = destId;
	this->destSwitch = destSwitch;
	this->head = head;
	this->tail = tail;
	this->val = 0;
	this->valId = valId;
	this->valNodeReached = 0;
	this->localMisroutingPetitionDone = 0;
	this->globalMisroutingPetitionDone = 0;
	this->localMisroutingDone = 0;
	this->globalMisroutingDone = 0;
	this->sourceId = sourceId;
	this->assignedRing = 0;
	this->hopCount = 0;
	this->localHopCount = 0;
	this->globalHopCount = 0;
	this->localRingHopCount = 0;
	this->globalRingHopCount = 0;
	this->localTreeHopCount = 0;
	this->globalTreeHopCount = 0;
	this->subnetworkInjectionsCount = 0;
	this->rootSubnetworkInjectionsCount = 0;
	this->destSubnetworkInjectionsCount = 0;
	this->sourceSubnetworkInjectionsCount = 0;
	this->localContentionCount = 0;
	this->globalContentionCount = 0;
	this->localRingContentionCount = 0;
	this->globalRingContentionCount = 0;
	this->localTreeContentionCount = 0;
	this->globalTreeContentionCount = 0;
	this->channel = 0;
	this->inj_latency = -1;
	this->inCycle = 0;
	this->inCyclePacket = 0;
	this->outCycle = 0;
	this->mandatoryGlobalMisrouting_flag = 0;
	local_mm_MisroutingPetitionDone = 0;

	m_misrouted = false;
	m_base_latency = 0;
	sourceGroup = (int) (sourceId / (g_dA * g_dP));
	sourceSW = (int) (sourceId / (g_dP));
	destGroup = (int) (destId / (g_dA * g_dP));

	m_local_misroute_count = 0;
	m_local_mm_misroute_count = 0;
	m_global_misroute_count = 0;
	m_mandatory_global_misroute_count = 0;
	m_global_misroute_at_injection_flag = false;

	m_current_misroute_type = NONE;
}

void flitModule::addHop() {
	hopCount++;
	if (g_max_hops < hopCount) {
		g_max_hops = hopCount;
	}
}

void flitModule::addLocalHop() {
	localHopCount++;
	if (g_max_local_hops < localHopCount) {
		g_max_local_hops = localHopCount;
	}
}

void flitModule::addGlobalHop() {
	globalHopCount++;
	if (g_max_global_hops < globalHopCount) {
		g_max_global_hops = globalHopCount;
	}
}

void flitModule::addGlobalRingHop() {
	globalRingHopCount++;
	if (g_max_global_ring_hops < globalRingHopCount) {
		g_max_global_ring_hops = globalRingHopCount;
	}
}

void flitModule::addLocalTreeHop() {
	localTreeHopCount++;
	if (g_max_local_tree_hops < localTreeHopCount) {
		g_max_local_tree_hops = localTreeHopCount;
	}
}

void flitModule::addGlobalTreeHop() {
	globalTreeHopCount++;
	if (g_max_global_tree_hops < globalTreeHopCount) {
		g_max_global_tree_hops = globalTreeHopCount;
	}
}

void flitModule::addLocalRingHop() {
	localRingHopCount++;
	if (g_max_local_ring_hops < localRingHopCount) {
		g_max_local_ring_hops = localRingHopCount;
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

void flitModule::addLocalContention() {
	localContentionCount += g_cycle;
}

void flitModule::addGlobalContention() {
	globalContentionCount += g_cycle;
}

void flitModule::addLocalRingContention() {
	localRingContentionCount += g_cycle;
}

void flitModule::addGlobalRingContention() {
	globalRingContentionCount += g_cycle;
}

void flitModule::addLocalTreeContention() {
	localTreeContentionCount += g_cycle;
}

void flitModule::addGlobalTreeContention() {
	globalTreeContentionCount += g_cycle;
}

void flitModule::subsLocalContention() {
	localContentionCount -= (g_cycle + g_delayTransmission_local);
}

void flitModule::subsGlobalContention() {
	globalContentionCount -= (g_cycle + g_delayTransmission_global);
}

void flitModule::subsLocalRingContention() {
	localRingContentionCount -= (g_cycle + g_delayTransmission_local);
}

void flitModule::subsGlobalRingContention() {
	globalRingContentionCount -= (g_cycle + g_delayTransmission_global);
}

void flitModule::subsLocalTreeContention() {
	localTreeContentionCount -= (g_cycle + g_delayTransmission_local);
}

void flitModule::subsGlobalTreeContention() {
	globalTreeContentionCount -= (g_cycle + g_delayTransmission_global);
}

void flitModule::setChannel(int nextVC) {
	this->channel = nextVC;
}

int flitModule::nextChannel(int inp, int outp) {
	char outType, inType;
	int next_channel;
	next_channel = this->channel;

	if (outp < g_dP) {
		outType = 'p';
	} else if (outp < g_offsetH) {
		outType = 'a';
	} else {
		outType = 'h';
	}
	if (inp < g_dP) {
		inType = 'p';
	} else if (inp < g_offsetH) {
		inType = 'a';
	} else {
		inType = 'h';
	}

	if ((inType == 'p')) {
		next_channel = 0;
	}
	if (g_valiant_long == false) {
		if (((outType == 'a') && (inType == 'h'))
				|| ((outType == 'a') && (inType == 'a') && (g_strictMisroute == false))
				|| ((outType == 'h') && (inType == 'h'))) {

			next_channel = (this->channel) + 1;
			assert((this->channel) + 1 < g_channels);
		}
	} else {
		assert(g_vc_misrouting_increasingVC == false);
		if (inType == 'p') {
			next_channel = 0;
		} else if (outType == 'p') {
			next_channel = this->channel;
		} else {
			if (outType == 'h') {
				if (inType == 'h') {
					next_channel = (this->channel) + 1;
				} else if (inType == 'a') {
					if (this->channel >= 3) assert(0);

					next_channel = (this->channel);
					if (this->channel == 2) next_channel = 1;
				} else {
					assert(0);
				}
			} else if (outType == 'a') {
				if (this->channel >= 2) assert(0);
				next_channel = (this->channel) + 1;
				if (this->valNodeReached == true) next_channel = 2;
				if ((inType == 'h') && (this->channel == 1)) next_channel = 3;
			} else {
				assert(0);
			}
		}

		if (next_channel >= g_channels) {
			cout << "this->localHopCount=" << this->localHopCount << endl;
			cout << "this->globalHopCount=" << this->globalHopCount << endl;
			cout << "this->channel=" << this->channel << endl;
			cout << "g_channels=" << g_channels << endl;
			cout << "next_channel=" << next_channel << endl;
			cout << "out_type=" << outType << endl;
			cout << "in_type=" << inType << endl;
		}
		assert(next_channel < g_channels);
	}

	if ((g_vc_misrouting_increasingVC == true)) {
		if ((inType == 'a') && (outType == 'h')) {
			if (this->channel < 2) {
				next_channel = 0;
			} else if (this->channel < 4) {
				next_channel = 1;
			}
		} else if ((inType == 'h') && (outType == 'a')) {
			if (this->channel == 0) {
				next_channel = 2;
			} else if (this->channel == 1) {
				next_channel = 4;
			} else {
				assert(0);
			}
		}
	}
	assert(next_channel < g_channels);
	return (next_channel);
}

void flitModule::setMisrouted(bool misrouted) {
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

		case GLOBAL:
			m_global_misroute_count++;
			break;

		default:
			//should never enter here
			assert(false);
			break;
	}
}

bool flitModule::isMisrouted() const {
	return m_misrouted;
}

void flitModule::setBase_latency(long base_latency) {
	this->m_base_latency = base_latency;
}

long flitModule::getBase_latency() const {
	return m_base_latency;
}

void flitModule::setCurrent_misroute_type(MisrouteType current_misroute_type) {
	this->m_current_misroute_type = current_misroute_type;
}

MisrouteType flitModule::getCurrent_misroute_type() const {
	return m_current_misroute_type;
}

void flitModule::setGlobal_misroute_at_injection_flag(bool global_misroute_at_injection_flag) {
	this->m_global_misroute_at_injection_flag = global_misroute_at_injection_flag;
}

bool flitModule::isGlobal_misroute_at_injection_flag() const {
	return m_global_misroute_at_injection_flag;
}

int flitModule::getLocal_misroute_count() const {
	return m_local_misroute_count;
}

int flitModule::getLocal_mm_misroute_count() const {
	return m_local_mm_misroute_count;
}

int flitModule::getGlobal_misroute_count() const {
	return m_global_misroute_count;
}

int flitModule::getMandatory_global_misroute_count() const {
	return m_mandatory_global_misroute_count;
}

int flitModule::getMisroute_count() const {
	return m_local_misroute_count + m_global_misroute_count;
}
