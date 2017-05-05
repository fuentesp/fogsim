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

#ifndef GRAPH500GENERATOR_H
#define GRAPH500GENERATOR_H

#include "generatorModule.h"

enum GraphCNState {
	ROOTNODE, /*			Initialization for root node, go directly to SENDP2PREQUEST */
	LEVELSTART, /*			Initialization phase, calculate edges and p2p messages to send */
	COMPUTEGENERATION, /*	Computing generation phase */
	SENDP2PREQUEST, /*		Inject point to point message, after, process received messages in COMPUTERECEPTION */
	COMPUTERECEPTION, /*	Compute the reception of messages, move to SENDP2PREQUEST, COMPUTEGENERATION or ALLREDUCE */
	COMPUTEP2PSIGNAL, /*	Generate the signal to notify the end of the tree level within node to other nodes */
	SENDP2PSIGNAL, /*		Actually dispatch the P2P end-of-tree signal */
	ALLREDUCEAGGREGATE, /*	All reduce aggregation phase */
	ALLREDUCEBROADCAST, /*	All reduce broadcast phase */
	LEVELEND /*				Level finished by this process */
};

class graph500Generator: public generatorModule {
protected:
	GraphCNState state;
	int *messagesToNode;
	long long p2pmessagesToSend;
	long long queriesToSend = 0;
	int receptionPendingCycles;
	long int totalMessagesToReceive;
	int endsignalRxCounter;
	int endSignalSentCounter;
	int allreduceRxCounter;
	float queryTime;
	int instance;
	void rootnode();
	void levelstart();
	void computegeneration();
	void sendp2prequest();
	void computereception();
	void computep2psignal();
	void sendp2psignal();
	void allreduceaggregate();
	void allreducebroadcast();
	void levelend() {
		assert(false);
	}

public:
	graph500Generator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
			switchModule *switchM);
	~graph500Generator();
	void action() override;
	flitModule* generateFlit(FlitType flitType = RESPONSE, int destId = -1) override;
	void consumeFlit(flitModule *flit, int input_port, int input_channel) override;
	GraphCNState getState();
	void setState(GraphCNState nstate);
	void notifyEndSignalFromNode(int node, int messages);
};
// Auxiliar function to calculate the number of messages to send in function of number of queries
long long calculateNumberOfMessages(long long q);
double proportionOfSendingNodes(long long q);
int numberOfEffectiveNodes(long long q);

#endif /* GRAPH500GENERATOR_H */
