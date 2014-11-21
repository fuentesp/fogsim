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

#ifndef class_switch
#define class_switch

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "switchModule.h"
#include "localArbiter.h"
#include "globalArbiter.h"
#include "global.h"
#include "creditFlit.h"
#include <queue>
#include "pbFlit.h"
#include "pbState.h"
#include "gModule.h"

class buffer;
class flitModule;

using namespace std;

class switchModule: public gModule {
private:
	buffer ***pBuffers;
	int portCount, vcCount, delInj, delTrans;
	int p, c;

	int **credits;/* number of credits counter (in phits) for each VC in each output port*/

	int **maxCredits; /* MAX number of credits (in phits) for each VC in each output port*/
	queue<creditFlit> ** incomingCredits;
	queue<pbFlit> incomingPb;

	pbState PB; /* PiggyBacking: global links state handler*/

	/*
	 * vc_misrouting_congested_restriction info:
	 * wether each global link is congested or not
	 *
	 * (indexed from 0 to g_dH)
	 */
	bool * m_vc_misrouting_congested_restriction_globalLinkCongested;

	bool * m_reorderVCs;
	int * m_nextVCtoTry;

	void sendCredits(int port, int channel, int flitId);
	bool isGlobalLinkCongested(const flitModule * flit);
	long calculateBaseLatency(const flitModule * flit);

	bool portCanReceiveFlit(int port);

	// ring version, on the fly misrouting
	bool selectOFARMisrouteRandomGlobal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC);
	bool selectOFARMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC);

	//vc version, on the fly misrouting
	MisrouteType misrouteType(int inport, flitModule * flit, int prev_outP, int prev_nextC);
	MisrouteType misrouteTypeOFAR(int inport, int inchannel, flitModule * flit, int prev_outP, int prev_nextC);
	bool selectVcMisrouteRandomGlobal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC);
	bool selectVcMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC);
	bool selectStrictMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC);
	bool selectStrictMisrouteRandomLocal_MM(flitModule * flit, int input_port, int prev_outP, int prev_nextC,
			int &selectedPort, int &selectedVC);
	bool vcMisrouteCandidate(flitModule * flit, int input_port, int prev_outP, int prev_nextC, int &selectedPort,
			int &selectedVC, MisrouteType &misroute);
	bool OFARMisrouteCandidate(flitModule * flit, int input_port, int input_channel, int prev_outP, int prev_nextC,
			int &selectedPort, int &selectedVC, MisrouteType &misroute);
	bool misrouteCondition(flitModule * flit, int prev_outP, int prev_nextC);

	bool vc_misrouting_congested_restriction_ALLOWED(flitModule * flit, int prev_outP, int prev_nextC);
	void vc_misrouting_congested_restriction_UPDATE();

public:
	switchModule **neighList; /* SWITCH associated to each PORT of the current switch */
	int *neighPort; /* PORT NUMBER associated to each PORT of the current switch */
	localArbiter **localArbiters;
	globalArbiter **globalArbiters;
	int *tableIn, *tableOut, *tableInRing1, *tableOutRing1, *tableInRing2, *tableOutRing2, *tableInTree, *tableOutTree;
	int label, pPos, aPos, hPos;
	int lastConsumeCycle[100];
	int out_port_used[100];
	int in_port_served[100];
	int messagesInQueuesCounter;
	bool escapeNetworkCongested;
	long long packetsInj;
	switchModule **nb;
	switchModule(string name, int label, int aPos, int hPos, int ports, int vcCount);
	~switchModule();
	void findNeighbors(switchModule* swList[]);
	int getTotalCapacity();
	int getTotalFreeSpace();
	void insertFlit(int port, int vc, flitModule *flit);
	void setTables();
	int getCredits(int port, int channel);
	int getCreditsOccupancy(int port, int channel);
	void increasePortCount(int port);
	void increasePortContentionCount(int port);
	void orderQueues();
	void attendPetitions(int port);
	void attendPetitionsAgeArbiter(int port);
	void tryNextVC(int port);
	void escapeCongested();
	void resetCredits();
	void injectFlit(int port, int vc, flitModule* flit);
	void receiveCreditFlit(int port, const creditFlit& crdFlit);
	void visitNeighbours();
	void receivePbFlit(const pbFlit& crdFlit);
	int min_outport(flitModule * flit);

	float queueOccupancy[500];
	float injectionQueueOccupancy[10];
	float localQueueOccupancy[10];
	float globalQueueOccupancy[10];
	float localRingQueueOccupancy[10];
	float globalRingQueueOccupancy[10];
	float localTreeQueueOccupancy[10];
	float globalTreeQueueOccupancy[10];

	void updateCredits();
	void updatePb();
	void updateReadPb();

	void resetQueueOccupancy();
	void setQueueOccupancy();
	int olderPort(int output_port);
	int olderChannel(int input_port);
	bool servePort(int inputPort, int inputChannel, int outP, int nexP, int nextC);
	bool tryTransit(int input_port, int input_channel, int outP, int nextP, int nextC, flitModule *flitEx);
	bool tryConsume(int input_port, int input_channel, int outP, flitModule *flitEx);
	bool tryPetition(int input_port, int input_channel);
	bool couldTryTransit(int input_port, int input_channel, int outP, int nextP, int nextC, flitModule *flitEx);
	bool couldTryConsume(int input_port, int input_channel, int outP, flitModule *flitEx);
	bool couldTryPetition(int input_port, int input_channel);
	void initPetitions();
	void action();
};

#endif
