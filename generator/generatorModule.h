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

#ifndef class_generatorAdapt
#define class_generatorAdapt

#include "../global.h"
#include "../gModule.h"
#include "event.h"
#include "../flit/flitModule.h"
#include "../switch/switchModule.h"
#include "trafficPattern/steady.h"
#include "trafficPattern/burst.h"
#include "trafficPattern/all2all.h"
#include "trafficPattern/mix.h"
#include "trafficPattern/transient.h"
using namespace std;

class generatorModule: public gModule {
protected:
	int interArrivalTime;
	int lastTimeSent;
	int Count, in;
	int pPos;
	int aPos;
	int hPos;
	flitModule *flit;
	int m_valiantLabel;
	int m_assignedRing;
	int m_min_path;
	int m_val_path;
	int m_injVC;
	int m_flitSeq;
	int destLabel;
	int destSwitch;
	long long m_packet_id;
	int m_packet_in_cycle;
	vector<int> injPetVcs, injResVcs;
	float injection_probability;
	int pendingPetitions;
	void inject();
public:
	switchModule *switchM;
	int sourceLabel;
	steadyTraffic *pattern;
	long long lastConsumeCycle = LLONG_MIN;
	unsigned long sum_injection_probability;
	generatorModule(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
			switchModule *switchM);
	virtual ~generatorModule();
	virtual void action() override;
	virtual flitModule* generateFlit(FlitType flitType = RESPONSE, int destId = -1);
	void determinePaths(flitModule *flit);
	void getNodeCoords(int nodeId, int &nodeP, int &nodeA, int &nodeH);
	int getInjectionVC(int dest, FlitType flitType);
	vector<int> getArrayInjVC(int dest, bool response);
	void setInjectionProbability(float newInjectionProbability);
	void decreasePendingPetitions(int numPetitions = 1);
	virtual bool checkConsume(flitModule *flit);
	virtual void consumeFlit(flitModule *flit, int input_port, int input_channel);
	void trackConsumptionStatistics(flitModule *flit, int inP, int inC, int outP);
	/* Support functions, only intended for trace generator compatibility */
	virtual inline bool isGenerationEnded() {
		assert(0);
	}
	virtual inline void printHeadEvent() {
		assert(0);
	}
	virtual inline int numSkipCycles() {
		assert(0);
	}
	virtual inline void consumeCycles(int numCycles) {
		assert(0);
	}
	virtual inline void resetEventQueue() {
		assert(0);
	}
	virtual inline void insertEvent(event i, int destId) {
		assert(0);
	}
	virtual inline void insertOccurredEvent(flitModule *flit) {
		assert(0);
	}
};

#endif
