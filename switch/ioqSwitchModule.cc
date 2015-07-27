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

#include "ioqSwitchModule.h"

ioqSwitchModule::ioqSwitchModule(string name, int label, int aPos, int hPos, int ports, int vcCount) :
		switchModule(name, label, aPos, hPos, ports, vcCount) {
	this->m_speedup_interval = (long double) (1) / (long double) g_internal_speedup;
	this->m_internal_cycle = 0;

	int p, vc;

	for (p = 0; p < g_global_router_links_offset + g_h_global_ports_per_router; p++) {
		delete outPorts[p];
	}
	for (p = 0; p < g_local_router_links_offset; p++) {
		outPorts[p] = new bufferedOutPort(g_injection_channels, p, p + portCount * vcCount, g_out_queue_length,
				g_xbar_delay, this);
	}
	for (p = g_local_router_links_offset; p < g_global_router_links_offset; p++) {
		outPorts[p] = new bufferedOutPort(g_local_link_channels, p, p + portCount * vcCount, g_out_queue_length,
				g_xbar_delay, this);
	}
	for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router; p++) {
		outPorts[p] = new bufferedOutPort(g_global_link_channels, p, p + portCount * vcCount, g_out_queue_length,
				g_xbar_delay, this);
	}

	for (p = 0; p < g_local_router_links_offset; p++) {
		for (vc = 0; vc < g_injection_channels; vc++) {
			outPorts[p]->setMaxOutOccupancy(vc, g_out_queue_length);
		}
	}
	for (p = g_local_router_links_offset; p < g_global_router_links_offset; p++) {
		for (vc = 0; vc < g_local_link_channels; vc++) {
			outPorts[p]->setMaxOutOccupancy(vc, g_out_queue_length);
		}
	}
	for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router; p++) {
		for (vc = 0; vc < g_global_link_channels; vc++) {
			outPorts[p]->setMaxOutOccupancy(vc, g_out_queue_length);
		}
	}
}

ioqSwitchModule::~ioqSwitchModule() {
}

void ioqSwitchModule::action() {
	int port, vc;
	g_internal_cycle = g_cycle;

	switchModule::action();

	for (port = 0; port < this->portCount; port++) {
		updateOutputBuffer(port);
	}

	for (port = 0; port < this->portCount; port++) {
		outPorts[port]->reorderBuffer(0);
	}
}

void ioqSwitchModule::updateOutputBuffer(int port) {
	flitModule *flit = NULL;

	if (outPorts[port]->canSendFlit(0)) {
		outPorts[port]->checkFlit(0, flit);
		if (flit != NULL) {
			int nextP, nextC, inP, inC;
			nextC = flit->channel;
			nextP = routing->neighPort[port];
			inP = flit->prevP;
			inC = flit->prevVC;

			if (port < g_p_computing_nodes_per_router) {
				if (g_internal_cycle >= (this->lastConsumeCycle[port] + g_flit_size)) {
					/* Pkt is consumed */
#if DEBUG
					cout << "Tx pkt " << flit->flitId << " from source " << flit->sourceId << " (sw " << flit->sourceSW
					<< ", group " << flit->sourceGroup << ") to dest " << flit->destId << " (sw "
					<< flit->destSwitch << ", group " << flit->destGroup << "). Consuming pkt in node " << port
					<< endl;
#endif
					outPorts[port]->extract(nextC, flit, g_flit_size);

					/* Update contention counters (time waiting in queues) */
					flit->addContention(inP, this->label);

					trackConsumptionStatistics(flit, inP, inC, port);
					delete flit;
				}
			} else {
				bool nextVC_unLocked, input_phy_ring, output_phy_ring, input_emb_escape, output_emb_escape,
						subnetworkInjection;

				if (this->switchModule::nextPortCanReceiveFlit(port)
						&& switchModule::getCredits(port, nextC) >= g_flit_size) {
					/* Pkt is sent */
					outPorts[port]->extract(nextC, flit, g_flit_size);
					routing->neighList[port]->insertFlit(nextP, nextC, flit);

					/* Decrement credit counter in local switch and send credits to previous switch. */
					sendCredits(inP, inC, flit->flitId);

					flit->setChannel(nextC);

					if (inP < g_p_computing_nodes_per_router) {
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
							subnetworkInjection = (port >= this->portCount - 2)
									&& !((port == this->portCount - 2) && (this->aPos == 0))
									&& !((port == this->portCount - 1) && (this->aPos == g_a_routers_per_group - 1));
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
						//LOCAL MISROUTING
						if (misroute_type == LOCAL || misroute_type == LOCAL_MM) {
							assert(port >= g_local_router_links_offset);
							assert(port < g_global_router_links_offset);
							assert(not (output_emb_escape));
							flit->setMisrouted(true, misroute_type);
							flit->localMisroutingDone = 1;
						}
						//GLOBAL MISROUTING
						if (misroute_type == GLOBAL || misroute_type == GLOBAL_MANDATORY) {
							assert(port >= g_global_router_links_offset);
							assert(port < g_global_router_links_offset + g_h_global_ports_per_router);
							assert(not (output_emb_escape));
							flit->setMisrouted(true, misroute_type);
							flit->globalMisroutingDone = 1;

							//Global misrouting at injection
							if (inP < g_p_computing_nodes_per_router) {
								assert(misroute_type == GLOBAL);
								flit->setGlobalMisrouteAtInjection(true);
							}
						}
						//MANDATORY GLOBAL MISROUTING ASSERTS
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
										&& (port < g_global_router_links_offset) && (this->hPos == flit->sourceGroup)
										&& (this->hPos != flit->destGroup) && not (flit->globalMisroutingDone)
										&& not (output_emb_escape) && not (input_emb_escape)
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
								if (port != routing->minOutputPort(flit->destId) && inP < g_global_router_links_offset
										&& port >= g_local_router_links_offset && (port < g_global_router_links_offset)
										&& (this->hPos == flit->sourceGroup) && (this->hPos != flit->destGroup)
										&& not (flit->globalMisroutingDone) && not (output_emb_escape)
										&& not (input_emb_escape) && not (flit->getCurrentMisrouteType() == VALIANT)) {
									flit->mandatoryGlobalMisrouting_flag = 1;
								}
						}

						//RESET some flags when changing group
						if (hPos != routing->neighList[port]->hPos) {
							flit->mandatoryGlobalMisrouting_flag = 0;
							flit->localMisroutingDone = 0;
						}

					}

					trackTransitStatistics(flit, inP, port, nextC);
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
	assert(port < (portCount));

	assert(outPorts[port]->getSpace(0) >= g_flit_size);
	/* Confirm there will be enough space to hold flit when sent to next sw */
	assert(this->switchModule::getCredits(port, flit->channel) >= g_flit_size || port < g_p_computing_nodes_per_router);

	/* Calculate transmission length: max of xbar tx length, and input buffer remaining receive time */
	int inP = flit->prevP, inVC = flit->prevVC;
	double length = (double) g_flit_size / (double) g_internal_speedup;
	length = max(length, inPorts[inP]->getHeadEntryCycle(inVC) + g_flit_size - (double) g_internal_cycle);
	assert(length <= g_flit_size);

	outPorts[port]->insert(flit->channel, flit, length);
	return;
}

/*
 * Makes effective flit transferal from input to output buffer.
 */
void ioqSwitchModule::sendFlit(int input_port, int input_channel, int outP, int nextP, int nextC) {
	flitModule *flitEx;
	bool nextVC_unLocked, input_emb_escape = false, output_emb_escape = false, input_phy_ring = false, output_phy_ring =
			false, subnetworkInjection = false;

	assert(nextC >= 0 && nextC < vcCount);

	inPorts[input_port]->checkFlit(input_channel, flitEx);
	assert(inPorts[input_port]->canSendFlit(input_channel));

	/* Calculate transmission length: max of xbar tx length, and input buffer remaining receive time */
	double length = (double) g_flit_size / (double) g_internal_speedup;
	length = max(length,
			inPorts[input_port]->getHeadEntryCycle(input_channel) + g_flit_size - (double) g_internal_cycle);
#if DEBUG
	if (length > g_flit_size) cerr << "ERROR: length: "<<length<<", flit_size "<<g_flit_size<<endl;
#endif
	assert(length <= g_flit_size);

	assert(inPorts[input_port]->extract(input_channel, flitEx, length));

	/* Contention-Aware misrouting trigger notification */
	if (g_contention_aware && g_increaseContentionAtHeader) {
		m_ca_handler.flitSent(flitEx, input_port, length);
	}

	if (g_contention_aware && (!g_increaseContentionAtHeader)) {
		m_ca_handler.decreaseContention(routing->minOutputPort(flitEx->destId));
	}

	if (flitEx->head == 1) {
		inPorts[input_port]->setCurPkt(input_channel, flitEx->packetId);
		inPorts[input_port]->setOutCurPkt(input_channel, outP);
		inPorts[input_port]->setNextVcCurPkt(input_channel, nextC);
	}
	assert(flitEx->packetId == inPorts[input_port]->getCurPkt(input_channel));
	if (flitEx->tail == 1) {
		inPorts[input_port]->setCurPkt(input_channel, -1);
		inPorts[input_port]->setOutCurPkt(input_channel, -1);
		inPorts[input_port]->setNextVcCurPkt(input_channel, -1);
	}
	assert(flitEx != NULL);

	/* Pkt is commuted from input to output buffer */
	assert(outPorts[outP]->getSpace(0) >= g_flit_size);

	flitEx->setChannel(nextC);
	flitEx->prevP = input_port;
	flitEx->prevVC = input_channel;

	updateMisrouteCounters(outP, flitEx);

	txFlit(outP, flitEx);
}

/*
 * Interface to output port function to determine if port
 * can receive flit (e.g., has ended receiving previous
 * flit).
 */
bool ioqSwitchModule::nextPortCanReceiveFlit(int port) {
	bool can_receive_flit = true;
	return outPorts[port]->canReceiveFlit(0);
}

int ioqSwitchModule::getCredits(int port, int channel) {
	int crdts;

	assert(port < portCount);
	assert(channel < vcCount);
	assert(port >= g_p_computing_nodes_per_router); /* This function should only be called for transit ports */

	/* This version just returns output buffer occupancy */
	crdts = outPorts[port]->maxCredits[channel] - outPorts[port]->getTotalOccupancy(channel);
	crdts = min(crdts, outPorts[port]->getSpace(0));

	return (crdts);
}

/*
 * Returns the number of used FLITS in the next buffer
 * Transit port: based on the local credit count in transit
 * Injection port: based on local buffer real occupancy
 */
int ioqSwitchModule::getCreditsOccupancy(int port, int channel) {
	int occupancy;

	occupancy = outPorts[port]->getBufferOccupancy(0);
	if (port >= g_p_computing_nodes_per_router) {
		occupancy = outPorts[port]->getTotalOccupancy(channel);
	}
	return occupancy;
}

int ioqSwitchModule::checkConsumePort(int port) {
	return (outPorts[port]->getSpace(0) >= g_flit_size);
}

