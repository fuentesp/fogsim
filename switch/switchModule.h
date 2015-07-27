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

#ifndef class_switch
#define class_switch

#include "../global.h"
#include "../gModule.h"
#include "arbiter/localArbiter.h"
#include "arbiter/globalArbiter.h"
#include "arbiter/priorityGlobalArbiter.h"
#include "../flit/caFlit.h"
#include "../pbState.h"
#include "../caHandler.h"
#include "port/port.h"
#include "port/inPort.h"
#include "port/outPort.h"
#include "../routing/routing.h"

using namespace std;

class switchModule: public gModule {
protected:
	inPort **inPorts;
	outPort **outPorts;
	int portCount, vcCount;
	queue<creditFlit> ** incomingCredits;
	queue<pbFlit> incomingPb;
	pbState piggyBack; /* PiggyBacking: global links state handler */
	queue<caFlit> incomingCa;

	void sendCredits(int port, int channel, int flitId);
	double calculateBaseLatency(const flitModule * flit);
	virtual void sendFlit(int input_port, int input_channel, int outP, int nextP, int nextC);
	void updateMisrouteCounters(int outP, flitModule * flitEx);
	void trackConsumptionStatistics(flitModule *flitEx, int input_port, int input_channel, int outP);
	void trackTransitStatistics(flitModule *flitEx, int input_channel, int outP, int nextC);

	/* Befriended functions to grant access to buffers & other variables. */
	friend void caHandler::update();
	friend void caHandler::readIncomingCAFlits();
	friend bool localArbiter::checkPort();
	friend bool localArbiter::portCanSendFlit(int port, int vc);

public:
	caHandler m_ca_handler;
	baseRouting* routing;
	localArbiter **localArbiters;
	globalArbiter **globalArbiters;
	int label, pPos, aPos, hPos;
	float *lastConsumeCycle;
	int messagesInQueuesCounter;
	bool escapeNetworkCongested;
	long long packetsInj;
	float *queueOccupancy;
	float *injectionQueueOccupancy;
	float *localQueueOccupancy;
	float *globalQueueOccupancy;
	float *localEscapeQueueOccupancy;
	float *globalEscapeQueueOccupancy;
	bool *reservedOutPort;
	bool *reservedInPort;

	switchModule(string name, int label, int aPos, int hPos, int ports, int vcCount);
	~switchModule();
	int getTotalCapacity();
	int getTotalFreeSpace();
	void insertFlit(int port, int vc, flitModule *flit);
	virtual int getCredits(int port, int channel);
	virtual int getCreditsOccupancy(int port, int channel);
	virtual int checkConsumePort(int port);
	void increasePortCount(int port);
	void increaseVCCount(int vc, int port);
	void increasePortContentionCount(int port);
	void orderQueues();
	void escapeCongested();
	void resetCredits();
	void injectFlit(int port, int vc, flitModule* flit);
	void receiveCreditFlit(int port, const creditFlit& crdFlit);
	void receivePbFlit(const pbFlit& crdFlit);
	void receiveCaFlit(const caFlit& flit);

	inline int getSwPortSize() {
		return portCount;
	}
	inline int getMaxCredits(int port, int vc) {
		return outPorts[port]->maxCredits[vc];
	}
	virtual bool nextPortCanReceiveFlit(int port);
	flitModule * getFlit(int port, int vc);
	int getCurrentOutPort(int port, int vc);
	int getCurrentOutVC(int port, int vc);
	bool isVCUnlocked(flitModule * flit, int port, int vc);
	bool isGlobalLinkCongested(const flitModule * flit);

	void updateCredits();
	void updatePb();
	void updateReadPb();

	void resetQueueOccupancy();
	void setQueueOccupancy();
	void action();
};

#endif
