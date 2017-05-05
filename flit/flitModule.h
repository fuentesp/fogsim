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

#ifndef class_flit
#define class_flit

#include "../global.h"

using namespace std;

class flitModule {
protected:
public:
	bool head;
	bool tail;
	long long flitId;
	int flitSeq;
	long long packetId;
	int sourceId;
	int sourceSW;
	int sourceGroup;
	int destId;
	int destSwitch;
	int destGroup;
	int valId;
	int channel;
	int assignedRing;
	bool localMisroutingDone;
	bool globalMisroutingDone;
	int minPathLength;
	int valPathLength;
	long double injLatency;
	float inCycle;
	float inCyclePacket;
	bool valNodeReached;
	bool mandatoryGlobalMisrouting_flag;
	FlitType flitType;
	int petitionSrcId;
	long double petitionLatency;
	unsigned short cos; /* Class of service - Ethernet 802.1q */
	int length;

	/* Hop counters*/
	int hopCount;
	int localHopCount;
	int globalHopCount;
	int localEscapeHopCount;
	int globalEscapeHopCount;

	/* Injection counters */
	int subnetworkInjectionsCount;
	int rootSubnetworkInjectionsCount;
	int sourceSubnetworkInjectionsCount;
	int destSubnetworkInjectionsCount;

	/* Contention counters */
	long double localContentionCount;
	long double globalContentionCount;
	long double localEscapeContentionCount;
	long double globalEscapeContentionCount;

	/* Routing related (help switch to know which output will flit be routed through) */
	int nextP, nextVC, prevP, prevVC;

	/* TRACES */
	int task;
	int mpitype;

	/* Graph500 */
	int graph_queries;

	flitModule(int packetId, int flitId, int flitSeq, int sourceId, int destId, int destSwitch, int valId, bool head,
			bool tail, unsigned short cos = 0);
	void addHop(int outP, int swId);
	void addContention(int inP, int swId);
	void subsContention(int outP, int swId);
	void addSubnetworkInjection();
	void addRootSubnetworkInjection();
	void setChannel(int nextVC);

	void setMisrouted(bool misrouted);
	void setMisrouted(bool misrouted, MisrouteType misroute_type);
	bool getMisrouted() const;
	bool getPrevMisrouted() const;
	void setBaseLatency(double base_latency);
	double getBaseLatency() const;
	void setCurrentMisrouteType(MisrouteType current_misroute_type);
	MisrouteType getCurrentMisrouteType() const;
	void setGlobalMisrouteAtInjection(bool global_misroute_at_injection_flag);
	bool isGlobalMisrouteAtInjection() const;
	int getMisrouteCount(MisrouteType type) const;

private:
	bool m_misrouted;
	bool m_misrouted_prev;
	double m_base_latency;
	/* CurrentRouterGlobal and MixedMode global
	 * misrouting counters */
	int m_local_misroute_count;
	int m_local_mm_misroute_count;
	int m_global_misroute_count;
	int m_mandatory_global_misroute_count;
	MisrouteType m_current_misroute_type;
	bool m_global_misroute_at_injection_flag;

};

#endif
