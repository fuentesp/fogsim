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

#ifndef class_flit
#define class_flit

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dgflySimulator.h"
#include "global.h"

using namespace std;

class flitModule {
protected:
public:
	bool head;
	bool tail;
	int destId;
	int destSwitch;
	int destGroup;
	long long flitId;
	int flitSeq;
	int assignedRing;
	long long packetId;
	int val;
	int valId;
	bool localMisroutingPetitionDone;
	bool globalMisroutingPetitionDone;
	bool localMisroutingDone;
	bool globalMisroutingDone;
	int sourceId;
	int sourceSW;
	int sourceGroup;
	int hopCount;
	int localHopCount;
	int globalHopCount;
	int localRingHopCount;
	int globalRingHopCount;
	int localTreeHopCount;
	int globalTreeHopCount;
	int subnetworkInjectionsCount;
	int rootSubnetworkInjectionsCount;
	int sourceSubnetworkInjectionsCount;
	int destSubnetworkInjectionsCount;
	long long localContentionCount;
	long long globalContentionCount;
	long long localRingContentionCount;
	long long globalRingContentionCount;
	long long localTreeContentionCount;
	long long globalTreeContentionCount;
	int channel;
	int minPath_length;
	int valPath_length;
	long long inj_latency;
	int inCycle;
	int inCyclePacket;
	int outCycle;
	bool valNodeReached;
	bool mandatoryGlobalMisrouting_flag;
	bool local_mm_MisroutingPetitionDone;

	/* TRACES */
	int length;
	int task;
	int mpitype;

	flitModule(int packetId, int flitId, int flitSeq, int sourceId, int destId, int destSwitch, int valId, bool head,
			bool tail);
	//~flitModule();
	void addHop();
	void addLocalHop();
	void addGlobalHop();
	void addLocalRingHop();
	void addGlobalRingHop();
	void addLocalTreeHop();
	void addGlobalTreeHop();
	void addSubnetworkInjection();
	void addRootSubnetworkInjection();
	void addLocalContention();
	void addGlobalContention();
	void addLocalRingContention();
	void addGlobalRingContention();
	void addLocalTreeContention();
	void addGlobalTreeContention();
	void subsLocalContention();
	void subsGlobalContention();
	void subsLocalRingContention();
	void subsGlobalRingContention();
	void subsLocalTreeContention();
	void subsGlobalTreeContention();
	void setChannel(int nextVC);
	int nextChannel(int in, int out);

	void setMisrouted(bool misrouted);
	void setMisrouted(bool misrouted, MisrouteType misroute_type);
	bool isMisrouted() const;
	void setBase_latency(long base_latency);
	long getBase_latency() const;
	void setCurrent_misroute_type(MisrouteType current_misroute_type);
	MisrouteType getCurrent_misroute_type() const;
	void setGlobal_misroute_at_injection_flag(bool global_misroute_at_injection_flag);
	bool isGlobal_misroute_at_injection_flag() const;
	int getLocal_misroute_count() const;
	int getLocal_mm_misroute_count() const;
	int getGlobal_misroute_count() const;
	int getMandatory_global_misroute_count() const;
	int getMisroute_count() const;

private:
	bool m_misrouted;
	long m_base_latency;
	//counters for crg and mm misrouting
	int m_local_misroute_count;
	int m_local_mm_misroute_count;
	int m_global_misroute_count;
	int m_mandatory_global_misroute_count;
	MisrouteType m_current_misroute_type;
	bool m_global_misroute_at_injection_flag;

};

#endif
