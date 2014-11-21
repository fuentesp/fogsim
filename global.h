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

#ifndef GLOBAL_H
#define	GLOBAL_H

#include <fstream>
#include <string>
#include <vector>
#include <map>

using namespace std;

class generatorModule;
class switchModule;

extern bool g_ageArbiter;
extern long long g_cycle;
extern long long g_maxCycles;
extern long long g_warmCycles;
extern long long g_queueGen;
extern long long g_queueSwtch_local;
extern long long g_queueSwtch_global;
extern int g_BUBBLE;
extern int g_dP;
extern int g_dA;
extern int g_dH;
extern int g_offsetA;
extern int g_offsetH;
extern int g_globalLinksPerGroup;
extern long long g_delayTransmission_local;
extern long long g_delayTransmission_global;
extern long long g_delayInjection;
extern int g_switchesCount;
extern int g_generatorsCount;
extern int g_channels;
extern int g_channels_useful;
extern int g_channels_inj;
extern int g_channels_glob;
extern long long g_flitCount;
extern long long g_packetCount;
extern long long g_warmFlitCount;
extern long long g_flitReceivedCount;
extern long long g_warmFlitReceivedCount;
extern long long g_warmPacketCount;
extern long long g_packetReceivedCount;
extern long long g_warmPacketReceivedCount;
/* number of received messages that were misrouted (not counting warmup)*/
extern long long g_misroutedFlitCount;
extern long long g_global_misroutedFlitCount;
extern long long g_local_misroutedFlitCount;
extern int g_arbiter_iterations;

extern int g_flitSize;
extern int g_packetSize;
extern int g_flits_per_packet;
extern int g_probability;
extern ifstream g_cofigurationFile;
extern ofstream g_outputFile;
extern char g_trcFile[128];
extern char g_pcfFile[128];
extern long long g_portUseCount[];
extern long long g_portContentionCount[];
extern long long g_portAlreadyAssignedCount[];
extern long long g_mi_seed;
extern long long g_totalHopCount;
extern long long g_totalLocalHopCount;
extern long long g_totalGlobalHopCount;
extern long long g_totalLocalRingHopCount;
extern long long g_totalGlobalRingHopCount;
extern long long g_totalLocalTreeHopCount;
extern long long g_totalGlobalTreeHopCount;
extern long long g_totalSubnetworkInjectionsCount;
extern long long g_totalRootSubnetworkInjectionsCount;
extern long long g_totalSourceSubnetworkInjectionsCount;
extern long long g_totalDestSubnetworkInjectionsCount;
extern long long g_totalLocalContentionCount;
extern long long g_totalGlobalContentionCount;
extern long long g_totalLocalRingContentionCount;
extern long long g_totalGlobalRingContentionCount;
extern long long g_totalLocalTreeContentionCount;
extern long long g_totalGlobalTreeContentionCount;

extern int g_valiant;
extern bool g_valiant_long;
extern int g_dally;
extern bool g_strictMisroute;
extern bool g_naive_strictMisroute;
extern bool g_odd_strictMisroute;
extern int g_ugal;
extern int g_rings;
extern int g_ringDirs;
extern bool g_onlyRing2;
extern int g_trees;
extern int g_treeRoot_node;
extern int g_rootP;
extern int g_rootH;
extern int g_traffic;
extern int g_mix_traffic;
extern int g_random_percent;
extern int g_adv_local_percent;
extern int g_adv_global_percent;
extern int g_restrictLocalCycles;
extern long long g_totalLatency;
extern long long g_totalPacketLatency;
extern long long g_totalInjectionQueueLatency;
extern long long g_totalBaseLatency;
extern long long g_flitExLatency;
extern long long g_packetLatency;
extern int g_forceMisrouting;
extern bool g_try_just_escape;
/* Number of physical dedicated ring ports */
extern int g_ring_ports;
/* Packets are sent to the ring ONLY when no credits left */
extern bool g_restrict_ring_injection;
/* NEVER send packets from inj.queues to the ring */
extern bool g_forbid_from_injQueues_to_ring;
extern bool g_variable_threshold;
extern int g_percent_local_threshold;
extern int g_percent_global_threshold;

extern unsigned int g_petitions;
extern unsigned int g_petitions_served;
extern unsigned int g_petitionsInj;
extern unsigned int g_petitionsInj_served;

extern generatorModule **g_generatorsList;
extern switchModule **g_switchesList;
extern vector<long long> g_received;
extern vector<long long> g_sent;
extern long long g_warmTotalLatency;
extern long long g_warmTotalPacketLatency;
extern long long g_warmPacketTotalLatency;
extern long long g_warmTotalInjLatency;
extern int g_ports;

extern long long g_maxPacketsInj;
extern int g_SWmaxPacketsInj;
extern long long g_minPacketsInj;
extern int g_SWminPacketsInj;

extern bool g_palm_tree_configuration;
extern bool g_piggyback;
extern int g_piggyback_coef;
extern int g_ugal_global_threshold;
extern int g_ugal_local_threshold;

extern int g_traffic_adv_dist;
extern int g_traffic_adv_dist_local;
extern int g_transient_traffic;
extern int g_transient_traffic_next_type;
extern int g_transient_traffic_next_dist;
extern int g_transient_traffic_cycle;
extern int g_transient_record_len;
extern int g_transient_record_num_cycles;
extern int g_transient_record_num_prev_cycles;
/* 
 * Number of generated packets and agregated latency, 
 * for each recorded cycle
 */
extern int *g_transient_record_latency;
extern int *g_transient_record_num_flits;
extern int *g_transient_record_num_misrouted;

extern int g_embeddedRing;
extern int g_channels_ring;
extern int g_globalEmbeddedRingSwitchesCount;
extern int g_localEmbeddedRingSwitchesCount;

extern int g_embeddedTree;
extern int g_channels_tree;
extern int g_globalEmbeddedTreeSwitchesCount;
extern int g_localEmbeddedTreeSwitchesCount;

extern bool g_baseCongestionControl;
extern int g_baseCongestionControl_bub;
extern bool g_escapeCongestionControl;
extern int g_escapeCongestion_th;

extern bool g_AllToAll;
extern int g_phases;
extern bool g_naive_AllToAll;
extern bool g_random_AllToAll;
extern int g_AllToAll_generators_finished_count;

extern bool g_burst;
extern int g_burst_length; //In packets
extern int g_burst_flits_length; //In flits
extern int *g_burst_traffic_type;
extern int *g_burst_traffic_adv_dist;
extern int *g_burst_traffic_type_percent;
extern int g_burst_generators_finished_count;

//GROUP 0, per switch average latency
extern long long *g_group0_totalLatency;
extern long long *g_group0_numFlits;
//GROUP ROOT, per switch average latency
extern long long *g_groupRoot_totalLatency;
extern long long *g_groupRoot_numFlits;

//Livelock control
extern long long g_max_hops;
extern long long g_max_local_hops;
extern long long g_max_global_hops;
extern long long g_max_local_ring_hops;
extern long long g_max_global_ring_hops;
extern long long g_max_local_tree_hops;
extern long long g_max_global_tree_hops;
extern long long g_max_global_tree_hops;
extern long long g_max_subnetwork_injections;
extern long long g_max_root_subnetwork_injections;
extern long long g_max_source_subnetwork_injections;
extern long long g_max_dest_subnetwork_injections;

//VC based on the fly misrouting
extern bool g_vc_misrouting;
extern bool g_vc_misrouting_mm;
extern bool g_vc_misrouting_crg;
extern bool g_OFAR_misrouting;
extern bool g_OFAR_misrouting_mm;
extern bool g_OFAR_misrouting_crg;
/* allow local misroute in intermediate and destination groups */
extern bool g_vc_misrouting_local;
extern bool g_OFAR_misrouting_local;
extern bool g_vc_misrouting_increasingVC;

extern int g_th_min;

//Latency histogram
extern int g_latency_histogram_maxLat;
extern long long * g_latency_histogram_no_global_misroute;
extern long long * g_latency_histogram_global_misroute_at_injection;
extern long long * g_latency_histogram_other_global_misroute;
//Hops histogram
extern int g_hops_histogram_maxHops;
extern long long * g_hops_histogram;

/*
 * VC_misroute restriction: 
 * if min output is a global link, 
 * then misroute only allowed if that
 * minimal global output is congested
 */
extern bool g_vc_misrouting_congested_restriction;
extern int g_vc_misrouting_congested_restriction_coef_percent;
/* Number of packets */
extern int g_vc_misrouting_congested_restriction_t;

extern bool g_DEBUG;

/*
 * TRACES
 */

extern int g_TRACE_SUPPORT;
extern long g_eventDeadlock;
extern long g_trace_nodes;
extern double g_cpuSpeed;
extern long g_op_per_cycle;
extern long g_multitask;
extern long g_phit_size;
extern bool g_cutmpi;
extern bool g_packet_created;
extern map<long, long> g_events_map;

// ENUM DECLARATIONS
/* 
 * Misrouting port type:
 * -LOCAL
 * -GLOBAL
 * -GLOBAL_MANDATORY
 * -NONE
 */
enum MisrouteType {
	LOCAL, LOCAL_MM, GLOBAL, GLOBAL_MANDATORY, NONE
};

#endif	/* GLOBAL_H */

