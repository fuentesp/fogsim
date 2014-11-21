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

#include <iostream>
#include <fstream>
#include <vector>
#include <map>

#include "global.h"

class generatorModule;
class switchModule;

using namespace std;

bool g_ageArbiter = false;
long long g_cycle = 0; /* current cycle */
long long g_maxCycles = 10000;
long long g_warmCycles = g_maxCycles;
long long g_queueGen = 999999;
long long g_queueSwtch_local = 10;
long long g_queueSwtch_global = 100;
int g_BUBBLE = 1;
int g_numDims = 2;
int g_dP = 4;
int g_dA = 4;
int g_dH = 1;
int g_offsetA = 0;
int g_offsetH = 0;
int g_globalLinksPerGroup = g_dA * g_dH;
long long g_delayTransmission_local = 1;
long long g_delayTransmission_global = 1;
long long g_delayInjection = 1;
int g_ports = g_dP + g_dA - 1 + g_dH;
int g_switchesCount = (g_dA * g_dH + 1) * g_dA;
int g_generatorsCount = g_switchesCount * g_dP;
int g_channels = 1;
int g_channels_useful = 1;
int g_channels_inj = 1;
int g_channels_glob = 1;
long long g_flitCount = 0;
long long g_packetCount = 0;
long long g_warmFlitCount = 0;
long long g_warmPacketCount = 0;
long long g_flitReceivedCount = 0;
long long g_warmFlitReceivedCount = 0;
long long g_warmPacketReceivedCount = 0;
long long g_packetReceivedCount = 0;
long long g_misroutedFlitCount = 0;
long long g_local_misroutedFlitCount = 0;
long long g_global_misroutedFlitCount = 0;
int g_arbiter_iterations = 3;
int g_packetSize = 1;
int g_flitSize = 1;
int g_flits_per_packet = 0;
long g_phit_size = 4; //In bytes
int g_probability = 1;
ofstream g_outputFile;
char g_trcFile[128];
char g_pcfFile[128];
long long g_totalHopCount = 0;
long long g_totalLocalHopCount = 0;
long long g_totalGlobalHopCount = 0;
long long g_totalLocalRingHopCount = 0;
long long g_totalGlobalRingHopCount = 0;
long long g_totalLocalTreeHopCount = 0;
long long g_totalGlobalTreeHopCount = 0;
long long g_totalSubnetworkInjectionsCount = 0;
long long g_totalRootSubnetworkInjectionsCount = 0;
long long g_totalSourceSubnetworkInjectionsCount = 0;
long long g_totalDestSubnetworkInjectionsCount = 0;
long long g_totalLocalContentionCount = 0;
long long g_totalGlobalContentionCount = 0;
long long g_totalLocalRingContentionCount = 0;
long long g_totalGlobalRingContentionCount = 0;
long long g_totalLocalTreeContentionCount = 0;
long long g_totalGlobalTreeContentionCount = 0;
long long g_mi_seed = 1;
long long g_portUseCount[100];
long long g_portContentionCount[100];
long long g_portAlreadyAssignedCount[100];
int g_valiant = 0;
bool g_valiant_long = false;
int g_dally = 0;
bool g_strictMisroute = false;
int g_ugal = 0;
int g_rings = 0;
int g_ringDirs = 0;
bool g_onlyRing2 = 0;
int g_trees = 0;
int g_treeRoot_node = -1;
int g_rootP = -1;
int g_rootH = -1;
int g_traffic = 0;
int g_mix_traffic = 0;
int g_random_percent = 0;
int g_adv_local_percent = 0;
int g_adv_global_percent = 0;
int g_restrictLocalCycles = 0;
long long g_totalLatency = 0;
long long g_totalPacketLatency = 0;
long long g_totalInjectionQueueLatency = 0;
long long g_totalBaseLatency = 0;
long long g_flitExLatency = 0;
long long g_packetLatency = 0;
int g_fjm_times = 0;
int g_forceMisrouting = 0;
bool g_SM = 0;
bool g_SMl = 0;
bool g_lgl = 0;
int g_ring_ports = 0;
bool g_try_just_escape = 0;
bool g_restrict_ring_injection = 1;
bool g_forbid_from_injQueues_to_ring = 1;
bool g_variable_threshold = 0;
int g_percent_local_threshold = 100;
int g_percent_global_threshold = 100;

unsigned int g_petitions = 0;
unsigned int g_petitions_served = 0;
unsigned int g_petitionsInj = 0;
unsigned int g_petitionsInj_served = 0;

generatorModule **g_generatorsList;
switchModule **g_switchesList;
vector<long long> g_received;

vector<long long> g_sent;
long long g_warmTotalLatency;
long long g_warmTotalPacketLatency;
long long g_warmTotalInjLatency;
bool g_palm_tree_configuration = 0;
/* Piggybaking global link states to other switches within the same group */
bool g_piggyback = 0;
int g_piggyback_coef = 200;
int g_ugal_global_threshold = 5;
int g_ugal_local_threshold = 0;
/* Distance to the adverse traffic destination group */
int g_traffic_adv_dist = 1;
int g_traffic_adv_dist_local = 1;
/* Transient traffic */
int g_transient_traffic = 0;
int g_transient_traffic_next_type = 3;
int g_transient_traffic_next_dist = 4;
int g_transient_traffic_cycle = 0;
int g_transient_record_len = 10100;
int g_transient_record_num_cycles = 10000;
int g_transient_record_num_prev_cycles = 100;
int *g_transient_record_latency;
int *g_transient_record_num_flits;
int *g_transient_record_num_misrouted;

/* embedded ring */
int g_embeddedRing = 0;
int g_channels_ring = 0;
int g_globalEmbeddedRingSwitchesCount = -1;
int g_localEmbeddedRingSwitchesCount = -1;
/* embedded tree */
int g_embeddedTree = 0;
int g_channels_tree = 0;
int g_globalEmbeddedTreeSwitchesCount = -1;
int g_localEmbeddedTreeSwitchesCount = -1;
/* Congestion control */
bool g_baseCongestionControl = false;
int g_baseCongestionControl_bub = 0;
bool g_escapeCongestionControl = false;
int g_escapeCongestion_th = 100;

/* FJM free global queues */
bool g_freeGlobalQueues = 0;
int g_fjm_percent_free = 100;

/* SM free local queues */
bool g_freeLocalQueues = 0;
int g_sm_percent_free = 100;

/* all-to-all */
bool g_AllToAll = 0;
int g_phases = -1;
bool g_naive_AllToAll = 0;
bool g_random_AllToAll = 0;
int g_AllToAll_generators_finished_count = 0;

/* Burst traffic */
bool g_burst = 0;
int g_burst_length = 100; //In packets
int g_burst_flits_length = 100; //In flits
int *g_burst_traffic_type;
int *g_burst_traffic_adv_dist;
int *g_burst_traffic_type_percent;
int g_burst_generators_finished_count = 0;

//GROUP 0, per switch average latency
long long *g_group0_totalLatency;
long long *g_group0_numFlits;
//GROUP ROOT, per switch average latency
long long *g_groupRoot_totalLatency;
long long *g_groupRoot_numFlits;

long long g_maxPacketsInj = 0;
int g_SWmaxPacketsInj = -1;
long long g_minPacketsInj = 0;
int g_SWminPacketsInj = -1;

//Livelock control
long long g_max_hops = 0;
long long g_max_local_hops = 0;
long long g_max_global_hops = 0;
long long g_max_local_ring_hops = 0;
long long g_max_global_ring_hops = 0;
long long g_max_local_tree_hops = 0;
long long g_max_global_tree_hops = 0;
long long g_max_subnetwork_injections = 0;
long long g_max_root_subnetwork_injections = 0;
long long g_max_source_subnetwork_injections = 0;
long long g_max_dest_subnetwork_injections = 0;

//VC based on the fly misrouting
bool g_vc_misrouting = 0;
bool g_vc_misrouting_mm = 0;
bool g_vc_misrouting_crg = 0;
bool g_OFAR_misrouting = 0;
bool g_OFAR_misrouting_mm = 0;
bool g_OFAR_misrouting_crg = 0;
/* allow local misroute in intermediate and destination groups */
bool g_vc_misrouting_local = 0;
bool g_OFAR_misrouting_local = 0;
bool g_vc_misrouting_increasingVC = 0;

int g_th_min = 0;

//Latency histogram
int g_latency_histogram_maxLat = 2000;
/* Records the number of flits recieved for each latency value */
long long * g_latency_histogram_no_global_misroute;
long long * g_latency_histogram_global_misroute_at_injection;
long long * g_latency_histogram_other_global_misroute;

//Hops histogram
int g_hops_histogram_maxHops = 200;
/* Records the number of flits recieved for each hops value */
long long * g_hops_histogram;

/*
 * VC_misroute restriction: 
 * if min output is a global link, 
 * then misroute only allowed if that
 * minimal global output is congested
 */
bool g_vc_misrouting_congested_restriction = false;
int g_vc_misrouting_congested_restriction_coef_percent = 150;
/* Number of flits */
int g_vc_misrouting_congested_restriction_t = 5;

/*
 * TRACES
 */

int g_TRACE_SUPPORT = 0;
long g_eventDeadlock = 0;
long g_trace_nodes = 0;
double g_cpuSpeed = 1e9;
long g_op_per_cycle = 50;
long g_multitask = 1;
bool g_cutmpi = 1;
bool g_packet_created = 0;
map<long, long> g_events_map;

/*
 ************************************************
 *  DEBUG MODE
 ************************************************
 */
bool g_DEBUG = false;
