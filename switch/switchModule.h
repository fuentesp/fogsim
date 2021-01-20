/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2014-2021 University of Cantabria

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
#include "arbiter/arbiter.h"
#include "arbiter/inputArbiter.h"
#include "arbiter/outputArbiter.h"
#include "../flit/caFlit.h"
#include "../pbState.h"
#include "../caHandler.h"
#include "port/port.h"
#include "port/inPort.h"
#include "port/dynBufInPort.h"
#include "port/outPort.h"
#include "port/dynBufOutPort.h"
#include "../routing/routing.h"

using namespace std;

class switchModule : public gModule {
protected:
	inPort **inPorts;
	outPort **outPorts;
	int portCount, vcCount;
	queue<creditFlit> **incomingCredits;
	queue<pbFlit> incomingPb;
	pbState piggyBack; /* PiggyBacking: global links state handler */
	queue<caFlit> incomingCa;
	int qcnQlen;
	int *qcnQlenOld; /* Queue length old (last qcn sampling calculation) by port */
	int *qcnCpSamplingCounter; /* Counter of cycles */
	int *qcnRpTxBCount; /* Counter of phits left for RP execution */
	float *portEnrouteMinProb; /* Probability of routing a packet minimal or non-m. for each outport - MINCOND */
	int *qcnPortFb;
        /* ACOR per SWITCH management variables */
        acorState acorSwStatus; /* Acor state for this router */
        int acor_hyst_cycles_counter; /* Hysteresis cycles duration counter */
        int acor_inc_state_th_packets;
        int acor_dec_state_th_packets;

	void sendCredits(int port, unsigned short cos, int channel, flitModule * flit);
	double calculateBaseLatency(const flitModule * flit);
	virtual void xbarTraversal(int input_port, unsigned short cos, int input_channel, int outP, int nextP, int nextC);
	void updateMisrouteCounters(int outP, flitModule * flitEx);
	void trackTransitStatistics(flitModule *flitEx, int input_channel, int outP, int nextC);
	virtual void printSwitchStatus();
	virtual void qcnOccupancySampling(int port);
	void qcnMinProbabilityDecrease(int port, float qcnfb);
	void qcnFeedbackComparison(int port, float qcnfb);

	/* Befriended functions to grant access to buffers & other variables. */
	friend void caHandler::update();
	friend void caHandler::readIncomingCAFlits();
	friend bool inputArbiter::checkPort();
	friend bool cosArbiter::portCanSendFlit(int port, unsigned short cos, int vc);
	friend void ageArbiter::reorderPortList();
	friend void priorityAgeArbiter::reorderPortList();
	friend void arbiter::reorderListQcn();

public:
	unsigned short cosLevels;
	caHandler m_ca_handler;
	baseRouting* routing;
	baseRouting* qcnRouting;
	inputArbiter **inputArbiters;
	outputArbiter **outputArbiters;
	int label, aPos, hPos;
	int messagesInQueuesCounter;
	bool escapeNetworkCongested;
	long long packetsInj;
	long long cnmPacketsInj;
	float *queueOccupancy;
	float *injectionQueueOccupancy;
	float *localQueueOccupancy;
	float *globalQueueOccupancy;
	float outputQueueOccupancy;
	float *localEscapeQueueOccupancy;
	float *globalEscapeQueueOccupancy;
	bool *reservedOutPort;
	bool **reservedInPort;
        int acor_packets_blocked_counter; /* Counter of ACOR packets blocked */

	switchModule(string name, int label, int aPos, int hPos, int ports, int vcCount);
	virtual ~switchModule();
	int getTotalCapacity();
	int getTotalFreeSpace();
	void insertFlit(int port, int vc, flitModule *flit);
	virtual int getCredits(int port, unsigned short cos, int channel);
	virtual int getCreditsOccupancy(int port, unsigned short cos, int channel, int buffer = 0);
	int getCreditsMinOccupancy(int port, unsigned short cos, int channel);
	int getPortCredits(int port, unsigned short cos, vector<int> vc_array);
	virtual bool checkConsumePort(int port, flitModule *flit);
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
	inline int getMaxCredits(int port, unsigned short cos, int vc) {
		return outPorts[port]->maxCredits[cos][vc];
	}
	virtual bool nextPortCanReceiveFlit(int port);
	flitModule * getFlit(int port, unsigned short cos, int vc);
	int getCurrentOutPort(int port, unsigned short cos, int vc);
	int getCurrentOutVC(int port, unsigned short cos, int vc);
	bool isVCUnlocked(flitModule * flit, int port, int vc);
	bool isGlobalLinkCongested(const flitModule * flit);

	void updateCredits();
	void updatePb();
	void updateReadPb();

	void resetQueueOccupancy();
	void setQueueOccupancy();
	void action();

	short int getPortEnrouteMinimalProbability(int port);

        void acorResetSwitchHysteresisStatus();
        acorState acorGetState();
        void acorIncState();
        void acorDecState();
};

#endif
