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

#include "graph500Generator.h"
#include "algorithm"

graph500Generator::graph500Generator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
		switchModule *switchM) :
		generatorModule(interArrivalTime, name, sourceLabel, pPos, aPos, hPos, switchM) {
	assert(g_traffic == GRAPH500);
	assert(g_flit_size == g_packet_size); // Ensure Virtual Cut-Through
	/* Assign to each generator its associated process from the benchmark.
	 * Some nodes won't have any associated process and will stay idle
	 * during the whole simulation. */
	if (g_gen_2_trace_map.count(sourceLabel) == 0)
		this->instance = -1;
	else {
		TraceNodeId trace_node = g_gen_2_trace_map[sourceLabel];
		for (int inst = 0; inst < g_trace_instances[0]; inst++)
			if (g_trace_2_gen_map[0][trace_node.trace_node][inst] == sourceLabel) {
				this->instance = inst;
				break;
			}
		/* At level 0, only process hosting root vertex send messages */
		this->state = GraphCNState::COMPUTERECEPTION;
		if (sourceLabel == g_graph_root_node[this->instance]) // Root node starts in ROOTNODE state (mProc = root_degree))
			this->state = GraphCNState::ROOTNODE;
		this->messagesToNode = new int[g_number_generators];
		for (int i = 0; i < g_number_generators; i++) {
			this->messagesToNode[i] = 0;
		}
		this->flit = NULL; // Use this variable as flitPending for graph500 algorithm
		this->receptionPendingCycles = 0;
		this->totalMessagesToReceive = 0;
		// At state 0 all nodes only receive only 1 signal and assume its signal has been sent
		this->allreduceRxCounter = 0;
		this->endSignalSentCounter = g_trace_nodes[0];
		this->endsignalRxCounter = g_trace_nodes[0] - 2;
		this->p2pmessagesToSend = 0;
		this->queryTime = g_graph_query_time;
		//TODO: update this part to raise/lower inj prob for each node!
		//	if (find(g_graph_nodes_cap_mod.begin(), g_graph_nodes_cap_mod.end(), sourceLabel) != g_graph_nodes_cap_mod.end()) {
		//		setInjectionProbability(g_injection_probability * g_graph_cap_mod_factor / 100.0);
		//		queryTime /= g_graph_cap_mod_factor / 100.0;
		//		assert(injection_probability >= 0 && injection_probability <= 100);
		//		assert(queryTime > 0);
		//	}
	}
}

graph500Generator::~graph500Generator() {
	delete[] messagesToNode;
}

void graph500Generator::action() {
	/* Skip those nodes without a mapped process from the benchmark */
	if (instance == -1) return;

	bool endCycle = false;
	while (!endCycle) {
		switch (state) {
			case GraphCNState::ROOTNODE:
				rootnode();
				setState(GraphCNState::COMPUTEGENERATION);
				break;
			case GraphCNState::LEVELSTART:
				levelstart();
				if (p2pmessagesToSend > 0)
					setState(GraphCNState::COMPUTEGENERATION);
				else {
					endCycle = true;
					setState(GraphCNState::COMPUTEP2PSIGNAL);
				}
				break;
			case GraphCNState::COMPUTEGENERATION:
				computegeneration();
				if (flit == NULL)
					endCycle = true;
				else
					// pending flit -> try to send it
					setState(GraphCNState::SENDP2PREQUEST);
				break;
			case GraphCNState::SENDP2PREQUEST:
				if (flit->inCycle == g_cycle)
					endCycle = true;
				else
					setState(GraphCNState::COMPUTERECEPTION);
				sendp2prequest();
				if (p2pmessagesToSend == 0) {
					endCycle = true;
					setState(GraphCNState::COMPUTEP2PSIGNAL);
				} else if (flit == NULL) setState(GraphCNState::COMPUTERECEPTION);
				break;
			case GraphCNState::COMPUTERECEPTION:
				computereception();
				endCycle = true;
				if (p2pmessagesToSend > 0) {
					if (flit != NULL)
						setState(GraphCNState::SENDP2PREQUEST);
					else if (receptionPendingCycles < 0) setState(GraphCNState::COMPUTEGENERATION);
				} else if (endSignalSentCounter < g_trace_nodes[0]) {
					if (flit != NULL)
						setState(GraphCNState::SENDP2PSIGNAL);
					else
						/*if (receptionPendingCycles < 0)*/
						setState(GraphCNState::COMPUTEP2PSIGNAL);
				} else if (totalMessagesToReceive == 0 && endsignalRxCounter == g_trace_nodes[0] - 1)
				// all nodes end injection and all p2p mess. have been received
					setState(GraphCNState::ALLREDUCEAGGREGATE);
				// else : keep in computeReception (flit == NULL && receptionPendingCycles >= 0)
				break;
			case GraphCNState::COMPUTEP2PSIGNAL:
				computep2psignal();
				/* If flit is NULL, current node is last in the network
				 * and won't be dispatching any message; jump directly to
				 * the COMPUTERECEPTION state. */
				if (flit == NULL)
					setState(GraphCNState::COMPUTERECEPTION);
				else
					setState(GraphCNState::SENDP2PSIGNAL);
				break;
			case GraphCNState::SENDP2PSIGNAL:
				if (flit->inCycle == g_cycle)
					endCycle = true;
				else
					setState(GraphCNState::COMPUTERECEPTION);
				sendp2psignal();
				if (flit == NULL) setState(GraphCNState::COMPUTERECEPTION);
				break;
			case GraphCNState::ALLREDUCEAGGREGATE:
				if (g_graph_root_node[instance] != sourceLabel) allreduceaggregate();
				// else : rootNode only wait for messages - see consumeFlit())
				endCycle = true;
				if ((g_graph_root_node[instance] != sourceLabel && flit == NULL)
						|| (g_graph_root_node[instance] == sourceLabel
								&& allreduceRxCounter == g_trace_nodes[0] - 1)) {
					setState(GraphCNState::ALLREDUCEBROADCAST);
					allreduceRxCounter = 0;
				}
				/* else:
				 -  rootNode : continue receiving aggregate messages
				 - !rootNode : continue trying to send if flit != NULL */
				break;
			case GraphCNState::ALLREDUCEBROADCAST:
				/* Use allreduceRxCounter for:
				 *	- rootNode: count sent messages
				 *	- other: set to 1 if a message has been received from root node */
				if (g_graph_root_node[instance] == sourceLabel) // other nodes wait for message - see consumeFlit()
					allreducebroadcast();
				endCycle = true;
				if ((g_graph_root_node[instance] != sourceLabel && allreduceRxCounter == 1)
						|| (g_graph_root_node[instance] == sourceLabel
								&& allreduceRxCounter == g_trace_nodes[0])) setState(GraphCNState::LEVELEND);
				break;
			case GraphCNState::LEVELEND:
				endCycle = true;
				break;
			default:
				cerr << "Invalid state for graph500Generator" << endl;
				assert(false);
		}
	}
	assert((ULONG_MAX - sum_injection_probability) >= injection_probability);
	sum_injection_probability += injection_probability;
}

flitModule* graph500Generator::generateFlit(FlitType flitType, int destId) {
	flitModule * genFlit = NULL;

	if (destId >= 0) {
		assert(g_cycle == 0 || g_cycle >= lastTimeSent + interArrivalTime);
		assert(destId < g_number_generators && ((graph500Generator *) g_generators_list[destId])->instance == this->instance);
		destLabel = destId;
		destSwitch = int(destLabel / g_p_computing_nodes_per_router);
		genFlit = new flitModule(m_packet_id, g_tx_flit_counter, 0, sourceLabel, destLabel,
				destSwitch, 0, true, true);
		genFlit->channel = this->getInjectionVC(destLabel, flitType);
		genFlit->flitType = flitType;
	} else if (g_cycle == 0 || g_cycle >= lastTimeSent + g_packet_size) {
		if (rand() / ((double) RAND_MAX + 1) < ((double) injection_probability / 100.0)) {
			m_packet_id = g_tx_packet_counter;
			do
				destLabel = pattern->setDestination(UN);
			while (((graph500Generator *) g_generators_list[destLabel])->instance != this->instance);
			if (destLabel >= 0) m_injVC = this->getInjectionVC(destLabel, flitType);
			if (destLabel >= 0 && m_injVC >= 0) {
				destSwitch = int(destLabel / g_p_computing_nodes_per_router);
				genFlit = new flitModule(m_packet_id, g_tx_flit_counter, 0, sourceLabel, destLabel,
						destSwitch, 0, 0, 0);
				genFlit->channel = m_injVC;
				genFlit->flitType = flitType;
				genFlit->head = 1;
				genFlit->tail = 1;
			}
		}
	}

	return genFlit;
}

void graph500Generator::consumeFlit(flitModule *flit, int input_port, int input_channel) {
	assert(state != GraphCNState::ROOTNODE && state != GraphCNState::LEVELSTART);
	assert(instance != -1);
	assert(((graph500Generator *) g_generators_list[flit->destId])->instance == this->instance);

	if (flit->flitType == SIGNAL) {
		assert(state != GraphCNState::ALLREDUCEAGGREGATE && state != GraphCNState::ALLREDUCEBROADCAST);
		endsignalRxCounter++;
		assert(endsignalRxCounter <= g_trace_nodes[0]);
		assert(flit->graph_queries < 0);
	} else if (flit->flitType == ALLREDUCE) {
		assert(
				state == GraphCNState::ALLREDUCEBROADCAST
						|| (sourceLabel == g_graph_root_node[instance]
								&& (state == GraphCNState::COMPUTERECEPTION || state == GraphCNState::COMPUTEP2PSIGNAL
										|| state == GraphCNState::SENDP2PSIGNAL
										|| state == GraphCNState::ALLREDUCEAGGREGATE)));
		if (g_graph_root_node[instance] == sourceLabel) { /* Message for ALLREDUCEAGGREGATE state */
			allreduceRxCounter++;
			assert(flit->graph_queries < 0 && allreduceRxCounter < g_trace_nodes[0]);
		} else { /* Message for ALLREDUCEBROADCAST state */
			assert(flit->sourceId == g_graph_root_node[instance] && g_graph_root_node[instance] != sourceLabel);
			allreduceRxCounter = 1;
			assert(flit->graph_queries < 0);
		}
	} else { /* Point-to-point message */
		assert(state != GraphCNState::ALLREDUCEAGGREGATE && state != GraphCNState::ALLREDUCEBROADCAST);
		receptionPendingCycles = max(0, receptionPendingCycles);
		receptionPendingCycles += ceil(queryTime * flit->graph_queries);
		assert(flit->graph_queries > 0);
		g_graph_queries_remain[instance] -= flit->graph_queries;
		totalMessagesToReceive--; // messages to receive by this compute node
	}
	generatorModule::consumeFlit(flit, input_port, input_channel);
}

void graph500Generator::rootnode() {
	queriesToSend = ceil(g_graph_root_degree[instance] * (g_trace_nodes[0] - 1.0) / g_trace_nodes[0]);
	p2pmessagesToSend = max((long long) 1, calculateNumberOfMessages(queriesToSend));
	// Compute nodes does not notify itself; when it is done sending p2prequests
	endSignalSentCounter = 0;
	endsignalRxCounter++;
}

void graph500Generator::levelstart() {
	double A, B, C, D, F, G, H, J, K, mean, stddev, logrd;
	normal_distribution<float> normalDistr;
	// Reset variables by level
	for (int toNode = 0; toNode < g_number_generators; toNode++)
		messagesToNode[toNode] = 0;
	totalMessagesToReceive = 0;
	receptionPendingCycles = 0;
	allreduceRxCounter = 0;
	endsignalRxCounter = 0;
	endSignalSentCounter = 0;
	// Calculate auxiliar variables and mean and standard deviation
	A = -0.133 + 0.0046 * g_graph_scale;
	A += exp(0.01257 * g_graph_edgefactor - 0.1829 * g_graph_scale - exp(1.75 - 0.7 * (g_graph_tree_level[instance])));
	B = 2 - g_graph_tree_level[instance] * (0.91 + 0.002 * g_graph_edgefactor - 0.012 * g_graph_scale);
	C = exp(-(pow(g_graph_tree_level[instance] - 3.2, 2)) / 8);
	C = exp(1 + (1 + 0.004 * g_graph_edgefactor) * (-1.105 + 1.01 * log(g_graph_scale)) * C);
	D = 0.002 * g_graph_scale - 0.14
			- (0.015 * g_graph_scale - 0.285) * (0.56 + 0.033 * g_graph_edgefactor) * (g_graph_tree_level[instance] - 1);
	F = 0.97 - (g_graph_tree_level[instance] - 1) * (4.438 - 0.168 * g_graph_scale) * (0.83 + 0.01 * g_graph_edgefactor);
	G = ((2.1 + g_graph_tree_level[instance]) * (5.35 + 0.093 * g_graph_scale) - 13.625)
			* (1.23 - (2.9 + 0.69 * g_graph_tree_level[instance]) / g_graph_edgefactor);
	H = pow(2, g_graph_tree_level[instance]) * (0.011 + 0.00012 * g_graph_edgefactor) * exp(1.7 - g_graph_scale / 10);
	J = (1 - 0.012 * g_graph_edgefactor)
			* (9.55 - 1.3 * g_graph_tree_level[instance] - g_graph_scale * (0.6 - 0.1 * g_graph_tree_level[instance]));
	J = 1 - exp(J);
	K = 13 * (0.062 * g_graph_scale - 0.046) * (2 - 0.24 * g_graph_tree_level[instance]) - J;
	logrd = log(g_graph_root_degree[instance]);
	mean = exp(A * logrd * logrd + B * logrd + C);
	mean = min(mean, ceil((double) g_graph_queries_remain[instance] * g_trace_nodes[0] / (g_trace_nodes[0] - 1.0)));
	if (g_graph_tree_level[instance] <= 1)
		stddev = exp(D * logrd * logrd + F * logrd + G);
	else if (g_graph_tree_level[instance] <= 2) {
		stddev = exp(D * logrd * logrd + F * logrd + G);
		D = 0.002 * g_graph_scale - 0.14;
		F = 0.97;
		G = (3.1 * (5.35 + 0.093 * g_graph_scale) - 13.625) * (1.23 - 3.59 / g_graph_edgefactor);
		stddev = max(stddev, exp(D * logrd * logrd + F * logrd + G));
	} else if (g_graph_tree_level[instance] >= 4)
		stddev = exp(K / pow(g_graph_root_degree[instance], H) + J);
	else {
		D = 0.002 * g_graph_scale - 0.14 - (0.015 * g_graph_scale - 0.285) * (0.56 + 0.033 * g_graph_edgefactor);
		F = 0.97 - (4.438 - 0.168 * g_graph_scale) * (0.83 + 0.01 * g_graph_edgefactor);
		G = (4.1 * (5.35 + 0.093 * g_graph_scale) - 13.625) * (1.23 - 4.28 / g_graph_edgefactor);
		H = 16 * (0.011 + 0.00012 * g_graph_edgefactor) * exp(1.7 - g_graph_scale / 10);
		J = 1 - exp((1 - 0.012 * g_graph_edgefactor) * (4.35 - 0.2 * g_graph_scale));
		K = 13.52 * (0.062 * g_graph_scale - 0.046) - J;
		stddev = max(exp(D * logrd * logrd + F * logrd + G), exp(K / pow(g_graph_root_degree[instance], H) + J));
	}
	normalDistr = normal_distribution<float>(mean, stddev);
	// Calculate p2pmessages to send into this stage
	do {
		queriesToSend = normalDistr(g_reng) * (g_trace_nodes[0] - 1.0) / g_trace_nodes[0];
	} while (queriesToSend < 0);
	if (queriesToSend + g_graph_queries_remain[instance] - g_graph_queries_rem_minus_means[instance] > 0)
		queriesToSend += g_graph_queries_remain[instance] - g_graph_queries_rem_minus_means[instance];
	assert(queriesToSend >= 0);
	/* Determine whether a node will inject or not. This prevents granularity errors
	 * when the number of queries is much lower than the size of the network. */
	if ((double) rand() / RAND_MAX <= proportionOfSendingNodes(queriesToSend))
		queriesToSend /= numberOfEffectiveNodes(queriesToSend);
	else
		queriesToSend = 0;

	if (queriesToSend > 0)
		p2pmessagesToSend = calculateNumberOfMessages(queriesToSend);
	else
		p2pmessagesToSend = 0;
	// Ensure fully populated p2pmessagesToSend are enought for allocating the number of queriesToSend
	assert(p2pmessagesToSend * g_graph_coalescing_size >= queriesToSend);
	assert(p2pmessagesToSend >= 0);
	// TODO: fix with static variables (A, B, ...) shared by all instances and function to calculate them only once
	if (sourceLabel == g_trace_2_gen_map[0][g_trace_nodes[0] - 1][instance]) g_graph_queries_rem_minus_means[instance] -= mean;
}

void graph500Generator::computegeneration() {
	assert(flit == NULL);
	flit = generateFlit();
	if (flit != NULL) {
		assert(flit->destId != this->sourceLabel);
		/* Only try to fill up messages when remaining queries are bigger than
		 * message aggregation and the number of remaining messages; this
		 * prevents consuming too many queries on a message when the number of
		 * remaining queries is low and there are more messages within whose to
		 * spread the queries. */
		if (queriesToSend >= g_graph_coalescing_size + p2pmessagesToSend - 1)
			flit->graph_queries = g_graph_coalescing_size;
		else
			flit->graph_queries = ceil((double) queriesToSend / p2pmessagesToSend);
		// Ensure last message queries is equal to remaining queries in this level
		if (p2pmessagesToSend == 1)
		assert(flit->graph_queries == queriesToSend);
		assert(flit->graph_queries > 0);
		flit->inCycle = g_cycle;
	}
}

void graph500Generator::sendp2prequest() {
	assert(flit != NULL);
	assert(flit->destId >= 0 && flit->destId < g_number_generators);
	assert(((graph500Generator *) g_generators_list[flit->destId])->instance == this->instance);
	assert(flit->sourceId == this->sourceLabel);

	switchM->routing->setValNode(flit);
	this->determinePaths(flit);

	if (switchM->switchModule::getCredits(this->pPos, 0, flit->channel) >= g_flit_size) {
		switchM->injectFlit(this->pPos, flit->channel, flit);
		switchM->packetsInj++;
		lastTimeSent = g_cycle;
		flit->inCycle = g_cycle;
		g_tx_packet_counter++;
		flit->inCyclePacket = g_cycle;
		m_packet_in_cycle = flit->inCyclePacket;
		messagesToNode[flit->destId]++;
#if DEBUG	/* These statistics are not computed in a release compilation because they eat too much memory */
		assert(LLONG_MAX - g_graph_p2pmess_node2node[g_graph_tree_level][sourceLabel][flit->destId] >= 1);
		g_graph_p2pmess_node2node[g_graph_tree_level][sourceLabel][flit->destId]++;
#endif
		p2pmessagesToSend--;
		queriesToSend -= flit->graph_queries;
		g_graph_p2pmess++;
		flit = NULL;
	}
}

void graph500Generator::computep2psignal() {
	assert(flit == NULL);
	assert(p2pmessagesToSend == 0);

	if (g_trace_2_gen_map[0][endSignalSentCounter][instance] == sourceLabel) endSignalSentCounter++;
	/* If skipped sender is last node in the network, don't try to
	 * send a signal to the next (there's none). */
	if (endSignalSentCounter < g_trace_nodes[0]) flit = generateFlit(SIGNAL, g_trace_2_gen_map[0][endSignalSentCounter][instance]);
}

void graph500Generator::sendp2psignal() {
	assert(p2pmessagesToSend == 0);
	assert(endSignalSentCounter < g_trace_nodes[0]);
	assert(flit != NULL);

	assert(flit->destId == g_trace_2_gen_map[0][endSignalSentCounter][instance]);
	switchM->routing->setValNode(flit);
	this->determinePaths(flit);
	if (switchM->switchModule::getCredits(this->pPos, 0, flit->channel) >= g_flit_size) {
		switchM->injectFlit(this->pPos, flit->channel, flit);
		switchM->packetsInj++;
		lastTimeSent = g_cycle;
		flit->inCycle = g_cycle;
		g_tx_packet_counter++;
		flit->inCyclePacket = g_cycle;
		m_packet_in_cycle = flit->inCyclePacket;
		((graph500Generator *) g_generators_list[flit->destId])->notifyEndSignalFromNode(this->sourceLabel,
				messagesToNode[flit->destId]);
		endSignalSentCounter++;
		flit = NULL;
	}
}

void graph500Generator::computereception() {
	receptionPendingCycles--;
}

void graph500Generator::allreduceaggregate() {
	assert(g_graph_root_node[instance] != sourceLabel);
	if (flit == NULL) // Generate flit to root node
		flit = generateFlit(ALLREDUCE, g_graph_root_node[instance]);

	if (flit != NULL) { // Try to send
		assert(flit->destId == g_graph_root_node[instance]);
		switchM->routing->setValNode(flit);
		this->determinePaths(flit);
		if (switchM->switchModule::getCredits(this->pPos, 0, flit->channel) >= g_flit_size) {
			switchM->injectFlit(this->pPos, flit->channel, flit);
			switchM->packetsInj++;
			lastTimeSent = g_cycle;
			flit->inCycle = g_cycle;
			g_tx_packet_counter++;
			flit->inCyclePacket = g_cycle;
			m_packet_in_cycle = flit->inCyclePacket;
			flit = NULL;
		}
	}
}

void graph500Generator::allreducebroadcast() {
	assert(g_graph_root_node[instance] == sourceLabel);
	assert(allreduceRxCounter < g_trace_nodes[0]);

	if (flit == NULL) {
		if (g_trace_2_gen_map[0][allreduceRxCounter][instance] != sourceLabel)
			flit = generateFlit(ALLREDUCE, g_trace_2_gen_map[0][allreduceRxCounter][instance]);
		else
			allreduceRxCounter++;
	}

	if (flit != NULL) {
		assert(flit->destId == g_trace_2_gen_map[0][allreduceRxCounter][instance]);
		switchM->routing->setValNode(flit);
		this->determinePaths(flit);
		if (switchM->switchModule::getCredits(this->pPos, 0, flit->channel) >= g_flit_size) {
			switchM->injectFlit(this->pPos, flit->channel, flit);
			switchM->packetsInj++;
			lastTimeSent = g_cycle;
			flit->inCycle = g_cycle;
			g_tx_packet_counter++;
			flit->inCyclePacket = g_cycle;
			m_packet_in_cycle = flit->inCyclePacket;
			flit = NULL;
			allreduceRxCounter++;
		}
	}
}

GraphCNState graph500Generator::getState() {
	return this->state;
}

void graph500Generator::setState(GraphCNState nstate) {
	assert(nstate != GraphCNState::ROOTNODE);
	assert(
			(this->state == GraphCNState::LEVELSTART
					&& (nstate == GraphCNState::COMPUTEGENERATION || nstate == GraphCNState::COMPUTEP2PSIGNAL))
					|| (this->state == GraphCNState::ROOTNODE && nstate == GraphCNState::COMPUTEGENERATION)
					|| (this->state == GraphCNState::COMPUTEGENERATION
							&& (nstate == GraphCNState::COMPUTEGENERATION || nstate == GraphCNState::SENDP2PREQUEST))
					|| (this->state == GraphCNState::SENDP2PREQUEST
							&& (nstate == GraphCNState::COMPUTERECEPTION || nstate == GraphCNState::COMPUTEP2PSIGNAL))
					|| (this->state == GraphCNState::COMPUTEP2PSIGNAL
							&& (nstate == SENDP2PSIGNAL || nstate == COMPUTERECEPTION))
					|| (this->state == GraphCNState::SENDP2PSIGNAL && nstate == GraphCNState::COMPUTERECEPTION)
					|| (this->state == GraphCNState::COMPUTERECEPTION
							&& (nstate == GraphCNState::COMPUTERECEPTION || nstate == GraphCNState::SENDP2PREQUEST
									|| nstate == GraphCNState::COMPUTEGENERATION || nstate == COMPUTEP2PSIGNAL
									|| nstate == GraphCNState::SENDP2PSIGNAL
									|| nstate == GraphCNState::ALLREDUCEAGGREGATE))
					|| (this->state == GraphCNState::ALLREDUCEAGGREGATE
							&& (nstate == GraphCNState::ALLREDUCEAGGREGATE || nstate == GraphCNState::ALLREDUCEBROADCAST))
					|| (this->state == GraphCNState::ALLREDUCEBROADCAST
							&& (nstate == GraphCNState::ALLREDUCEBROADCAST || nstate == GraphCNState::LEVELEND))
					|| (this->state == GraphCNState::LEVELEND && nstate == GraphCNState::LEVELSTART));
	this->state = nstate;
}

void graph500Generator::notifyEndSignalFromNode(int node, int messages) {
	totalMessagesToReceive += messages;
}

long long calculateNumberOfMessages(long long q) {
	int n;
	long long nmess;
	n = numberOfEffectiveNodes(q);
	nmess = n * ceil((double) q / (n * g_graph_coalescing_size));

	return nmess;
}

double proportionOfSendingNodes(long long q) {
	return 1 - pow(1 - 1.0 / g_trace_nodes[0], q);
}

int numberOfEffectiveNodes(long long q) {
	return max(1.0, g_trace_nodes[0] * proportionOfSendingNodes(q));
}
