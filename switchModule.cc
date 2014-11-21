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

#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>

#include "switchModule.h"

using namespace std;

switchModule::switchModule(string name, int label, int aPos, int hPos, int ports, int vcCount) :
		PB(aPos) {
	this->name = name;
	this->portCount = ports;
	this->vcCount = vcCount;
	this->pBuffers = new buffer **[this->portCount];
	this->neighList = new switchModule *[this->portCount];
	this->localArbiters = new localArbiter *[this->portCount];
	this->globalArbiters = new globalArbiter *[this->portCount];
	this->label = label;
	this->delInj = delInj; //Delay at injection
	this->delTrans = delTrans; // Delay at transit
	this->pPos = 0;
	this->aPos = aPos;
	this->hPos = hPos;
	this->messagesInQueuesCounter = 0;
	neighPort = new int[portCount];
	this->escapeNetworkCongested = false;
	int i, j, p, c;
	bool match, match1, match2;
	bool localEmbeddedTreeSwitch = false;
	bool globalEmbeddedTreeSwitch = false;
	this->packetsInj = 0;

	m_vc_misrouting_congested_restriction_globalLinkCongested = new bool[g_dH];
	for (int i = 0; i < g_dH; i++) {
		m_vc_misrouting_congested_restriction_globalLinkCongested[i] = false;
	}
	/*
	 * Each output port has initially -1 credits
	 * Function findneighbours sets this values according
	 * to the destination buffer capacity
	 */
	m_nextVCtoTry = new int[ports];
	for (i = 0; i < ports; i++) {
		m_nextVCtoTry[i] = -1;
	}

	m_reorderVCs = new bool[ports];
	for (i = 0; i < ports; i++) {
		m_reorderVCs[i] = true;
	}

	credits = new int*[ports];
	maxCredits = new int*[ports];
	for (i = 0; i < ports; i++) {
		credits[i] = new int[vcCount];
		maxCredits[i] = new int[vcCount];
		for (j = 0; j < vcCount; j++) {
			credits[i][j] = -1;
			maxCredits[i][j] = 0;
		}
	}

	/*
	 * one credit received per port
	 */
	incomingCredits = new queue<creditFlit> *[ports];
	for (i = 0; i < ports; i++) {
		incomingCredits[i] = new queue<creditFlit>;
	}

	for (i = 0; i < ports; i++) {
		this->out_port_used[i] = 0;
	}
	for (i = 0; i < ports; i++) {
		this->in_port_served[i] = 0;
	}
	this->tableOut = new int[g_generatorsCount];
	this->tableIn = new int[g_generatorsCount];
	this->tableOutRing1 = new int[g_generatorsCount];
	this->tableInRing1 = new int[g_generatorsCount];
	this->tableOutRing2 = new int[g_generatorsCount];
	this->tableInRing2 = new int[g_generatorsCount];
	this->tableOutTree = new int[g_generatorsCount];
	this->tableInTree = new int[g_generatorsCount];

	for (i = 0; i < 100; i++) {
		this->lastConsumeCycle[i] = -999;
	}

	for (p = 0; p < g_generatorsCount; p++) {
		this->tableIn[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableOut[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableInRing1[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableOutRing1[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableInRing2[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableOutRing2[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableInTree[p] = -1;
	}
	for (p = 0; p < g_generatorsCount; p++) {
		this->tableOutTree[p] = -1;
	}
	setTables();

	for (i = 0; i < this->portCount; i++) {
		for (j = 0; j < this->vcCount; j++) {
			this->queueOccupancy[(i * this->vcCount) + j] = 0;
		}
	}

	for (i = 0; i < vcCount; i++) {
		this->injectionQueueOccupancy[i] = 0;
		this->localQueueOccupancy[i] = 0;
		this->globalQueueOccupancy[i] = 0;
		this->localRingQueueOccupancy[i] = 0;
		this->globalRingQueueOccupancy[i] = 0;
		this->localTreeQueueOccupancy[i] = 0;
		this->globalTreeQueueOccupancy[i] = 0;
	}

	for (p = 0; p < this->portCount; p++) {
		this->pBuffers[p] = new buffer *[this->vcCount];
	}
	for (p = 0; p < g_dP; p++) { //Injection queues
		for (c = 0; c < this->vcCount; c++) {
			if (c < g_channels_inj) {
				pBuffers[p][c] = new buffer((p * this->vcCount + c), g_queueGen, g_delayInjection, this);
			} else {
				pBuffers[p][c] = new buffer((p * this->vcCount + c), 0, g_delayInjection, this);
			}
		}
	}
	for (p = g_dP; p < this->portCount; p++) {
		match = false;
		match1 = false;
		match2 = false;
		if (g_trees > 0) {
			for (i = 0; i < g_generatorsCount; i++) {
				if (p == tableOutTree[i]) {
					match = true; //embedded tree port
					localEmbeddedTreeSwitch = true;
					break;
				}
			}
		}
		if (g_rings > 0) {
			for (i = 0; i < g_generatorsCount; i++) {
				if (p == tableOutRing1[i]) {
					match1 = true; //embedded ring1 port
					break;
				}
				if ((g_rings > 1) && (p == tableOutRing2[i])) {
					assert(g_ringDirs > 1);
					assert(match1 == false);
					match2 = true; //embedded ring2 port
					cout << "Switch " << this->label << ", port " << p << ". ESCAPE BUFFER" << endl;
					break;
				}
			}
		}
		if (g_onlyRing2) match1 = false;
		if ((p < g_offsetH)) { // Local ports
			for (c = 0; c < this->vcCount; c++) {
				if (c < g_channels_useful) {
					pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_local,
							g_delayTransmission_local, this);
				} else if ((c < (g_channels_useful + g_channels_ring)) && ((g_embeddedRing == 1))) { //Embedded ring
					if (match1 || match2) { //RING 1 OR 2
						assert(!((match1 == true) && (match2 == true)));
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_local,
								g_delayTransmission_local, this);
						pBuffers[p][c]->escapeBuffer = true;
					} else {
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_local, this);
					}
				} else if ((c < (g_channels_useful + g_channels_tree)) && (g_trees > 0)) { //Embedded tree
					if (match) {
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_local,
								g_delayTransmission_local, this);
						pBuffers[p][c]->escapeBuffer = true;
					} else {
						assert(match == false);
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_local, this);
					}
				} else {
					pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_local, this);
				}
				if (pBuffers[p][c]->escapeBuffer)
					cout << "Switch " << this->label << ", port " << p << ", channel " << c << ". ESCAPE BUFFER"
							<< endl;
			}
		} else {
			if (p < (g_offsetH) + g_dH) { // Global ports
				for (c = 0; c < vcCount; c++) {
					if (c < g_channels_glob) {
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_global,
								g_delayTransmission_global, this);
					} else if ((c >= g_channels_useful) && (c < (g_channels_useful + g_channels_ring))
							&& ((g_embeddedRing == 1))) { //Embedded ring1
						if (match1 || match2) { //RING 1 or 2
							assert(!((match1 == true) && (match2 == true)));
							pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_global,
									g_delayTransmission_global, this);
							pBuffers[p][c]->escapeBuffer = true;
						} else {
							pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_global, this);
						}
					} else if ((c >= g_channels_useful) && (c < (g_channels_useful + g_channels_tree))
							&& (g_trees > 0)) { //Embedded tree
						if (match) {
							pBuffers[p][c] = new buffer((p * (this->vcCount) + c), g_queueSwtch_global,
									g_delayTransmission_global, this);
							pBuffers[p][c]->escapeBuffer = true;
						} else {
							assert(match == false);
							pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_global, this);
						}
					} else {
						pBuffers[p][c] = new buffer((p * (this->vcCount) + c), 0, g_delayTransmission_global, this);

					}
					if (pBuffers[p][c]->escapeBuffer)
						cout << "Switch " << this->label << ", port " << p << ", channel " << c << ". ESCAPE BUFFER"
								<< endl;
				}
			} else {
				if (((p == this->portCount - 2) && (this->aPos == 0))
						|| ((p == this->portCount - 1) && (this->aPos == g_dA - 1))) { // Ring ports that get into and out of the group
					for (c = 0; c < vcCount; c++) {
						if (c < g_channels_useful) {
							pBuffers[p][c] = new buffer((p * this->vcCount + c), g_queueSwtch_global,
									g_delayTransmission_global, this);
							pBuffers[p][c]->escapeBuffer = true;
						} else {
							pBuffers[p][c] = new buffer((p * this->vcCount + c), 0, g_delayTransmission_global, this);
						}
					}
				} else {
					for (c = 0; c < this->vcCount; c++) {
						if (c < g_channels_useful) {
							pBuffers[p][c] = new buffer((p * this->vcCount + c), g_queueSwtch_local,
									g_delayTransmission_local, this);
							pBuffers[p][c]->escapeBuffer = true;
						} else {
							pBuffers[p][c] = new buffer((p * this->vcCount + c), 0, g_delayTransmission_local, this);
						}
					}
				}
			}
		}
	}
	if (localEmbeddedTreeSwitch) g_localEmbeddedTreeSwitchesCount++;
	if (globalEmbeddedTreeSwitch) g_globalEmbeddedTreeSwitchesCount++;

	for (p = 0; p < this->portCount; p++) {
		this->globalArbiters[p] = new globalArbiter(p, this->portCount);
	}
	for (p = 0; p < this->portCount; p++) {
		this->localArbiters[p] = new localArbiter(p);
	}

}

switchModule::~switchModule() {
	int i, c;
	for (c = 0; c < this->portCount; c++) {
		delete localArbiters[c];
	}
	delete[] localArbiters;

	for (c = 0; c < this->portCount; c++) {
		delete globalArbiters[c];
	}
	delete[] globalArbiters;

	delete[] tableIn;
	delete[] tableOut;

	delete[] tableInRing1;
	delete[] tableOutRing1;
	delete[] tableInRing2;
	delete[] tableOutRing2;
	delete[] tableInTree;
	delete[] tableOutTree;

	for (i = 0; i < this->portCount; i++) {
		for (c = 0; c < this->vcCount; c++) {
			delete pBuffers[i][c];
		}
		delete[] pBuffers[i];
	}
	delete[] pBuffers;

	for (i = 0; i < portCount; i++) {
		delete[] credits[i];
		delete[] maxCredits[i];
	}
	delete[] credits;
	delete[] maxCredits;

	for (int i = 0; i < portCount; i++) {
		delete incomingCredits[i];
	}
	delete[] incomingCredits;

	delete[] neighList;
	delete[] neighPort;

	delete[] m_vc_misrouting_congested_restriction_globalLinkCongested;

	delete[] m_reorderVCs;
	delete[] m_nextVCtoTry;
}

void switchModule::resetQueueOccupancy() {
	for (int i = 0; i < this->portCount; i++) {
		for (int j = 0; j < this->vcCount; j++) {
			this->queueOccupancy[(i * this->vcCount) + j] = 0;
		}
	}
	for (int i = 0; i < vcCount; i++) {
		this->injectionQueueOccupancy[i] = 0;
		this->localQueueOccupancy[i] = 0;
		this->globalQueueOccupancy[i] = 0;
		this->localRingQueueOccupancy[i] = 0;
		this->globalRingQueueOccupancy[i] = 0;
		this->localTreeQueueOccupancy[i] = 0;
		this->globalTreeQueueOccupancy[i] = 0;
	}
}

int switchModule::olderChannel(int prevOlderChannel) {
	flitModule *flit;
	int olderCycle = g_cycle;
	int olderChannel = -1;
	for (int c = 0; c < g_channels; c++) {
		pBuffers[p][c]->checkFlit(flit);
		if (flit != NULL) {
			if (pBuffers[p][c]->canSendFlit() && (flit->inCycle <= olderCycle) && (flit->inCycle >= 0)
					&& (couldTryPetition(p, c))) {
				olderCycle = flit->inCycle;
				olderChannel = c;
			}
		}
	}
	assert(olderChannel < g_channels);
	return (olderChannel);
}

int switchModule::olderPort(int output_port) {
	flitModule *flit;
	int olderCycle = g_cycle;
	int olderPort = -1;
	for (int p = 0; p < this->portCount; p++) {
		pBuffers[p][this->globalArbiters[output_port]->inputChannels[p]]->checkFlit(flit);
		if (flit != NULL) {
			if (this->globalArbiters[output_port]->petitions[p] && (flit->inCycle <= olderCycle)
					&& (flit->inCycle >= 0)) {
				olderCycle = flit->inCycle;
				olderPort = p;
			}
		}
	}
	assert(olderPort < this->portCount);
	return (olderPort);
}

void switchModule::escapeCongested() {
	escapeNetworkCongested = true;
	for (int p = 0; p < this->portCount; p++) {
		for (int c = 0; c < this->vcCount; c++) {
			if (pBuffers[p][c]->escapeBuffer) {
				assert(c >= g_channels_useful);
				if (getCreditsOccupancy(p, c) * 100.0 / maxCredits[p][c] <= g_escapeCongestion_th)
					escapeNetworkCongested = false;
			}
		}
	}
}

void switchModule::setQueueOccupancy() {
	int p, c, lq, gq, iq, grq, lrq, gtq, ltq;
	// Counters to divide total values into the number of queues aggregated
	iq = 0;
	lq = 0;
	gq = 0;
	lrq = 0;
	grq = 0;
	ltq = 0;
	gtq = 0;
	for (p = 0; p < this->portCount; p++) {
		if ((p < g_dP)) { // Injection ports
			iq++;
			for (c = 0; c < this->vcCount; c++) {
				this->injectionQueueOccupancy[c] = this->injectionQueueOccupancy[c]
						+ this->queueOccupancy[(p * this->vcCount) + c];
			}
		} else {
			if ((p < g_offsetH)) { // Local ports
				lq++;

				if (g_embeddedRing == true) {
					if (this->pBuffers[p][g_channels_useful]->bufferCapacity > 0) lrq++;
				} else if (g_embeddedTree == true) {
					if (this->pBuffers[p][g_channels_useful]->bufferCapacity > 0) ltq++;
				}
				for (c = 0; c < g_channels_useful; c++) {
					this->localQueueOccupancy[c] = this->localQueueOccupancy[c]
							+ this->queueOccupancy[(p * this->vcCount) + c];
				}
				for (c = g_channels_useful; c < (g_channels_useful + g_channels_ring); c++) {
					this->localRingQueueOccupancy[c] = this->localRingQueueOccupancy[c]
							+ this->queueOccupancy[(p * this->vcCount) + c];
				}
				for (c = g_channels_useful; c < (g_channels_useful + g_channels_tree); c++) {
					this->localTreeQueueOccupancy[c] = this->localTreeQueueOccupancy[c]
							+ this->queueOccupancy[(p * this->vcCount) + c];
				}
			} else {
				if (p < (g_offsetH) + g_dH) { // Global ports
					gq++;

					if (g_embeddedRing == true) {
						if (this->pBuffers[p][g_channels_useful]->bufferCapacity > 0) grq++;
					} else if (g_embeddedTree == true) {
						if (this->pBuffers[p][g_channels_useful]->bufferCapacity > 0) gtq++;
					}
					for (c = 0; c < g_channels_useful; c++) {
						this->globalQueueOccupancy[c] = this->globalQueueOccupancy[c]
								+ this->queueOccupancy[(p * this->vcCount) + c];
					}
					for (c = g_channels_useful; c < (g_channels_useful + g_channels_ring); c++) {
						this->globalRingQueueOccupancy[c] = this->globalRingQueueOccupancy[c]
								+ this->queueOccupancy[(p * this->vcCount) + c];
					}
					for (c = g_channels_useful; c < (g_channels_useful + g_channels_tree); c++) {
						this->globalTreeQueueOccupancy[c] = this->globalTreeQueueOccupancy[c]
								+ this->queueOccupancy[(p * this->vcCount) + c];
					}
				} else {
					assert((g_rings != 0) && (g_embeddedRing == 0));
					if (((p == this->portCount - 2) && (this->aPos == 0))
							|| ((p == this->portCount - 1) && (this->aPos == g_dA - 1))) { // Ring ports
						grq++;
						for (c = 0; c < vcCount; c++) {
							this->globalRingQueueOccupancy[c] = this->globalRingQueueOccupancy[c]
									+ this->queueOccupancy[(p * this->vcCount) + c];
						}
					} else {
						lrq++;
						for (c = 0; c < this->vcCount; c++) {
							this->localRingQueueOccupancy[c] = this->localRingQueueOccupancy[c]
									+ this->queueOccupancy[(p * this->vcCount) + c];
						}
					}
				}
			}
		}
	}

	for (int vc = 0; vc < vcCount; vc++) {
		this->injectionQueueOccupancy[vc] = this->injectionQueueOccupancy[vc] / iq;
		this->localQueueOccupancy[vc] = this->localQueueOccupancy[vc] / lq;
		this->globalQueueOccupancy[vc] = this->globalQueueOccupancy[vc] / gq;
		if ((grq == 0) || (vc < g_channels_useful)) {
			this->globalRingQueueOccupancy[vc] = 0;
		} else {
			this->globalRingQueueOccupancy[vc] = this->globalRingQueueOccupancy[vc] / grq;
		}
		if ((lrq == 0) || (vc < g_channels_useful)) {
			this->localRingQueueOccupancy[vc] = 0;
		} else {
			this->localRingQueueOccupancy[vc] = this->localRingQueueOccupancy[vc] / lrq;
		}
		if ((gtq == 0) || (vc < g_channels_useful)) {
			this->globalTreeQueueOccupancy[vc] = 0;
		} else {
			this->globalTreeQueueOccupancy[vc] = this->globalTreeQueueOccupancy[vc] / gtq;
		}
		if ((ltq == 0) || (vc < g_channels_useful)) {
			this->localTreeQueueOccupancy[vc] = 0;
		} else {
			this->localTreeQueueOccupancy[vc] = this->localTreeQueueOccupancy[vc] / ltq;
		}
	}

}

void switchModule::findNeighbors(switchModule* swList[]) {
	int thisPort, thisA, thisH, prt, neighA, neighH;
	char portType;
	thisA = this->aPos;
	thisH = this->hPos;
	for (thisPort = g_dP; thisPort < this->portCount; thisPort++) {
		if (g_palm_tree_configuration == 1) {
			if (thisPort < g_offsetH) {
				prt = thisPort - g_offsetA;
				portType = 'a';
			} else if (thisPort < (g_dP + g_dA - 1 + g_dH)) {
				prt = thisPort - g_offsetH;
				portType = 'h';
			} else {
				assert(thisPort >= (this->portCount - 2));
				assert(g_rings != 0);
				prt = thisPort - (g_dP + g_dA - 1 + g_dH);
				portType = 'f';
			}
			switch (portType) {
				case 'a':
					neighH = thisH;
					if (prt < thisA) {
						neighA = prt;
					}
					if (prt >= thisA) {
						neighA = prt + 1;
					}
					break;
				case 'h':
					neighH = module(thisH - (thisA * g_dH + prt + 1), (g_dA * g_dH + 1));
					assert(neighH != thisH);
					assert(neighH < (g_dA * g_dH + 1));

					neighA = int((module((neighH - thisH), (g_dA * g_dH + 1)) - 1) / g_dH);
					break;
				case 'f':
					if (prt == 0) { // Port 0 of physical cycle routes to previous router
						neighA = module(thisA - 1, g_dA);
					}
					if (prt == 1) { // Port 1 of physical cycle routes to next router
						neighA = module(thisA + 1, g_dA);
					}
					assert(neighA < g_dA);
					if ((neighA == 0) && (thisA == (g_dA - 1))) { // if next router has A position = 0, we have advanced to the next group H
						neighH = module(thisH + 1, ((g_dH * g_dA) + 1));
					} else if ((neighA == (g_dA - 1)) && (thisA == 0)) { // if next router has A position = dA-1, we have advanced to the previous group H
						neighH = module(thisH - 1, ((g_dH * g_dA) + 1));
					} else {
						neighH = thisH;
					}
					break;
			}
		} else {
			if (thisPort < g_offsetH) {
				prt = thisPort - g_offsetA;
				portType = 'a';
			} else if (thisPort < (g_dP + g_dA - 1 + g_dH)) {
				prt = thisPort - g_offsetH;
				portType = 'h';
			} else {
				assert(thisPort >= (this->portCount - 2));
				assert(g_rings != 0);
				prt = thisPort - (g_dP + g_dA - 1 + g_dH);
				portType = 'f';
			}
			switch (portType) {
				case 'a':
					neighH = thisH;
					if (prt < thisA) {
						neighA = prt;
					}
					if (prt >= thisA) {
						neighA = prt + 1;
					}
					break;
				case 'h':
					if ((prt + thisA * g_dH) < thisH) {
						neighH = prt + (thisA) * g_dH;
					}
					if ((prt + thisA * g_dH) >= thisH) {
						neighH = prt + (thisA) * g_dH + 1;
					}
					assert(neighH != thisH);
					if (thisH < neighH) {
						neighA = int(thisH / g_dH);
					} else {
						neighA = int((thisH - 1) / g_dH);
					}
					break;
				case 'f':
					if (prt == 0) {
						neighA = module(thisA - 1, g_dA);
					}
					if (prt == 1) {
						neighA = module(thisA + 1, g_dA);
					}
					assert(neighA < g_dA);
					if ((neighA == 0) && (thisA == (g_dA - 1))) {
						neighH = module(thisH + 1, ((g_dH * g_dA) + 1));
					} else if ((neighA == (g_dA - 1)) && (thisA == 0)) {
						neighH = module(thisH - 1, ((g_dH * g_dA) + 1));
					} else {
						neighH = thisH;
					}
					break;
			}
		}
		assert((neighA + (neighH * g_dA)) < (g_switchesCount));
		assert(thisPort < (portCount));
		neighList[thisPort] = swList[neighA + (neighH * g_dA)];
	}
}

/* 
 * Visits every neighbour so that they know which switch is on the upstream
 * of each input port.
 * Uses tableIn and NeighbourList: call findNeighbours() before visiting them
 */
void switchModule::visitNeighbours() {

	int port, nextP, numRingPorts;

	if ((g_rings == 0) || (g_embeddedRing != 0))
		numRingPorts = 0;
	else
		numRingPorts = 2;

	//local and global links
	for (port = 0; port < g_offsetA; port++) {
		neighPort[port] = 0;
	}

	for (port = g_offsetA; port < portCount - numRingPorts; port++) {

		nextP = tableIn[neighList[port]->label * g_dP];
		neighPort[port] = nextP;
	}

	//ring ports
	for (port = portCount - numRingPorts; port < portCount; port++) {

		assert((g_ringDirs == 1) || (g_ringDirs == 2));

		//swap ring port
		if (port == portCount - 1) {
			nextP = portCount - 2;
		} else {
			nextP = portCount - 1;
		}

		neighPort[port] = nextP;
	}
}

/* 
 * Set credits to each output channel according to the capacity of the
 * next input buffer capacity
 * 
 * Needs neighbour information:
 * Call setTables and findNeighbours before this
 */
void switchModule::resetCredits() {
	int thisPort, nextP, chan;

	//injection ports (check buffers in local switch)
	for (thisPort = 0; thisPort < g_dP; thisPort++) {
		for (chan = 0; chan < vcCount; chan++) {
			credits[thisPort][chan] = pBuffers[thisPort][chan]->getSpace();
			maxCredits[thisPort][chan] = pBuffers[thisPort][chan]->bufferCapacity * g_flitSize;

			//initial credits must be equal to the buffer capacity (in phits)
			assert(getCredits(thisPort, chan) == maxCredits[thisPort][chan]);
		}
	}

	//transit ports (check buffers in the next switch)
	for (thisPort = g_dP; thisPort < portCount; thisPort++) {
		for (chan = 0; chan < vcCount; chan++) {
			/*
			 * -neighList[thisPort]->label is de id number of the next switch.
			 *
			 * -tableIn[generatorId] is the inputPort of the next switch
			 * in the route to generatorId.
			 *
			 * -We use the first generator connected to the next switch to
			 * index tableIn, wich means: switchId * g_dP (numberOfGeneratosPerSwitch),
			 * to find out the input port in the next switch.
			 */
			nextP = neighPort[thisPort];
			credits[thisPort][chan] = neighList[thisPort]->pBuffers[nextP][chan]->getSpace();
			maxCredits[thisPort][chan] = neighList[thisPort]->pBuffers[nextP][chan]->bufferCapacity * g_flitSize;

			//initial credits must be equal to the buffer capacity (in phits)
			assert(getCredits(thisPort, chan) == maxCredits[thisPort][chan]);
		}
	}
}

int switchModule::getTotalCapacity() {
	int totalCapacity = 0;
	for (p = 0; p < portCount; p++) {
		for (c = 0; c < vcCount; c++) {
			totalCapacity = totalCapacity + (pBuffers[p][c]->bufferCapacity);
		}
	}
	return (totalCapacity);
}

int switchModule::getTotalFreeSpace() {
	int totalFreeSpace = 0;
	for (p = 0; p < portCount; p++) {
		for (c = 0; c < vcCount; c++) {
			totalFreeSpace = totalFreeSpace + (pBuffers[p][c]->getSpace());
		}
	}
	return (totalFreeSpace);

}

/*
 * Insert flit into a buffer. Used in TRANSIT.
 * Credit count reduction is done in the upstream switch, not here.
 */
void switchModule::insertFlit(int port, int vc, flitModule *flit) {
	assert(port < (portCount));
	assert(vc < (vcCount));

	assert(pBuffers[port][vc]->getSpace() >= g_flitSize);

	pBuffers[port][vc]->insert(flit);

	return;
}

/*
 * Insert flit into an injection buffer. Used in INJECTION. NOT USED???????????
 */
void switchModule::injectFlit(int port, int vc, flitModule* flit) {
	long base_latency;

	assert(port < g_dP);
	assert(vc < (vcCount));
	assert(getCredits(port, vc) >= g_flitSize);
	pBuffers[port][vc]->insert(flit);

	//calculate message's base latency and record it
	base_latency = calculateBaseLatency(flit);
	flit->setBase_latency(base_latency);

	return;
}

void switchModule::setTables() {
	int thisA, thisH, destP, destA, destH, s, a, b, take_one_out, inPort, outPort, dist;
	thisA = this->aPos;
	thisH = this->hPos;

	for (destH = 0; destH < ((g_dH * g_dA) + 1); destH++) {
		for (destA = 0; destA < g_dA; destA++) {
			for (destP = 0; destP < g_dP; destP++) {
				s = destP + destA * g_dP + destH * g_dA * g_dP; // destination node
				if (g_palm_tree_configuration == 1) {
					a = (int(module((thisH - destH), (g_dA * g_dH + 1)) - 1) / g_dH);
					if (destH != thisH) {
						if (a != thisA) {
							take_one_out = (thisA < a) ? 1 : 0;
							outPort = a - take_one_out + g_offsetA;
							inPort = thisA + g_offsetA - (1 - take_one_out);
						} else {
							outPort = (module((thisH - destH), (g_dA * g_dH + 1)) - 1) % g_dH + g_offsetH;
							inPort = (module((destH - thisH), (g_dA * g_dH + 1)) - 1) % g_dH + g_offsetH;
						}

					} else if (thisA < destA) {
						outPort = destA + g_offsetA - 1;
						inPort = thisA + g_offsetA;
					} else if (thisA > destA) {
						outPort = destA + g_offsetA;
						inPort = thisA + g_offsetA - 1;
					} else {
						outPort = destP;
						inPort = 0;
					}
				} else {
					a = int((destH - 1) / g_dH);
					b = int((destH) / g_dH);
					if (destH > thisH) {
						if (a != thisA) {
							take_one_out = (thisA < a) ? 1 : 0;
							outPort = a - take_one_out + g_offsetA;
							inPort = thisA + g_offsetA - (1 - take_one_out);
						} else {
							outPort = module((destH - 1), g_dH) + g_offsetH;
							inPort = module(thisH, g_dH) + g_offsetH;
						}
					} else if (destH < thisH) {
						if (b != thisA) {
							take_one_out = (thisA < b) ? 1 : 0;
							outPort = b - take_one_out + g_offsetA;
							inPort = thisA + g_offsetA - (1 - take_one_out);
						} else {
							outPort = module(destH, g_dH) + g_offsetH;
							inPort = module((thisH - 1), g_dH) + g_offsetH;
						}
					} else if (thisA < destA) {
						outPort = destA + g_offsetA - 1;
						inPort = thisA + g_offsetA;
					} else if (thisA > destA) {
						outPort = destA + g_offsetA;
						inPort = thisA + g_offsetA - 1;
					} else {
						outPort = destP;
						inPort = 0;
					}
				}

				this->tableIn[s] = inPort;
				this->tableOut[s] = outPort;
				if ((g_rings != 0) && (g_embeddedRing == 0)) {
					assert(outPort < (portCount - 2));
					assert(inPort < (portCount - 2));
				} else {
					assert(outPort < (portCount));
					assert(inPort < (portCount));
				}

			}
		}
	}

	if (g_rings > 0) {
		for (destH = 0; destH < ((g_dH * g_dA) + 1); destH++) {
			for (destA = 0; destA < g_dA; destA++) {
				for (destP = 0; destP < g_dP; destP++) {
					s = destP + destA * g_dP + destH * g_dA * g_dP; // destination node

					//RING 1

					dist = module((int(s / g_dP) - this->label), g_switchesCount); //distance to the destination node going through ring 1
					if (g_ringDirs == 1) {
						if (this->aPos == g_dA - 1) {
							inPort = this->tableIn[module((this->hPos + 1), ((g_dH * g_dA) + 1)) * g_dA * g_dP]; //Ring, net input port 23
							outPort = this->tableOut[module((this->hPos + 1), ((g_dH * g_dA) + 1)) * g_dA * g_dP]; // ring output port 24
						} else {
							inPort = this->tableIn[(this->hPos) * g_dA * g_dP + (this->aPos + 1) * g_dP]; //ring, net input port 23
							outPort = this->tableOut[(this->hPos) * g_dA * g_dP + (this->aPos + 1) * g_dP]; // ring output port 24
						}
					}
					if (g_ringDirs == 2) {
						if (dist == 0) {
							outPort = tableOut[s];
							inPort = tableIn[s];
						} else {
							if ((dist == g_switchesCount / 2) && ((g_switchesCount % 2) == 0)) { // if number of switches is even and we have
								// same distance to destination through both senses, choose random
								dist = rand() % 2; // if 'dist'= 0 we go through the left, if = 1 through the right
							}

							if ((dist > int(g_switchesCount / 2))) { // go 1 to the left
								if (this->aPos == 0) {
									inPort = this->tableIn[module((this->hPos - 1), ((g_dH) * g_dA) + 1) * g_dA * g_dP
											+ (g_dA - 1) * g_dP];
									outPort = this->tableOut[module((this->hPos - 1), ((g_dH) * g_dA) + 1) * g_dA * g_dP
											+ (g_dA - 1) * g_dP];
									assert(inPort >= g_offsetH);
									assert(outPort >= g_offsetH);
								} else {
									inPort = this->tableIn[(this->hPos) * g_dA * g_dP + (this->aPos - 1) * g_dP];
									outPort = this->tableOut[(this->hPos) * g_dA * g_dP + (this->aPos - 1) * g_dP];
									assert(inPort < g_offsetH);
									assert(outPort < g_offsetH);
								}
							} else { // go 1 to the right
								if (this->aPos == g_dA - 1) {
									inPort = this->tableIn[module((this->hPos + 1), ((g_dH) * g_dA) + 1) * g_dA * g_dP];
									outPort =
											this->tableOut[module((this->hPos + 1), ((g_dH) * g_dA) + 1) * g_dA * g_dP];
									assert(inPort >= g_offsetH);
									assert(outPort >= g_offsetH);
								} else {
									inPort = this->tableIn[(this->hPos) * g_dA * g_dP + (this->aPos + 1) * g_dP];
									outPort = this->tableOut[(this->hPos) * g_dA * g_dP + (this->aPos + 1) * g_dP];
									assert(inPort < g_offsetH);
									assert(outPort < g_offsetH);
								}
							}
						}
					}

					this->tableInRing1[s] = inPort;
					this->tableOutRing1[s] = outPort;
					assert(outPort < (portCount));
					assert(inPort < (portCount));

					//RING 2

					if (g_rings > 1) {
						assert(g_ringDirs == 2);
						dist = module((int(s / g_dP) - this->label), g_switchesCount); // dist in number of groups

						if (g_ringDirs == 1) {
							if (this->aPos == g_dA - 2) {
								inPort =
										this->tableIn[module((this->hPos + (g_dH)), ((g_dH * g_dA) + 1)) * g_dA * g_dP];
								outPort = this->tableOut[module((this->hPos + (g_dH)), ((g_dH * g_dA) + 1)) * g_dA
										* g_dP];
							} else {
								inPort = this->tableIn[(this->hPos) * g_dA * g_dP + (this->aPos + 2) * g_dP];
								outPort = this->tableOut[(this->hPos) * g_dA * g_dP + (this->aPos + 2) * g_dP];
							}
						}
						if (g_ringDirs == 2) {
							if (dist == 0) {
								outPort = tableOut[s];
								inPort = tableIn[s];
							} else {
								if ((dist == g_switchesCount / 2) && ((g_switchesCount % 2) == 0)) {
									dist = rand() % 2;
								}

								if ((dist > int(g_switchesCount / 2))) {
									if (this->aPos == g_dH / 2) {
										inPort = this->tableIn[module((this->hPos - (g_dH * g_dH / 2 + 2)),
												((g_dH * g_dA) + 1)) * g_dA * g_dP];
										outPort = this->tableOut[module((this->hPos - (g_dH * g_dH / 2 + 2)),
												((g_dH * g_dA) + 1)) * g_dA * g_dP];
										assert(inPort >= g_offsetH);
										assert(outPort >= g_offsetH);
									} else {
										inPort = this->tableIn[(this->hPos) * g_dA * g_dP
												+ module((this->aPos - (g_dH + 1)), g_dA) * g_dP]; //Router this->aPos - 2h in this group
										outPort = this->tableOut[(this->hPos) * g_dA * g_dP
												+ module((this->aPos - (g_dH + 1)), g_dA) * g_dP]; //Router this->aPos - 2h in this group
										assert(inPort < g_offsetH);
										assert(outPort < g_offsetH);
									}
								} else { // to the right
									if (this->aPos == g_dA - 1 - (g_dH / 2)) {
										inPort = this->tableIn[module((this->hPos + (g_dH * g_dH / 2 + 2)),
												((g_dH * g_dA) + 1)) * g_dA * g_dP];
										outPort = this->tableOut[module((this->hPos + (g_dH * g_dH / 2 + 2)),
												((g_dH) * g_dA + 1)) * g_dA * g_dP];
										assert(inPort >= g_offsetH);
										assert(outPort >= g_offsetH);
									} else {
										inPort = this->tableIn[(this->hPos) * g_dA * g_dP
												+ module((this->aPos + (g_dH + 1)), g_dA) * g_dP]; //Router this->aPos + 2h in this group
										outPort = this->tableOut[(this->hPos) * g_dA * g_dP
												+ module((this->aPos + (g_dH + 1)), g_dA) * g_dP]; //Router this->aPos + 2h in this group
										assert(inPort < g_offsetH);
										assert(outPort < g_offsetH);
									}
								}
							}
						}

						this->tableInRing2[s] = inPort;
						this->tableOutRing2[s] = outPort;
						assert(outPort < (portCount));
						assert(inPort < (portCount));
					}
				}
			}
		}
	}

	if (g_trees > 0) {
		//Only for palm tree configuration
		for (destH = 0; destH < ((g_dH * g_dA) + 1); destH++) {
			for (destA = 0; destA < g_dA; destA++) {
				for (destP = 0; destP < g_dP; destP++) {
					s = destP + destA * g_dP + destH * g_dA * g_dP; // destination node
					//Not possible to be in the destination switch
					if (this->label == int(g_treeRoot_node / g_dP)) { //If this switch is the root, minimal path
						outPort = tableOut[s];
						inPort = tableIn[s];
					} else if (thisH == destH) { //If this switch is in the same group as de destination node
						assert(tableIn[s] < g_offsetH);
						assert(tableOut[s] < g_offsetH);
						if (tableOut[g_treeRoot_node] < g_offsetH) { //If the minimal path to the root is through a local link, the go to the root
							outPort = tableOut[g_treeRoot_node];
							inPort = tableIn[g_treeRoot_node];
							assert(tableIn[g_treeRoot_node] < g_offsetH);
						} else { //If the minimal path to the root is through a global link, then it is minimally connected to the destination node through the tree. so, minima path
							outPort = tableOut[s];
							inPort = tableIn[s];
							assert(tableIn[s] < g_offsetH);
							assert(tableOut[s] < g_offsetH);
						}
					} else if (thisH == g_rootH) { //If this switch is in the same group as the root switch
						assert(tableOut[g_treeRoot_node] < g_offsetH);
						assert(tableIn[g_treeRoot_node] < g_offsetH);
						if (tableOut[s] >= g_offsetH) { //If the minimal path to the destination is through a global link, the go to the destination
							outPort = tableOut[s];
							inPort = tableIn[s];
							assert(tableIn[s] >= g_offsetH);
						} else { //Go to the root
							outPort = tableOut[g_treeRoot_node];
							inPort = tableIn[g_treeRoot_node];
						}
					} else { //If this switch is in any other group. Go to the root
						outPort = tableOut[g_treeRoot_node];
						inPort = tableIn[g_treeRoot_node];
					}
					this->tableInTree[s] = inPort;
					this->tableOutTree[s] = outPort;
					assert(outPort < (portCount));
					assert(inPort < (portCount));
				}
			}
		}
	}
}

int switchModule::getCredits(int port, int channel) {
	int crdts;

	assert(port < (portCount));
	assert(channel < (vcCount));

	/*
	 * Two different versions:
	 * 1- Magically looking buffer free space
	 * 2- Checking local count of credits (needs actually sending
	 *      credits through the channel)
	 */

	if (port < g_dP) {
		// Version 1, used for injection
		crdts = pBuffers[port][channel]->getSpace();
	} else {
		// Version 2, used for transit
		crdts = credits[port][channel];
	}
	return (crdts);
}

/*
 * Returns the number of used FLITS in the next buffer
 * Transit port: based on the local credit count in transit
 * Injection port: based on local buffer real occupancy
 */
int switchModule::getCreditsOccupancy(int port, int channel) {

	int occupancy;

	if (port < g_dP) {
		//Injection
		occupancy = pBuffers[port][channel]->getBufferOccupancy();
	} else {
		//Transit
		occupancy = maxCredits[port][channel] - credits[port][channel];
	}

	return occupancy;
}

void switchModule::increasePortCount(int port) {
	assert(port < (portCount));
	g_portUseCount[port]++;
}

void switchModule::increasePortContentionCount(int port) {
	assert(port < (portCount));
	g_portContentionCount[port]++;
}

void switchModule::orderQueues() {
	for (int count_ports = 0; count_ports < (this->portCount); count_ports++) {
		for (int count_channels = 0; count_channels < vcCount; count_channels++) {
			pBuffers[count_ports][count_channels]->reorderBuffer();
		}
	}
}

bool switchModule::tryPetition(int input_port, int input_channel) {
	int destination, nextP, outP, nextPval, outPval, valNode, nextC, nextCval, minQueue_length, valQueue_length, k, crd,
			cred;
	flitModule *flitEx;
	bool r;

	assert(!this->pBuffers[input_port][input_channel]->emptyBuffer());
	pBuffers[input_port][input_channel]->checkFlit(flitEx);
	assert(flitEx != NULL);
	destination = flitEx->destId;
	valNode = flitEx->valId;
	flitEx->localMisroutingPetitionDone = 0;
	flitEx->local_mm_MisroutingPetitionDone = 0;
	flitEx->globalMisroutingPetitionDone = 0;

	if (flitEx->head == true) {
		if (destination >= g_generatorsCount) {
			cout << "Generators count = " << g_generatorsCount << ",  Destination = " << destination << endl;
			cout << (long) flitEx << endl;
			cout << "Message:" << flitEx->flitId << "  This switch: " << this->label << "  Source Switch:"
					<< flitEx->sourceId << "   Destination switch:" << destination << endl;
			cout << "destId" << flitEx->destId << " destSwitch" << flitEx->destSwitch << " destGroup"
					<< flitEx->destGroup << " flitId" << flitEx->flitId << "val" << flitEx->val << "valId"
					<< flitEx->valId << "localMisroutingPetitionDone" << flitEx->localMisroutingPetitionDone
					<< "globalMisroutingPetitionDone" << flitEx->globalMisroutingPetitionDone << "localMisroutingDone"
					<< flitEx->localMisroutingDone << "globalMisroutingDone" << "sourceId" << flitEx->sourceId
					<< "sourceSW" << flitEx->sourceSW << "sourceGroup" << flitEx->sourceGroup << "hopCount"
					<< flitEx->hopCount << endl;
		}
		assert(destination < g_generatorsCount);
		assert(valNode < g_generatorsCount);

		nextP = this->tableIn[destination];
		outP = this->tableOut[destination];
		nextPval = this->tableIn[valNode];
		outPval = this->tableOut[valNode];

		if ((g_rings != 0) && (g_embeddedRing == 0)) {
			assert(nextP < (portCount - 2));
			assert(outP < (portCount - 2));
			assert(nextPval < (portCount - 2));
			assert(outPval < (portCount - 2));
		} else {
			assert(outP < (portCount));
			assert(nextP < (portCount));
			assert(nextPval < (portCount));
			assert(outPval < (portCount));
		}
		assert(outP >= 0);
		assert(nextP >= 0);
		assert(outPval >= 0);
		assert(nextPval >= 0);

		nextC = 0;
		if ((g_dally == 1)) {
			nextC = flitEx->nextChannel(input_port, outP);
		}
		assert(nextC < g_channels);
		nextCval = 0;
		if ((g_ugal == 1) && (not (flitEx->valNodeReached))) {
			nextCval = flitEx->nextChannel(input_port, outPval);
		}
		assert(nextCval < g_channels);

		if ((g_ugal == 1) && (input_port < g_dP)) {
			if (outP >= g_dP) {
				minQueue_length = getCreditsOccupancy(outP, nextC);
				valQueue_length = getCreditsOccupancy(outPval, nextCval);

				if ((g_ugal == 1) && (input_port < g_dP)) {
					//UGAL-L condition
					if ((minQueue_length * (flitEx->minPath_length))
							<= (valQueue_length * (flitEx->valPath_length)) + g_flitSize * g_ugal_local_threshold) { //Minimal path * minimal queue occupancy
						flitEx->val = 0;
					} else { //Valiant path * valiant queue occupancy
						flitEx->val = 1;
						flitEx->setMisrouted(true);
					}

					//UGAL-G condition (via piggybacking):
					// to minimally route both restrictions
					// (local AND global) have to be accomplished
					// at the same time
					if (g_piggyback && isGlobalLinkCongested(flitEx)) {
						flitEx->val = 1;
						flitEx->setMisrouted(true);
					}
				} else {
					flitEx->val = 0;
				}

			}
		}

		if (g_valiant == 1) {
			flitEx->val = 1;
			flitEx->setMisrouted(true);
		}

		if ((g_dally == 1) && ((g_valiant == 1) || ((g_ugal == 1) && (flitEx->val == 1)))) {

			int valH = int(valNode / (g_dA * g_dP));
			int valP = module(valNode, g_dP);
			int valA = (valNode - valP - valH * g_dP * g_dA) / g_dP;
			assert(valP < g_dP);
			assert(valA < g_dA);
			assert(valH < ((g_dA * g_dH) + 1));
			if (((g_valiant_long == false) && (this->hPos == valH))
					|| ((g_valiant_long == true) && (this->label == (valNode / g_dP)))) {
				flitEx->valNodeReached = 1;
			}
			if (flitEx->valNodeReached == 0) {
				nextP = nextPval;
				outP = outPval;
				assert(nextP == neighPort[outP]);

				if ((g_rings != 0) && (g_channels_ring == 0)) {
					assert(nextP < (portCount - 2));
					assert(outP < (portCount - 2));
				} else {
					assert(outP < (portCount));
					assert(nextP < (portCount));
				}
				assert(outP >= 0);
				assert(nextP >= 0);
			}
		} else {
			if ((g_dally == 1) && not (g_vc_misrouting)
					&& ((g_valiant == 0) || ((g_ugal == 1) && (flitEx->val == 0)))) {
				if (outP == input_port) {
					cout << "Flit=" << flitEx->flitId << "  flitSource=" << flitEx->sourceId << "  flitDest="
							<< flitEx->destId << "  thisSwitch=" << this->label << "  outP=" << outP << "  input_port="
							<< input_port << endl;
				}
				assert(outP != input_port); // Check that output port is not same as input. Can NOT be done under minimal routing.
			}
		}

		if ((g_rings != 0) && (g_channels_ring == 0)) {
			assert(outP < (portCount - 2));
		} else {
			assert(outP < (portCount));
		}

		nextC = 0;
		if ((g_dally == 1)) {
			nextC = flitEx->nextChannel(input_port, outP);
		} else {
			crd = 0;
			for (k = 0; k < g_channels_useful; k++) {
				cred = getCredits(outP, k);
				if (cred > crd) {
					crd = cred;
					nextC = k;
				}
			}
		}
	} else {
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
		outP = pBuffers[input_port][input_channel]->m_outPort_currentPkt;
		nextP = neighPort[outP];
		nextC = pBuffers[input_port][input_channel]->m_nextVC_currentPkt;
	}
	if (outP < g_dP) { // Consumption
		r = tryConsume(input_port, input_channel, outP, flitEx);
		return (r);
	} else { //Transit
		r = tryTransit(input_port, input_channel, outP, nextP, nextC, flitEx);
		return (r);
	}
}

bool switchModule::couldTryPetition(int input_port, int input_channel) {
	int destination, nextP, outP, nextPval, outPval, valNode, nextC, nextCval, minQueue_length, valQueue_length, k, crd,
			cred;
	flitModule *flitEx;
	bool r;

	assert(!this->pBuffers[input_port][input_channel]->emptyBuffer());
	pBuffers[input_port][input_channel]->checkFlit(flitEx);
	assert(flitEx != NULL);
	destination = flitEx->destId;
	valNode = flitEx->valId;
	flitEx->localMisroutingPetitionDone = 0;
	flitEx->local_mm_MisroutingPetitionDone = 0;
	flitEx->globalMisroutingPetitionDone = 0;

	if (flitEx->head == true) {
		if (destination >= g_generatorsCount) {
			cout << "Generators count = " << g_generatorsCount << ",  Destination = " << destination << endl;
			cout << (long) flitEx << endl;
			cout << "Message:" << flitEx->flitId << "  This switch: " << this->label << "  Source Switch:"
					<< flitEx->sourceId << "   Destination switch:" << destination << endl;
			cout << "destId" << flitEx->destId << " destSwitch" << flitEx->destSwitch << " destGroup"
					<< flitEx->destGroup << " flitId" << flitEx->flitId << "val" << flitEx->val << "valId"
					<< flitEx->valId << "localMisroutingPetitionDone" << flitEx->localMisroutingPetitionDone
					<< "globalMisroutingPetitionDone" << flitEx->globalMisroutingPetitionDone << "localMisroutingDone"
					<< flitEx->localMisroutingDone << "globalMisroutingDone" << "sourceId" << flitEx->sourceId
					<< "sourceSW" << flitEx->sourceSW << "sourceGroup" << flitEx->sourceGroup << "hopCount"
					<< flitEx->hopCount << endl;
		}
		assert(destination < g_generatorsCount);
		assert(valNode < g_generatorsCount);

		nextP = this->tableIn[destination];
		outP = this->tableOut[destination];
		nextPval = this->tableIn[valNode];
		outPval = this->tableOut[valNode];

		if ((g_rings != 0) && (g_embeddedRing == 0)) {
			assert(nextP < (portCount - 2));
			assert(outP < (portCount - 2));
			assert(nextPval < (portCount - 2));
			assert(outPval < (portCount - 2));
		} else {
			assert(outP < (portCount));
			assert(nextP < (portCount));
			assert(nextPval < (portCount));
			assert(outPval < (portCount));
		}
		assert(outP >= 0);
		assert(nextP >= 0);
		assert(outPval >= 0);
		assert(nextPval >= 0);

		nextC = 0;
		if ((g_dally == 1)) {
			nextC = flitEx->nextChannel(input_port, outP);
		}
		assert(nextC < g_channels);
		nextCval = 0;
		if ((g_ugal == 1) && (not (flitEx->valNodeReached))) {
			nextCval = flitEx->nextChannel(input_port, outPval);
		}
		assert(nextCval < g_channels);

		if ((g_ugal == 1) && (input_port < g_dP)) {
			if (outP >= g_dP) {
				minQueue_length = getCreditsOccupancy(outP, nextC);
				valQueue_length = getCreditsOccupancy(outPval, nextCval);

				if ((g_ugal == 1) && (input_port < g_dP)) {
					//UGAL-L condition
					if ((minQueue_length * (flitEx->minPath_length))
							<= (valQueue_length * (flitEx->valPath_length)) + g_flitSize * g_ugal_local_threshold) { //Minimal path * minimal queue occupancy
						flitEx->val = 0;
					} else { //Valiant path * valiant queue occupancy
						flitEx->val = 1;
						flitEx->setMisrouted(true);
					}

					//UGAL-G condition (via piggybacking)
					if (g_piggyback && isGlobalLinkCongested(flitEx)) {
						flitEx->val = 1;
						flitEx->setMisrouted(true);
					}
				} else {
					flitEx->val = 0;
				}
			}
		}

		if (g_valiant == 1) {
			flitEx->val = 1;
			flitEx->setMisrouted(true);
		}

		if ((g_dally == 1) && ((g_valiant == 1) || ((g_ugal == 1) && (flitEx->val == 1)))) {

			int valH = int(valNode / (g_dA * g_dP));
			int valP = module(valNode, g_dP);
			int valA = (valNode - valP - valH * g_dP * g_dA) / g_dP;
			assert(valP < g_dP);
			assert(valA < g_dA);
			assert(valH < ((g_dA * g_dH) + 1));
			if (((g_valiant_long == false) && (this->hPos == valH))
					|| ((g_valiant_long == true) && (this->label == (valNode / g_dP)))) {
				flitEx->valNodeReached = 1;
			}
			if (flitEx->valNodeReached == 0) {
				nextP = nextPval;
				outP = outPval;
				assert(nextP == neighPort[outP]);

				if ((g_rings != 0) && (g_channels_ring == 0)) {
					assert(nextP < (portCount - 2));
					assert(outP < (portCount - 2));
				} else {
					assert(outP < (portCount));
					assert(nextP < (portCount));
				}
				assert(outP >= 0);
				assert(nextP >= 0);
			}
		} else {
			if ((g_dally == 1) && not (g_vc_misrouting)
					&& ((g_valiant == 0) || ((g_ugal == 1) && (flitEx->val == 0)))) {
				if (outP == input_port) {
					cout << "Flit=" << flitEx->flitId << "  flitSource=" << flitEx->sourceId << "  flitDest="
							<< flitEx->destId << "  thisSwitch=" << this->label << "  outP=" << outP << "  input_port="
							<< input_port << endl;
				}
				assert(outP != input_port);
			}
		}

		if ((g_rings != 0) && (g_channels_ring == 0)) {
			assert(outP < (portCount - 2));
		} else {
			assert(outP < (portCount));
		}

		nextC = 0;
		if ((g_dally == 1)) {
			nextC = flitEx->nextChannel(input_port, outP);
		} else {
			crd = 0;
			for (k = 0; k < g_channels_useful; k++) {
				cred = getCredits(outP, k);
				if (cred > crd) {
					crd = cred;
					nextC = k;
				}
			}
		}
	} else {
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
		outP = pBuffers[input_port][input_channel]->m_outPort_currentPkt;
		nextP = neighPort[outP];
		nextC = pBuffers[input_port][input_channel]->m_nextVC_currentPkt;
	}
	if (outP < g_dP) { // Consumption
		r = couldTryConsume(input_port, input_channel, outP, flitEx);
		return (r);
	} else { // Transit
		r = couldTryTransit(input_port, input_channel, outP, nextP, nextC, flitEx);
		return (r);
	}
}

bool switchModule::tryConsume(int input_port, int input_channel, int outP, flitModule *flitEx) {

	int can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();
	assert(can_send_flit == 1);

	if ((g_cycle >= (this->lastConsumeCycle[outP] + g_flitSize))) {
		this->globalArbiters[outP]->petitions[input_port] = 1;
		this->globalArbiters[outP]->inputChannels[input_port] = input_channel;
		this->globalArbiters[outP]->nextChannels[input_port] = 0;
		this->globalArbiters[outP]->nextPorts[input_port] = 0;
		return (1);
	} else {
		this->globalArbiters[outP]->petitions[input_port] = 0;
		return (0);

	}
}

bool switchModule::couldTryConsume(int input_port, int input_channel, int outP, flitModule *flitEx) {

	int can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();
	assert(can_send_flit == 1);

	if ((g_cycle >= (this->lastConsumeCycle[outP] + g_flitSize))) {
		return (1);
	} else {
		return (0);

	}
}

bool switchModule::tryTransit(int input_port, int input_channel, int outP, int nextP, int nextC, flitModule *flitEx) {
	int cred, crd, bub, k, dist;
	int selectedPort, selectedVC;
	bool can_send_flit, can_receive_flit, canMisroute, nextVC_unLocked, input_vc_emb_ring, input_vc_emb_tree,
			input_phy_ring, input_escape_network;
	MisrouteType misroute_type = NONE;
	cred = 0;
	crd = 0;
	dist = 0;
	canMisroute = false;

	input_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_ring);

	input_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_tree);

	input_phy_ring = (g_rings != 0) && (g_embeddedRing == 0) && (input_port >= (this->portCount - 2));

	assert(outP >= g_dP);

	if ((input_port < g_dP) && (g_baseCongestionControl) && ((g_trees != 0) || (g_rings != 0))) {
		bub = g_baseCongestionControl_bub; // Priorize transit forcing to be 3 free slots to inyect
	} else {
		bub = 0;
	}

	if (flitEx->head == true) {
		// RING MISROUTING
		if (g_dally == 0) { // When not using virtual channels as proposed by Dally
			canMisroute = OFARMisrouteCandidate(flitEx, input_port, input_channel, outP, nextC, selectedPort,
					selectedVC, misroute_type);
			if (canMisroute) {
				outP = selectedPort;
				nextC = selectedVC;
				nextP = neighPort[outP];

				// Set wich petition is done [could be done in vcMisrouteCandidate, but
				//	is kept here because is in tryTransit() where the petitionDone stats
				//	will be modified for the other cases]
				switch (misroute_type) {
					case LOCAL:
						flitEx->localMisroutingPetitionDone = 1;
						break;
					case LOCAL_MM:
						flitEx->localMisroutingPetitionDone = 1;
						flitEx->local_mm_MisroutingPetitionDone = 1;
						break;
					case GLOBAL:
					case GLOBAL_MANDATORY:
						flitEx->globalMisroutingPetitionDone = 1;
						break;
					case NONE:
					default:
						//should never enter here
						assert(false);
						break;
				}

				if (g_DEBUG) {
					cout << "\t\tSELECTED candidate (flit " << flitEx->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << endl;
				}
			} else {
				if (g_DEBUG) {
					cout << "\t Can NOT misroute" << endl;
				}
			}

			if ((g_rings != 0) && (g_embeddedRing == 0)) {
				assert(outP < (portCount - 2));
			} else {
				assert(outP < (portCount));
			}
			assert(nextC < (vcCount));

			crd = getCredits(outP, nextC);
			input_escape_network = (input_vc_emb_ring || input_vc_emb_tree || input_phy_ring);

			if ((g_try_just_escape)
					|| ((not ((g_forbid_from_injQueues_to_ring == 1) && (input_port < g_dP)) && (crd == 0))
							|| ((g_restrictLocalCycles) && (input_escape_network) && (outP < g_offsetH)))) {
				// Through the ring if there aren't any slots through misroute and we aren't directly injecting, being forbidden
				// OR if we have to stay in the ring before changing group
				if (g_rings != 0) {
					if (g_embeddedRing == 0) {

						can_receive_flit = neighList[outP]->portCanReceiveFlit(neighPort[outP]);
						dist = module((flitEx->destSwitch - this->label), g_switchesCount); //distance to the destination node through the ring
						assert(dist != 0); // if dist=0, we have reached destination node and should not be trying to transit
						/* Unidirectional ring						*/

						if (g_ringDirs == 1) {
							nextP = this->portCount - 2; // ring, net input port 23
							outP = this->portCount - 1; // ring output port 24
						}
						/* Bidirectional ring						 */
						if (g_ringDirs == 2) {
							if ((dist == g_switchesCount / 2) && ((g_switchesCount % 2) == 0)) {
								dist = rand() % 2;
							}
							if ((dist > int(g_switchesCount / 2)) || (dist == 0)) { // go to the left
								nextP = this->portCount - 1;
								outP = this->portCount - 2;
							} else { // go to the right
								nextP = this->portCount - 2;
								outP = this->portCount - 1;
							}
						}
						if (input_port < (this->portCount - 2)) {
							bub = g_BUBBLE; // When injecting we need to leave slot for bubble
							if ((g_baseCongestionControl) && (input_port < (g_dP))) bub = g_baseCongestionControl_bub; // when injecting in the net there always has to be another slot!!
						} else if (input_port >= (this->portCount - 2)) {
							bub = 0; // when transitting the ring there's no need to apply bubble
						}

						crd = 0;
						for (k = 0; k < g_channels_useful; k++) {
							cred = getCredits(outP, k);
							if (cred > crd) {
								crd = cred;
								nextC = k;
							}
						}
					}
					if (g_embeddedRing == 1) {

						if (flitEx->assignedRing == 1) {
							assert(g_rings > 0);
							nextP = this->tableInRing1[flitEx->destId]; //Minimal path
							outP = this->tableOutRing1[flitEx->destId]; // Minimal path
						} else if (flitEx->assignedRing == 2) {
							assert(g_rings > 1);
							nextP = this->tableInRing2[flitEx->destId]; //Minimal path
							outP = this->tableOutRing2[flitEx->destId]; // Minimal path
						} else {
							assert(g_rings == 0);
						}

						if (input_channel < (g_channels_useful)) {
							bub = g_BUBBLE; // Injection -->> Bubble
						} else {
							bub = 0; // if transitting through the ring, just check for slot to place the flit
						}
						if ((g_baseCongestionControl) && (input_port < (g_dP))) bub = g_baseCongestionControl_bub;

						crd = 0;
						for (k = g_channels_useful; k < g_channels_useful + g_channels_ring; k++) {
							cred = getCredits(outP, k);
							if (cred >= crd) {
								crd = cred;
								nextC = k;
							}
						}
						assert(nextC >= g_channels_useful);

					}

				}

				if (g_trees != 0) { // if no credits ---> Escape tree
					if (g_embeddedTree == 1) {
						if (g_trees == 1) {
							nextP = this->tableInTree[flitEx->destId]; //Minimal path
							outP = this->tableOutTree[flitEx->destId]; // Minimal path
						}

						crd = 0;
						for (k = g_channels_useful; k < (g_channels_useful + g_channels_tree); k++) {
							cred = getCredits(outP, k);
							if (cred >= crd) {
								crd = cred;
								nextC = k;
							}
						}
						assert(nextC >= g_channels_useful);
						assert(nextC < g_channels);
						assert(nextP >= g_offsetA);
						assert(outP >= g_offsetA);
						assert(nextP < portCount);
						assert(outP < portCount);
					}
				}
			}
		}

		// VC MISROUTING
		else if (g_vc_misrouting) {

			canMisroute = vcMisrouteCandidate(flitEx, input_port, outP, nextC, selectedPort, selectedVC, misroute_type);
			if (canMisroute) {

				outP = selectedPort;
				nextC = selectedVC;
				nextP = neighPort[outP];
				crd = getCredits(outP, nextC);

				// Set wich petition is done
				switch (misroute_type) {
					case LOCAL:
						flitEx->localMisroutingPetitionDone = 1;
						break;
					case LOCAL_MM:
						flitEx->localMisroutingPetitionDone = 1;
						flitEx->local_mm_MisroutingPetitionDone = 1;
						break;
					case GLOBAL:
					case GLOBAL_MANDATORY:
						flitEx->globalMisroutingPetitionDone = 1;
						break;
					case NONE:
					default:
						//should never enter here
						assert(false);
						break;
				}

				if (g_DEBUG) {
					cout << "\t\tSELECTIONED candidate (flit " << flitEx->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << endl;
				}
			} else {
				if (g_DEBUG) {
					cout << "\tCan NOT misroute " << endl;
				}
			}
		}

		// Note flit's current misrouting type
		flitEx->setCurrent_misroute_type(misroute_type);
	} else {
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
	}
	can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();
	assert((can_send_flit == 1));

	can_receive_flit = neighList[outP]->portCanReceiveFlit(neighPort[outP]);

	if (neighList[outP]->pBuffers[neighPort[outP]][nextC]->unLocked()) { //if blocked vc
		nextVC_unLocked = true;
	} else if (flitEx->packetId == neighList[outP]->pBuffers[neighPort[outP]][nextC]->m_pktLock) {
		nextVC_unLocked = true;
	} else {
		nextVC_unLocked = false;
	}

	crd = getCredits(outP, nextC);

	bool output_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_ring);

	bool output_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_tree);

	/* If we have mandatory misrouting and are not asking either global or ring,
	 * can not ask any port.     */
	if (flitEx->mandatoryGlobalMisrouting_flag && outP < g_offsetH && not (output_vc_emb_tree)
			&& not (output_vc_emb_ring)) {

		assert(hPos != flitEx->destGroup);
		assert(hPos == flitEx->sourceGroup);
		this->globalArbiters[outP]->petitions[input_port] = 0;
		if (g_DEBUG) {
			cout << "TRY_TRANSIT: cycle " << g_cycle << "--> SW " << label << " input Port " << input_port << " CV "
					<< flitEx->channel << " flit " << flitEx->flitId << " destSW " << flitEx->destSwitch
					<< " min-output Port " << min_outport(flitEx)
					<< " : NO hace peticion (MANDATORY_GLOBAL restriction)" << endl << endl;
		}
		return (0);
	}

	/* Normal case, make petition */
	else if ((this->out_port_used[outP] == 0) && (crd >= ((1 + bub) * g_flitSize)) && (can_receive_flit == 1)
			&& (nextVC_unLocked == 1)) { //Si hay hueco para 1 mensaje mas para los que indique la burbuja
		assert(this->out_port_used[outP] == 0);
		this->globalArbiters[outP]->petitions[input_port] = 1;
		this->globalArbiters[outP]->nextChannels[input_port] = nextC;
		this->globalArbiters[outP]->nextPorts[input_port] = nextP;
		this->globalArbiters[outP]->inputChannels[input_port] = input_channel;

		if (g_DEBUG) {
			cout << "TRY_TRANSIT: cycle " << g_cycle << "--> SW " << label << " input Port " << input_port << " CV "
					<< flitEx->channel << " flit " << flitEx->flitId << " destSW " << flitEx->destSwitch
					<< " min-output Port " << min_outport(flitEx) << " output Port " << outP << " CV " << nextC << endl
					<< endl;
		}
		return (1);
	}
	/* any other case, don't make petition */
	else {
		this->globalArbiters[outP]->petitions[input_port] = 0;

		if (g_DEBUG) {
			cout << "TRY_TRANSIT: cycle " << g_cycle << "--> SW " << label << " input Port " << input_port << " CV "
					<< flitEx->channel << " flit " << flitEx->flitId << " destSW " << flitEx->destSwitch
					<< " min-output Port " << min_outport(flitEx) << " : NO hace peticion";
			if (not (this->out_port_used[outP] == 0)) cout << " (puerto ocupado)";
			if (not (crd >= ((1 + bub) * g_flitSize))) cout << " (sin creditos)";
			if (not (can_receive_flit == 1)) cout << " (enlace ocupado)";
			cout << "." << endl << endl;
		}
		return (0);
	}

}

bool switchModule::couldTryTransit(int input_port, int input_channel, int outP, int nextP, int nextC,
		flitModule *flitEx) {
	int cred, crd, bub, k, dist;
	int selectedPort, selectedVC;
	bool can_send_flit, can_receive_flit, canMisroute, nextVC_unLocked, input_vc_emb_ring, input_vc_emb_tree,
			input_phy_ring, input_escape_network;
	MisrouteType misroute_type = NONE;
	cred = 0;
	crd = 0;
	dist = 0;
	canMisroute = false;

	input_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_ring);

	input_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_tree);

	input_phy_ring = (g_rings != 0) && (g_embeddedRing == 0) && (input_port >= (this->portCount - 2));

	assert(outP >= g_dP);

	if ((input_port < g_dP) && (g_baseCongestionControl) && ((g_trees != 0) || (g_rings != 0))) {
		bub = g_baseCongestionControl_bub; // Priorize transit: require 3 slots to inject
	} else {
		bub = 0;
	}

	if (flitEx->head == true) {
		// RING MISROUTING
		if (g_dally == 0) { // when NOT using vcs as Dally.

			canMisroute = OFARMisrouteCandidate(flitEx, input_port, input_channel, outP, nextC, selectedPort,
					selectedVC, misroute_type);

			if (canMisroute) {

				outP = selectedPort;
				nextC = selectedVC;
				nextP = neighPort[outP];

				// Set wich petition is done
				switch (misroute_type) {
					case LOCAL:
						flitEx->localMisroutingPetitionDone = 1;
						break;
					case LOCAL_MM:
						flitEx->localMisroutingPetitionDone = 1;
						flitEx->local_mm_MisroutingPetitionDone = 1;
						break;
					case GLOBAL:
					case GLOBAL_MANDATORY:
						flitEx->globalMisroutingPetitionDone = 1;
						break;
					case NONE:
					default:
						//should never enter here
						assert(false);
						break;
				}
				if (g_DEBUG) {
					cout << "\t\tSELECTIONED candidate (flit " << flitEx->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << endl;
				}
			} else {
				if (g_DEBUG) {
					cout << "\tCan NOT misroute " << endl;
				}
			}

			if ((g_rings != 0) && (g_embeddedRing == 0)) {
				assert(outP < (portCount - 2));
			} else {
				assert(outP < (portCount));
			}
			assert(nextC < (vcCount));

			crd = getCredits(outP, nextC);
			input_escape_network = (input_vc_emb_ring || input_vc_emb_tree || input_phy_ring);

			if ((g_try_just_escape)
					|| ((not ((g_forbid_from_injQueues_to_ring == 1) && (input_port < g_dP)) && (crd == 0))
							|| ((g_restrictLocalCycles) && (input_escape_network) && (outP < g_offsetH)))) {
				if (g_rings != 0) {
					if (g_embeddedRing == 0) {

						can_receive_flit = neighList[outP]->portCanReceiveFlit(neighPort[outP]);
						dist = module((flitEx->destSwitch - this->label), g_switchesCount);
						assert(dist != 0);
						if (g_ringDirs == 1) { //Unidirectional ring
							nextP = this->portCount - 2; // ring, net input port 23
							outP = this->portCount - 1; // ring output port 24
						}
						if (g_ringDirs == 2) { //bidirectional ring
							if ((dist == g_switchesCount / 2) && ((g_switchesCount % 2) == 0)) {
								dist = rand() % 2;
							}
							if ((dist > int(g_switchesCount / 2)) || (dist == 0)) {
								nextP = this->portCount - 1;
								outP = this->portCount - 2;
							} else {
								nextP = this->portCount - 2;
								outP = this->portCount - 1;
							}
						}
						if (input_port < (this->portCount - 2)) {
							bub = g_BUBBLE;
							if ((g_baseCongestionControl) && (input_port < (g_dP))) bub = g_baseCongestionControl_bub;
						} else if (input_port >= (this->portCount - 2)) {
							bub = 0;
						}

						crd = 0;
						for (k = 0; k < g_channels_useful; k++) {
							cred = getCredits(outP, k);
							if (cred > crd) {
								crd = cred;
								nextC = k;
							}
						}
					}
					if (g_embeddedRing == 1) {

						if (flitEx->assignedRing == 1) {
							assert(g_rings > 0);
							nextP = this->tableInRing1[flitEx->destId]; //Minimal path
							outP = this->tableOutRing1[flitEx->destId]; // Minimal path
						} else if (flitEx->assignedRing == 2) {
							assert(g_rings > 1);
							nextP = this->tableInRing2[flitEx->destId]; //Minimal path
							outP = this->tableOutRing2[flitEx->destId]; // Minimal path
						} else {
							assert(g_rings == 0);
						}

						if (input_channel < (g_channels_useful)) {
							bub = g_BUBBLE;
						} else {
							bub = 0;
						}
						if ((g_baseCongestionControl) && (input_port < (g_dP))) bub = g_baseCongestionControl_bub;

						crd = 0;
						for (k = g_channels_useful; k < g_channels_useful + g_channels_ring; k++) {
							cred = getCredits(outP, k);
							if (cred >= crd) {
								crd = cred;
								nextC = k;
							}
						}
						assert(nextC >= g_channels_useful);

					}

				}

				if (g_trees != 0) { // if no credits ---> Escape tree
					if (g_embeddedTree == 1) {
						if (g_trees == 1) {
							nextP = this->tableInTree[flitEx->destId]; //Minimal path
							outP = this->tableOutTree[flitEx->destId]; // Minimal path
						}

						crd = 0;
						for (k = g_channels_useful; k < (g_channels_useful + g_channels_tree); k++) {
							cred = getCredits(outP, k);
							if (cred >= crd) {
								crd = cred;
								nextC = k;
							}
						}
						assert(nextC >= g_channels_useful);
						assert(nextC < g_channels);
						assert(nextP >= g_offsetA);
						assert(outP >= g_offsetA);
						assert(nextP < portCount);
						assert(outP < portCount);
					}
				}
			}
		}

		// VC MISROUTING
		else if (g_vc_misrouting) {

			canMisroute = vcMisrouteCandidate(flitEx, input_port, outP, nextC, selectedPort, selectedVC, misroute_type);
			if (canMisroute) {

				outP = selectedPort;
				nextC = selectedVC;
				nextP = neighPort[outP];
				crd = getCredits(outP, nextC);

				// Set wich petition is done
				switch (misroute_type) {
					case LOCAL:
						flitEx->localMisroutingPetitionDone = 1;
						break;
					case LOCAL_MM:
						flitEx->localMisroutingPetitionDone = 1;
						flitEx->local_mm_MisroutingPetitionDone = 1;
						break;
					case GLOBAL:
					case GLOBAL_MANDATORY:
						flitEx->globalMisroutingPetitionDone = 1;
						break;
					case NONE:
					default:
						//should never enter here
						assert(false);
						break;
				}

				if (g_DEBUG) {
					cout << "\t\tSELECTED candidate (flit " << flitEx->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << endl;
				}
			} else {
				if (g_DEBUG) {
					cout << "\tCan NOT misroute " << endl;
				}
			}
		}

		// Note flit's current misrouting type
		flitEx->setCurrent_misroute_type(misroute_type);
	} else {
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
	}
	can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();
	assert((can_send_flit == 1));

	can_receive_flit = neighList[outP]->portCanReceiveFlit(neighPort[outP]);

	if (neighList[outP]->pBuffers[neighPort[outP]][nextC]->unLocked()) {
		nextVC_unLocked = true;
	} else if (flitEx->packetId == neighList[outP]->pBuffers[neighPort[outP]][nextC]->m_pktLock) {
		nextVC_unLocked = true;
	} else {
		nextVC_unLocked = false;
	}

	crd = getCredits(outP, nextC);

	bool output_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_ring);

	bool output_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_tree);

	if (flitEx->mandatoryGlobalMisrouting_flag && outP < g_offsetH && not (output_vc_emb_tree)
			&& not (output_vc_emb_ring)) {
		return (0);
	}

	else if ((this->out_port_used[outP] == 0) && (crd >= ((1 + bub) * g_flitSize)) && (can_receive_flit == 1)
			&& (nextVC_unLocked == 1)) { //Si hay hueco para 1 mensaje mas para los que indique la burbuja

		return (1);
	} else {
		return (0);
	}

}

/* 
 * Returns true if there is a valid link to misroute through.
 * It figures out wether to use global or local misrouting,
 * according to the configuration and flit state.
 * Selected Port and VC are stored in the argument references.
 * 
 * @param flitModule * flit: message to misroute.
 * 
 * @param (IN) int input_port: input_port where flit is stored
 * 
 * @param (IN) int outP: original output port.
 * 
 * @param (IN) int outVC: original output virtual channel
 * 
 * @param (OUT) int & selectedPort: reference to the variable where the selected port is saved
 * 
 * @param (OUT) int & selectedVC: reference to the variable where the selected port is saved
 * 
 * @param (OUT) MisrouteType & misroute_type: reference to the variable where the misroute type port is saved
 * 
 * @Return true if there is a valid link to misroute through
 */
bool switchModule::vcMisrouteCandidate(flitModule * flit, int input_port, int prev_outP, int prev_nextC,
		int &selectedPort, int &selectedVC, MisrouteType &misroute_type) {

	bool result = false;

	assert(prev_outP == min_outport(flit));

	//MisrouteCondition allows misrouting
	if (misrouteCondition(flit, prev_outP, prev_nextC)) {

		misroute_type = misrouteType(input_port, flit, prev_outP, prev_nextC);

		switch (misroute_type) {

			case NONE:
				result = false;
				break;

			case LOCAL:
			case LOCAL_MM:
				if (g_strictMisroute == true) {
					if (misroute_type == LOCAL) {
						result = selectStrictMisrouteRandomLocal(flit, prev_outP, prev_nextC, selectedPort, selectedVC);
					} else {
						assert(misroute_type == LOCAL_MM);
						result = selectStrictMisrouteRandomLocal_MM(flit, input_port, prev_outP, prev_nextC,
								selectedPort, selectedVC);
					}
				} else {
					result = selectVcMisrouteRandomLocal(flit, prev_outP, prev_nextC, selectedPort, selectedVC);
				}
				break;

			case GLOBAL:
			case GLOBAL_MANDATORY:
				result = selectVcMisrouteRandomGlobal(flit, prev_outP, prev_nextC, selectedPort, selectedVC);
				break;

			default:
				//should never enter here
				assert(false);
				break;
		}
	}

	return result;
}

bool switchModule::OFARMisrouteCandidate(flitModule * flit, int input_port, int input_channel, int prev_outP,
		int prev_nextC, int &selectedPort, int &selectedVC, MisrouteType &misroute_type) {

	bool result = false;

	assert(prev_outP == min_outport(flit));

	//MisrouteCondition allows misrouting
	if (misrouteCondition(flit, prev_outP, prev_nextC)) {

		misroute_type = misrouteTypeOFAR(input_port, input_channel, flit, prev_outP, prev_nextC);

		switch (misroute_type) {

			case NONE:
				result = false;
				break;

			case LOCAL:
			case LOCAL_MM:
				result = selectOFARMisrouteRandomLocal(flit, prev_outP, prev_nextC, selectedPort, selectedVC);
				break;

			case GLOBAL:
			case GLOBAL_MANDATORY:
				result = selectOFARMisrouteRandomGlobal(flit, prev_outP, prev_nextC, selectedPort, selectedVC);
				break;

			default:
				//should never enter here
				assert(false);
				break;
		}
	}
	return result;
}

bool switchModule::servePort(int input_port, int input_channel, int outP, int nextP, int nextC) {
	flitModule *flitEx;
	int k, in_cycle;
	generatorModule * src_generator;

	bool can_receive_flit, can_send_flit, nextVC_unLocked, input_vc_emb_ring, output_vc_emb_ring, input_vc_emb_tree,
			output_vc_emb_tree, input_phy_ring, output_phy_ring;

	input_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_ring);

	output_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_ring);

	input_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= input_channel)
			&& (input_channel < g_channels_useful + g_channels_tree);

	output_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= nextC)
			&& (nextC < g_channels_useful + g_channels_tree);

	input_phy_ring = (g_rings != 0) && (g_embeddedRing == 0) && (input_port >= (this->portCount - 2));

	output_phy_ring = (g_rings != 0) && (g_embeddedRing == 0) && (nextP >= (this->portCount - 2));

	if (g_DEBUG) {
		pBuffers[input_port][input_channel]->checkFlit(flitEx);
		cout << "SERVE PORT: cycle " << g_cycle << "--> SW " << label << " input Port " << input_port << " CV "
				<< input_channel << " flit " << flitEx->flitId << " output Port " << outP << " CV " << nextC;
		if (outP >= g_offsetA) {
			cout << " --> SW " << neighList[outP]->label << " input Port " << neighPort[outP];
		}
		cout << endl << "------------------------------------------------------------" << endl;
		cout << endl << endl;
	}

	pBuffers[input_port][input_channel]->checkFlit(flitEx);

	if (outP < g_dP) { // Packet is consumed
		assert(pBuffers[input_port][input_channel]->canSendFlit());
		assert(g_cycle >= (this->lastConsumeCycle[outP] + g_flitSize));
		assert(pBuffers[input_port][input_channel]->extract(flitEx));

		if (flitEx->head == 1) {
			pBuffers[input_port][input_channel]->m_currentPkt = flitEx->packetId;
			pBuffers[input_port][input_channel]->m_outPort_currentPkt = outP;
			pBuffers[input_port][input_channel]->m_nextVC_currentPkt = nextC;
		}
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
		if (flitEx->tail == 1) {
			pBuffers[input_port][input_channel]->m_currentPkt = -1;
			pBuffers[input_port][input_channel]->m_outPort_currentPkt = -1;
			pBuffers[input_port][input_channel]->m_nextVC_currentPkt = -1;
		}

		/* CONTENTION COUNTERS (time waiting in queues)          */

		// injection
		if (input_port < g_dP) {            // see injection_latency counters
		}            // local link
		else if (input_port < g_offsetH) {
			if (input_vc_emb_ring) {
				flitEx->addLocalRingContention();
			} else if (input_vc_emb_tree) {
				flitEx->addLocalTreeContention();
			} else {
				flitEx->addLocalContention();
			}
		}            // global link
		else if (input_port < g_offsetH + g_dH) {
			if (input_vc_emb_ring) {
				flitEx->addGlobalRingContention();
			} else if (input_vc_emb_tree) {
				flitEx->addGlobalTreeContention();
			} else {
				flitEx->addGlobalContention();
			}
		}            // physical ring, global link
		else if ((g_ring_ports != 0)
				&& (((input_port == this->portCount - 1) && (this->aPos == g_dA - 1))
						|| ((input_port == this->portCount - 2) && (this->aPos == 0)))) {

			flitEx->addGlobalRingContention();
		}            // physical ring, local link
		else {
			flitEx->addLocalRingContention();
		}

		if (input_port < g_dP) {
			assert(flitEx->inj_latency < 0);
			flitEx->inj_latency = g_cycle - flitEx->inCycle;
			assert(flitEx->inj_latency >= 0);
		}

		assert(flitEx->localContentionCount >= 0);
		assert(flitEx->globalContentionCount >= 0);
		assert(flitEx->localRingContentionCount >= 0);
		assert(flitEx->globalRingContentionCount >= 0);
		assert(flitEx->localTreeContentionCount >= 0);
		assert(flitEx->globalTreeContentionCount >= 0);

		if (flitEx->tail == 1) g_packetReceivedCount += 1;
		g_flitReceivedCount += 1;
		g_totalHopCount += flitEx->hopCount;
		g_totalLocalHopCount += flitEx->localHopCount;
		g_totalGlobalHopCount += flitEx->globalHopCount;
		g_totalLocalRingHopCount += flitEx->localRingHopCount;
		g_totalGlobalRingHopCount += flitEx->globalRingHopCount;
		g_totalLocalTreeHopCount += flitEx->localTreeHopCount;
		g_totalGlobalTreeHopCount += flitEx->globalTreeHopCount;
		g_totalSubnetworkInjectionsCount += flitEx->subnetworkInjectionsCount;
		g_totalRootSubnetworkInjectionsCount += flitEx->rootSubnetworkInjectionsCount;
		g_totalSourceSubnetworkInjectionsCount += flitEx->sourceSubnetworkInjectionsCount;
		g_totalDestSubnetworkInjectionsCount += flitEx->destSubnetworkInjectionsCount;
		g_totalLocalContentionCount += flitEx->localContentionCount;
		g_totalGlobalContentionCount += flitEx->globalContentionCount;
		g_totalLocalRingContentionCount += flitEx->localRingContentionCount;
		g_totalGlobalRingContentionCount += flitEx->globalRingContentionCount;
		g_totalLocalTreeContentionCount += flitEx->localTreeContentionCount;
		g_totalGlobalTreeContentionCount += flitEx->globalTreeContentionCount;

		if (g_cycle >= g_warmCycles) {
			g_totalBaseLatency += flitEx->getBase_latency();
		}

		//misrouted flits counters
		if ((flitEx->isMisrouted()) && (g_cycle >= g_warmCycles)) {
			g_misroutedFlitCount += 1;

			if (flitEx->getLocal_misroute_count() > 0) {
				g_local_misroutedFlitCount++;
			}
			if (flitEx->getGlobal_misroute_count() > 0) {
				g_global_misroutedFlitCount++;
			}
		}

		this->lastConsumeCycle[outP] = g_cycle;
		flitEx->outCycle = g_cycle;
		g_flitExLatency = flitEx->outCycle - flitEx->inCycle + g_flitSize;

		if (flitEx->tail == 1) {
			g_packetLatency = flitEx->outCycle - flitEx->inCyclePacket + g_flitSize;
			assert(g_packetLatency >= 1);
		}

		long long lat = flitEx->inj_latency\

				+ (flitEx->localHopCount + flitEx->localRingHopCount + flitEx->localTreeHopCount)
						* g_delayTransmission_local
				+ (flitEx->globalHopCount + flitEx->globalRingHopCount + flitEx->globalTreeHopCount)
						* g_delayTransmission_global + flitEx->localContentionCount + flitEx->globalContentionCount
				+ flitEx->localTreeContentionCount + flitEx->globalTreeContentionCount
				+ flitEx->localRingContentionCount + flitEx->globalRingContentionCount;

		assert(g_flitExLatency == lat + g_flitSize);

		//Latency HISTOGRAM
		if ((g_cycle >= g_warmCycles) & (g_flitExLatency < g_latency_histogram_maxLat)) {

			// when not using VC_MISROUTE, everything is stored in a single histogram
			if (flitEx->head == 1) {
				if (not (g_vc_misrouting)) {
					g_latency_histogram_other_global_misroute[g_flitExLatency]++;
				}
				// otherwise, it is split into three:

				//1- NO GLOBAL MISROUTE
				else if (flitEx->globalHopCount <= 1) {
					assert(flitEx->hopCount <= 5);
					assert(flitEx->localHopCount <= 4);
					assert(flitEx->globalHopCount <= 1);
					assert(flitEx->getGlobal_misroute_count() == 0);
					assert(flitEx->getMandatory_global_misroute_count() == 0);
					g_latency_histogram_no_global_misroute[g_flitExLatency]++;
				}
				//2- GLOBAL MISROUTING AT INJECTION
				else if (flitEx->isGlobal_misroute_at_injection_flag()) {
					assert(flitEx->getMisroute_count() >= 1);
					assert(flitEx->getMisroute_count() <= 3);
					assert(flitEx->getGlobal_misroute_count() == 1);
					assert(flitEx->hopCount <= 6);
					assert(flitEx->localHopCount <= 4);
					assert(flitEx->globalHopCount == 2);
					assert(flitEx->getMandatory_global_misroute_count() == 0);
					g_latency_histogram_global_misroute_at_injection[g_flitExLatency]++;
				}
				//3- OTHER GLOBAL MISROUTE (after local hop in source group)
				else {
					assert(flitEx->getMisroute_count() >= 1);
					assert(flitEx->getMisroute_count() <= 4);
					assert(flitEx->getGlobal_misroute_count() == 1);
					assert(flitEx->hopCount <= 8);
					assert(flitEx->localHopCount <= 6);
					assert(flitEx->globalHopCount == 2);
					g_latency_histogram_other_global_misroute[g_flitExLatency]++;
				}
			}

		}

		if ((g_cycle >= g_warmCycles) && (flitEx->hopCount < g_hops_histogram_maxHops) && (flitEx->head == 1)) {
			g_hops_histogram[flitEx->hopCount]++;
		}

		//Group 0, per switch averaged latency
		if ((g_cycle >= g_warmCycles) && (flitEx->sourceGroup == 0)) {
			assert(flitEx->sourceSW < g_dA);
			g_group0_numFlits[flitEx->sourceSW]++;
			g_group0_totalLatency[flitEx->sourceSW] += lat;
		}
		//Group ROOT, per switch averaged latency
		if ((g_cycle >= g_warmCycles) && (flitEx->sourceGroup == g_rootH)) {
			assert((flitEx->sourceSW >= g_dA * g_rootH) && (flitEx->sourceSW < g_dA * (g_rootH + 1)));
			g_groupRoot_numFlits[flitEx->sourceSW - g_dA * g_rootH]++;
			g_groupRoot_totalLatency[flitEx->sourceSW - g_dA * g_rootH] += lat;
		}

		g_totalLatency += g_flitExLatency;
		if (flitEx->tail == 1) g_totalPacketLatency += g_packetLatency;
		assert(g_totalLatency >= 0);

		if (flitEx->inj_latency < 0) {
			assert(flitEx->inj_latency >= 0);
		}
		assert(flitEx->inj_latency >= 0);
		g_totalInjectionQueueLatency += flitEx->inj_latency;
		assert(g_totalInjectionQueueLatency >= 0);

		//Transient traffic recording
		if (g_transient_traffic) {
			in_cycle = flitEx->inCycle;

			//cycle within recording range
			if ((in_cycle >= g_warmCycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles)
					&& (in_cycle < g_warmCycles + g_transient_traffic_cycle + g_transient_record_num_cycles)) {

				//calculate record index
				k = in_cycle - (g_warmCycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles);
				assert(k < g_transient_record_len);

				//record Transient traffic data
				g_transient_record_num_flits[k] += 1;
				g_transient_record_latency[k] += flitEx->outCycle - flitEx->inCycle;
				if (flitEx->isMisrouted()) {
					g_transient_record_num_misrouted[k] += 1;
				}
			}
		}

		// BURST
		if (g_burst) {
			// locate source generator
			src_generator = g_generatorsList[flitEx->sourceId];
			// Count flit as received
			src_generator->burstFlitReceived();
			if (flitEx->tail == true) src_generator->burstPacketReceived();

			// if all generator messages have been received,
			// count generator finished
			if (src_generator->isBurstReceived()) {
				g_burst_generators_finished_count++;
			}
		}

		// All-to-all
		if (g_AllToAll) {
			// locate source generator
			src_generator = g_generatorsList[flitEx->sourceId];
			// Count flit as received
			src_generator->AllToAllFlitReceived();
			if (flitEx->tail == true) src_generator->AllToAllPacketReceived();

			// if all generator messages have been received,
			// count generator finished
			if (src_generator->isAllToAllReceived()) {
				g_AllToAll_generators_finished_count++;
			}
		}

		// send credits to previos switch
		sendCredits(input_port, input_channel, flitEx->flitId);

		if (g_TRACE_SUPPORT != 0) {            // Adds Event in an ocurred event's list
			event e;
			e.type = RECEPTION;
			e.pid = flitEx->sourceId;
			e.task = flitEx->task;
			e.length = flitEx->length;
			e.mpitype = (enum coll_ev_t) flitEx->mpitype;
			ins_occur(&g_generatorsList[flitEx->destId]->occurs, e);
		}

		delete flitEx;

		increasePortCount(outP);
		return (1);
	} else { // Packet is transmitted
		can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();

		can_receive_flit = neighList[outP]->portCanReceiveFlit(neighPort[outP]);

		assert((can_send_flit == true) && (can_receive_flit == true));

		assert(pBuffers[input_port][input_channel]->extract(flitEx));

		if (neighList[outP]->pBuffers[neighPort[outP]][nextC]->unLocked()) {
			nextVC_unLocked = true;
		} else if (flitEx->packetId == neighList[outP]->pBuffers[neighPort[outP]][nextC]->m_pktLock) {
			nextVC_unLocked = true;
		} else {
			nextVC_unLocked = false;
		}
		assert(nextVC_unLocked == true);

		if (flitEx->head == 1) {
			pBuffers[input_port][input_channel]->m_currentPkt = flitEx->packetId;
			pBuffers[input_port][input_channel]->m_outPort_currentPkt = outP;
			pBuffers[input_port][input_channel]->m_nextVC_currentPkt = nextC;
		}
		assert(flitEx->packetId == pBuffers[input_port][input_channel]->m_currentPkt);
		if (flitEx->tail == 1) {
			pBuffers[input_port][input_channel]->m_currentPkt = -1;
			pBuffers[input_port][input_channel]->m_outPort_currentPkt = -1;
			pBuffers[input_port][input_channel]->m_nextVC_currentPkt = -1;
		}
		assert(flitEx != NULL);

		flitEx->addHop();

		/*
		 * CONTENTION COUNTERS (time waiting in queues)
		 * INPUT
		 */

		// injection
		if (input_port < g_dP) {
			// see injection_latency counters
		} // local link
		else if (input_port < g_offsetH) {
			if (input_vc_emb_ring) {
				flitEx->addLocalRingContention();
			} else if (input_vc_emb_tree) {
				flitEx->addLocalTreeContention();
			} else {
				flitEx->addLocalContention();
			}
		} // global link
		else if (input_port < g_offsetH + g_dH) {
			if (input_vc_emb_ring) {
				flitEx->addGlobalRingContention();
			} else if (input_vc_emb_tree) {
				flitEx->addGlobalTreeContention();
			} else {
				flitEx->addGlobalContention();
			}
		} // physical ring, global link
		else if ((g_ring_ports != 0)
				&& (((input_port == this->portCount - 1) && (this->aPos == g_dA - 1))
						|| ((input_port == this->portCount - 2) && (this->aPos == 0)))) {

			flitEx->addGlobalRingContention();
		} // physical ring, local link
		else {
			flitEx->addLocalRingContention();
		}

		assert(getCredits(outP, nextC) >= g_flitSize);

		neighList[outP]->insertFlit(nextP, nextC, flitEx);

		// DECREMENT CREDIT COUNTER IN LOCAL SWITCH
		credits[outP][nextC] = credits[outP][nextC] - g_flitSize;

		// SEND CREDITS TO PREVIOUS SWITCH
		sendCredits(input_port, input_channel, flitEx->flitId);

		if (flitEx->head == 1) {
			neighList[outP]->pBuffers[nextP][nextC]->m_pktLock = flitEx->packetId;
			neighList[outP]->pBuffers[nextP][nextC]->m_portLock = input_port;
			neighList[outP]->pBuffers[nextP][nextC]->m_vcLock = input_channel;
			neighList[outP]->pBuffers[nextP][nextC]->m_unLocked = 0;
		}
		assert(flitEx->packetId == neighList[outP]->pBuffers[nextP][nextC]->m_pktLock);
		if (flitEx->tail == 1) {
			neighList[outP]->pBuffers[nextP][nextC]->m_pktLock = -1;
			neighList[outP]->pBuffers[nextP][nextC]->m_portLock = -1;
			neighList[outP]->pBuffers[nextP][nextC]->m_vcLock = -1;
			neighList[outP]->pBuffers[nextP][nextC]->m_unLocked = 1;
		}

		/*
		 * CONTENTION COUNTERS (time waiting in queues)
		 * OUTPUT
		 */

		//local link
		if (outP < g_offsetH) {
			if (output_vc_emb_ring) {
				flitEx->addLocalRingHop();
				flitEx->subsLocalRingContention();
				if (!input_vc_emb_ring) {
					if (this->hPos == g_rootH) flitEx->rootSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
					flitEx->addSubnetworkInjection();
				}
			} else if (output_vc_emb_tree) {
				flitEx->addLocalTreeHop();
				flitEx->subsLocalTreeContention();
				if (!input_vc_emb_tree) {
					if (this->hPos == g_rootH) flitEx->rootSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
					flitEx->addSubnetworkInjection();
				}
			} else {
				flitEx->addLocalHop();
				flitEx->subsLocalContention();
			}
		} //global link
		else if (outP < g_offsetH + g_dH) {
			if (output_vc_emb_ring) {
				flitEx->addGlobalRingHop();
				flitEx->subsGlobalRingContention();
				if (!input_vc_emb_ring) {
					if (this->hPos == g_rootH) flitEx->rootSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
					flitEx->addSubnetworkInjection();
				}
			} else if (output_vc_emb_tree) {
				flitEx->addGlobalTreeHop();
				flitEx->subsGlobalTreeContention();
				if (!input_vc_emb_tree) {
					if (this->hPos == g_rootH) flitEx->rootSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
					if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
					flitEx->addSubnetworkInjection();
				}
			} else {
				flitEx->addGlobalHop();
				flitEx->subsGlobalContention();
			}
		} // physical ring, global link
		else if ((g_ring_ports != 0)
				&& (((outP == this->portCount - 2) && (this->aPos == 0))
						|| ((outP == this->portCount - 1) && (this->aPos == g_dA - 1)))) {
			flitEx->addGlobalRingHop();
			flitEx->subsGlobalRingContention();
		} // physical ring, local link
		else {
			flitEx->addLocalRingHop();
			flitEx->subsLocalRingContention();
			if (input_port < g_offsetH + g_dH) {
				if (this->hPos == g_rootH) flitEx->rootSubnetworkInjectionsCount++;
				if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
				if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
				flitEx->addSubnetworkInjection();
			}
		}

		flitEx->setChannel(nextC);

		if (flitEx->head == 1) {  // CHECKS FOR ON-THE-FLY MISROUTING. ONLY IF HEADER FLIT
			MisrouteType misroute_type = flitEx->getCurrent_misroute_type();
			//LOCAL MISROUTING
			if ((flitEx->localMisroutingPetitionDone == 1)) {
				assert(outP >= g_offsetA);
				assert(outP < g_offsetH);
				assert(not (output_vc_emb_ring));
				assert(not (output_vc_emb_tree));
				assert(not (flitEx->mandatoryGlobalMisrouting_flag));
				assert(not (flitEx->globalMisroutingPetitionDone));
				assert(misroute_type == LOCAL || misroute_type == LOCAL_MM);
				flitEx->setMisrouted(true, misroute_type);
				flitEx->localMisroutingDone = 1;
			}
			//GLOBAL MISROUTING
			if ((flitEx->globalMisroutingPetitionDone == 1)) {
				assert(outP >= g_offsetH);
				assert(outP < g_offsetH + g_dH);
				assert(not (output_vc_emb_ring));
				assert(not (output_vc_emb_tree));
				assert(not (flitEx->localMisroutingPetitionDone));
				assert(misroute_type == GLOBAL || misroute_type == GLOBAL_MANDATORY);
				flitEx->setMisrouted(true, misroute_type);
				flitEx->globalMisroutingDone = 1;

				//global misroute at injection flag
				if (input_port < g_dP) {
					assert(misroute_type == GLOBAL);
					flitEx->setGlobal_misroute_at_injection_flag(true);
				}
			}
			//MANDATORY GLOBAL MISROUTING ASSERTS
			if (flitEx->mandatoryGlobalMisrouting_flag && not (input_vc_emb_ring) && not (input_phy_ring)
					&& not (input_vc_emb_tree) && not (output_vc_emb_ring) && not (output_phy_ring)
					&& not (output_vc_emb_tree)) {
				assert(flitEx->globalMisroutingPetitionDone);
				assert(flitEx->globalMisroutingDone);
				assert(outP >= g_offsetH);
				assert(outP < g_offsetH + g_dH);
				assert(misroute_type == GLOBAL_MANDATORY);
			}

			//RESET flag when changing group
			if (hPos != neighList[outP]->hPos) {
				flitEx->mandatoryGlobalMisrouting_flag = 0;
				flitEx->localMisroutingDone = 0;
			}

			// MANDATORY (lgl OR vc_misrouting_mm)
			// SET flag mandatory when sending local to local, being in source group, not in
			// destination group and have not misrouted before
			if ((g_OFAR_misrouting_mm || g_vc_misrouting_mm) && (input_port >= g_offsetA) && (input_port < g_offsetH)
					&& (outP >= g_offsetA) && (outP < g_offsetH) && (this->hPos == flitEx->sourceGroup)
					&& (this->hPos != flitEx->destGroup) && (not (flitEx->globalMisroutingDone))) {
				//and not in the ring
				if (not (output_vc_emb_ring) && not (output_vc_emb_tree) && not (input_vc_emb_ring)
						&& not (input_vc_emb_tree)) {
					flitEx->mandatoryGlobalMisrouting_flag = 1;
					if (not flitEx->local_mm_MisroutingPetitionDone)
						cout << "Cycle: " << g_cycle << "  Message" << flitEx->packetId << "  Flit" << flitEx->flitId
								<< "   FlitSeq" << flitEx->flitSeq << "   Hop:" << flitEx->hopCount << "  from port"
								<< input_port << ", channel" << input_channel << "  sent to Switch:"
								<< neighList[outP]->label << " ,port" << nextP << " channel:" << nextC
								<< "  through port" << outP << endl;
					assert(flitEx->local_mm_MisroutingPetitionDone);
				}
			}
		}
		if (input_port < g_dP) {
			if (g_dally == 1) {
			}
			if ((flitEx->inj_latency > 0)) {
				cout << "Cycle: " << g_cycle << "  Message" << flitEx->packetId << "  Flit" << flitEx->flitId
						<< "   FlitSeq" << flitEx->flitSeq << "  sent to Switch:" << neighList[outP]->label << " ,port"
						<< nextP << " channel:" << nextC << "from port" << input_port << "  through port" << outP
						<< endl;

			}

			flitEx->setChannel(nextC);
			assert(flitEx->inj_latency < 0);
			flitEx->inj_latency = g_cycle - flitEx->inCycle;
			assert(flitEx->inj_latency >= 0);
		}

		increasePortCount(outP);

		return (1);
	}

}

void switchModule::initPetitions() {
	for (int p = 0; p < this->portCount; p++) {
		for (int q = 0; q < this->portCount; q++) {
			globalArbiters[p]->petitions[q] = 0;
			globalArbiters[p]->inputChannels[q] = 0;
			globalArbiters[p]->nextPorts[q] = 0;
			globalArbiters[p]->nextChannels[q] = 0;
		}
	}
}

/*
 * Attends petitions. It priorizes transit over injection. It
 * tries with transit ports and, if none has a request, checks
 * injection ports.
 */
void switchModule::attendPetitions(int output_port) {
	int input_port, in_channel, nextP, nextC, inPrt_count;
	inPrt_count = 0;
	bool port_served, can_receive_flit;
	nextP = neighPort[output_port];
	can_receive_flit = 1;
	if (output_port >= g_dP) {
		can_receive_flit = neighList[output_port]->portCanReceiveFlit(nextP);
	}
	if (can_receive_flit == 1) {
		do {

			input_port = this->globalArbiters[output_port]->LRSPortList[inPrt_count];

			if ((this->globalArbiters[output_port]->petitions[input_port] == 1) && (input_port >= g_dP)) { // Injection port that can send

				in_channel = this->globalArbiters[output_port]->inputChannels[input_port];
				if (nextP != this->globalArbiters[output_port]->nextPorts[input_port])
					cout << "output_port=" << output_port << "NextP=" << nextP << ",  nextPorts="
							<< this->globalArbiters[output_port]->nextPorts[input_port] << endl;
				assert(nextP == this->globalArbiters[output_port]->nextPorts[input_port]);
				nextC = this->globalArbiters[output_port]->nextChannels[input_port];

				port_served = servePort(input_port, in_channel, output_port, nextP, nextC);
				if (port_served) {
					inPrt_count = this->portCount;
					g_petitions_served++;
					this->in_port_served[input_port] = 1;
					this->out_port_used[output_port] = 1;
					if (input_port < g_dP) {
						g_petitionsInj_served++;
					}
					this->globalArbiters[output_port]->petitions[input_port] = 0;
					this->globalArbiters[output_port]->reorderLRSPortList(input_port);
				} else {
					cout << "Port combination could not be served. Something ISN'T WORKING!!!!" << endl;
					assert(0);
				}
			} else {
				inPrt_count++;
			}
		} while (inPrt_count < (this->portCount));

		if ((this->out_port_used[output_port] == 0)) { // If none of the transit input ports has been attended, try with injection
			inPrt_count = 0;
			do {
				input_port = this->globalArbiters[output_port]->LRSPortList[inPrt_count];

				if ((this->globalArbiters[output_port]->petitions[input_port] == 1) && (input_port < g_dP)) {

					in_channel = this->globalArbiters[output_port]->inputChannels[input_port];
					if (nextP != this->globalArbiters[output_port]->nextPorts[input_port])
						cout << "output_port=" << output_port << "NextP=" << nextP << ",  nextPorts="
								<< this->globalArbiters[output_port]->nextPorts[input_port] << endl;
					assert(nextP == this->globalArbiters[output_port]->nextPorts[input_port]);
					nextC = this->globalArbiters[output_port]->nextChannels[input_port];

					port_served = servePort(input_port, in_channel, output_port, nextP, nextC);
					if (port_served) {
						inPrt_count = this->portCount;
						g_petitions_served++;
						this->in_port_served[input_port] = 1;
						this->out_port_used[output_port] = 1;
						if (input_port < g_dP) {
							g_petitionsInj_served++;
						}
						this->globalArbiters[output_port]->petitions[input_port] = 0;
						this->globalArbiters[output_port]->reorderLRSPortList(input_port);
					} else {
						cout << "Port combination could not be served. Something ISN'T WORKING!!!!" << endl;
						assert(0);
					}
				} else {
					inPrt_count++;
				}
			} while (inPrt_count < (this->portCount));
		}
	} else { // If still receiving, can not serve another input port
		increasePortContentionCount(output_port);
	}
}

void switchModule::attendPetitionsAgeArbiter(int output_port) {
	int input_port, in_channel, nextP, nextC;
	bool port_served, can_receive_flit;
	nextP = neighPort[output_port];
	can_receive_flit = 1;
	if (output_port >= g_dP) {
		can_receive_flit = neighList[output_port]->portCanReceiveFlit(nextP);
	}
	input_port = this->olderPort(output_port);
	if ((can_receive_flit == 1) && (input_port >= 0)) {
		if (this->globalArbiters[output_port]->petitions[input_port] == 1) {

			in_channel = this->globalArbiters[output_port]->inputChannels[input_port];
			if (nextP != this->globalArbiters[output_port]->nextPorts[input_port])
				cout << "output_port=" << output_port << "NextP=" << nextP << ",  nextPorts="
						<< this->globalArbiters[output_port]->nextPorts[input_port] << endl;
			assert(nextP == this->globalArbiters[output_port]->nextPorts[input_port]);
			nextC = this->globalArbiters[output_port]->nextChannels[input_port];

			port_served = servePort(input_port, in_channel, output_port, nextP, nextC);
			if (port_served) {
				g_petitions_served++;
				this->in_port_served[input_port] = 1;
				this->out_port_used[output_port] = 1;
				if (input_port < g_dP) {
					g_petitionsInj_served++;
				}
				this->globalArbiters[output_port]->petitions[input_port] = 0;
			} else {
				cout << "Port combination could not be served. Something ISN'T WORKING!!!!" << endl;
				assert(0);
			}
		}

	} else {
		increasePortContentionCount(output_port);
	}
}

void switchModule::tryNextVC(int input_port) {
	int input_channel, petition_done, inCh_count, can_send_flit;

	inCh_count = 0;

	do {

		if (g_ageArbiter) {
			input_channel = this->olderChannel(input_port);
		} else {
			input_channel = this->localArbiters[input_port]->LRSChannelList[inCh_count];
		}

		if ((input_channel >= 0)) {
			can_send_flit = pBuffers[input_port][input_channel]->canSendFlit();
			if ((!pBuffers[input_port][input_channel]->emptyBuffer()) && (can_send_flit == 1)) {
				// If vc has nothing to send, can not send it, or is still transmitting, check next vc.
				petition_done = tryPetition(input_port, input_channel);
				if (petition_done) {
					inCh_count = g_channels;
					g_petitions++;
					if (input_port < g_dP) {
						g_petitionsInj++;
					}
					if (!g_ageArbiter) this->localArbiters[input_port]->reorderLRSChannelList(input_channel);
				}
			}
		}
		inCh_count++;
		if (g_ageArbiter) inCh_count = g_channels;
	} while (inCh_count < (g_channels));
}

void switchModule::action() {

	int p, iterations, in_ports_count, out_ports_count;

	for (p = 0; p < this->portCount; p++) {
		this->out_port_used[p] = 0;
	}
	for (p = 0; p < this->portCount; p++) {
		this->in_port_served[p] = 0;
	}

	initPetitions();

	updateCredits();

	if (g_piggyback) updatePb();

	if (g_vc_misrouting_congested_restriction) vc_misrouting_congested_restriction_UPDATE();

	for (iterations = 0; iterations < (g_arbiter_iterations); iterations++) {
		// Iterative mechanism: as many petition iterations as number of VCs.
		for (in_ports_count = 0; in_ports_count < (this->portCount); in_ports_count++) {
			if ((this->in_port_served[in_ports_count] == 0)
					&& (!(in_ports_count < g_dP && this->escapeNetworkCongested))) {
				tryNextVC(in_ports_count);
			}
		}

		for (out_ports_count = 0; out_ports_count < (this->portCount); out_ports_count++) {
			if ((this->out_port_used[out_ports_count] == 0)) {
				if (g_ageArbiter) {
					attendPetitionsAgeArbiter(out_ports_count);
				} else {
					attendPetitions(out_ports_count);
				}
			}
		}
	}

	for (int p = 0; p < (portCount); p++) {
		for (int vc = 0; vc < (this->vcCount); vc++) {
			this->queueOccupancy[p * vcCount + vc] = this->queueOccupancy[p * vcCount + vc]
					+ this->pBuffers[p][vc]->getBufferOccupancy();
		}
	}

	orderQueues();
}

/*
 * Reads the incoming credit messages, and updates its counters.
 */
void switchModule::updateCredits() {

	int port;

	//check incoming credits in each port
	for (port = 0; port < portCount; port++) {

		//read flits that already arrived
		while ((!incomingCredits[port]->empty()) && (incomingCredits[port]->front().GetArrivalCycle() <= g_cycle)) {
			// access creditFlit with the earliest arrival in the priority queue
			const creditFlit& flit = incomingCredits[port]->front();

			//update credit counter
			credits[port][flit.GetVc()] = credits[port][flit.GetVc()] + flit.GetNumCreds();

			assert(credits[port][flit.GetVc()] <= maxCredits[port][flit.GetVc()]);

			// pop oldest creditFlit in the queue (deletes the flit)
			incomingCredits[port]->pop();
		}
	}
}

/*
 * Update global link info from:
 * -switch global links
 * -Pb flits
 * 
 * Send own info to other switches within the group
 */
void switchModule::updatePb() {

	double qMean = 0.0; //mean global queue occupancy
	int port, channel, threshold;
	bool isCongested;
	pbFlit *flit;

	//calculate and update congestion state for EACH CHANNEL
	for (channel = 0; channel < g_channels_glob; channel++) {

		qMean = 0;
		//LOCAL INFO: current switch global links
		//calculate qMean
		for (port = g_offsetH; port < g_offsetH + g_dH; port++) {
			qMean += getCreditsOccupancy(port, channel);
		}
		qMean = qMean / (g_dH);

		//update values
		for (port = g_offsetH; port < g_offsetH + g_dH; port++) {
			threshold = g_piggyback_coef / 100.0 * qMean + g_ugal_global_threshold * g_flitSize;
			isCongested = getCreditsOccupancy(port, channel) > threshold;
			PB.update(port, channel, isCongested);
			if (g_DEBUG) {
				cout << "(PB UPDATE) SW " << label << " Port " << port << ": Q= " << getCreditsOccupancy(port, channel)
						<< " Threshold=" << threshold << endl;
				if (isCongested) cout << "SW " << label << " Port " << port << " esta CONGESTIONADO" << endl;
			}
		}
	}

	//READ PB MSGS
	//read ONLY flits that have already arrived
	while ((!incomingPb.empty()) && (incomingPb.front().GetArrivalCycle() <= g_cycle)) {

		//update
		PB.readFlit(incomingPb.front());

		// pop oldest flit in the queue (deletes the flit)
		incomingPb.pop();
	}

	//SEND PB MSGS: to each neighbour within the group
	for (port = g_offsetA; port < g_offsetH; port++) {
		flit = PB.createFlit(g_delayTransmission_local);
		//receivePbFlit makes a copy of flit
		neighList[port]->receivePbFlit(*flit);
		delete flit;
	}

}

/*
 * Update global link info from:
 * -Just read PBmssages Received
 */
void switchModule::updateReadPb() {
//READ PB MSGS
	//read ONLY flits that have already arrived
	while ((!incomingPb.empty()) && (incomingPb.front().GetArrivalCycle() <= g_cycle)) {

		//update
		PB.readFlit(incomingPb.front());

		// pop oldest flit in the queue (deletes the flit)
		incomingPb.pop();
	}
}

/* 
 * Send credits to the upstream switch associated to the given input port and channel
 * Does nothing for injection ports
 */
void switchModule::sendCredits(int port, int channel, int flitId) {

	int latency, neighInputPort;

	if (port < g_offsetA) {
		//Injection link
		return;
	}

	latency = pBuffers[port][channel]->getDelay();
	//prepare credit flit
	creditFlit flit(g_cycle + latency, g_flitSize, channel, flitId);
	//find out which port to deliver the message
	neighInputPort = neighPort[port];
	//send credits
	neighList[port]->receiveCreditFlit(neighInputPort, flit);
}

/*
 * Adds a copy of the creditFlit to the incomingCredits queue
 */
void switchModule::receiveCreditFlit(int port, const creditFlit& crdFlit) {
	incomingCredits[port]->push(crdFlit);
}

/*
 * Adds a copy of the pbFlit to the incomingPb queue
 */
void switchModule::receivePbFlit(const pbFlit& flit) {
	incomingPb.push(flit);
}

/* 
 * Finds out the global link the flit would transit if
 * routed minimally, and returns true if it is congested
 */
bool switchModule::isGlobalLinkCongested(const flitModule * flit) {
	int globalLinkId, port, outP, dest;
	bool result = false;
	switchModule* neigh;

	dest = flit->destId;
	outP = tableOut[dest];
	neigh = neighList[outP];

	//check destination in current switch
	if (flit->destSwitch == label)
		result = false;

	//check destination in current group
	// 1 local hop
	else if ((flit->destSwitch == neigh->label) && (outP < g_offsetH) && (outP >= g_offsetA)) {
		result = false;
	}        //check destination in another group
			 // 1 global hop + ...
	else if ((outP >= g_offsetH)) {
		port = outP;
		globalLinkId = port2groupGlobalLinkID(port, aPos);

		result = PB.isCongested(globalLinkId, 0);
	}        //check destination in another group
			 // 1 local hop + 1 global hop + ..
	else if ((outP < g_offsetH) && (outP >= g_offsetA) && (neigh->tableOut[dest] >= g_offsetH)) {
		port = neigh->tableOut[dest];
		globalLinkId = port2groupGlobalLinkID(port, neighList[tableOut[flit->destId]]->aPos);

		result = PB.isCongested(globalLinkId, 0);
	} else {
		cout << "src " << flit->sourceId << " srcSW " << label << " dst " << dest << " dstSW " << flit->destSwitch
				<< " outP " << outP << " neigh " << neigh->label << endl;
		// This should never happen
		assert(0);
	}

	return result;
}

/* 
 * Returns the 'flight time' (cycles) the message would need to
 * reach its destination if it were routed minimally, from the current switch.
 * Does NOT change the message object.
 */
long switchModule::calculateBaseLatency(const flitModule * flit) {

	int dest = flit->destId;
	int dest_sw = flit->destSwitch;
	int outP;
	switchModule * current_sw = this;
	long base_latency = 0, current_latency;

	while (current_sw->label != dest_sw) {
		//add output link delay
		outP = current_sw->tableOut[dest];
		current_latency = (long) (current_sw->pBuffers[outP][0]->getDelay());
		base_latency += current_latency;
		//advance to next switch
		current_sw = current_sw->neighList[outP];
	}

	return base_latency;
}

/* 
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 */
bool switchModule::selectOFARMisrouteRandomGlobal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
		int &selectedVC) {

	int result = false;
	int i, outP, nextP, crd, cred, k, nextC, random;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit, valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dH];
	int candidates_VC[g_dH];
	for (i = 0; i < g_dH; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}

	assert(this->hPos == flit->sourceGroup);

	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin AND min outP used
	if (((q_min >= g_th_min) && (this->out_port_used[prev_outP] == 1)) || flit->mandatoryGlobalMisrouting_flag) {

		//find all valid candidates (GLOBAL LINKS)
		for (outP = g_offsetH; outP < g_offsetH + g_dH; outP++) {
			nextP = neighPort[outP];

			//find VC with more credits
			crd = 0;
			nextC = 0;
			for (k = 0; k < g_channels_useful; k++) {
				cred = getCredits(outP, k);
				if (cred > crd) {
					crd = cred;
					nextC = k;
				}
			}

			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);

			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_global_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_global_threshold;
			}

			if (g_DEBUG) {
				cout << "\t\tTrying GLOBAL candidate (flit " << flit->flitId << " ): P " << outP << " VC " << nextC
						<< " creds " << crd << "/" << maxCredits[outP][nextC] << " : q_min=" << q_min << " q_non_min="
						<< q_non_min << " th_non_min=" << th_non_min << endl;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting || flit->mandatoryGlobalMisrouting_flag)
					&& (crd >= g_flitSize) && can_receive_flit && (outP != prev_outP)
					&& (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;

				if (g_DEBUG) {
					cout << "\t\tVALID candidate (flit " << flit->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << " : q_min=" << q_min
							<< " q_non_min=" << q_non_min << " th_non_min=" << th_non_min << endl;
				}
			}

			else {
				if (g_DEBUG) {
					if (not ((q_non_min <= th_non_min) || g_forceMisrouting || flit->mandatoryGlobalMisrouting_flag)) {
						cout << "\t\tDISCARDED candidate: Th_non_min (flit " << flit->flitId << " )" << endl;
					}
					if (not (crd >= g_flitSize)) {
						cout << "\t\tDISCARDED candidate: out of credits (flit " << flit->flitId << " )" << endl;
					}
					if (not (can_receive_flit)) {
						cout << "\t\tDISCARDED candidate: busy link (flit " << flit->flitId << " )" << endl;
					}
					if (not (this->out_port_used[prev_outP] == 0)) {
						cout << "\t\tDISCARDED candidate: used port (flit " << flit->flitId << " )" << endl;
					}
				}
			}

		}

		assert(num_candidates <= g_dH);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetH);
			assert(selectedPort < g_offsetH + g_dH);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}
	return result;
}

/* 
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 * 
 * @param (IN) flitModule * flit: message to misroute.
 * 
 * @param (IN) int outP: original output port.
 * 
 * @param (IN) int outVC: original output virtual channel
 * 
 * @param (OUT) int & selectedPort: reference to the variable where the selected port is saved
 * 
 * @param (OUT) int & selectedVC: reference to the variable where the selected port is saved
 * 
 * @Return true if there is a valid link to misroute through
 */
bool switchModule::selectVcMisrouteRandomGlobal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
		int &selectedVC) {

	int result = false;
	int i, outP, nextP, nextC, crd, random;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit;
	bool valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dH];
	int candidates_VC[g_dH];
	for (i = 0; i < g_dH; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}

	assert(this->hPos == flit->sourceGroup);

	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin
	if (((q_min >= g_th_min) && (this->out_port_used[prev_outP] == 1)) || flit->mandatoryGlobalMisrouting_flag) {

		nextC = 0;
		assert(nextC <= g_channels_glob);

		//find all valid candidates (GLOBAL LINKS)
		for (outP = g_offsetH; outP < g_offsetH + g_dH; outP++) {

			if (outP == prev_outP) continue;

			nextP = neighPort[outP];
			crd = getCredits(outP, nextC);
			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);
			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_global_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_global_threshold;
			}

			if (g_DEBUG) {
				cout << "\t\tTrying GLOBAL candidate (flit " << flit->flitId << " ): P " << outP << " VC " << nextC
						<< " creds " << crd << "/" << maxCredits[outP][nextC] << " : q_min=" << q_min << " q_non_min="
						<< q_non_min << " th_non_min=" << th_non_min << endl;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting || flit->mandatoryGlobalMisrouting_flag)
					&& (crd >= g_flitSize) && can_receive_flit && (outP != prev_outP)
					&& (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;

				if (g_DEBUG) {
					cout << "\t\tVALID candidate (flit " << flit->flitId << " ): P " << outP << " VC " << nextC
							<< " creds " << crd << "/" << maxCredits[outP][nextC] << " : q_min=" << q_min
							<< " q_non_min=" << q_non_min << " th_non_min=" << th_non_min << endl;
				}
			}

			else {
				if (g_DEBUG) {
					if (not ((q_non_min <= th_non_min) || g_forceMisrouting || flit->mandatoryGlobalMisrouting_flag)) {
						cout << "\t\tDISCARDED candidate: Th_non_min (flit " << flit->flitId << " )" << endl;
					}
					if (not (crd >= g_flitSize)) {
						cout << "\t\tDISCARDED candidate: out of credits (flit " << flit->flitId << " )" << endl;
					}
					if (not (can_receive_flit)) {
						cout << "\t\tDISCARDED candidate: busy link (flit " << flit->flitId << " )" << endl;
					}
					if (not (this->out_port_used[prev_outP] == 0)) {
						cout << "\t\tDISCARDED candidate: used port (flit " << flit->flitId << " )" << endl;
					}
				}
			}

		}

		assert(num_candidates <= g_dH);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetH);
			assert(selectedPort < g_offsetH + g_dH);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}

	return result;
}

/*
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 */
bool switchModule::selectOFARMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
		int &selectedVC) {

	int result = false;
	int i, outP, nextP, crd, cred, k, nextC, random;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit, valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dA - 1];
	int candidates_VC[g_dA - 1];
	for (i = 0; i < g_dA - 1; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}

	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin AND min outP used
	if ((q_min >= g_th_min) && (this->out_port_used[prev_outP] == 1)) {

		//find all valid candidates (LOCAL LINKS)
		for (outP = g_offsetA; outP < g_offsetA + (g_dA - 1); outP++) {
			nextP = neighPort[outP];

			//find VC with more credits
			crd = 0;
			nextC = 0;
			for (k = 0; k < g_channels_useful; k++) {
				cred = getCredits(outP, k);
				if (cred > crd) {
					crd = cred;
					nextC = k;
				}
			}

			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);

			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_local_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_local_threshold;
			}

			if (g_DEBUG) {
				cout << "\t\tTrying GLOBAL candidate (flit " << flit->flitId << " ): P " << outP << " VC " << nextC
						<< " creds " << crd << "/" << maxCredits[outP][nextC] << " : q_min=" << q_min << " q_non_min="
						<< q_non_min << " th_non_min=" << th_non_min << endl;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting) && (crd >= g_flitSize)
					&& can_receive_flit && (outP != prev_outP) && (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;
			}
		}

		assert(num_candidates <= g_dA - 1);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetA);
			assert(selectedPort < g_offsetA + g_dA - 1);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}
	return result;
}

/* 
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 * 
 * @param (IN) flitModule * flit: message to misroute.
 * 
 * @param (IN) int outP: original output port.
 * 
 * @param (IN) int outVC: original output virtual channel
 * 
 * @param (OUT) int & selectedPort: reference to the variable where the selected port is saved
 * 
 * @param (OUT) int & selectedVC: reference to the variable where the selected port is saved
 * 
 * @Return true if there is a valid link to misroute through
 */
bool switchModule::selectVcMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
		int &selectedVC) {

	int result = false;
	int i, outP, nextP, nextC, crd, random;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit;
	bool valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dA - 1];
	int candidates_VC[g_dA - 1];
	for (i = 0; i < g_dA - 1; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}

	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin
	if ((q_min >= g_th_min) && (this->out_port_used[prev_outP] == 1)) {

		//if by doing misrouting the vc id does NOT increase regards previous hop
		// (vc from previous hop is noted in the flit)
		nextC = flit->channel;
		if (g_vc_misrouting_increasingVC == true) {
			if (this->hPos == flit->sourceGroup) {
				assert(flit->channel == 0);
				nextC = 1;
			} else if (this->hPos == flit->destGroup) {
				if (flit->channel == 0) {
					nextC = 2;
				} else if (flit->channel == 1) {
					nextC = 4;
				} else {
					assert(0);
				}
			} else {
				assert((flit->channel == 0));
				nextC = 2;
			}
		}

		assert(nextC <= g_channels_useful);

		//find all valid candidates (LOCAL LINKS)
		for (outP = g_offsetA; outP < g_offsetA + (g_dA - 1); outP++) {

			if (outP == prev_outP) continue;

			nextP = neighPort[outP];
			crd = getCredits(outP, nextC);
			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);
			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_local_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_local_threshold;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting) && (crd >= g_flitSize)
					&& can_receive_flit && (outP != prev_outP) && (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;
			}
		}

		assert(num_candidates <= g_dA - 1);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetA);
			assert(selectedPort < g_offsetA + g_dA - 1);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}

	return result;
}

/*
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 *
 * @param (IN) flitModule * flit: message to misroute.
 *
 * @param (IN) int outP: original output port.
 *
 * @param (IN) int outVC: original output virtual channel
 *
 * @param (OUT) int & selectedPort: reference to the variable where the selected port is saved
 *
 * @param (OUT) int & selectedVC: reference to the variable where the selected port is saved
 *
 * @Return true if there is a valid link to misroute through
 */
bool switchModule::selectStrictMisrouteRandomLocal(flitModule * flit, int prev_outP, int prev_nextC, int &selectedPort,
		int &selectedVC) {

	int result = false;
	bool strictRoutesCondition = false;
	int i, outP, nextP, nextC, crd, random, d_router, i_router, s_router;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit;
	bool valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dA - 1];
	int candidates_VC[g_dA - 1];
	for (i = 0; i < g_dA - 1; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}
	assert(g_strictMisroute == true);
	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin
	if (q_min >= g_th_min) {

		nextC = 1 + (flit->channel);
		assert(nextC <= g_channels_useful);

		//find all valid candidates (LOCAL LINKS)
		for (outP = g_offsetA; outP < g_offsetA + (g_dA - 1); outP++) {

			if (outP == prev_outP) continue;

			d_router = neighList[prev_outP]->aPos; // aPos of the destination router
			i_router = neighList[outP]->aPos; // aPos of the intermediate router
			s_router = this->aPos; // aPos of the source router

			if (s_router > i_router) {
				strictRoutesCondition = !(parity(i_router, s_router));
			}
			if (i_router > d_router) {
				strictRoutesCondition = parity(i_router, d_router);
			}
			if ((s_router < i_router) && (i_router < d_router)) {
				if (parity(d_router, s_router)) {
					strictRoutesCondition = true;
				} else {
					strictRoutesCondition = parity(i_router, s_router);
				}
			}

			if (strictRoutesCondition == false) continue;

			nextP = neighPort[outP];
			crd = getCredits(outP, nextC);
			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);
			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_local_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_local_threshold;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting) && (crd >= g_flitSize)
					&& can_receive_flit && (outP != prev_outP) && (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;

			}
		}

		assert(num_candidates <= g_dA - 1);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetA);
			assert(selectedPort < g_offsetA + g_dA - 1);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}

	return result;
}

/*
 * Returns true if there is a valid link to misroute through.
 * Selected Port and VC are stored in the argument references
 *
 * @param (IN) flitModule * flit: message to misroute.
 *
 * @param (IN) int outP: original output port.
 *
 * @param (IN) int outVC: original output virtual channel
 *
 * @param (OUT) int & selectedPort: reference to the variable where the selected port is saved
 *
 * @param (OUT) int & selectedVC: reference to the variable where the selected port is saved
 *
 * @Return true if there is a valid link to misroute through
 */
bool switchModule::selectStrictMisrouteRandomLocal_MM(flitModule * flit, int input_port, int prev_outP, int prev_nextC,
		int &selectedPort, int &selectedVC) {

	int result = false;
	bool strictRoutesCondition = false;
	int i, outP, nextP, nextC, crd, random, d_router, i_router, s_router;
	double q_min, q_non_min, th_non_min;
	bool can_receive_flit;
	bool valid_candidate;
	//maximun H valid candidates. Initialized to -1 (not valid)
	int num_candidates = 0;
	int candidates_port[g_dA - 1];
	int candidates_VC[g_dA - 1];
	for (i = 0; i < g_dA - 1; i++) {
		candidates_port[i] = -1;
		candidates_VC[i] = -1;
	}
	assert(g_strictMisroute == true);
	assert(this->hPos == flit->sourceGroup);

	q_min = getCreditsOccupancy(prev_outP, prev_nextC) * 100.0 / maxCredits[prev_outP][prev_nextC];

	//misrouting is allowed ONLY IF q_min >= Thmin
	if (q_min >= g_th_min) {

		nextC = flit->channel;
		assert(nextC == 0);

		//find all valid candidates (LOCAL LINKS)
		for (outP = g_offsetA; outP < g_offsetA + (g_dA - 1); outP++) {

			if (outP == prev_outP) continue;

			d_router = neighList[outP]->aPos; // aPos of the destination router
			i_router = this->aPos; // aPos of the intermediate router
			s_router = neighList[input_port]->aPos; // aPos of the source router

			if (s_router > i_router) {
				strictRoutesCondition = !(parity(i_router, s_router));
			}
			if (i_router > d_router) {
				strictRoutesCondition = parity(i_router, d_router);
			}
			if ((s_router < i_router) && (i_router < d_router)) {
				if (parity(d_router, s_router)) {
					strictRoutesCondition = true;
				} else {
					strictRoutesCondition = parity(i_router, s_router);
				}
			}

			if (strictRoutesCondition == false) continue;

			nextP = neighPort[outP];
			crd = getCredits(outP, nextC);
			can_receive_flit = neighList[outP]->portCanReceiveFlit(nextP);
			q_non_min = getCreditsOccupancy(outP, nextC) * 100.0 / maxCredits[outP][nextC];

			//set th_non_min
			if (g_variable_threshold) {
				th_non_min = g_percent_local_threshold * q_min / 100.0;
			} else {
				th_non_min = g_percent_local_threshold;
			}

			valid_candidate = ((q_non_min <= th_non_min) || g_forceMisrouting) && (crd >= g_flitSize)
					&& can_receive_flit && (outP != prev_outP) && (this->out_port_used[outP] == 0);

			//if it's a valid candidate, store its info
			if (valid_candidate) {
				candidates_port[num_candidates] = outP;
				candidates_VC[num_candidates] = nextC;
				num_candidates++;

			}
		}

		assert(num_candidates <= g_dA - 1);

		//select a random candidate, if there is any
		if (num_candidates > 0) {
			random = rand() % num_candidates;

			result = true;
			//update argument references
			selectedPort = candidates_port[random];
			selectedVC = candidates_VC[random];

			assert(selectedPort >= g_offsetA);
			assert(selectedPort < g_offsetA + g_dA - 1);
			assert(selectedVC >= 0);
			assert(selectedVC < g_channels_useful);
		}
	}

	return result;
}

/* 
 * Returns the misroute port type that
 * might be used, according to the
 * vc_misrouting configuration
 * 
 * @return MisrouteType
 */
MisrouteType switchModule::misrouteType(int inport, flitModule * flit, int prev_outP, int prev_nextC) {

	MisrouteType result = NONE;

	assert(g_vc_misrouting);

	// Source group available for global misroute
	if ((flit->sourceGroup == hPos) && (flit->destGroup != hPos)) {

		assert(not (flit->globalMisroutingDone));

		// VC_CRG
		if (g_vc_misrouting_crg) {
			assert(not (flit->localMisroutingDone));
			result = GLOBAL;
		}

		// VC_MM
		else if (g_vc_misrouting_mm) {
			// mandatory global
			if (flit->mandatoryGlobalMisrouting_flag) {
				assert(flit->localMisroutingDone);
				assert((inport >= g_offsetA) && (inport < g_offsetH));
				result = GLOBAL_MANDATORY;
			}

			// injection ports
			else if (inport < g_dP) {
				result = GLOBAL;
			}

			// local transit ports
			else if ((inport >= g_offsetA) && (inport < g_offsetH) && not (flit->localMisroutingDone)) {

				result = LOCAL_MM;
			}
		}
	}

	// +L: any group
	else if (g_vc_misrouting_local && not (flit->localMisroutingDone) && not (min_outport(flit) >= g_offsetH)) {
		assert(not (flit->mandatoryGlobalMisrouting_flag));
		result = LOCAL;
	}

	if (g_DEBUG) {
		cout << "cycle " << g_cycle << "--> SW " << label << " input Port " << inport << " CV " << flit->channel
				<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
				<< min_outport(flit) << " POSSIBLE misroute: ";
		switch (result) {
			case LOCAL:
				cout << "LOCAL" << endl;
				break;
			case LOCAL_MM:
				cout << "LOCAL_MM" << endl;
				break;
			case GLOBAL:
				cout << "GLOBAL" << endl;
				break;
			case GLOBAL_MANDATORY:
				cout << "GLOBAL_MANDATORY" << endl;
				break;
			case NONE:
				cout << "NONE" << endl;
				break;
			default:
				cout << "DEFAULT" << endl;
				break;
		}
	}

	// VC_misroute_congestion_restriction
	if (g_vc_misrouting_congested_restriction) {

		// misroute NOT ALLOWED
		if (not (vc_misrouting_congested_restriction_ALLOWED(flit, prev_outP, prev_nextC))) {

			// cancel misroute
			result = NONE;

			if (g_DEBUG) {
				cout << "cycle " << g_cycle << "--> SW " << label << " input Port " << inport << " CV " << flit->channel
						<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
						<< min_outport(flit) << " creds: " << getCredits(prev_outP, prev_nextC) << "/"
						<< maxCredits[prev_outP][prev_nextC] << ": qmin = "
						<< getCreditsOccupancy(prev_outP, prev_nextC)
						<< " misroute NOT ALLOWED (congestion restriction)" << endl;
			}
		}
	}

	return result;
}

/*
 * Returns the misroute port type that
 * might be used, according to the
 * mm+l in a ring configuration
 *
 * @return MisrouteType
 */
MisrouteType switchModule::misrouteTypeOFAR(int inport, int inchannel, flitModule * flit, int prev_outP,
		int prev_nextC) {

	bool input_vc_emb_ring = (g_embeddedRing != 0) && (g_channels_useful <= inchannel)
			&& (inchannel < g_channels_useful + g_channels_ring);

	bool input_vc_emb_tree = (g_embeddedTree != 0) && (g_channels_useful <= inchannel)
			&& (inchannel < g_channels_useful + g_channels_tree);

	bool input_phy_ring = (g_rings != 0) && (g_embeddedRing == 0) && (inport >= (this->portCount - 2));

	MisrouteType result = NONE;

	assert(g_vc_misrouting == 0);
	assert(g_rings > 0 || g_trees > 0);

	if ((flit->sourceGroup == hPos) && (flit->destGroup != hPos) && (not (flit->globalMisroutingDone))) {

		assert(not (flit->globalMisroutingDone));

		// VC_CRG
		if (g_OFAR_misrouting_crg) {
			assert(not (flit->localMisroutingDone));
			result = GLOBAL;
		}

		// VC_MM
		else if (g_OFAR_misrouting_mm) {
			// mandatory global
			if (flit->mandatoryGlobalMisrouting_flag) {
				assert(flit->localMisroutingDone);
				assert(((inport >= g_offsetA) && (inport < g_offsetH)) || input_phy_ring);
				result = GLOBAL_MANDATORY;
			}

			// injection ports
			else if (inport < g_dP) {
				result = GLOBAL;
			}

			// local transit ports
			else if ((inport >= g_offsetA) && (inport < g_offsetH) && (not input_vc_emb_ring) && (not input_vc_emb_tree)
					&& not (flit->localMisroutingDone)) {

				result = LOCAL_MM;
			}
		}
	}

	// +L: any group
	else if (g_OFAR_misrouting_local && not (flit->localMisroutingDone) && not (min_outport(flit) >= g_offsetH)) {
		assert(not (flit->mandatoryGlobalMisrouting_flag));
		result = LOCAL;
	}

	if (g_DEBUG) {
		cout << "cycle " << g_cycle << "--> SW " << label << " input Port " << inport << " CV " << flit->channel
				<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
				<< min_outport(flit) << " POSSIBLE misroute: ";
		switch (result) {
			case LOCAL:
				cout << "LOCAL" << endl;
				break;
			case LOCAL_MM:
				cout << "LOCAL_MM" << endl;
				break;
			case GLOBAL:
				cout << "GLOBAL" << endl;
				break;
			case GLOBAL_MANDATORY:
				cout << "GLOBAL_MANDATORY" << endl;
				break;
			case NONE:
				cout << "NONE" << endl;
				break;
			default:
				cout << "DEFAULT" << endl;
				break;
		}
	}

	// VC_misroute_congestion_restriction
	if (g_vc_misrouting_congested_restriction) {

		// misroute NOT ALLOWED
		if (not (vc_misrouting_congested_restriction_ALLOWED(flit, prev_outP, prev_nextC))) {

			// cancel misroute
			result = NONE;

			if (g_DEBUG) {
				cout << "cycle " << g_cycle << "--> SW " << label << " input Port " << inport << " CV " << flit->channel
						<< " flit " << flit->flitId << " destSW " << flit->destSwitch << " min-output Port "
						<< min_outport(flit) << " creds: " << getCredits(prev_outP, prev_nextC) << "/"
						<< maxCredits[prev_outP][prev_nextC] << ": qmin = "
						<< getCreditsOccupancy(prev_outP, prev_nextC)
						<< " misroute NOT ALLOWED (congestion restriction)" << endl;
			}

		}
	}

	return result;
}

bool switchModule::portCanReceiveFlit(int port) {
	bool can_receive_flit = true;
	for (int k = 0; k < g_channels; k++) {
		can_receive_flit = can_receive_flit && pBuffers[port][k]->canReceiveFlit();
	}
	return can_receive_flit;
}

int switchModule::min_outport(flitModule * flit) {
	return tableOut[flit->destId];
}

/* 
 * updates the value of m_vc_misrouting_congested_restriction_globalLinkCongested, 
 * (only uses channel 0)
 */
void switchModule::vc_misrouting_congested_restriction_UPDATE() {

	double qMean = 0.0; //mean global queue occupancy
	int port;
	int channel = 0;      // (CHANNEL 0 ONLY)
	bool isCongested;
	double threshold;

	//LOCAL INFO: current switch global links
	//calculate qMean
	for (port = g_offsetH; port < g_offsetH + g_dH; port++) {
		qMean += getCreditsOccupancy(port, channel);
	}
	qMean = qMean / g_dH;

	//update values
	threshold = g_vc_misrouting_congested_restriction_coef_percent / 100.0 * qMean
			+ g_vc_misrouting_congested_restriction_t * g_flitSize;
	for (port = g_offsetH; port < g_offsetH + g_dH; port++) {
		isCongested = getCreditsOccupancy(port, channel) > threshold;
		m_vc_misrouting_congested_restriction_globalLinkCongested[port - g_offsetH] = isCongested;
	}

	if (g_DEBUG) {
		cout << "cycle " << g_cycle << "--> SW " << label << " MCR qMEAN = " << qMean << " MCR Threshold = "
				<< threshold << endl;
	}
}

/* 
 * returns TRUE if misrouting is allowed according to
 * the vc_misrouting_congested_restriction
 * 
 * (Not allowed when minimal path output is a congested global link)
 */
bool switchModule::vc_misrouting_congested_restriction_ALLOWED(flitModule* flit, int prev_outP, int prev_nextC) {

	bool result = true;

	assert(g_vc_misrouting_congested_restriction);
	assert(prev_outP == min_outport(flit));

	// may restrict misrouting if outp is a global link,
	// AND next channel is 0 (still in minimal path)
	if (prev_outP >= g_offsetH && prev_outP < g_offsetH + g_dH && prev_nextC == 0) {

		//return TRUE if that global link is congested,
		//otherwise return FALSE
		result = m_vc_misrouting_congested_restriction_globalLinkCongested[prev_outP - g_offsetH];
	}

	return result;
}

/* 
 * Returns true if the flit should be misrouted
 */
bool switchModule::misrouteCondition(flitModule * flit, int prev_outP, int prev_nextC) {
	int crd;
	bool result = false;

	crd = getCredits(prev_outP, prev_nextC);
	assert(prev_outP == min_outport(flit));
	/*
	 * Misrouting attempt condition:
	 * 		- forced misrouting
	 * 		- port in use (already assigned)
	 * 		- out of credits
	 * 		- global mandatory
	 * 		- busy link
	 */
	if ((g_forceMisrouting == 1) || (this->out_port_used[prev_outP] == 1) || (crd < g_flitSize)
			|| (flit->mandatoryGlobalMisrouting_flag == 1)
			|| not (neighList[prev_outP]->portCanReceiveFlit(neighPort[prev_outP]))) {

		result = true;

		if (g_DEBUG) {
			cout << "\t\tMisroute causes: (flit " << flit->flitId << " ):";
			if (g_forceMisrouting == 1) cout << " (Forced)";
			if (this->out_port_used[prev_outP] == 1) cout << " (port in use)";
			if (crd < g_flitSize) cout << " (out of credits)";
			if (flit->mandatoryGlobalMisrouting_flag == 1) cout << " (mandatory)";
			if (not (neighList[prev_outP]->portCanReceiveFlit(neighPort[prev_outP]))) cout << " (busy link)";
			cout << endl;
		}
	}

	return result;
}

