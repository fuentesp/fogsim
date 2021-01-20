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

#include "switchModule.h"
#include "../generator/generatorModule.h"
#include "../routing/min.h"
#include "../routing/minCond.h"
#include "../routing/ofar.h"
#include "../routing/olm.h"
#include "../routing/par.h"
#include "../routing/pb.h"
#include "../routing/pbAny.h"
#include "../routing/rlm.h"
#include "../routing/ugal.h"
#include "../routing/val.h"
#include "../routing/valAny.h"
#include "../routing/obl.h"
#include "../routing/acor.h"
#include "../routing/pbAcor.h"
#include "../routing/srcAdp.h"
#include "../routing/car.h"
#include "../flit/creditFlit.h"
#include "../routing/routing.h"
#include "../switch/vcManagement/qcnVcMngmt.h"
#include <iomanip>

switchModule::switchModule(string name, int label, int aPos, int hPos, int ports, int vcCount) :
		piggyBack(aPos), m_ca_handler(this) {
	this->name = name;
	this->portCount = ports;
	this->cosLevels = g_cos_levels;
	this->vcCount = vcCount;
	/* portCount + 1 additional port for QCN injection */
	this->inPorts = new inPort *[(g_congestion_management == QCNSW) ? portCount + 1 : portCount];
	this->outPorts = new outPort *[portCount];
	this->inputArbiters =
			new inputArbiter *[(g_congestion_management == QCNSW) ? this->portCount + 1 : this->portCount];
	this->outputArbiters = new outputArbiter *[portCount];
	this->label = label;
	this->aPos = aPos;
	this->hPos = hPos;
	this->messagesInQueuesCounter = 0;
	this->escapeNetworkCongested = false;
	this->queueOccupancy = new float[
			(g_congestion_management == QCNSW) ? (portCount + 1) * vcCount : portCount * vcCount];
	this->injectionQueueOccupancy = new float[this->vcCount];
	this->localQueueOccupancy = new float[this->vcCount];
	this->globalQueueOccupancy = new float[this->vcCount];
	this->localEscapeQueueOccupancy = new float[this->vcCount];
	this->globalEscapeQueueOccupancy = new float[this->vcCount];
	this->packetsInj = 0;
	this->cnmPacketsInj = 0;
	this->reservedOutPort = new bool[this->portCount];
	this->reservedInPort = new bool*[cosLevels];
	for (int cos = 0; cos < cosLevels; cos++)
		this->reservedInPort[cos] = new bool[this->vcCount];
	this->qcnQlen = 0;
    if (g_acor_state_management == SWITCHCGCSRS || g_acor_state_management == SWITCHCGRS) {
        this->acorSwStatus = CRGLGr;
        this->acorResetSwitchHysteresisStatus();
    } else if (g_acor_state_management == SWITCHCSRS) {
        this->acorSwStatus = CRGLSw;
        this->acorResetSwitchHysteresisStatus();
    }
	int p, c, bufferCap;
	float linkDelay;
	bool escape;

	switch (g_routing) {
		case MIN:
			this->routing = new minimal(this);
			break;
		case MIN_COND:
			assert(g_vc_usage == BASE);
			this->routing = new minCond(this);
			break;
		case OFAR:
			this->routing = new ofar(this);
			break;
		case OLM:
			this->routing = new olm(this);
			break;
		case PAR:
			assert(g_vc_usage == BASE);
			this->routing = new par(this);
			break;
		case PB:
			assert(g_vc_usage == BASE);
			this->routing = new pb(this);
			break;
		case PB_ANY:
			assert(g_vc_usage == BASE);
			this->routing = new pbAny(this);
			break;
		case RLM:
			assert(g_vc_usage == BASE);
			this->routing = new rlm(this);
			break;
		case UGAL:
			assert(g_vc_usage == BASE);
			this->routing = new ugal(this);
			break;
		case VAL:
			this->routing = new val(this);
			break;
		case VAL_ANY:
			this->routing = new valAny(this);
			break;
		case OBL:
			this->routing = new oblivious(this);
			break;
        case ACOR:
            this->routing = new acor(this);
            break;
        case PB_ACOR:
            this->routing = new pbAcor(this);
            break;
		case SRC_ADP:
			this->routing = new sourceAdp(this);
			break;
		case CAR:
			this->routing = new contAdpRouting(this);
			break;
		default:
			//YET UNDEFINED!
			assert(0);
			break;
	}

	/* Each output port has initially -1 credits
	 * Function findneighbours sets this values according
	 * to the destination buffer capacity */
	incomingCredits = new queue<creditFlit> *[portCount];
	for (p = 0; p < portCount; p++) {
		for (c = 0; c < this->vcCount; c++) {
			this->queueOccupancy[(p * this->vcCount) + c] = 0;
		}
		incomingCredits[p] = new queue<creditFlit>;
	}
	if (g_congestion_management == QCNSW) {
		for (c = 0; c < this->vcCount; c++) {
			this->queueOccupancy[(p * this->vcCount) + c] = 0;
		}
	}

	for (c = 0; c < this->vcCount; c++) {
		this->injectionQueueOccupancy[c] = 0;
		this->localQueueOccupancy[c] = 0;
		this->globalQueueOccupancy[c] = 0;
		this->localEscapeQueueOccupancy[c] = 0;
		this->globalEscapeQueueOccupancy[c] = 0;
	}
	this->outputQueueOccupancy = 0;

	switch (g_buffer_type) {
		case SEPARATED:
			/* Injection queues */
			for (p = 0; p < g_p_computing_nodes_per_router; p++) {
				this->inPorts[p] = new inPort(cosLevels, g_injection_channels, p, p * cosLevels * this->vcCount,
						g_injection_queue_length, g_injection_delay, this);
				this->outPorts[p] = new outPort(this->cosLevels, g_injection_channels, p, this);
			}
			/* Local queues */
			for (p = g_p_computing_nodes_per_router; p < g_global_router_links_offset; p++) {
				this->inPorts[p] = new inPort(cosLevels, g_local_link_channels, p, p * cosLevels * this->vcCount,
						g_local_queue_length, g_local_link_transmission_delay, this);
				this->outPorts[p] = new outPort(this->cosLevels, g_local_link_channels, p, this);
			}
			/* Global queues */
			for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router;
					p++) {
				this->inPorts[p] = new inPort(cosLevels, g_global_link_channels, p, p * cosLevels * this->vcCount,
						g_global_queue_length, g_global_link_transmission_delay, this);
				this->outPorts[p] = new outPort(this->cosLevels, g_global_link_channels, p, this);
			}
			/* QCN queue */
			if (g_congestion_management == QCNSW) {
				assert(g_qcn_port == g_global_router_links_offset + g_h_global_ports_per_router);
				this->inPorts[g_qcn_port] = new inPort(this->cosLevels, g_injection_channels, g_qcn_port,
						g_qcn_port * cosLevels * this->vcCount, g_qcn_queue_length, g_injection_delay, this);
			}
			break;
		case DYNAMIC:
			/* Injection queues */
			for (p = 0; p < g_p_computing_nodes_per_router; p++) {
				this->inPorts[p] = new inPort(cosLevels, g_injection_channels, p, p * cosLevels * this->vcCount,
						g_injection_queue_length, g_injection_delay, this);
				this->outPorts[p] = new outPort(this->cosLevels, g_injection_channels, p, this);
			}
			/* Local queues */
			for (p = g_p_computing_nodes_per_router; p < g_global_router_links_offset; p++) {
				this->inPorts[p] = new dynamicBufferInPort(cosLevels, g_local_link_channels, p,
						p * cosLevels * this->vcCount, g_local_queue_length / cosLevels / g_local_link_channels,
						g_local_link_transmission_delay, this, g_local_queue_reserved);
				outPorts[p] = new dynamicBufferOutPort(this->cosLevels, g_local_link_channels, p, this,
						g_local_queue_reserved);
			}
			/* Global queues */
			for (p = g_global_router_links_offset; p < g_global_router_links_offset + g_h_global_ports_per_router;
					p++) {
				this->inPorts[p] = new dynamicBufferInPort(cosLevels, g_global_link_channels, p,
						p * cosLevels * this->vcCount, g_global_queue_length / cosLevels / g_global_link_channels,
						g_global_link_transmission_delay, this, g_global_queue_reserved);
				outPorts[p] = new dynamicBufferOutPort(this->cosLevels, g_global_link_channels, p, this,
						g_global_queue_reserved);
			}
			// TODO: ADD QCN port to dynamic byffer type for support QCN with dynamic buffers
			break;
	}

	/* Now we can instantiate arbiters */
	for (p = 0; p < this->portCount; p++) {
		this->inputArbiters[p] = new inputArbiter(p, cosLevels, this, g_input_arbiter_type);
		this->outputArbiters[p] = new outputArbiter(p, cosLevels,
				(g_congestion_management == QCNSW) ? this->portCount + 1 : this->portCount, this,
				g_output_arbiter_type);
	}

	/* QCN Congestion Point variables */
	if (g_congestion_management == QCNSW) {
		/* QCN sampling counter interval and queue length by port */
		this->qcnCpSamplingCounter = new int[this->portCount];
		this->qcnQlenOld = new int[this->portCount];
		for (p = 0; p < this->portCount; p++) {
			this->qcnCpSamplingCounter[p] = g_qcn_cp_sampling_interval;
			this->qcnQlenOld[p] = 0;
		}
		/* Additional arbiter for QCN input port */
		this->inputArbiters[g_qcn_port] = new inputArbiter(p, cosLevels, this, g_input_arbiter_type);
		/* QCN routing policy = MIN */
		this->qcnRouting = new minimal(this);
		delete this->qcnRouting->vcM;
		portClass aux[] = { portClass::local, portClass::global, portClass::local };
		vector<portClass> typeVc(aux, aux + 2 + 1);
		this->qcnRouting->vcM = new qcnVcMngmt(&typeVc, this, routing->vcM->getHighestVc(portClass::local)+1,
				routing->vcM->getHighestVc(portClass::global)+1);
		this->qcnRouting->vcM->checkVcArrayLengths(routing->vcM->localVcDest.size(), routing->vcM->globalVc.size());
	}
	/* QCNSW Reaction point in switch variables */
	if (g_congestion_management == QCNSW) {
		// QCNSWBASE variables
		this->qcnRpTxBCount = new int[this->portCount];
		this->portEnrouteMinProb = new float[this->portCount];
		// QCNSWFBCOMP and QCNSWOUTFBCOMP variables
		if (g_qcn_implementation == QCNSWFBCOMP || g_qcn_implementation == QCNSWOUTFBCOMP
				|| g_qcn_implementation == QCNSWCOMPLETE) this->qcnPortFb = new int[this->portCount];
		for (p = 0; p < this->portCount; p++) {
			this->portEnrouteMinProb[p] = 100;
			this->qcnRpTxBCount[p] = g_qcn_bc_limit;
			if (g_qcn_implementation == QCNSWFBCOMP || g_qcn_implementation == QCNSWOUTFBCOMP
					|| g_qcn_implementation == QCNSWCOMPLETE) this->qcnPortFb[p] = 0;
		}
	}
}

switchModule::~switchModule() {
	for (int p = 0; p < this->portCount; p++) {
		delete inputArbiters[p];
		delete outputArbiters[p];
		delete incomingCredits[p];
		delete inPorts[p];
		delete outPorts[p];
	}

	if (g_congestion_management == QCNSW) {
		delete[] qcnCpSamplingCounter;
		delete[] qcnRpTxBCount;
		delete[] portEnrouteMinProb;
		if (g_qcn_implementation == QCNSWFBCOMP || g_qcn_implementation == QCNSWOUTFBCOMP
				|| g_qcn_implementation == QCNSWCOMPLETE) delete[] qcnPortFb;
	}
	delete[] inputArbiters;
	delete[] outputArbiters;
	delete[] incomingCredits;
	delete[] queueOccupancy;
	delete[] inPorts;
	delete[] outPorts;
	delete[] injectionQueueOccupancy;
	delete[] localQueueOccupancy;
	delete[] globalQueueOccupancy;
	delete[] localEscapeQueueOccupancy;
	delete[] globalEscapeQueueOccupancy;
	for (int cos = 0; cos < this->cosLevels; cos++)
		delete[] reservedInPort[cos];
	delete[] reservedInPort;
	delete[] reservedOutPort;
	delete routing;
}

void switchModule::resetQueueOccupancy() {
	for (int i = 0; i < vcCount; i++) {
		this->injectionQueueOccupancy[i] = 0;
		this->localQueueOccupancy[i] = 0;
		this->globalQueueOccupancy[i] = 0;
		this->localEscapeQueueOccupancy[i] = 0;
		this->globalEscapeQueueOccupancy[i] = 0;
		for (int j = 0; j < this->portCount; j++) {
			this->queueOccupancy[(j * this->vcCount) + i] = 0;
		}
	}
	this->outputQueueOccupancy = 0;
}

void switchModule::escapeCongested() {
//	escapeNetworkCongested = true;
//	for (int p = 0; p < this->portCount; p++) {
//		for (int c = 0; c < this->vcCount; c++) {
//			if (pBuffers[p][c]->escapeBuffer) {
//				assert(c >= g_local_link_channels);
//				if (getCreditsOccupancy(p, c) * 100.0 / maxCredits[p][c]
//						<= g_escapeCongestion_th)
//					escapeNetworkCongested = false;
//			}
//		}
//	}
}

/*
 * Tracks queue occupancy per vc, to serve statistics purposes
 * (results are later displayed through output file).
 */
void switchModule::setQueueOccupancy() {
	int p, c;
	int iq = 0; /* Injection queue */
	int lq = 0; /* Local transit queue */
	int gq = 0; /* Global transit queue */
	int leq = 0; /* Escape local queue */
	int geq = 0; /* Escape global queue */

	for (p = 0; p < this->portCount; p++) {
		if (p < g_p_computing_nodes_per_router) {
			/* Injection ports */
			iq++;
			for (c = 0; c < this->vcCount; c++) {
				this->injectionQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
			}
		} else {
			if ((p < g_global_router_links_offset)) {
				/* Local transit ports */
				lq++;
				if (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == EMBEDDED_TREE) {
//					if (this->pBuffers[p][g_local_link_channels]->bufferCapacity > 0)
//											leq++;
				}
				for (c = 0; c < vcCount /*g_local_link_channels*/; c++) {
					this->localQueueOccupancy[c] += +this->queueOccupancy[(p * this->vcCount) + c];
				}
//				for (c = g_local_link_channels; c < (g_local_link_channels + g_channels_escape); c++) {
//					this->localEscapeQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
//								}
			} else {
				if (p < (g_global_router_links_offset) + g_h_global_ports_per_router) {
					/* Global transit ports */
					gq++;
					if (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == EMBEDDED_TREE) {
//						if (this->pBuffers[p][g_local_link_channels]->bufferCapacity > 0)
//													geq++;
					}
					for (c = 0; c < vcCount /*g_local_link_channels*/; c++) {
						this->globalQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
					}
//					for (c = g_local_link_channels; c < (g_local_link_channels + g_channels_escape); c++) {
//						this->globalEscapeQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
//										}
				} else {
					assert(g_rings != 0 && g_deadlock_avoidance == RING);
					if (((p == this->portCount - 2) && (this->aPos == 0))
							|| ((p == this->portCount - 1) && (this->aPos == g_a_routers_per_group - 1))) {
						geq++; /* Consider only those ring ports that belong to an ingress/egress to the group */
						for (c = 0; c < vcCount; c++) {
							this->globalEscapeQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
						}
					} else {
						leq++;
						for (c = 0; c < this->vcCount; c++) {
							this->localEscapeQueueOccupancy[c] += this->queueOccupancy[(p * this->vcCount) + c];
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
		if ((geq == 0) || (vc < g_local_link_channels))
			this->globalEscapeQueueOccupancy[vc] = 0;
		else
			this->globalEscapeQueueOccupancy[vc] = this->globalEscapeQueueOccupancy[vc] / geq;
		if ((leq == 0) || (vc < g_local_link_channels))
			this->localEscapeQueueOccupancy[vc] = 0;
		else
			this->localEscapeQueueOccupancy[vc] = this->localEscapeQueueOccupancy[vc] / leq;
	}
}

/* 
 * Set credits to each output channel according to the capacity of the
 * next input buffer capacity
 * 
 * [Needs neighbour information: ensure routing member is constructed
 * and initialized before this.]
 */
void switchModule::resetCredits() {
	int thisPort, nextP, cos, chan, numVCs;

	/* Injection ports (check buffers in local switch) */
	for (thisPort = 0; thisPort < g_p_computing_nodes_per_router; thisPort++)
		for (cos = 0; cos < this->cosLevels; cos++)
			for (chan = 0; chan < /*vcCount*/g_injection_channels; chan++) {
				outPorts[thisPort]->setMaxOccupancy(cos, chan,
						inPorts[thisPort]->getBufferCapacity(cos, chan) * g_flit_size);
				/* Sanity check: initial credits must be equal to the buffer capacity (in phits) */
				assert(switchModule::getCredits(thisPort, cos, chan) == outPorts[thisPort]->getMaxOccupancy(cos, chan));
			}

	/* Transit ports (check buffers in the next switch). We use the first generator
	 * connected to the next switch to index tableIn, which means:
	 * switchId * g_p_computing_nodes_per_router (numberOfGeneratorsPerSwitch),
	 * to find out the input port in the next switch. */
	numVCs = (g_vc_usage == FLEXIBLE || g_vc_usage == TBFLEX) ? vcCount : g_local_link_channels;
	for (thisPort = g_p_computing_nodes_per_router; thisPort < g_global_router_links_offset; thisPort++)
		for (cos = 0; cos < this->cosLevels; cos++)
			for (chan = 0; chan < numVCs; chan++) {
				nextP = routing->neighPort[thisPort];
				outPorts[thisPort]->setMaxOccupancy(cos, chan,
						routing->neighList[thisPort]->inPorts[nextP]->getBufferCapacity(cos, chan) * g_flit_size);
				/* Sanity check: initial credits must be equal to the buffer capacity (in phits) */
				assert(switchModule::getCredits(thisPort, cos, chan) == outPorts[thisPort]->getMaxOccupancy(cos, chan));
			}
	numVCs = (g_vc_usage == FLEXIBLE || g_vc_usage == TBFLEX) ? vcCount : g_global_link_channels;
	for (thisPort = g_global_router_links_offset; thisPort < portCount; thisPort++)
		for (cos = 0; cos < this->cosLevels; cos++)
			for (chan = 0; chan < numVCs; chan++) {
				nextP = routing->neighPort[thisPort];
				outPorts[thisPort]->setMaxOccupancy(cos, chan,
						routing->neighList[thisPort]->inPorts[nextP]->getBufferCapacity(cos, chan) * g_flit_size);
				/* Sanity check: initial credits must be equal to the buffer capacity (in phits) */
				assert(switchModule::getCredits(thisPort, cos, chan) == outPorts[thisPort]->getMaxOccupancy(cos, chan));
			}
}

int switchModule::getTotalCapacity() {
	int p, c, cos, totalCapacity = 0;
	for (p = 0; p < portCount; p++)
		for (cos = 0; cos < this->cosLevels; cos++)
			for (c = 0; c < vcCount; c++)
				totalCapacity = totalCapacity + (inPorts[p]->getBufferCapacity(cos, c));
	return (totalCapacity);
}

int switchModule::getTotalFreeSpace() {
	int p, cos, c, totalFreeSpace = 0;
	for (p = 0; p < portCount; p++)
		for (cos = 0; cos < cosLevels; cos++)
			for (c = 0; c < vcCount; c++)
				totalFreeSpace = totalFreeSpace + inPorts[p]->getSpace(cos, c);

	return (totalFreeSpace);

}

/*
 * Insert flit into a buffer. Used in TRANSIT.
 * Credit count reduction is done in the upstream switch, not here.
 */
void switchModule::insertFlit(int port, int vc, flitModule *flit) {
	assert(port < portCount && port >= g_p_computing_nodes_per_router);
	assert(vc < vcCount);
	assert(inPorts[port]->getSpace(flit->cos, vc) >= g_flit_size);

	if (g_contention_aware && (!g_increaseContentionAtHeader)) {
		m_ca_handler.increaseContention(routing->minOutputPort(flit->destId));
	}
	inPorts[port]->insert(vc, flit, g_flit_size);
	/* Update QCN CP counter */
	if (g_congestion_management == QCNSW) this->qcnCpSamplingCounter[port]--;
}

/*
 * Insert flit into an injection buffer. Used in INJECTION (from generator).
 */
void switchModule::injectFlit(int port, int vc, flitModule * flit) {
	double base_latency;
	assert(((port / g_p_computing_nodes_per_router) < 1) || (g_congestion_management == QCNSW && port == portCount));
	assert((vc / vcCount) < 1 && (vc / g_injection_channels) < 1);
	assert(switchModule::getCredits(port, flit->cos, vc) >= g_flit_size);
	if (g_contention_aware && (!g_increaseContentionAtHeader))
		m_ca_handler.increaseContention(routing->minOutputPort(flit->destId));
	if (flit->flitType == CNM)
		g_tx_cnmFlit_counter++;
	else
		g_tx_flit_counter++;
	inPorts[port]->insert(vc, flit, g_flit_size);

	/* Calculate message's base latency and record it. */
	base_latency = calculateBaseLatency(flit);
	flit->setBaseLatency(base_latency);
}

int switchModule::getCredits(int port, unsigned short cos, int channel) {
	int crdts;

	assert(port >= 0 && port <= portCount);
	assert(port < portCount || g_congestion_management == QCNSW);
	assert(channel >= 0 && channel < vcCount);

	/*
	 * Two different versions:
	 * 1- Magically looking buffer free space
	 * 2- Checking local count of credits (needs actually
	 * 		sending credits through the channel)
	 */
	if (port < g_p_computing_nodes_per_router || port == portCount) {
		// Version 1, used for injection and qcn port
		crdts = inPorts[port]->getSpace(cos, channel);
	} else {
		// Version 2, used for transit
		crdts = outPorts[port]->getMaxOccupancy(cos, channel) - outPorts[port]->getOccupancy(cos, channel);
	}

	return crdts;
}

/*
 * Determines the available space for all the buffers of a given
 * port and cos. For consumption ports, available space is the sum of
 * space in the input buffers. For transit ports is calculated as
 * the number of available credits. If no range of VCs is given,
 * it will automatically calculate it.
 */
int switchModule::getPortCredits(int port, unsigned short cos, vector<int> vc_array) {
	int crdts = 0, vc;
	std::vector<int>::iterator it;

	assert(port < portCount);
	if (vc_array.size() == 0) {
		if (port < g_p_computing_nodes_per_router) {
			for (vc = 0; vc < g_injection_channels; vc++)
				vc_array.push_back(vc);
		} else if (port < g_global_router_links_offset) {
			for (vc = 0; vc < g_local_link_channels; vc++)
				vc_array.push_back(vc);
		} else {
			for (vc = 0; vc < g_global_link_channels; vc++)
				vc_array.push_back(vc);
		}
	}

	if (port < g_p_computing_nodes_per_router) {
		for (it = vc_array.begin(); it != vc_array.end(); ++it) {
			crdts += inPorts[port]->getSpace(cos, *it);
		}
	} else {
		for (it = vc_array.begin(); it != vc_array.end(); ++it) {
			crdts += outPorts[port]->getMaxOccupancy(cos, *it) - outPorts[port]->getOccupancy(cos, *it);
		}
	}

	return (crdts);
}

bool switchModule::checkConsumePort(int port, flitModule * flit) {
	assert(port >= 0 && port < g_p_computing_nodes_per_router);
	return g_generators_list[this->label * g_p_computing_nodes_per_router + port]->checkConsume(flit);
}

/*
 * Returns the number of used FLITS in the next buffer
 * Transit port: based on the local credit count in transit
 * Injection port: based on local buffer real occupancy
 */
int switchModule::getCreditsOccupancy(int port, unsigned short cos, int channel, int buffer) {
	assert(port >= 0 && port < portCount);
	assert(cos >= 0 && cos < this->cosLevels);
	assert(channel >= 0 && channel < vcCount);
	int occupancy;

	if (port < g_p_computing_nodes_per_router) {
		//Injection
		occupancy = inPorts[port]->getBufferOccupancy(cos, channel);
	} else {
		//Transit
		occupancy = outPorts[port]->getTotalOccupancy(cos, channel);
	}

	return occupancy;
}

int switchModule::getCreditsMinOccupancy(int port, unsigned short cos, int channel) {
	assert(port >= g_p_computing_nodes_per_router);
	assert(cos >= 0 && cos < this->cosLevels);
	return outPorts[port]->getMinOccupancy(cos, channel);
}

void switchModule::increasePortCount(int port) {
	assert(port < portCount);
	g_port_usage_counter[port]++;
}

void switchModule::increaseVCCount(int vc, int port) {
	assert(port < portCount);
	g_vc_counter[port - g_local_router_links_offset][vc]++;
}

void switchModule::increasePortContentionCount(int port) {
	assert(port < portCount);
	g_port_contention_counter[port]++;
}

void switchModule::orderQueues() {
	int portCount = (g_congestion_management == QCNSW) ? this->portCount + 1 : this->portCount;
	for (int count_ports = 0; count_ports < portCount; count_ports++)
		for (int cos = 0; cos < this->cosLevels; cos++)
			for (int count_channels = 0; count_channels < vcCount; count_channels++)
				inPorts[count_ports]->reorderBuffer(cos, count_channels);
}

/*
 * Switch main action: simulates normal switch execution
 * during a cycle. Resets some variables, and repeats
 * allocator behavior (local + global arbiter phases)
 * as many times a allocator iterations the switches has
 * assigned.
 */
void switchModule::action() {
	int p, in_ports_count, out_ports_count, vc, in_req, max_reqs;
	unsigned short cos;
	flitModule *flit = NULL;

	max_reqs = g_issue_parallel_reqs ? g_local_arbiter_speedup : 1;

	updateCredits();

	if (g_routing == PB || g_routing == PB_ANY || g_routing == SRC_ADP || g_routing == PB_ACOR) updatePb();

    if ((g_routing == ACOR || g_routing == PB_ACOR) && 
            (g_acor_state_management == SWITCHCGCSRS || g_acor_state_management == SWITCHCGRS ||
             g_acor_state_management == SWITCHCSRS)) {
        acor_hyst_cycles_counter--;
        if (acor_packets_blocked_counter >= acor_inc_state_th_packets)
            acorIncState();
        if (acor_hyst_cycles_counter == 0 && 
                acor_packets_blocked_counter <= acor_dec_state_th_packets)
            acorDecState();
        if (acor_packets_blocked_counter >= acor_inc_state_th_packets ||
                acor_hyst_cycles_counter == 0)
            acorResetSwitchHysteresisStatus();
        /* ACOR group 0 switch statistics */
        if (this->label < g_a_routers_per_group)
            g_acor_group0_sws_status[label][g_cycle] = acorSwStatus;
    }

	if (g_contention_aware && g_increaseContentionAtHeader) m_ca_handler.update();

	if (g_vc_misrouting_congested_restriction) this->routing->updateCongestionStatusGlobalLinks();

	/* In case a history window of congestion is employed, shift entries and add an empty one. */
	if (g_congestion_detection == HISTORY_WINDOW || g_congestion_detection == HISTORY_WINDOW_AVG) {
		for (int port = 0; port < this->portCount; port++) {
			for (int cos = 0; cos < this->cosLevels; cos++) {
				((contAdpRouting *) routing)->congestionWindow[port][cos].erase(
						((contAdpRouting *) routing)->congestionWindow[port][cos].begin());
				((contAdpRouting *) routing)->congestionWindow[port][cos].push_back(0);
			}
		}
	}

	/* QCN CP Occupancy sampling */
	if (g_congestion_management == QCNSW)
		for (p = g_p_computing_nodes_per_router; p < g_global_router_links_offset + g_h_global_ports_per_router; p++)
			if (--this->qcnCpSamplingCounter[p] <= 0) qcnOccupancySampling(p);

	/* QCN RP behaviour - Timer and set portEnrouteMinProb */
	if (g_congestion_management == QCNSW) {
		for (p = g_p_computing_nodes_per_router; p < g_qcn_port; p++)
			if (qcnRpTxBCount[p] <= 0) {
				if (g_qcn_policy == AIMD || g_qcn_policy == AIAD)
					this->portEnrouteMinProb[p] += g_qcn_r_ai;
				else
					this->portEnrouteMinProb[p] *= g_qcn_r_ai;
				if (this->portEnrouteMinProb[p] > 100) this->portEnrouteMinProb[p] = 100;
				if (this->label < g_a_routers_per_group)
					g_qcn_g0_port_enroute_min_prob[g_cycle][this->label][p - g_p_computing_nodes_per_router] =
							this->portEnrouteMinProb[p];
				// Reset counter for setting timer
				qcnRpTxBCount[p] = g_qcn_bc_limit;
			}
	}

	/* Switch can take several iterations over allocation (input + output arbitration) per cycle.
	 * This helps guaranteeing the attendance of many VCs per channel. */
	for (g_iteration = 0; g_iteration < g_allocator_iterations; g_iteration++) {
		/* Reset petitions */
		for (p = 0; p < this->portCount; p++)
			this->outputArbiters[p]->initPetitions();

		int inputArbitersPortCount = (g_congestion_management == QCNSW) ? this->portCount + 1 : this->portCount;
		/* Input arbiters execution */
		for (in_ports_count = 0; in_ports_count < inputArbitersPortCount; in_ports_count++) {
			/* Reset reserved_port trackers */
			for (p = 0; p < this->portCount; p++)
				reservedOutPort[p] = false;

			for (cos = 0; cos < this->cosLevels; cos++)
				for (p = 0; p < this->vcCount; p++)
					reservedInPort[cos][p] = false;
			/* Injection throttling: when using escape subnetwork as deadlock avoidance mechanism,
			 * injection is prevented if subnet becomes congested. */
			if ((!(in_ports_count < g_p_computing_nodes_per_router && this->escapeNetworkCongested))) {
				/* Input arbiter selects an input to advance and makes a petition to output arbiter */
				for (in_req = 0; in_req < max_reqs; in_req++) {
					vc = this->inputArbiters[in_ports_count]->action();
					if (vc != -1) {
						// Make petition
						cos = this->inputArbiters[in_ports_count]->getCurCos();
						flit = getFlit(in_ports_count, cos, vc);
						assert(cos == flit->cos);
						if (reservedOutPort[flit->nextP]) continue;
						reservedOutPort[flit->nextP] = true;
						reservedInPort[cos][vc] = true;
						this->outputArbiters[flit->nextP]->petitions[in_ports_count] = 1;
						this->outputArbiters[flit->nextP]->inputChannels[in_ports_count] = vc;
						this->outputArbiters[flit->nextP]->inputCos[in_ports_count] = cos;
						this->outputArbiters[flit->nextP]->nextChannels[in_ports_count] = flit->nextVC;
						this->outputArbiters[flit->nextP]->nextPorts[in_ports_count] = routing->neighPort[flit->nextP];
					}
				}
			}
		}
		/* Output arbiters execution */
		for (out_ports_count = 0; out_ports_count < this->portCount; out_ports_count++) {
			in_ports_count = this->outputArbiters[out_ports_count]->action();
			if (in_ports_count != -1) {
				// Attends petition (consumes packet or sends it through 'sendFlit')
				vc = this->outputArbiters[out_ports_count]->inputChannels[in_ports_count];
				cos = this->outputArbiters[out_ports_count]->inputCos[in_ports_count];
				inPorts[in_ports_count]->checkFlit(cos, vc, flit);
				// QCN RP behaviour at reception of CNM
				if (g_congestion_management == QCNSW && (flit->destSwitch == this->label && // destination node is this switch
						flit->flitType == CNM)) { // message is a CNM
					assert(in_ports_count >= g_p_computing_nodes_per_router);
					assert(in_ports_count < g_global_router_links_offset + g_h_global_ports_per_router);
					assert(flit->sourceSW != this->label);
					switch (g_qcn_implementation) {
						case QCNSWSELF:
							// Reduce the probability like QCNSWBASE (!break;)
						case QCNSWOUT:
							// Reduce the probability like QCNSWBASE (!break;)
						case QCNSWBASE:
							/* Multiplicative decrease of probability */
							qcnMinProbabilityDecrease(in_ports_count, flit->fb);
							break;
						case QCNSWCOMPLETE:
							// Modify probability as FBCOMP
						case QCNSWOUTFBCOMP:
							// Modify probability
						case QCNSWFBCOMP:
							qcnFeedbackComparison(in_ports_count, flit->fb);
							break;
					}
					// Reset counter for setting timer
					qcnRpTxBCount[in_ports_count] = g_qcn_bc_limit;
				}
				xbarTraversal(in_ports_count, cos, vc, out_ports_count, routing->neighPort[out_ports_count],
						this->outputArbiters[out_ports_count]->nextChannels[in_ports_count]);
			}
		}
	}

	for (int p = 0; p < portCount; p++)
		for (unsigned short cos = 0; cos < this->cosLevels; cos++)
			for (int vc = 0; vc < (this->vcCount); vc++)
				this->queueOccupancy[p * vcCount + vc] += this->inPorts[p]->getBufferOccupancy(cos, vc);
	orderQueues();

	if (g_cycle >= g_warmup_cycles + g_max_cycles - 100
			&& g_cycle < g_warmup_cycles + g_max_cycles - 100 + g_verbose_cycles
			&& g_verbose_switches.find(this->label) != g_verbose_switches.end()) this->printSwitchStatus();
}

/*
 * Reads the incoming credit messages, and updates its counters.
 */
void switchModule::updateCredits() {
	int port;

	/* Check incoming credits in each port */
	for (port = 0; port < portCount; port++) {

		/* Read flits that already arrived */
		while ((!incomingCredits[port]->empty())
				&& (incomingCredits[port]->front().getArrivalCycle() <= g_internal_cycle)) {
			/* Access creditFlit with the earliest arrival in the priority
			 * queue, update credit counter and delete checked flit by
			 * popping oldest creditFlit in the queue. */
			const creditFlit& flit = incomingCredits[port]->front();
			outPorts[port]->decreaseOccupancy(flit.getCos(), flit.getVc(), flit.getNumCreds());
			outPorts[port]->decreaseMinOccupancy(flit.getCos(), flit.getVc(), flit.getNumMinCreds());
#if DEBUG
			cout << flit.getArrivalCycle() << " cycle--> switch " << label << "(Port " << port << ", VC "
			<< flit.getVc() << "): +" << flit.getNumCreds() << " credits = "
			<< outPorts[port]->getOccupancy(flit.getVc()) << " / "
			<< outPorts[port]->getMaxOccupancy(flit.getVc()) << " (message " << flit.getFlitId() << " )"
			<< endl;
#endif
			incomingCredits[port]->pop();
		}
	}
}

/*
 * Update global link info from switch global links
 * & PiggyBacking flits. Send own info to other
 * switches within the group.
 */
void switchModule::updatePb() {
	double qMean, qCur; /* Mean global queue occupancy */
	int port, channel, threshold, vc;
	unsigned short cos;
	bool isCongested;
	pbFlit *flit;

	/* Calculate and update congestion state for EACH COS and EACH CHANNEL */
	for (cos = 0; cos < this->cosLevels; cos++) {
		qMean = 0.0;
		qCur = 0.0;
		if (g_congestion_detection == PER_PORT || g_congestion_detection == PER_PORT_MIN) {
			/* Calculate and update congestion state for EACH PORT */
			for (port = g_global_router_links_offset; port < g_global_router_links_offset + g_h_global_ports_per_router;
					port++) {
				for (channel = 0; channel < g_global_link_channels; channel++) {
					if (g_vc_usage == BASE)
						vc = channel;
					else if (g_reactive_traffic)
						vc = routing->vcM->globalResVc[channel];
					else
						vc = routing->vcM->globalVc[channel];

					if (g_congestion_detection == PER_PORT_MIN)
						qMean += switchModule::getCreditsMinOccupancy(port, cos, vc);
					else
						qMean += switchModule::getCreditsOccupancy(port, cos, vc);
				}
			}
			qMean /= (g_h_global_ports_per_router * g_global_link_channels);
			for (port = g_global_router_links_offset; port < g_global_router_links_offset + g_h_global_ports_per_router;
					port++) {
				threshold = g_piggyback_coef / 100.0 * qMean + g_ugal_global_threshold * g_flit_size;
				qCur = 0;
				for (channel = 0; channel < g_global_link_channels; channel++) {
					if (g_vc_usage == BASE)
						vc = channel;
					else if (g_reactive_traffic)
						vc = routing->vcM->globalResVc[channel];
					else
						vc = routing->vcM->globalVc[channel];

					if (g_congestion_detection == PER_PORT_MIN)
						qCur += switchModule::getCreditsMinOccupancy(port, cos, vc);
					else
						qCur += switchModule::getCreditsOccupancy(port, cos, vc);
				}
				isCongested = (qCur / g_global_link_channels) > threshold;
				piggyBack.update(port, cos, 0, isCongested);
			}
		} else if (g_congestion_detection == PER_VC || g_congestion_detection == PER_VC_MIN) {
			/* Calculate and update congestion state for EACH CHANNEL */
			for (channel = 0; channel < g_global_link_channels; channel++) {
				/* LOCAL INFO: current switch global links. Calculate qMean*/
				qMean = 0;
				for (port = g_global_router_links_offset;
						port < g_global_router_links_offset + g_h_global_ports_per_router; port++) {
					if (g_vc_usage == BASE)
						vc = channel;
					else if (g_reactive_traffic)
						vc = routing->vcM->globalResVc[channel];
					else
						vc = routing->vcM->globalVc[channel];

					if (g_congestion_detection == PER_VC_MIN)
						qMean += switchModule::getCreditsMinOccupancy(port, cos, vc);
					else
						qMean += switchModule::getCreditsOccupancy(port, cos, vc);
				}
				qMean = qMean / g_h_global_ports_per_router;

				/* Update values */
				for (port = g_global_router_links_offset;
						port < g_global_router_links_offset + g_h_global_ports_per_router; port++) {
					threshold = g_piggyback_coef / 100.0 * qMean + g_ugal_global_threshold * g_flit_size;
					if (g_vc_usage == BASE)
						vc = channel;
					else if (g_reactive_traffic)
						vc = routing->vcM->globalResVc[channel];
					else
						vc = routing->vcM->globalVc[channel];

					if (g_congestion_detection == PER_VC_MIN)
						isCongested = switchModule::getCreditsMinOccupancy(port, cos, vc) > threshold;
					else
						isCongested = switchModule::getCreditsOccupancy(port, cos, vc) > threshold;
					piggyBack.update(port, cos, channel, isCongested);
#if DEBUG
					cout << "(PB UPDATE) SW " << label << " Port " << port << ": Q= " << getCreditsOccupancy(port, channel)
					<< " Threshold=" << threshold << endl;
					if (isCongested) cout << "SW " << label << " Port " << port << " is CONGESTED" << endl;
#endif
				}
			}
		}
	}

	/* Read PiggyBacking flits. Read ONLY those flits that have already arrived, and pop them out of the queue. */
	while ((!incomingPb.empty()) && (incomingPb.front().getArrivalCycle() <= g_internal_cycle)) {
		piggyBack.readFlit(incomingPb.front());
		incomingPb.pop();
	}

	/* Send PiggyBacking flits to each neighbor within the group */
	for (port = g_local_router_links_offset; port < g_global_router_links_offset; port++) {
		flit = piggyBack.createFlit(g_local_link_transmission_delay);
#if DEBUG
		cout << "SW " << label << " --> Sending flit " << flit->getId() << " to SW " << routing->neighList[port]->label
		<< endl;
#endif
		routing->neighList[port]->receivePbFlit(*flit);
		delete flit; /* ReceivePbFlit makes a copy of flit */
	}
}

/*
 * Update global link PiggyBacking info from
 * PB flits that have already arrived.
 */
void switchModule::updateReadPb() {
	while ((!incomingPb.empty()) && (incomingPb.front().getArrivalCycle() <= g_internal_cycle)) {
		piggyBack.readFlit(incomingPb.front());
		incomingPb.pop(); /* Delete oldest (already read) PB flit */
	}
}

/* 
 * Send credits to the upstream switch associated to the given input port and channel.
 * Does nothing for injection ports.
 */
void switchModule::sendCredits(int port, unsigned short cos, int channel, flitModule * flit) {

	int neighInputPort;
	float latency;

	if (port < g_local_router_links_offset || port == g_global_router_links_offset + g_h_global_ports_per_router) { /* Injection link */
		return;
	}

	latency = inPorts[port]->getDelay(cos, channel);
	int minSize = (!flit->getPrevMisrouted()) * g_flit_size;
	creditFlit crdFlit(g_internal_cycle + latency, g_flit_size, minSize, cos, channel, flit->flitId);
	neighInputPort = routing->neighPort[port];
#if DEBUG
	cout << g_cycle << " cycle--> switch " << label << ": (message " << flitId << " ) sending CREDITS to sw "
	<< routing->neighList[port]->label << "(Port " << neighInputPort << ", VC " << channel << ")" << endl;
#endif
	routing->neighList[port]->receiveCreditFlit(neighInputPort, crdFlit);
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
#if DEBUG
	cout << "SW " << label << " --> Pushing PB flit " << flit.getId() << endl;
#endif
	incomingPb.push(flit);
}

/*
 * Stores an incoming ContentionAware notification flit
 * in a queue to later read its data.
 */
void switchModule::receiveCaFlit(const caFlit& flit) {
	incomingCa.push(flit);
}

/* 
 * Finds out the global link the flit would transit if
 * routed minimally, and returns true if it is congested.
 */
bool switchModule::isGlobalLinkCongested(const flitModule * flit) {
	int globalLinkId, port, outP, dest;
	bool result = false;
	switchModule* neigh;

	dest = flit->destId;
	outP = routing->minOutputPort(dest);
	neigh = routing->neighList[outP];

	int vc;
	if (g_congestion_detection == PER_PORT || g_congestion_detection == PER_PORT_MIN)
		vc = 0;
	else {
		if (g_vc_usage == FLEXIBLE || g_vc_usage == TBFLEX)
			vc = g_global_link_channels - 1;
		else {
			if (flit->flitType == RESPONSE && g_reactive_traffic)
				vc = 2;
			else
				vc = 0;
		}
	}
	/* Find destination in following order:
	 * -Check in current switch.
	 * -Check in current group.
	 * -Check in another group linked directly to this switch.
	 * -Check in another group (not linked to this switch).
	 */
	if (flit->destSwitch == label)
		result = false;
	else if ((flit->destSwitch == neigh->label) && (outP < g_global_router_links_offset)
			&& (outP >= g_local_router_links_offset)) {
		result = false;
	} else if ((outP >= g_global_router_links_offset)) {
		globalLinkId = port2groupGlobalLinkID(outP, aPos);
		result = piggyBack.isCongested(globalLinkId, flit->cos, vc);
	} else if ((outP < g_global_router_links_offset) && (outP >= g_local_router_links_offset)
			&& (neigh->routing->minOutputPort(dest) >= g_global_router_links_offset)) {
		port = neigh->routing->minOutputPort(dest);
		globalLinkId = port2groupGlobalLinkID(port, routing->neighList[outP]->aPos);
		result = piggyBack.isCongested(globalLinkId, flit->cos, vc);
	} else { /* ERROR: THIS SHALL NOT HAPPEN */
#if DEBUG
		cout << "Source " << flit->sourceId << " sw " << label << " Dest " << dest << " sw " << flit->destSwitch
		<< " outP " << outP << " neigh " << neigh->label << endl;
#endif
		assert(0);
	}

	return result;
}

/* 
 * Returns the 'flight time' (cycles) the message would need to
 * reach its destination if it were routed minimally, from the current switch.
 * Does NOT change the message object.
 */
double switchModule::calculateBaseLatency(const flitModule * flit) {
	int dest = flit->destId;
	assert(dest >= 0 && dest < g_number_generators);
	int dest_sw = flit->destSwitch;
	assert(dest_sw >= 0 && dest_sw < g_number_switches);
	int outP;
	switchModule * current_sw = this;
	double base_latency = 0, current_latency;
	while (current_sw->label != dest_sw) {
		outP = current_sw->routing->minOutputPort(dest);
		assert(outP >= 0);
		current_latency = (double) (current_sw->inPorts[outP]->getDelay(flit->cos, 0));
		base_latency += current_latency; /* Add output link delay */
		current_sw = current_sw->routing->neighList[outP]; /* Advance to next switch */
	}
	/* Add consume delay (since the pkt has a non-null length, it
	 * needs to be accounted for the pkt to exit the network. */
	base_latency += g_flit_size;

	return base_latency;
}

/*
 * Checks if an input port is ready to receive a flit.
 */
bool switchModule::nextPortCanReceiveFlit(int port) {
	assert(port >= g_p_computing_nodes_per_router);
	bool canReceiveFlit = true;

	int nextPort = routing->neighPort[port];
	switchModule * nextSw = routing->neighList[port];
	for (int cos = 0; cos < this->cosLevels; cos++)
		for (int vc = 0; vc < g_channels; vc++)
			canReceiveFlit &= nextSw->inPorts[nextPort]->canReceiveFlit(cos, vc);

	return canReceiveFlit;
}

/*
 * Retrieves head-of-buffer flit for a given combination of
 * input port, cos level and virtual channel.
 */
flitModule * switchModule::getFlit(int port, unsigned short cos, int vc) {
	flitModule *flitEx;
	inPorts[port]->checkFlit(cos, vc, flitEx);
	assert(flitEx != NULL);
	return flitEx;
}

/*
 * Returns current set output port (when flit is part of the
 * body of a packet)
 */
int switchModule::getCurrentOutPort(int port, unsigned short cos, int vc) {
	return inPorts[port]->getOutCurPkt(cos, vc);
}

/*
 * Returns current set output channel (when flit is part of
 * the body of a packet)
 */
int switchModule::getCurrentOutVC(int port, unsigned short cos, int vc) {
	return inPorts[port]->getNextVcCurPkt(cos, vc);
}

bool switchModule::isVCUnlocked(flitModule * flit, int port, int vc) {
	return (routing->neighList[port]->inPorts[routing->neighPort[port]]->unLocked(flit->cos, vc)
			|| flit->packetId == routing->neighList[port]->inPorts[routing->neighPort[port]]->getPktLock(flit->cos, vc));
}

void switchModule::trackTransitStatistics(flitModule *flitEx, int input_channel, int outP, int nextC) {
	assert(
			(flitEx->flitType != CNM
					&& ((outP < g_global_router_links_offset && nextC <= this->routing->vcM->getHighestVc(portClass::local))
							|| (outP >= g_global_router_links_offset && nextC <= this->routing->vcM->getHighestVc(portClass::global))))
					|| (flitEx->flitType == CNM
							&& ((outP < g_global_router_links_offset && nextC > this->routing->vcM->getHighestVc(portClass::local)
									&& nextC <= this->qcnRouting->vcM->getHighestVc(portClass::local))
									|| (outP >= g_global_router_links_offset
											&& nextC > this->routing->vcM->getHighestVc(portClass::global)
											&& nextC <= this->qcnRouting->vcM->getHighestVc(portClass::global)))));
	bool input_emb_escape, output_emb_escape, subnetworkInjection = false;

	/* Update hop & contention counters */
	flitEx->addHop(outP, this->label);
	flitEx->subsContention(outP, this->label);

	/* CONTENTION COUNTERS (time waiting in queues) */
	switch (g_deadlock_avoidance) {
		case RING:
			subnetworkInjection = (outP >= this->portCount - 2) && !((outP == this->portCount - 2) && (this->aPos == 0))
					&& !((outP == this->portCount - 1) && (this->aPos == g_a_routers_per_group - 1));
			break;
		case EMBEDDED_RING:
		case EMBEDDED_TREE:
			input_emb_escape = g_local_link_channels <= input_channel
					&& input_channel < (g_local_link_channels + g_channels_escape);
			output_emb_escape = g_local_link_channels <= nextC && nextC < (g_local_link_channels + g_channels_escape);
			subnetworkInjection = output_emb_escape && !input_emb_escape;
			break;
		default:
			break;
	}

	increasePortCount(outP);
	increaseVCCount(nextC, outP);

	if (subnetworkInjection) {
		if (this->hPos == g_tree_root_switch) flitEx->rootSubnetworkInjectionsCount++;
		if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
		if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
		flitEx->addSubnetworkInjection();
	}
}

/* PROVISIONAL: when a history window is used, it
 * also tracks the number of sent flits in the last previous cycles. */
void switchModule::updateMisrouteCounters(int outP, flitModule * flitEx) {
	/* Misrouted flits counters */
	if (g_internal_cycle >= g_warmup_cycles) {
		g_attended_flit_counter++;
		MisrouteType misroute_type = flitEx->getCurrentMisrouteType();
		switch (misroute_type) {
			case NONE:
				assert(outP == routing->minOutputPort(flitEx->destId));
				g_min_flit_counter[g_iteration]++;
				break;
			case LOCAL:
			case LOCAL_MM:
				g_local_misrouted_flit_counter[g_iteration]++;
				break;
			case GLOBAL_MANDATORY:
				g_global_mandatory_misrouted_flit_counter[g_iteration]++;
			case VALIANT:
				if (outP == routing->minOutputPort(flitEx->destId))
					g_min_flit_counter[g_iteration]++;
				else
					g_global_misrouted_flit_counter[g_iteration]++;
				break;
			case GLOBAL:
				g_global_misrouted_flit_counter[g_iteration]++;
				break;
			default:
				assert(0);
				break;
		}
	}

	if ((g_congestion_detection == HISTORY_WINDOW || g_congestion_detection == HISTORY_WINDOW_AVG)
			&& outP >= g_p_computing_nodes_per_router) {
		((contAdpRouting *) routing)->congestionWindow[outP][flitEx->cos].back() += flitEx->length;
		assert(((contAdpRouting *) routing)->congestionWindow[outP][flitEx->cos].back() <= flitEx->length);
	}

}

/*
 * Makes effective flit transferal from one buffer to another. If flit is to be consumed, tracks
 * its statistics and deletes it.
 */
void switchModule::xbarTraversal(int input_port, unsigned short cos, int input_channel, int outP, int nextP,
		int nextC) {
	assert(outP >= 0 && outP < this->portCount);
	flitModule *flitEx;
	bool nextVC_unLocked, input_emb_escape = false, output_emb_escape = false, input_phy_ring = false, output_phy_ring =
			false, subnetworkInjection = false;

#if DEBUG
	inPorts[input_port]->checkFlit(input_channel, flitEx);
	cout << "SERVE PORT: cycle " << g_cycle << "--> SW " << label << " input Port " << input_port << " VC "
	<< input_channel << " flit " << flitEx->flitId << " output Port " << outP << " VC " << nextC;
	if (outP >= g_local_router_links_offset) {
		cout << " --> SW " << routing->neighList[outP]->label << " input Port " << routing->neighPort[outP];
	}
	cout << endl << "------------------------------------------------------------" << endl;
	cout << endl << endl;
#endif

	assert(nextC >= 0 && nextC < vcCount);

	inPorts[input_port]->checkFlit(cos, input_channel, flitEx);
	assert(inPorts[input_port]->canSendFlit(cos, input_channel));
	assert(inPorts[input_port]->extract(cos, input_channel, flitEx, g_flit_size));

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

	/* Send credits to previous switch */
	sendCredits(input_port, cos, input_channel, flitEx);

	/* Update contention counters (time waiting in queues) */
	flitEx->addContention(input_port, this->label);

	if (outP < g_p_computing_nodes_per_router) {
		if (g_generators_list[this->label * g_p_computing_nodes_per_router + outP]->checkConsume(flitEx))
			/* Pkt is consumed */
			g_generators_list[this->label * g_p_computing_nodes_per_router + outP]->consumeFlit(flitEx, input_port,
					input_channel);
	} else {
		/* Pkt is transmitted */
#if DEBUG
		cout << "Tx pkt " << flitEx->flitId << " from source " << flitEx->sourceId << " (sw " << flitEx->sourceSW
		<< ", group " << flitEx->sourceGroup << ") to dest " << flitEx->destId << " (sw " << flitEx->destSwitch
		<< ", group " << flitEx->destGroup << "). Sending pkt from sw " << this->label << " to sw "
		<< routing->neighList[outP]->label << " through port " << outP << " (VC " << nextC << ")" << endl;
		if (outP != routing->minOutputPort(flitEx))
		cout << "Min out " << routing->minOutputPort(flitEx) << " [VAL node is " << flitEx->valId << "]" << endl;
#endif

		if (!this->nextPortCanReceiveFlit(outP))
			cerr << "Cycle " << g_internal_cycle << "\tSw " << this->label << " can NOT tx flit " << flitEx->flitId
					<< "through outP " << outP << " to sw " << routing->neighList[outP]->label << " since port "
					<< routing->neighPort[outP] << " is not available." << endl;
		assert(this->nextPortCanReceiveFlit(outP));
		/* Check VC is unlocked */
		if (routing->neighList[outP]->inPorts[routing->neighPort[outP]]->unLocked(flitEx->cos, nextC)) {
			nextVC_unLocked = true;
		} else if (flitEx->packetId
				== routing->neighList[outP]->inPorts[routing->neighPort[outP]]->getPktLock(flitEx->cos, nextC)) {
			nextVC_unLocked = true;
		} else {
			nextVC_unLocked = false;
		}
		assert(nextVC_unLocked);

		assert(getCredits(outP, cos, nextC) >= g_flit_size);

		if (flitEx->head == 1) {
			routing->neighList[outP]->inPorts[nextP]->setPktLock(cos, nextC, flitEx->packetId);
			routing->neighList[outP]->inPorts[nextP]->setPortPktLock(cos, nextC, input_port);
			routing->neighList[outP]->inPorts[nextP]->setVcPktLock(cos, nextC, input_channel);
			routing->neighList[outP]->inPorts[nextP]->setUnlocked(cos, nextC, 0);
		}
		assert(flitEx->packetId == routing->neighList[outP]->inPorts[nextP]->getPktLock(cos, nextC));
		if (flitEx->tail == 1) {
			routing->neighList[outP]->inPorts[nextP]->setPktLock(cos, nextC, -1);
			routing->neighList[outP]->inPorts[nextP]->setPortPktLock(cos, nextC, -1);
			routing->neighList[outP]->inPorts[nextP]->setVcPktLock(cos, nextC, -1);
			routing->neighList[outP]->inPorts[nextP]->setUnlocked(cos, nextC, 1);
		}
		switch (g_deadlock_avoidance) {
			case RING:
				input_phy_ring = input_port >= (this->portCount - 2);
				output_phy_ring = nextP >= (this->portCount - 2);
				subnetworkInjection = (outP >= this->portCount - 2)
						&& !((outP == this->portCount - 2) && (this->aPos == 0))
						&& !((outP == this->portCount - 1) && (this->aPos == g_a_routers_per_group - 1));
				break;
			case EMBEDDED_RING:
			case EMBEDDED_TREE:
				input_emb_escape = g_local_link_channels <= input_channel
						&& input_channel < (g_local_link_channels + g_channels_escape);
				output_emb_escape = g_local_link_channels <= nextC
						&& nextC < (g_local_link_channels + g_channels_escape);
				subnetworkInjection = output_emb_escape && !input_emb_escape;
				break;
			default:
				break;
		}
		if (subnetworkInjection) {
			if (this->hPos == g_tree_root_switch) flitEx->rootSubnetworkInjectionsCount++;
			if (this->hPos == flitEx->sourceGroup) flitEx->sourceSubnetworkInjectionsCount++;
			if (this->hPos == flitEx->destGroup) flitEx->destSubnetworkInjectionsCount++;
			flitEx->addSubnetworkInjection();
		}

		flitEx->setChannel(nextC);

		/* Update various misrouting flags */
		if (flitEx->head == 1) {
			MisrouteType misroute_type = flitEx->getCurrentMisrouteType();
			//LOCAL MISROUTING
			if (misroute_type == LOCAL || misroute_type == LOCAL_MM) {
				assert(outP >= g_local_router_links_offset);
				assert(outP < g_global_router_links_offset);
				assert(not (output_emb_escape));
				flitEx->setMisrouted(true, misroute_type);
				flitEx->localMisroutingDone = 1;
			}
			//GLOBAL MISROUTING
			if (misroute_type == GLOBAL || misroute_type == GLOBAL_MANDATORY) {
				assert(outP >= g_global_router_links_offset);
				assert(outP < g_global_router_links_offset + g_h_global_ports_per_router);
				assert(not (output_emb_escape));
				flitEx->setMisrouted(true, misroute_type);
				flitEx->globalMisroutingDone = 1;

				//Global misrouting at injection
				if (input_port < g_p_computing_nodes_per_router) {
					assert(misroute_type == GLOBAL);
					flitEx->setGlobalMisrouteAtInjection(true);
				}
			}
			//MANDATORY GLOBAL MISROUTING ASSERTS
			if (flitEx->mandatoryGlobalMisrouting_flag && not (input_phy_ring) && not (output_phy_ring)
					&& not (input_emb_escape) && not (output_emb_escape)) {
				assert(flitEx->globalMisroutingDone);
				assert(outP >= g_global_router_links_offset);
				assert(outP < g_global_router_links_offset + g_h_global_ports_per_router);
				assert(misroute_type == GLOBAL_MANDATORY);
			}

			//RESET some flags when changing group
			if (hPos != routing->neighList[outP]->hPos) {
				flitEx->mandatoryGlobalMisrouting_flag = 0;
				flitEx->localMisroutingDone = 0;
			}

			/* If global misrouting policy is MM (+L), and current group is source group and different
			 * from destination group, if no previous misrouting has been conducted mark flag for
			 * global mandatory misrouting (to prevent the appearence of cycles). We also need to test
			 * flit is not transitting through subescape network.
			 */
			switch (g_global_misrouting) {
				case MM:
				case MM_L:
					if ((input_port >= g_local_router_links_offset) && (input_port < g_global_router_links_offset)
							&& (outP >= g_local_router_links_offset) && (outP < g_global_router_links_offset)
							&& (this->hPos == flitEx->sourceGroup) && (this->hPos != flitEx->destGroup)
							&& not (flitEx->globalMisroutingDone) && not (output_emb_escape) && not (input_emb_escape)
							&& not (flitEx->getCurrentMisrouteType() == VALIANT)) {
						flitEx->mandatoryGlobalMisrouting_flag = 1;
#if DEBUG
						if (outP == routing->minOutputPort(flit->destId))
						cerr << "ERROR in sw " << this->label << " with flit " << flitEx->flitId
						<< " from src " << flitEx->sourceId << " to dest " << flitEx->destId
						<< " traversing from inPort " << input_port << " to outPort " << outP << endl;
#endif
						assert(outP != routing->minOutputPort(flitEx->destId));
					}
					break;
				case NRG:
				case NRG_L:
				case RRG:
				case RRG_L:
					if ((outP != routing->minOutputPort(flitEx->destId)) && (input_port < g_global_router_links_offset)
							&& (outP >= g_local_router_links_offset) && (outP < g_global_router_links_offset)
							&& (this->hPos == flitEx->sourceGroup) && (this->hPos != flitEx->destGroup)
							&& not (flitEx->globalMisroutingDone) && not (output_emb_escape) && not (input_emb_escape)
							&& not (flitEx->getCurrentMisrouteType() == VALIANT)) {
						flitEx->mandatoryGlobalMisrouting_flag = 1;
					}
			}
		}

		outPorts[outP]->insert(nextC, flitEx, g_flit_size);
		if (g_congestion_management == QCNSW) this->qcnRpTxBCount[outP] -= g_flit_size;

		if (input_port < g_p_computing_nodes_per_router
				|| (g_congestion_management == QCNSW && input_port == g_qcn_port)) {
			flitEx->setChannel(nextC);
			assert(flitEx->injLatency < 0);
			flitEx->injLatency = g_internal_cycle - flitEx->inCycle;
			assert(flitEx->injLatency >= 0);
		}

		/* Contention-Aware misrouting trigger notification */
		if (g_contention_aware && g_increaseContentionAtHeader) {
			m_ca_handler.flitSent(flitEx, input_port, g_flit_size);
		}

		updateMisrouteCounters(outP, flitEx); /* MIN/Misroute counters have to be updated here not to mess up with IOQ switches */

		trackTransitStatistics(flitEx, input_channel, outP, nextC);
	}
}

void switchModule::printSwitchStatus() {
	int p, vc, occ, f;
	flitModule *fl;
	cerr << "Switch " << this->label << " (cycle " << g_cycle << ")" << endl;
	for (p = 0; p < g_p_computing_nodes_per_router; p++) {
		for (vc = 0; vc < g_injection_channels; vc++) {
			occ = inPorts[p]->getBufferOccupancy(0, vc);
			if (occ != 0) {
				cerr << "Port " << setfill(' ') << setw(2) << p << ", vc " << setfill(' ') << setw(2) << vc
						<< " - occup " << setfill(' ') << setw(3) << occ << ": ";
				for (f = occ / g_flit_size - 1; f >= 0; f--) {
					inPorts[p]->checkFlit(0, vc, fl, f);
					cerr << "Dst " << setfill(' ') << setw(5) << fl->destId << " (gr " << setfill(' ') << setw(3)
							<< fl->destGroup << ") |";
				}
				cerr << endl;
			}
		}
	}
	for (p = g_p_computing_nodes_per_router; p < g_global_router_links_offset + g_h_global_ports_per_router; p++) {
		for (vc = 0; vc < g_channels; vc++) {
			occ = inPorts[p]->getBufferOccupancy(0, vc);
			if (occ != 0) {
				cerr << "Port " << setfill(' ') << setw(2) << p << ", vc " << setfill(' ') << setw(2) << vc
						<< " - occup " << setfill(' ') << setw(3) << occ << ": ";
				for (f = occ / g_flit_size - 1; f >= 0; f--) {
					inPorts[p]->checkFlit(0, vc, fl, f);
					cerr << "Dst " << setfill(' ') << setw(5) << fl->destId << " (gr " << setfill(' ') << setw(3)
							<< fl->destGroup << ") |";
				}
				cerr << endl;
			}
		}
	}
	cerr << "-------------------------------------------------------------------------" << endl;
}

short int switchModule::getPortEnrouteMinimalProbability(int port) {
	assert(port >= g_p_computing_nodes_per_router && port < g_global_router_links_offset + g_h_global_ports_per_router);
	return this->portEnrouteMinProb[port];
}

/*
 * Occupancy Sampling for QCN defined in: Occupancy Sampling for Terabit CEE Switches
 * by:
 * Fredy D. Neeser, Nikolaos I. Chrysos, Roft Clauberf, Daniel Crisan, Mith Gusat, Cyriel Minkenberg
 * Kenneth M. Valk, Claude Basso
 * This process is part of QCN Congestion Point
 */
void switchModule::qcnOccupancySampling(int port) {
	assert(g_congestion_management == QCNSW);
	assert(port >= g_p_computing_nodes_per_router && port < g_global_router_links_offset + g_h_global_ports_per_router);
	assert(this->qcnCpSamplingCounter[port] <= 0);

	short qcnFb;
	int qcnQlen = 0, qcnQoff, qcnQdelta;
	int maxQcnFb = g_qcn_q_eq * (2 * g_qcn_w + 1);
	int portOccupancy = 0;

	/* Calculate occupancy of port in phits */
	for (int i = 0; i < vcCount; i++)
		for (short c = 0; c < this->cosLevels - 1; c++)
			qcnQlen += inPorts[port]->getBufferOccupancy(c, i); /* phits */
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

	/* Select culprit flow and send congestion notification if: */
	if (portOccupancy > 0) { // currently there are packets in port
		if (qcnFb > 0 and ((rand() % 100 + 1) <= g_qcn_cnms_percent)) {
			/* If feedback is possitive and only send g_qcn_cnms_percent% of CNMs */
			/* Get a random flit of input buffer */
			flitModule *flitRand = NULL, *cnmFlit;
			int ranPktIdx = (rand()) % portOccupancy;
			int aux = 0, aux2 = 0;
			for (short c = 0; c < this->cosLevels - 1; c++)
				for (int i = 0; i < vcCount && flitRand == NULL; i++) {
					aux2 = aux;
					aux += inPorts[port]->getBufferOccupancy(c, i) / g_flit_size;
					if (aux >= ranPktIdx) inPorts[port]->checkFlit(c, i, flitRand, ranPktIdx - aux2);
				}
			assert(flitRand != NULL && flitRand->flitType != CNM);
			/* Generate Congestion Nofitication Message (this messages are not accounted as data traffic */
			cnmFlit = new flitModule(g_tx_cnmFlit_counter, g_tx_cnmFlit_counter, 0,
					this->label * g_p_computing_nodes_per_router, flitRand->sourceId,
					(int) (flitRand->sourceId / g_p_computing_nodes_per_router), 0, true, true, g_cos_levels - 1, CNM);
			cnmFlit->channel = g_generators_list[this->label * g_p_computing_nodes_per_router]->getInjectionVC(
					flitRand->sourceId, CNM);
			cnmFlit->setQcnParameters(qcnFb, qcnQoff, qcnQdelta);
			cnmFlit->inCycle = g_cycle;
			cnmFlit->inCyclePacket = g_cycle;
			if (this->switchModule::getCredits(g_qcn_port, g_cos_levels - 1, cnmFlit->channel) >= g_flit_size) { // there are space in qcnPort
				injectFlit(g_qcn_port, cnmFlit->channel, cnmFlit);
				if (g_cycle >= g_warmup_cycles) this->cnmPacketsInj++;
			} else
				delete cnmFlit;
			/* When a CNM is generated for a switch, this sw reuse the same information for modifying its routing table */
			if ((g_qcn_implementation == QCNSWSELF || g_qcn_implementation == QCNSWCOMPLETE)
					&& flitRand->destGroup != (this->label / g_a_routers_per_group)) {
				int outP = this->routing->minOutputPort(flitRand->destId);
				assert(outP >= g_local_router_links_offset and outP < g_qcn_port);
				if (g_qcn_policy == AIMD || g_qcn_policy == MIMD)
					portEnrouteMinProb[outP] *= (float) (1 - g_qcn_gd * qcnFb);
				else
					portEnrouteMinProb[outP] -= (float) (g_qcn_gd * qcnFb);
				if (portEnrouteMinProb[outP] < g_qcn_min_rate) portEnrouteMinProb[outP] = g_qcn_min_rate;
				if (this->label < g_a_routers_per_group)
					g_qcn_g0_port_enroute_min_prob[g_cycle][this->label][outP - g_p_computing_nodes_per_router] =
							portEnrouteMinProb[outP];
			}
		}
		// Reset timer and save current queue length in old qlen.
		this->qcnCpSamplingCounter[port] = g_qcn_cp_sampling_interval;
		qcnQlenOld[port] = qcnQlen;
	}
}

void switchModule::qcnMinProbabilityDecrease(int port, float qcnfb) {
	assert((1 - g_qcn_gd * qcnfb) >= 0.0 && (1 - g_qcn_gd * qcnfb) < 1.0);
	if (g_qcn_policy == AIMD || g_qcn_policy == MIMD)
		portEnrouteMinProb[port] *= (float) (1 - g_qcn_gd * qcnfb);
	else
		portEnrouteMinProb[port] -= (float) (g_qcn_gd * qcnfb);
	if (portEnrouteMinProb[port] < g_qcn_min_rate) portEnrouteMinProb[port] = g_qcn_min_rate;
	if (this->label < g_a_routers_per_group)
		g_qcn_g0_port_enroute_min_prob[g_cycle][this->label][port - g_p_computing_nodes_per_router] =
				portEnrouteMinProb[port];
}

void switchModule::qcnFeedbackComparison(int port, float qcnfb) {
	// Save feedback values received */
	qcnPortFb[port] = qcnfb;
	// Calculate average feedback in local and global ports (Fb avg for this switch)
	qcnPortFb[0] = 0;
	for (int aux = g_p_computing_nodes_per_router; aux < g_qcn_port; aux++)
		qcnPortFb[0] += qcnPortFb[aux];
	qcnPortFb[0] /= (g_a_routers_per_group - 1 + g_h_global_ports_per_router);
	if (this->label < g_a_routers_per_group) {
		g_qcn_g0_port_congestion[g_cycle][this->label][port] = qcnPortFb[port];
		g_qcn_g0_port_congestion[g_cycle][this->label][0] = qcnPortFb[0];
	}
	/*
	 * # = average
	 * Fb can be in position 1, 2, 3 with the following result:
	 *                              #<---->th1         th2
	 * <------------ increase (1)--------->
	 *                                     <nothing (2)>
	 *                                                  <--- reduce (3) -->
	 * --------------------------------------------------------------------
	 */
	// If port feedback is less than average value plus threshold to increase -> increase probability
	if (qcnPortFb[port] <= qcnPortFb[0] + g_qcn_th1) {
		if (g_qcn_policy == AIMD || g_qcn_policy == AIAD)
			this->portEnrouteMinProb[port] += g_qcn_r_ai;
		else
			this->portEnrouteMinProb[port] *= g_qcn_r_ai;
		if (this->portEnrouteMinProb[port] > 100) this->portEnrouteMinProb[port] = 100;
		if (this->label < g_a_routers_per_group)
			g_qcn_g0_port_enroute_min_prob[g_cycle][this->label][port - g_p_computing_nodes_per_router] =
					portEnrouteMinProb[port];
	} else if (qcnPortFb[port] > qcnPortFb[0] + g_qcn_th2) {
		// Multiplicative decrease of probability based on feedback comparison
		if (g_qcn_policy == AIMD || g_qcn_policy == MIMD) {
			float reduction = 1 - g_qcn_gd * (qcnPortFb[port] - qcnPortFb[0]);
			assert(reduction >= 0 && reduction < 1);
			portEnrouteMinProb[port] *= reduction;
		} else {
			float reduction = g_qcn_gd * (qcnPortFb[port] - qcnPortFb[0]);
			assert(reduction >= 0 && reduction < 64);
			portEnrouteMinProb[port] -= reduction;
		}
		if (portEnrouteMinProb[port] < g_qcn_min_rate) portEnrouteMinProb[port] = g_qcn_min_rate;
		if (this->label < g_a_routers_per_group)
			g_qcn_g0_port_enroute_min_prob[g_cycle][this->label][port - g_p_computing_nodes_per_router] =
					portEnrouteMinProb[port];
	} // if fb is between mean + th1 and mean + th2 -> do nothing
}

void switchModule::acorResetSwitchHysteresisStatus() {
    this->acor_hyst_cycles_counter = g_acor_hysteresis_cycle_duration_cycles;
    this->acor_packets_blocked_counter = 0;
    switch (g_acor_state_management) {
        case SWITCHCGCSRS:
            switch (acorSwStatus) {
                case CRGLGr:
                    acor_inc_state_th_packets = g_acor_inc_state_first_th_packets;
                    acor_dec_state_th_packets = INT_MIN; // Never happens
                    break;
                case CRGLSw:
                    acor_inc_state_th_packets = g_acor_inc_state_second_th_packets;
                    acor_dec_state_th_packets = g_acor_dec_state_first_th_packets;
                    break;
                case RRGLSw:
                    acor_inc_state_th_packets = INT_MAX; // Never happens
                    acor_dec_state_th_packets = g_acor_dec_state_second_th_packets;
                    break;
                default:
                    cerr << "ERROR: Status not defined" << endl;
                    assert(false);
            }
            break;
        case SWITCHCGRS:
            switch (acorSwStatus) {
                case CRGLGr:
                    acor_inc_state_th_packets = g_acor_inc_state_first_th_packets;
                    acor_dec_state_th_packets = INT_MIN; // Never happens
                    break;
                case RRGLSw:
                    acor_inc_state_th_packets = INT_MAX; // Never happens
                    acor_dec_state_th_packets = g_acor_dec_state_first_th_packets;
                    break;
                default:
                    cerr << "ERROR: Status not defined" << endl;
                    assert(false);
            }
            break;
        case SWITCHCSRS:
            switch (acorSwStatus) {
                case CRGLSw:
                    acor_inc_state_th_packets = g_acor_inc_state_first_th_packets;
                    acor_dec_state_th_packets = INT_MIN; // Never happens
                    break;
                case RRGLSw:
                    acor_inc_state_th_packets = INT_MAX; // Never happens
                    acor_dec_state_th_packets = g_acor_dec_state_first_th_packets;
                    break;
                default:
                    cerr << "ERROR: Status not defined" << endl;
                    assert(false);
            }
            break;
        default:
            cerr << "ERROR: Switch state management not defined" << endl;
            assert(false);
    }
}

acorState switchModule::acorGetState() {
    return this->acorSwStatus;
}

void switchModule::acorIncState() {
    switch (g_acor_state_management) {
        case SWITCHCGCSRS:
        case SWITCHCSRS:
            acorSwStatus = static_cast<acorState> (acorSwStatus + 1);
            break;
        case SWITCHCGRS:
            acorSwStatus = static_cast<acorState> (acorSwStatus + 2);
            break;
        default:
            cerr << "ERROR: Switch state management not defined" << endl;
            assert(false);
    }
    assert(acorSwStatus >= CRGLGr && acorSwStatus <= RRGLSw);
}

void switchModule::acorDecState() {
    switch (g_acor_state_management) {
        case SWITCHCGCSRS:
        case SWITCHCSRS:
            acorSwStatus = static_cast<acorState> (acorSwStatus - 1);
            break;
        case SWITCHCGRS:
            acorSwStatus = static_cast<acorState> (acorSwStatus - 2);
            break;
        default:
            cerr << "ERROR: Switch state management not defined" << endl;
            assert(false);
    }
    assert(acorSwStatus >= CRGLGr && acorSwStatus <= RRGLSw);
}
