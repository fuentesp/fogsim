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

#include "ioqSwitchModule.h"
#include <iomanip>

ioqSwitchModule::ioqSwitchModule(string name, int label, int aPos, int hPos, int ports, int vcCount) :
		switchModule(name, label, aPos, hPos, ports, vcCount) {
	this->m_speedup_interval = (long double) (1) / (long double) g_internal_speedup;
	this->m_internal_cycle = 0;

	int p, vc, cos;

	for (p = 0; p < g_global_router_links_offset + g_h_global_ports_per_router; p++)
		delete outPorts[p];
	switch (g_buffer_type) {
		case SEPARATED:
			for (p = 0; p < g_local_router_links_offset; p++)
				outPorts[p] = new bufferedOutPort(this->cosLevels, g_injection_channels, p, p + portCount * vcCount,
						g_out_queue_length, g_xbar_delay, this, 0, g_segregated_flows);
			for (p = g_local_router_links_offset; p < g_global_router_links_offset; p++)
				outPorts[p] = new bufferedOutPort(this->cosLevels, g_local_link_channels, p, p + portCount * vcCount,
						g_out_queue_length, g_xbar_delay, this);
			for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router; p++)
				outPorts[p] = new bufferedOutPort(this->cosLevels, g_global_link_channels, p, p + portCount * vcCount,
						g_out_queue_length, g_xbar_delay, this);
			break;

		case DYNAMIC:
			for (p = 0; p < g_local_router_links_offset; p++)
				outPorts[p] = new bufferedOutPort(this->cosLevels, g_injection_channels, p, p + portCount * vcCount,
						g_out_queue_length, g_xbar_delay, this, 0, g_segregated_flows);
			for (p = g_local_router_links_offset; p < g_global_router_links_offset; p++)
				outPorts[p] = new dynamicBufferBufferedOutPort(this->cosLevels, g_local_link_channels, p,
						p + portCount * vcCount, g_out_queue_length, g_xbar_delay, this, g_local_queue_reserved);
			for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router; p++)
				outPorts[p] = new dynamicBufferBufferedOutPort(this->cosLevels, g_global_link_channels, p,
						p + portCount * vcCount, g_out_queue_length, g_xbar_delay, this, g_global_queue_reserved);
			break;
	}

	switch (g_vc_usage) {
		case BASE:
			for (p = 0; p < g_local_router_links_offset; p++)
				for (vc = 0; vc < g_injection_channels; vc++)
					for (cos = 0; cos < this->cosLevels; cos++)
						outPorts[p]->setMaxOutOccupancy(cos, vc, g_out_queue_length);

			for (p = g_local_router_links_offset; p < g_global_router_links_offset; p++)
				for (vc = 0; vc < g_local_link_channels; vc++)
					for (cos = 0; cos < this->cosLevels; cos++)
						outPorts[p]->setMaxOutOccupancy(cos, vc, g_out_queue_length);

			for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router; p++)
				for (vc = 0; vc < g_global_link_channels; vc++)
					for (cos = 0; cos < this->cosLevels; cos++)
						outPorts[p]->setMaxOutOccupancy(cos, vc, g_out_queue_length);
			break;
		case TBFLEX:
		case FLEXIBLE:
			for (p = 0; p < this->portCount; p++)
				for (vc = 0; vc < vcCount; vc++)
					for (cos = 0; cos < this->cosLevels; cos++)
						outPorts[p]->setMaxOutOccupancy(cos, vc, g_out_queue_length);
			break;
	}
	lastCheckedOutBuffer = new int[this->portCount];
	for (int i = 0; i < this->portCount; i++)
		lastCheckedOutBuffer[i] = 0;
}

ioqSwitchModule::~ioqSwitchModule() {
	delete[] lastCheckedOutBuffer;
}

void ioqSwitchModule::action() {
	int port, vc;
	g_internal_cycle = g_cycle;

	switchModule::action();
	for (port = 0; port < this->portCount; port++)
		updateOutputBuffer(port);
	for (port = 0; port < this->portCount; port++)
		outPorts[port]->reorderBuffer(0);

	float oqo = 0;
	for (int p = 0; p < g_p_computing_nodes_per_router; p++)
		oqo += this->outPorts[p]->getBufferOccupancy(0);
	this->outputQueueOccupancy += oqo / g_p_computing_nodes_per_router;
}

void ioqSwitchModule::updateOutputBuffer(int port) {
	assert(port >= 0 && port < this->portCount);
	flitModule *flit = NULL;

	int num_buffers = (port < g_p_computing_nodes_per_router) ? g_segregated_flows : 1;
	int outBuffer = 0;
	for (int aux = 0; aux < num_buffers; aux++) {
		outBuffer = (lastCheckedOutBuffer[port] + aux + 1) % num_buffers;
		if (outPorts[port]->canSendFlit(0, 0, outBuffer)) {
			lastCheckedOutBuffer[port] = outBuffer;
			outPorts[port]->checkFlit(cosOutPort, 0, flit, outBuffer);
			if (flit != NULL) {
				int nextP, nextC, inP, inC;
				nextC = flit->channel;
				nextP = routing->neighPort[port];
				inP = flit->prevP;
				inC = flit->prevVC;

				if (port < g_p_computing_nodes_per_router) {
					if (g_generators_list[this->label * g_p_computing_nodes_per_router + port]->checkConsume(flit)) {
						/* Pkt is consumed */
#if DEBUG
						cout << "Tx pkt " << flit->flitId << " from source " << flit->sourceId << " (sw " << flit->sourceSW
						<< ", group " << flit->sourceGroup << ") to dest " << flit->destId << " (sw "
						<< flit->destSwitch << ", group " << flit->destGroup << "). Consuming pkt in node " << port
						<< endl;
#endif
						outPorts[port]->extract(cosOutPort, nextC, flit, g_flit_size, outBuffer);

						/* Update contention counters (time waiting in queues) */
						flit->addContention(inP, this->label);

						g_generators_list[this->label * g_p_computing_nodes_per_router + port]->consumeFlit(flit, inP,
								inC);
					}
				} else {
					bool nextVC_unLocked, input_phy_ring, output_phy_ring, input_emb_escape, output_emb_escape,
							subnetworkInjection;

					if (this->switchModule::nextPortCanReceiveFlit(port)) {
						assert(switchModule::getCredits(port, flit->cos, nextC) >= g_flit_size);
						flit->setChannel(nextC);
						if (inP < g_p_computing_nodes_per_router
								|| (g_congestion_management == QCNSW && inP == g_qcn_port)) {
							assert(flit->injLatency < 0);
							flit->injLatency = g_internal_cycle - flit->inCycle;
							assert(flit->injLatency >= 0);
						}

						/* Update contention counters (time waiting in queues) */
						flit->addContention(inP, this->label);

						switch (g_deadlock_avoidance) {
							case RING:
								input_phy_ring = inP >= (this->portCount - 2);
								output_phy_ring = nextP >= (this->portCount - 2);
								subnetworkInjection =
										(port >= this->portCount - 2)
												&& !((port == this->portCount - 2) && (this->aPos == 0))
												&& !((port == this->portCount - 1)
														&& (this->aPos == g_a_routers_per_group - 1));
								break;
							case EMBEDDED_RING:
							case EMBEDDED_TREE:
								input_emb_escape = g_local_link_channels <= inC
										&& inC < (g_local_link_channels + g_channels_escape);
								output_emb_escape = g_local_link_channels <= nextC
										&& nextC < (g_local_link_channels + g_channels_escape);
								subnetworkInjection = output_emb_escape && !input_emb_escape;
								break;
							default:
								input_phy_ring = output_phy_ring = input_emb_escape = output_emb_escape =
										subnetworkInjection = false;
								break;
						}
						if (subnetworkInjection) {
							if (this->hPos == g_tree_root_switch) flit->rootSubnetworkInjectionsCount++;
							if (this->hPos == flit->sourceGroup) flit->sourceSubnetworkInjectionsCount++;
							if (this->hPos == flit->destGroup) flit->destSubnetworkInjectionsCount++;
							flit->addSubnetworkInjection();
						}

						/* Update various misrouting flags */
						if (flit->head == 1) {
							MisrouteType misroute_type = flit->getCurrentMisrouteType();
							/* Local misrouting */
							if (misroute_type == LOCAL || misroute_type == LOCAL_MM) {
								assert(port >= g_local_router_links_offset);
								assert(port < g_global_router_links_offset);
								assert(not (output_emb_escape));
								flit->setMisrouted(true, misroute_type);
								flit->localMisroutingDone = 1;
							}
							/* Global misrouting */
							if (misroute_type == GLOBAL || misroute_type == GLOBAL_MANDATORY) {
								assert(port >= g_global_router_links_offset);
								assert(port < g_global_router_links_offset + g_h_global_ports_per_router);
								assert(not (output_emb_escape));
								flit->setMisrouted(true, misroute_type);
								flit->globalMisroutingDone = 1;

								/* Global misrouting at injection */
								if (inP < g_p_computing_nodes_per_router) {
									assert(misroute_type == GLOBAL);
									flit->setGlobalMisrouteAtInjection(true);
								}
							}
							/* Mandatory global misrouting asserts */
							if (flit->mandatoryGlobalMisrouting_flag && not (input_phy_ring) && not (output_phy_ring)
									&& not (input_emb_escape) && not (output_emb_escape)) {
								assert(flit->globalMisroutingDone);
								assert(port >= g_global_router_links_offset);
								assert(port < g_global_router_links_offset + g_h_global_ports_per_router);
								assert(misroute_type == GLOBAL_MANDATORY);
							}

							/* If global misrouting policy is MM (+L), and current group is source group and different
							 * from destination group, if no previous misrouting has been conducted mark flag for
							 * global mandatory misrouting (to prevent the appearence of cycles). We also need to test
							 * flit is not transitting through subescape network. */
							switch (g_global_misrouting) {
								case MM:
								case MM_L:
									if ((inP >= g_local_router_links_offset) && (inP < g_global_router_links_offset)
											&& (port >= g_local_router_links_offset)
											&& (port < g_global_router_links_offset)
											&& (this->hPos == flit->sourceGroup) && (this->hPos != flit->destGroup)
											&& not (flit->globalMisroutingDone) && not (output_emb_escape)
											&& not (input_emb_escape)
											&& not (flit->getCurrentMisrouteType() == VALIANT)) {
										flit->mandatoryGlobalMisrouting_flag = 1;
#if DEBUG
										if (port == routing->minOutputPort(flit->destId))
										cerr << "ERROR in sw " << this->label << " with flit " << flit->flitId
										<< " from src " << flit->sourceId << " to dest " << flit->destId
										<< " traversing from inPort " << inP << " to outPort " << port << endl;
#endif
										assert(port != routing->minOutputPort(flit->destId));
									}
									break;
								case NRG:
								case NRG_L:
								case RRG:
								case RRG_L:
									if (port != routing->minOutputPort(flit->destId)
											&& inP < g_global_router_links_offset && port >= g_local_router_links_offset
											&& (port < g_global_router_links_offset)
											&& (this->hPos == flit->sourceGroup) && (this->hPos != flit->destGroup)
											&& not (flit->globalMisroutingDone) && not (output_emb_escape)
											&& not (input_emb_escape)
											&& not (flit->getCurrentMisrouteType() == VALIANT)) {
										flit->mandatoryGlobalMisrouting_flag = 1;
									}
							}

							/* RESET some flags when changing group */
							if (hPos != routing->neighList[port]->hPos) {
								flit->mandatoryGlobalMisrouting_flag = 0;
								flit->localMisroutingDone = 0;
							}

						}
						if (flit->getMisrouted() && !flit->getPrevMisrouted()) {
							if (inP < g_p_computing_nodes_per_router)
								g_nonminimal_inj++;
							else if (this->hPos == flit->sourceGroup)
								g_nonminimal_src++;
							else {
								g_nonminimal_int++;
							}
						}
						/* Pkt is sent */
						outPorts[port]->extract(cosOutPort, nextC, flit, g_flit_size);
						routing->neighList[port]->insertFlit(nextP, nextC, flit);
						trackTransitStatistics(flit, inP, port, nextC);
					}
				}
			}
		}
	}
}

/*
 * In Input-Output queued router model, flits are sent through the
 * crossbar to the output buffers, and then sent to the next switch
 * incoming buffer. FIXME! this should be merged with switchModule::insertFlit, noting
 * if buffer is at input or output.
 */
void ioqSwitchModule::txFlit(int port, flitModule * flit) {
	assert(port < portCount);

	if (g_segregated_flows > 1 && port < g_p_computing_nodes_per_router && flit->flitType == RESPONSE)
		assert(outPorts[port]->getSpace(flit->channel, 1) >= g_flit_size);
	else
		assert(outPorts[port]->getSpace(flit->channel) >= g_flit_size);
	/* Confirm there will be enough space to hold flit when sent to next sw */
	assert(
			this->switchModule::getCredits(port, flit->cos, flit->channel) >= g_flit_size
					|| port < g_p_computing_nodes_per_router);

	/* Calculate transmission length: max of xbar tx length, and input buffer remaining receive time */
	int inP = flit->prevP, inVC = flit->prevVC;
	double length = (double) g_flit_size / (double) g_internal_speedup;
	length = max(length, inPorts[inP]->getHeadEntryCycle(flit->cos, inVC) + g_flit_size - (double) g_internal_cycle);
	assert(length <= g_flit_size);

	if (port < g_p_computing_nodes_per_router && g_segregated_flows > 1 && flit->flitType == RESPONSE)
		outPorts[port]->insert(flit->channel, flit, length, 1);
	else
		outPorts[port]->insert(flit->channel, flit, length);
	return;
}

/*
 * Makes effective flit transferal from input to output buffer.
 */
void ioqSwitchModule::xbarTraversal(int input_port, unsigned short cos, int input_channel, int outP, int nextP,
		int nextC) {
	flitModule *flitEx;
	bool nextVC_unLocked, input_emb_escape = false, output_emb_escape = false, input_phy_ring = false, output_phy_ring =
			false, subnetworkInjection = false;

	assert(nextC >= 0 && nextC < vcCount);

	inPorts[input_port]->checkFlit(cos, input_channel, flitEx);
	assert(inPorts[input_port]->canSendFlit(cos, input_channel));

	/* Calculate transmission length: max of xbar tx length, and input buffer remaining receive time */
	double length = (double) g_flit_size / (double) g_internal_speedup;
	length = max(length,
			inPorts[input_port]->getHeadEntryCycle(cos, input_channel) - (double) g_internal_cycle
					+ (double) g_flit_size);
#if DEBUG
	if (length > g_flit_size) cerr << "ERROR: length "<<length<<" > flit size "<<g_flit_size << ", speedup "
	<< g_internal_speedup << ", entry cycle " << inPorts[input_port]->getHeadEntryCycle(input_channel)
	<< ", current cycle " << g_internal_cycle << "; xbar tx length " << (double) g_flit_size / (double) g_internal_speedup
	<< ", fwd tx length " << inPorts[input_port]->getHeadEntryCycle(input_channel) + g_flit_size - (double) g_internal_cycle
	<< endl;
#endif
	assert(length <= g_flit_size);

	assert(inPorts[input_port]->extract(cos, input_channel, flitEx, length));

	/* Contention-Aware misrouting trigger notification */
	if (g_contention_aware && g_increaseContentionAtHeader) {
		m_ca_handler.flitSent(flitEx, input_port, length);
	}

	if (g_contention_aware && (!g_increaseContentionAtHeader)) {
		m_ca_handler.decreaseContention(routing->minOutputPort(flitEx->destId));
	}

	if (flitEx->head == 1) {
		inPorts[input_port]->setCurPkt(cos, input_channel, flitEx->packetId);
		inPorts[input_port]->setOutCurPkt(cos, input_channel, outP);
		inPorts[input_port]->setNextVcCurPkt(cos, input_channel, nextC);
	}
	assert(flitEx->packetId == inPorts[input_port]->getCurPkt(cos, input_channel));
	if (flitEx->tail == 1) {
		inPorts[input_port]->setCurPkt(cos, input_channel, -1);
		inPorts[input_port]->setOutCurPkt(cos, input_channel, -1);
		inPorts[input_port]->setNextVcCurPkt(cos, input_channel, -1);
	}
	assert(flitEx != NULL);

	/* Decrement credit counter in local switch and send credits to previous switch. */
	sendCredits(input_port, cos, input_channel, flitEx);

	/* Pkt is commuted from input to output buffer */
	if (g_segregated_flows > 1 && outP < g_p_computing_nodes_per_router && flitEx->flitType == RESPONSE)
		assert(outPorts[outP]->getSpace(0, 1) >= g_flit_size);
	else
		assert(outPorts[outP]->getSpace(0) >= g_flit_size);

	flitEx->setChannel(nextC);
	flitEx->prevP = input_port;
	flitEx->prevVC = input_channel;

	updateMisrouteCounters(outP, flitEx);

	txFlit(outP, flitEx);
	if (g_congestion_management == QCNSW) this->qcnRpTxBCount[outP] -= g_flit_size;
}

/*
 * Interface to output port function to determine if port
 * can receive flit (e.g., has ended receiving previous
 * flit).
 */
bool ioqSwitchModule::nextPortCanReceiveFlit(int port) {
	return outPorts[port]->canReceiveFlit(0);
}

int ioqSwitchModule::getCredits(int port, unsigned short cos, int channel) {
	int crdts;

	assert(port >= 0 && port < portCount);
	assert(cos >= 0 && cos < cosLevels);
	assert(channel >= 0 && channel < vcCount);
	assert(port >= g_p_computing_nodes_per_router); /* This function should only be called for transit ports */

	/* This version just returns output buffer occupancy */
	crdts = outPorts[port]->maxCredits[cos][channel] - outPorts[port]->getTotalOccupancy(cos, channel);
	crdts = min(crdts, outPorts[port]->getSpace(channel));

	return crdts;
}

/*
 * Returns the number of used FLITS in the next buffer
 * Transit port: based on the local credit count in transit
 * Injection port: based on local buffer real occupancy
 */
int ioqSwitchModule::getCreditsOccupancy(int port, unsigned short cos, int channel, int buffer) {
	int occupancy;

	assert(port >= 0 && port < portCount);
	assert(cos >= 0 && cos < cosLevels);
	assert(channel >= 0 && channel < vcCount);

	occupancy = outPorts[port]->getBufferOccupancy(channel, buffer);
	if (port >= g_p_computing_nodes_per_router) {
		occupancy = outPorts[port]->getTotalOccupancy(cos, channel);
	}
	return occupancy;
}

/*
 * Check that destination node can generate/insert a response
 * flit in order to consume the petition.
 */
bool ioqSwitchModule::checkConsumePort(int port, flitModule* flit) {
	assert(port >= 0 && port < g_p_computing_nodes_per_router);
	bool portReady = true;
	if (g_segregated_flows == 1 && flit->flitType == PETITION) {
		vector<int> vcArray = g_generators_list[flit->destId]->getArrayInjVC(flit->sourceId, true);
		portReady = (outPorts[port]->getNumPetitions() + 1) * g_flit_size
				<= this->switchModule::getPortCredits(port, flit->cos, vcArray);
	}

	if (g_segregated_flows > 1 && flit->flitType == RESPONSE) {
		portReady = outPorts[port]->getSpace(flit->channel, 1) >= g_flit_size;
	} else {
		portReady &= outPorts[port]->getSpace(flit->channel) >= g_flit_size;
	}
	return portReady;
}

void ioqSwitchModule::printSwitchStatus() {
	int p, occ, f;
	flitModule *fl;

	this->switchModule::printSwitchStatus();

	for (p = 0; p < g_global_router_links_offset + g_h_global_ports_per_router; p++) {
		occ = outPorts[p]->getBufferOccupancy(0, 0);
		if (occ != 0) {
			cerr << "Out " << setfill(' ') << setw(2) << p << " - occup " << setfill(' ') << setw(3) << occ << ": ";
			for (f = occ / g_flit_size - 1; f >= 0; f--) {
				outPorts[p]->checkFlit(0, 0, fl, 0, f);
				cerr << "Dst " << setfill(' ') << setw(5) << fl->destId << " (gr " << setfill(' ') << setw(3)
						<< fl->destGroup << ") vc " << setw(2) << fl->channel << " |";
			}
			cerr << endl;
		}
	}
	cerr << "=========================================================================" << endl;
}

/*
 * Occupancy Sampling for QCN on output buffer. This process is part of QCN Congestion Point
 */
void ioqSwitchModule::qcnOccupancySampling(int port) {
	// if qcn implementation is OUT (sampling in output queues)
	if (g_qcn_implementation == QCNSWOUT || g_qcn_implementation == QCNSWOUTFBCOMP) {
		assert(g_congestion_management == QCNSW);
		assert(
				port >= g_p_computing_nodes_per_router
						&& port < g_global_router_links_offset + g_h_global_ports_per_router);
		assert(this->qcnCpSamplingCounter[port] <= 0);

		short qcnFb;
		int qcnQlen = 0, qcnQoff, qcnQdelta;
		int maxQcnFb = g_qcn_q_eq * (2 * g_qcn_w + 1);
		int portOccupancy = 0;
		bool culpritFlitAvail = false;
		flitModule *culpritFlit = NULL;

		/* Calculate occupancy of port in phits */
		qcnQlen = outPorts[port]->getBufferOccupancy(0, 0); /* phits */
		/* Calculate portOccupancy as a number of packets */
		portOccupancy = qcnQlen / g_flit_size; /* flits - packets */

		/* Calculation of QCN feedback value */
		qcnQoff = qcnQlen - g_qcn_q_eq;
		qcnQdelta = qcnQlen - qcnQlenOld[port];
		qcnFb = qcnQoff + g_qcn_w * qcnQdelta;
		if (qcnFb > maxQcnFb)
			qcnFb = maxQcnFb;
		else if (qcnFb < 0) qcnFb = 0;
		qcnFb = qcnFb * 63 / maxQcnFb; /* Quantify to six bits */
		assert(qcnFb >= 0 && qcnFb <= 63);

		for (int pktIdx = 0; !culpritFlitAvail && pktIdx < portOccupancy; pktIdx++) {
			outPorts[port]->checkFlit(0, 0, culpritFlit, 0, pktIdx);
			if (culpritFlit != NULL && culpritFlit->flitType != CNM) culpritFlitAvail = true;
		}

		if (culpritFlitAvail) {
			if (qcnFb > 0 and ((rand() % 100 + 1) <= g_qcn_cnms_percent)) {
				/* If feedback is possitive and only send g_qcn_cnms_percent% of CNMs */
				flitModule *cnmFlit;
				assert(culpritFlit != NULL && culpritFlit->flitType != CNM);
				/* Generate Congestion Nofitication Message (this messages are not accounted as data traffic) */
				cnmFlit = new flitModule(g_tx_cnmFlit_counter, g_tx_cnmFlit_counter, 0,
						this->label * g_p_computing_nodes_per_router, culpritFlit->sourceId,
						(int) (culpritFlit->sourceId / g_p_computing_nodes_per_router), 0, true, true, g_cos_levels - 1,
						CNM);
				cnmFlit->channel = g_generators_list[this->label * g_p_computing_nodes_per_router]->getInjectionVC(
						culpritFlit->sourceId, CNM);
				cnmFlit->setQcnParameters(qcnFb, qcnQoff, qcnQdelta);
				cnmFlit->inCycle = g_cycle;
				cnmFlit->inCyclePacket = g_cycle;
				if (cnmFlit->destSwitch != this->label)
					if (this->switchModule::getCredits(g_qcn_port, g_cos_levels - 1, cnmFlit->channel) >= g_flit_size) { // there are space in qcnPort
						injectFlit(g_qcn_port, cnmFlit->channel, cnmFlit);
						if (g_cycle >= g_warmup_cycles) this->cnmPacketsInj++;
					} else
						delete cnmFlit;
				else {
					int outP = this->routing->minOutputPort(culpritFlit->destId);
					assert(outP >= g_local_router_links_offset and outP < g_qcn_port);
					if (g_qcn_implementation == QCNSWOUT)
						qcnMinProbabilityDecrease(outP, qcnFb);
					else if (g_qcn_implementation == QCNSWOUTFBCOMP)
						qcnFeedbackComparison(outP, qcnFb);
					else
						assert(false);
				}
			}
			// Reset timer and save current queue length in old qlen.
			this->qcnCpSamplingCounter[port] = g_qcn_cp_sampling_interval;
			qcnQlenOld[port] = qcnQlen;
		}
	} else
		// base implementation of sampling
		switchModule::qcnOccupancySampling(port);
}
