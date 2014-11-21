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

#include <vector>
#include<limits.h>
#include "dgflySimulator.h"
using namespace std;

int module(int a, int b) {
	int c;
	c = a % int(b);
	if (c < 0) {
		c = c + b;
	}
	return (c);
}

bool parity(int a, int b) {
	bool p = (((a % 2 == 0) && (b % 2 == 0)) || ((a % 2 != 0) && (b % 2 != 0)));
	return (p);
}

int main(int argc, char *argv[]) {
	//input parameters
	read_configuration(argv[1]); //READ PARAMETERS IN CONFIG FILE
	g_queueSwtch_local = atoi(argv[3]);
	g_queueSwtch_global = atoi(argv[4]);
	g_BUBBLE = atoi(argv[5]);
	g_probability = atoi(argv[6]);
	g_mi_seed = atoi(argv[7]);
	srand(g_mi_seed);

	//open output file
	g_outputFile.open(argv[2], ios::out);
	if (!g_outputFile) {
		cout << "Can't open the output file" << endl;
		exit(-1);
	}

	if (g_TRACE_SUPPORT != 0) {
		translate_pcf_file();
	}

	create_network();

	if (g_TRACE_SUPPORT != 0) {
		read_trace();
	}

	//run simulation
	run_cycles();

	//write results into output file
	for (int i = 0; i < g_switchesCount; i++) {
		if (g_switchesList[i]->packetsInj > g_maxPacketsInj) {
			g_maxPacketsInj = g_switchesList[i]->packetsInj;
			g_SWmaxPacketsInj = g_switchesList[i]->label;
		}
	}
	g_minPacketsInj = g_maxPacketsInj;
	for (int i = 0; i < g_switchesCount; i++) {
		if (g_switchesList[i]->packetsInj < g_minPacketsInj) {
			g_minPacketsInj = g_switchesList[i]->packetsInj;
			g_SWminPacketsInj = g_switchesList[i]->label;
		}
	}
	assert(g_minPacketsInj <= g_maxPacketsInj);

	cout << "Write output" << endl;
	write_output();
	write_latency_histogram_output(argv[2]);
	write_hops_histogram_output(argv[2]);

	if (g_transient_traffic) {
		write_transient_output(argv[2]);
	}

	//free memory
	free_memory();

	g_outputFile.close();

	cout << "Simulation ended" << endl;

	return 0;
}

/* 
 * Reads config parameters from config file.
 * - Asserted parameters are required.
 * -'If-ed' parameters are optional (see default value in global.cc)
 */
void read_configuration(char * filename) {
	string value;
	configFile config;

	//open configuration file
	if (config.LoadConfig(filename) < 0) {
		cout << "Can't read the configuration file";
		exit(-1);
	}

	//initializations
	//BURST initialization
	g_burst_traffic_adv_dist = new int[3];
	g_burst_traffic_type = new int[3];
	g_burst_traffic_type_percent = new int[3];

	for (int i = 0; i < 3; i++) {
		g_burst_traffic_adv_dist[i] = 1;
		g_burst_traffic_type[i] = 1;
		g_burst_traffic_type_percent[i] = 0;
	}

	//read parameters
	assert(config.getKeyValue("CONFIG", "maxCycles", value) == 0);
	g_maxCycles = atoi(value.c_str());
	g_warmCycles = g_maxCycles;

	assert(config.getKeyValue("CONFIG", "ageArbiter", value) == 0);
	g_ageArbiter = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "arbiter_iterations", value) == 0);
	g_arbiter_iterations = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "queueGen", value) == 0);
	g_queueGen = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "dP", value) == 0);
	g_dP = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "dA", value) == 0);
	g_dA = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "dH", value) == 0);
	g_dH = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "delayTransmission_local", value) == 0);
	g_delayTransmission_local = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "delayTransmission_global", value) == 0);
	g_delayTransmission_global = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "delayInjection", value) == 0);
	g_delayInjection = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "packetSize", value) == 0);
	g_packetSize = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "flitSize", value) == 0);
	g_flitSize = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "valiant", value) == 0);
	g_valiant = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "valiant_long", value) == 0);
	g_valiant_long = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "dally", value) == 0);
	g_dally = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "strict_misroute", value) == 0);
	g_strictMisroute = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "ugal", value) == 0);
	g_ugal = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "rings", value) == 0);
	g_rings = atoi(value.c_str());
	assert(config.getKeyValue("CONFIG", "ringDirs", value) == 0);
	g_ringDirs = atoi(value.c_str());
	if (g_rings > 1) {
		assert(config.getKeyValue("CONFIG", "onlyRing2", value) == 0);
		g_onlyRing2 = atoi(value.c_str());
	}

	assert(config.getKeyValue("CONFIG", "trees", value) == 0);
	g_trees = atoi(value.c_str());

	if (g_trees > 0 || g_rings > 0) {
		assert(config.getKeyValue("CONFIG", "baseCongestionControl", value) == 0);
		g_baseCongestionControl = atoi(value.c_str());

		assert(config.getKeyValue("CONFIG", "baseCongestionControl_bub", value) == 0);
		g_baseCongestionControl_bub = atoi(value.c_str());

		assert(config.getKeyValue("CONFIG", "escapeCongestionControl", value) == 0);
		g_escapeCongestionControl = atoi(value.c_str());

		assert(config.getKeyValue("CONFIG", "escapeCongestion_th", value) == 0);
		g_escapeCongestion_th = atoi(value.c_str());
	}

	assert(config.getKeyValue("CONFIG", "restrictLocalCycles", value) == 0);
	g_restrictLocalCycles = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "traffic", value) == 0);
	g_traffic = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "channels_useful", value) == 0);
	g_channels_useful = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "channels_inj", value) == 0);
	g_channels_inj = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "channels_glob", value) == 0);
	g_channels_glob = atoi(value.c_str());

	if (config.getKeyValue("CONFIG", "channels_ring", value) == 0) {
		g_channels_ring = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "channels_tree", value) == 0) {
		g_channels_tree = atoi(value.c_str());
	}

	assert(config.getKeyValue("CONFIG", "palm_tree_configuration", value) == 0);
	g_palm_tree_configuration = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "forceMisrouting", value) == 0);
	g_forceMisrouting = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "piggybacking", value) == 0);
	g_piggyback = atoi(value.c_str());

	if (config.getKeyValue("CONFIG", "piggybacking_coef", value) == 0) {
		g_piggyback_coef = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "embeddedRing", value) == 0) {
		g_embeddedRing = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "embeddedTree", value) == 0) {
		g_embeddedTree = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "restrictRingInjection", value) == 0) {
		g_restrict_ring_injection = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "try_just_escape", value) == 0) {
		g_try_just_escape = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "forbid_from_injQueues_to_ring", value) == 0) {
		g_forbid_from_injQueues_to_ring = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "mixTraffic", value) == 0) {
		g_mix_traffic = atoi(value.c_str());
		assert(config.getKeyValue("CONFIG", "trafficAdvDistLocal", value) == 0);
		g_traffic_adv_dist_local = atoi(value.c_str());
		assert(config.getKeyValue("CONFIG", "randomTrafficPercent", value) == 0);
		g_random_percent = atoi(value.c_str());
		assert(config.getKeyValue("CONFIG", "localAdvTrafficPercent", value) == 0);
		g_adv_local_percent = atoi(value.c_str());
		assert(config.getKeyValue("CONFIG", "globalAdvTrafficPercent", value) == 0);
		g_adv_global_percent = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "trafficAdvDist", value) == 0) {
		g_traffic_adv_dist = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "transientTraffic", value) == 0) {
		g_transient_traffic = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "transientTrafficNextType", value) == 0) {
		g_transient_traffic_next_type = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "transientTrafficNextDist", value) == 0) {
		g_transient_traffic_next_dist = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "transientTrafficCycle", value) == 0) {
		g_transient_traffic_cycle = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "transientRecordCycles", value) == 0) {
		g_transient_record_num_cycles = atoi(value.c_str());
		assert(g_transient_record_num_cycles > 0);
	}

	if (config.getKeyValue("CONFIG", "transientRecordPrevCycles", value) == 0) {
		g_transient_record_num_prev_cycles = atoi(value.c_str());
		assert(g_transient_record_num_prev_cycles > 0);
	}

	if (config.getKeyValue("CONFIG", "all-to-all", value) == 0) {
		g_AllToAll = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "phases", value) == 0) {
		g_phases = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "naive_all-to-all", value) == 0) {
		g_naive_AllToAll = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "random_all-to-all", value) == 0) {
		g_random_AllToAll = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burst", value) == 0) {
		g_burst = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstLength", value) == 0) {
		g_burst_length = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficType1", value) == 0) {
		g_burst_traffic_type[0] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficAdvDist1", value) == 0) {
		g_burst_traffic_adv_dist[0] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficTypePercent1", value) == 0) {
		g_burst_traffic_type_percent[0] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficType2", value) == 0) {
		g_burst_traffic_type[1] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficAdvDist2", value) == 0) {
		g_burst_traffic_adv_dist[1] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficTypePercent2", value) == 0) {
		g_burst_traffic_type_percent[1] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficType3", value) == 0) {
		g_burst_traffic_type[2] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficAdvDist3", value) == 0) {
		g_burst_traffic_adv_dist[2] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "burstTrafficTypePercent3", value) == 0) {
		g_burst_traffic_type_percent[2] = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "variableThreshold", value) == 0) {
		g_variable_threshold = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "percentLocalThreshold", value) == 0) {
		g_percent_local_threshold = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "percentGlobalThreshold", value) == 0) {
		g_percent_global_threshold = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_crg", value) == 0) {
		g_vc_misrouting_crg = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_mm", value) == 0) {
		g_vc_misrouting_mm = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_local", value) == 0) {
		g_vc_misrouting_local = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "OFAR_misrouting_crg", value) == 0) {
		g_OFAR_misrouting_crg = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "OFAR_misrouting_mm", value) == 0) {
		g_OFAR_misrouting_mm = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "OFAR_misrouting_local", value) == 0) {
		g_OFAR_misrouting_local = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_increasingVC", value) == 0) {
		g_vc_misrouting_increasingVC = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "Th_min", value) == 0) {
		g_th_min = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_congested_restriction", value) == 0) {
		g_vc_misrouting_congested_restriction = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_congested_restriction_coef_percent", value) == 0) {
		g_vc_misrouting_congested_restriction_coef_percent = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "vc_misrouting_congested_restriction_t", value) == 0) {
		g_vc_misrouting_congested_restriction_t = atoi(value.c_str());
	}

	/*
	 * Parameters TRACES
	 */

	if (config.getKeyValue("CONFIG", "trace_nodes", value) == 0) {
		g_trace_nodes = atol(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "cpu_speed", value) == 0) {
		g_cpuSpeed = atof(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "op_per_cycle", value) == 0) {
		g_op_per_cycle = atol(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "cutmpi", value) == 0) {
		g_cutmpi = atol(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "TRACE_SUPPORT", value) == 0) {
		g_TRACE_SUPPORT = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "phit_size", value) == 0) {
		g_phit_size = atol(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "trcFile", value) == 0) {
		strcpy(g_trcFile, value.c_str());
	}

	if (config.getKeyValue("CONFIG", "pcfFile", value) == 0) {
		strcpy(g_pcfFile, value.c_str());
	}

	/*
	 * --- ADITIONAL CALCULATIONS ---
	 */

	//ring dedicated physical ports
	if ((g_rings > 0) && (g_embeddedRing == 0)) {
		g_ring_ports = 2 * g_rings;  // If these are disjoint rings
	} else {
		g_ring_ports = 0;
	}

	//virtual channels
	if (g_channels_inj > g_channels_useful) {
		g_channels = g_channels_inj;
	} else {
		g_channels = g_channels_useful;
	}

	if ((g_embeddedRing != 0) && (g_channels_inj < g_channels_useful + g_channels_ring)) {
		g_channels = g_channels_useful + g_channels_ring;
	}

	if ((g_embeddedTree != 0) && (g_channels_inj < g_channels_useful + g_channels_tree)) {
		g_channels = g_channels_useful + g_channels_tree;
	}

	//transient traffic
	g_transient_record_len = g_transient_record_num_cycles + g_transient_record_num_prev_cycles;

	//Burst
	if ((g_burst)) {
		g_probability = 100;
		g_warmCycles = 0;
		g_maxCycles = 1000000;
	}

	//vc misrouting
	if (g_vc_misrouting_crg || g_vc_misrouting_mm) {
		g_vc_misrouting = true;
		assert(g_dally == true);
	}

	if (g_OFAR_misrouting_crg || g_OFAR_misrouting_mm) {
		g_OFAR_misrouting = true;
		assert(g_dally == false);
	}

	g_flits_per_packet = g_packetSize / g_flitSize;
	if (g_burst) g_burst_flits_length = g_burst_length * g_flits_per_packet;
	if (g_AllToAll) g_burst_flits_length = (g_generatorsCount - 1) * g_phases * g_flits_per_packet;

	/*
	 *  --- CHECK PARAMETER COHERENCE ---
	 */
	assert(g_packetSize > 0);
	assert(g_flitSize > 0);
	assert(g_flits_per_packet % 1 == 0);

	if ((g_rings > 0) || (g_trees > 0)) {
		assert(g_dally == 0);
		assert((g_escapeCongestion_th >= 0) && (g_escapeCongestion_th <= 100));
	}
	if (g_dally == 1) {
		assert((g_rings == 0) && (g_trees == 0));
	}
	assert(!(g_rings == 0 && g_trees == 0 && g_dally == 0));
	if (g_valiant == 1) {
		assert(g_dally == 1);
		assert(g_ugal == 0);
	}
	if (g_ugal == 1) {
		assert(g_dally == 1);
		assert(g_valiant == 0);
	}
	assert(!(g_piggyback && (g_rings || g_trees)));
	// to avoid misunderstatements, piggybacking and ugal (ugal-L)
	//  can not be selected at the same time, although piggybacking
	//	implies using ugal
	assert(!(g_piggyback && g_ugal));
	if (g_piggyback) {
		g_ugal = 1;
		assert(g_piggyback_coef > 0);
	}

	//embedded ring
	if (g_embeddedRing == 0) {
		assert(g_channels_ring == 0);
	}
	if (g_embeddedTree == 0) {
		assert(g_channels_tree == 0);
	}

	//If 1 or more rings, the number of ringDirs must be 1 or 2
	if (g_ringDirs > 0) assert((g_ringDirs == 1) || (g_ringDirs == 2));

	if (g_palm_tree_configuration == 0) {
		assert((g_rings == 0) && (g_trees == 0));
		cout
				<< "Embedded Ring and Tree only work with the global network palm tree configuration, they would NOT work with the \"traditional\" configuration."
				<< endl;
	}

	if (g_embeddedRing == 1) {
		assert(g_palm_tree_configuration == 1);
		assert(g_channels_ring > 0);
		assert(g_embeddedTree == 0);
		assert(g_channels_tree == 0);
	}
	if (g_embeddedTree == 1) {
		assert(g_palm_tree_configuration == 1);
		assert(g_channels_tree > 0);
		assert(g_embeddedRing == 0);
		assert(g_channels_ring == 0);
	}

	// burst traffic
	if (g_burst) {
		assert(
				g_burst_traffic_type_percent[0] + g_burst_traffic_type_percent[1] + g_burst_traffic_type_percent[2]
						== 100);
	}
	if (g_mix_traffic) {
		assert(g_random_percent + g_adv_local_percent + g_adv_global_percent == 100);
	}
	if (g_AllToAll) {
		assert(g_phases > 0);
	}
	assert(!(g_burst && g_transient_traffic));
	assert(!(g_burst && g_AllToAll));
	assert(!(g_AllToAll && g_transient_traffic));

	//SM variable threshold
	assert(g_percent_global_threshold >= 0);
	assert(g_percent_local_threshold >= 0);

	//VC misrouting
	assert(not (g_vc_misrouting_crg && g_vc_misrouting_mm));
	if (g_vc_misrouting_local) {
		assert(g_vc_misrouting);
	}
	//OFAR misrouting
	assert(not (g_OFAR_misrouting_crg && g_OFAR_misrouting_mm));
	if (g_OFAR_misrouting_local) {
		assert(g_OFAR_misrouting);
	}

	assert(g_th_min >= 0);
	assert(g_th_min <= 100);

	// VC_misrouting_congested_restriction
	if (g_vc_misrouting_congested_restriction) {
		assert(g_vc_misrouting || g_OFAR_misrouting);
		assert(g_vc_misrouting_congested_restriction_coef_percent >= 0);
	}
}

void create_network() {
	int i, j, k;
	string swtch = "switcha_b";
	string gnrtr = "generatora_b_c";

	g_ports = g_dP + g_dA - 1 + g_dH; // We need to add 2 more ports to make physical ring

	g_globalLinksPerGroup = g_dA * g_dH;

	g_switchesCount = (g_dA * g_dH + 1) * g_dA;
	g_generatorsCount = g_switchesCount * g_dP;

	g_treeRoot_node = rand() % g_switchesCount; // We used this node to route packets to the root switch
	cout << "treeRoot_node=" << g_treeRoot_node << endl;
	cout << "treeRoot_switch=" << int(g_treeRoot_node / g_dP) << endl;
	g_rootH = int(g_treeRoot_node / (g_dA * g_dP));

	g_generatorsList = new generatorModule*[g_generatorsCount];
	g_switchesList = new switchModule*[g_switchesCount];

	if (g_rings != 0) { //If physical Ring
		if (g_embeddedRing == 0) g_ports = g_ports + 2;
		g_globalEmbeddedRingSwitchesCount = 2 * (g_dA * g_dH + 1); //2 switches per group with global links of the embedded ring
		g_localEmbeddedRingSwitchesCount = g_switchesCount; //every switch in the network has local links of the ring
	}

	g_latency_histogram_no_global_misroute = new long long[g_latency_histogram_maxLat];
	g_latency_histogram_global_misroute_at_injection = new long long[g_latency_histogram_maxLat];
	g_latency_histogram_other_global_misroute = new long long[g_latency_histogram_maxLat];

	g_hops_histogram = new long long[g_hops_histogram_maxHops];

	for (i = 0; i < g_latency_histogram_maxLat; i++) {
		g_latency_histogram_no_global_misroute[i] = 0;
		g_latency_histogram_global_misroute_at_injection[i] = 0;
		g_latency_histogram_other_global_misroute[i] = 0;
	}
	for (i = 0; i < g_hops_histogram_maxHops; i++) {
		g_hops_histogram[i] = 0;
	}

	g_offsetA = g_dP;
	g_offsetH = g_offsetA + g_dA - 1;

	g_group0_numFlits = new long long[g_dA];
	g_group0_totalLatency = new long long[g_dA];
	for (i = 0; i < g_dA; i++) {
		g_group0_numFlits[i] = 0;
		g_group0_totalLatency[i] = 0;
	}

	g_globalEmbeddedTreeSwitchesCount = 0;
	g_globalEmbeddedTreeSwitchesCount = 0;
	g_groupRoot_numFlits = new long long[g_dA];
	g_groupRoot_totalLatency = new long long[g_dA];
	for (i = 0; i < g_dA; i++) {
		g_groupRoot_numFlits[i] = 0;
		g_groupRoot_totalLatency[i] = 0;
	}

	for (i = 0; i < g_ports; i++) {
		g_portUseCount[i] = 0;
		g_portContentionCount[i] = 0;
		g_portAlreadyAssignedCount[i] = 0;

	}

	g_outputFile << endl << endl;

	cout << "switchModule size: " << sizeof(switchModule);
	cout << "generatorModule size: " << sizeof(generatorModule) << endl;

	for (i = 0; i < ((g_dH * g_dA) + 1); i++) {
		for (j = 0; j < (g_dA); j++) {
			try {
				g_switchesList[j + i * g_dA] = new switchModule("switch", (j + i * g_dA), j, i, g_ports, (g_channels));
			} catch (exception& e) {
				g_outputFile << "Error when generating switchModule, i= " << i << ", j=" << j;
				cout << "Error when generating switchModule, i= " << i << ", j=" << j;
				exit(0);
			}
			for (k = 0; k < (g_dP); k++) {
				try {
					g_generatorsList[(k + j * g_dP + i * (g_dP * g_dA))] = new generatorModule(1, "generator",
							(k + j * g_dP + i * (g_dP * g_dA)), k, j, i, g_switchesList[(j + i * g_dA)]);
				} catch (exception& e) {
					g_outputFile << "Error when generating generatorModule, i= " << i << ", j=" << j << ", k=" << k;
					cout << "Error when generating generatorModule, i= " << i << ", j=" << j << ", k=" << k;
					exit(0);
				}
			}
		}
	}

	for (i = 0; i < g_switchesCount; i++) {
		g_switchesList[i]->findNeighbors(g_switchesList);
	}

	for (i = 0; i < g_switchesCount; i++) {
		g_switchesList[i]->visitNeighbours();
	}

	for (i = 0; i < g_switchesCount; i++) {
		g_switchesList[i]->resetCredits();
	}

	//transient Traffic variables
	//initialize records
	if (g_transient_traffic) {
		g_transient_record_latency = new int[g_transient_record_len];
		g_transient_record_num_flits = new int[g_transient_record_len];
		g_transient_record_num_misrouted = new int[g_transient_record_len];
		for (i = 0; i < g_transient_record_len; i++) {
			g_transient_record_latency[i] = 0;
			g_transient_record_num_flits[i] = 0;
			g_transient_record_num_misrouted[i] = 0;
		}
	}
}

void run_cycles() {
	int i, c, totalSwitchSpace, totalSwitchFreeSpace, flitWaitingCount;

	totalSwitchSpace = 0;
	totalSwitchFreeSpace = 0;
	flitWaitingCount = 0;
	g_warmTotalLatency = 0;
	g_warmTotalPacketLatency = 0;
	g_warmTotalInjLatency = 0;

	if (g_TRACE_SUPPORT == 0) {
		/* cycle loop: WARMUP */
		for (g_cycle = 0; g_cycle < g_warmCycles; g_cycle++) {
			c = g_cycle % 100;
			for (i = 0; i < g_switchesCount; i++) {
				if (g_escapeCongestionControl) g_switchesList[i]->escapeCongested();
				assert(g_switchesList[i]->messagesInQueuesCounter >= 0);
				if (g_switchesList[i]->messagesInQueuesCounter >= 1) {
					g_switchesList[i]->action();
				}
			}
			for (i = 0; i < g_generatorsCount; i++) {
				g_generatorsList[i]->action();
			}
			if (c == 0) {
				g_sent.push_back(g_flitCount);
				g_received.push_back(g_flitReceivedCount);
				cout << "cycle:" << g_cycle << "   Messages sent:" << g_flitCount << "   Messages received:"
						<< g_flitReceivedCount << endl;
			}
		}
	}

	g_totalHopCount = 0;
	g_totalLocalHopCount = 0;
	g_totalGlobalHopCount = 0;
	g_totalLocalRingHopCount = 0;
	g_totalGlobalRingHopCount = 0;
	g_totalLocalTreeHopCount = 0;
	g_totalGlobalTreeHopCount = 0;
	g_totalLocalContentionCount = 0;
	g_totalGlobalContentionCount = 0;
	g_totalLocalRingContentionCount = 0;
	g_totalGlobalRingContentionCount = 0;
	g_totalLocalTreeContentionCount = 0;
	g_totalGlobalTreeContentionCount = 0;
	for (i = 0; i < g_ports; i++) {
		g_portUseCount[i] = 0;
		g_portContentionCount[i] = 0;
		g_portAlreadyAssignedCount[i] = 0;

	}
	for (i = 0; i < g_switchesCount; i++) {
		totalSwitchSpace = g_switchesList[i]->getTotalCapacity();
		totalSwitchFreeSpace = g_switchesList[i]->getTotalFreeSpace();
		flitWaitingCount = flitWaitingCount + (totalSwitchSpace - totalSwitchFreeSpace);

		g_switchesList[i]->resetQueueOccupancy();
		g_warmFlitCount = g_flitCount;
		g_warmFlitReceivedCount = g_flitReceivedCount;
		g_warmPacketCount = g_packetCount;
		g_warmPacketReceivedCount = g_packetReceivedCount;
		g_warmTotalLatency = g_totalLatency;
		g_warmTotalPacketLatency = g_totalPacketLatency;
		g_warmTotalInjLatency = g_totalInjectionQueueLatency;
	}
	g_petitions = 0;
	g_petitions_served = 0;
	g_petitionsInj = 0;
	g_petitionsInj_served = 0;

	if (g_TRACE_SUPPORT == 0) {
		for (; g_cycle < (g_maxCycles + g_warmCycles); g_cycle++) {
			c = g_cycle % 100;
			for (i = 0; i < g_switchesCount; i++) {
				if (g_escapeCongestionControl) g_switchesList[i]->escapeCongested();
				assert(g_switchesList[i]->messagesInQueuesCounter >= 0);
				if (g_switchesList[i]->messagesInQueuesCounter >= 1) {
					g_switchesList[i]->action();
				}
			}
			for (i = 0; i < g_generatorsCount; i++) {
				if (!g_generatorsList[i]->switchM->escapeNetworkCongested) g_generatorsList[i]->action();
			}
			if (c == 0) {
				g_sent.push_back(g_flitCount);
				g_received.push_back(g_flitReceivedCount);
				cout << "cycle:" << g_cycle << "   Messages sent:" << g_flitCount << "   Messages received:"
						<< g_flitReceivedCount << endl;
			}

			//BURST ends when al messages have been received
			if (g_burst && (g_burst_generators_finished_count >= g_generatorsCount)) {
				break;
			}

			//ALL-TO-ALL ends when al messages have been received
			if (g_AllToAll && (g_AllToAll_generators_finished_count >= g_generatorsCount)) {
				break;
			}
		}
	} else { //  TRACE TRAFFIC
		g_warmCycles = 0;
		g_eventDeadlock = 0;
		bool go_on = true;
		int gen;
		long l;
		event e;
		for (g_cycle = 0; true; g_cycle++) {
			bool normal = true;
			if (g_flitCount == g_flitReceivedCount) {
				long min_length = LONG_MAX;
				for (gen = 0; gen < g_generatorsCount; gen++) {
					if (!event_empty(&g_generatorsList[gen]->events)
							&& head_event(&g_generatorsList[gen]->events).type != RECEPTION) {
						e = head_event(&g_generatorsList[gen]->events);
						if (e.type != COMPUTATION) {
							min_length = 0;
							break;
						} else {
							l = (e.length - e.count);
							if (l < min_length) min_length = l;
						}
					}
				}
				if (min_length > 0 && min_length != LONG_MAX) {
					normal = false;
					cout << "cycle:" << g_cycle << " skipping " << min_length << " cycles" << endl;
					g_cycle += min_length;
					for (gen = 0; gen < g_generatorsCount; gen++) {
						if (!event_empty(&g_generatorsList[gen]->events)
								&& head_event(&g_generatorsList[gen]->events).type != RECEPTION) {
							e = head_event(&g_generatorsList[gen]->events);
							assert(e.type == COMPUTATION);
							do_multiple_events(&g_generatorsList[gen]->events, &e, min_length);
						}
					}
					g_eventDeadlock = 0;
				}
			}

			if (normal) {
				c = g_cycle % 100;
				if (g_eventDeadlock > 2000) {
					cout << "EVENT DEADLOCK detected at cycle " << g_cycle << endl;
					for (i = 0; i < g_generatorsCount; i++) {
						if (event_empty(&g_generatorsList[i]->events)) {
							cout << "[" << i << "] empty\n" << endl;
						} else {
							e = head_event(&g_generatorsList[i]->events);
							cout << "[" << i << "] " << (char) e.type << " (" << e.pid << ") flag=" << e.task
									<< " length=" << e.length << " count=" << e.count << " mpitype=" << e.mpitype
									<< endl;
						}

					}
					cout << "EVENT DEADLOCK!!" << endl;
					assert(0);
				}

				if (go_on == false) {
					break;
				}
				g_eventDeadlock++;
				for (i = 0; i < g_switchesCount; i++) {
					assert(g_switchesList[i]->messagesInQueuesCounter >= 0);
					if (g_switchesList[i]->messagesInQueuesCounter >= 1) {
						g_switchesList[i]->action();
					} else {
						if (g_piggyback) {
							// Read PiggyBacking messages. Reade only those
							//	msgs that have already arrived
							g_switchesList[i]->updateReadPb();
						}
					}
				}
				for (i = 0; i < g_generatorsCount; i++) {
					g_generatorsList[i]->action();
				}
				go_on = false;
				for (i = 0; i < g_generatorsCount && !go_on; i++) {
					{
						if (!event_empty(&g_generatorsList[i]->events)) {
							go_on = true;
							break;
						}
					}
				}
				if (c == 0) {
					g_sent.push_back(g_flitCount);
					g_received.push_back(g_flitReceivedCount);
					cout << "cycle:" << g_cycle << "   Messages sent:" << g_flitCount << "   Messages received:"
							<< g_flitReceivedCount << endl;
				}
			}
		}
	}

	flitWaitingCount = 0;
	/* stats */
	cout << "Start statistics" << endl;
	for (i = 0; i < g_switchesCount; i++) {
		totalSwitchSpace = g_switchesList[i]->getTotalCapacity();
		totalSwitchFreeSpace = g_switchesList[i]->getTotalFreeSpace();
		flitWaitingCount = flitWaitingCount + (totalSwitchSpace - totalSwitchFreeSpace);
	}
}

void write_output() {
	float IQO[g_channels], GRQO[g_channels], LRQO[g_channels], GTQO[g_channels], LTQO[g_channels], GQO[g_channels],
			LQO[g_channels];
	int i;
	for (int vc = 0; vc < g_channels; vc++) {
		LQO[vc] = 0;
		GQO[vc] = 0;
		IQO[vc] = 0;
		LRQO[vc] = 0;
		GRQO[vc] = 0;
		LTQO[vc] = 0;
		GTQO[vc] = 0;
	}
	g_outputFile << "TotalNumberOfCycles: " << g_cycle << endl;
	g_outputFile << "maxCycles: " << g_maxCycles << endl;
	g_outputFile << "g_ageArbiter: " << g_ageArbiter << endl;
	g_outputFile << "arbiter_iterations: " << g_arbiter_iterations << endl;
	g_outputFile << "queueGen: " << g_queueGen << endl;
	g_outputFile << "dH: " << g_dH << endl;
	g_outputFile << "dP: " << g_dP << endl;
	g_outputFile << "dA: " << g_dA << endl;
	g_outputFile << "delayTransmission_local: " << g_delayTransmission_local << endl;
	g_outputFile << "delayTransmission_global: " << g_delayTransmission_global << endl;
	g_outputFile << "delayInjectionl: " << g_delayInjection << endl;
	g_outputFile << "packetSize(in phits): " << g_packetSize << endl;
	g_outputFile << "flitSize(in phits): " << g_flitSize << endl;
	g_outputFile << "flits per packet: " << g_flits_per_packet << endl;
	g_outputFile << "queueSwtch_local: " << g_queueSwtch_local << endl;
	g_outputFile << "queueSwtch_global: " << g_queueSwtch_global << endl;
	g_outputFile << "BUBBLE: " << g_BUBBLE << endl;
	g_outputFile << "channels: " << g_channels << endl;
	g_outputFile << "channels_useful: " << g_channels_useful << endl;
	g_outputFile << "channels_glob: " << g_channels_glob << endl;
	g_outputFile << "rings: " << g_rings << endl;
	g_outputFile << "ringDirs: " << g_ringDirs << endl;
	g_outputFile << "onlyRing2: " << g_onlyRing2 << endl;
	g_outputFile << "trees: " << g_trees << endl;
	g_outputFile << "dally: " << g_dally << endl;
	g_outputFile << "strictMisroute: " << g_strictMisroute << endl;
	g_outputFile << "ugal: " << g_ugal << endl;
	g_outputFile << "piggybacking: " << g_piggyback << endl;
	g_outputFile << "piggyback_coef: " << g_piggyback << endl;
	g_outputFile << "traffic: " << g_traffic << endl;
	g_outputFile << "restrictLocalCycles: " << g_restrictLocalCycles << endl;
	g_outputFile << "probability: " << g_probability << endl;
	g_outputFile << "mi_seed: " << g_mi_seed << endl << endl << endl;
	g_outputFile << "palm_tree_configuration: " << g_palm_tree_configuration << endl;
	g_outputFile << "forcemisrouting: " << g_forceMisrouting << endl;
	g_outputFile << "ugal_local_threshold: " << g_ugal_local_threshold << endl;
	g_outputFile << "ugal_global_threshold: " << g_ugal_global_threshold << endl;
	g_outputFile << "restrict_ring_injection: " << g_restrict_ring_injection << endl;
	g_outputFile << "forbid_from_injQueues_to_ring: " << g_forbid_from_injQueues_to_ring << endl;
	// VC_MISROUTING
	g_outputFile << "OFAR_misrouting: " << g_OFAR_misrouting << endl;
	g_outputFile << "OFAR_misrouting_crg: " << g_OFAR_misrouting_crg << endl;
	g_outputFile << "OFAR_misrouting_mm: " << g_OFAR_misrouting_mm << endl;
	g_outputFile << "OFAR_misrouting_local: " << g_OFAR_misrouting_local << endl;
	// VC_MISROUTING
	g_outputFile << "vc_misrouting: " << g_vc_misrouting << endl;
	g_outputFile << "vc_misrouting_crg: " << g_vc_misrouting_crg << endl;
	g_outputFile << "vc_misrouting_mm: " << g_vc_misrouting_mm << endl;
	g_outputFile << "vc_misrouting_local: " << g_vc_misrouting_local << endl;
	g_outputFile << "vc_misrouting_congested_restriction: " << g_vc_misrouting_congested_restriction << endl;
	g_outputFile << "vc_misrouting_congested_restriction_coef_percent: "
			<< g_vc_misrouting_congested_restriction_coef_percent << endl;
	g_outputFile << "vc_misrouting_congested_restriction_t: " << g_vc_misrouting_congested_restriction_t << endl;

	// ADVERSARIAL TRAFFIC
	g_outputFile << "trafficAdvDist: " << g_traffic_adv_dist << endl;
	//TRANSIENT TRAFFIC:
	g_outputFile << "transientTraffic: " << g_transient_traffic << endl;
	g_outputFile << "transientTrafficNextType: " << g_transient_traffic_next_type << endl;
	g_outputFile << "transientTrafficNextDist: " << g_transient_traffic_next_dist << endl;
	g_outputFile << "transientTrafficCycle: " << g_transient_traffic_cycle << endl;
	g_outputFile << "transientRecordCycles: " << g_transient_record_num_cycles << endl;
	g_outputFile << "transientRecordPrevCycles: " << g_transient_record_num_prev_cycles << endl;

	//EMBEDDED RING
	g_outputFile << "channels_ring: " << g_channels_ring << endl;
	g_outputFile << "embeddedRing: " << g_embeddedRing << endl;
	//EMBEDDED TREE
	g_outputFile << "channels_tree: " << g_channels_tree << endl;
	g_outputFile << "embeddedTree: " << g_embeddedTree << endl;

	//CONGESTION_CONTROL
	g_outputFile << "baseCongestionControl: " << g_baseCongestionControl << endl;
	g_outputFile << "baseCongestionControl_bub: " << g_baseCongestionControl_bub << endl;
	g_outputFile << "escapeCongestionControl: " << g_escapeCongestionControl << endl;
	g_outputFile << "escapeCongestion_th: " << g_escapeCongestion_th << endl;

	//FJM VARIABLE THRESHOLD
	g_outputFile << "variable_threshold: " << g_variable_threshold << endl;
	g_outputFile << "percent_local_threshold: " << g_percent_local_threshold << endl;
	g_outputFile << "percent_global_threshold: " << g_percent_global_threshold << endl;
	if (g_burst) {
		//BURST
		g_outputFile << "burst: " << g_burst << endl;
		g_outputFile << "burstLength (packets): " << g_burst_length << endl;
		g_outputFile << "burstLength (flits): " << g_burst_flits_length << endl;
		g_outputFile << "burstTrafficType1: " << g_burst_traffic_type[0] << endl;
		g_outputFile << "burstTrafficAdvDist1: " << g_burst_traffic_adv_dist[0] << endl;
		g_outputFile << "burstTypePercent1: " << g_burst_traffic_type_percent[0] << endl;
		g_outputFile << "burstTrafficType2: " << g_burst_traffic_type[1] << endl;
		g_outputFile << "burstTrafficAdvDist2: " << g_burst_traffic_adv_dist[1] << endl;
		g_outputFile << "burstTypePercent2: " << g_burst_traffic_type_percent[1] << endl;
		g_outputFile << "burstTrafficType3: " << g_burst_traffic_type[2] << endl;
		g_outputFile << "burstTrafficAdvDist3: " << g_burst_traffic_adv_dist[2] << endl;
		g_outputFile << "burstTypePercent3: " << g_burst_traffic_type_percent[2] << endl;
		g_outputFile << "burstEndCycle: " << g_cycle << endl;
	}

	if (g_AllToAll) {
		//BURST
		g_outputFile << "all-to-all: " << g_AllToAll << endl;
		g_outputFile << "naive_all-to-all: " << g_naive_AllToAll << endl;
		g_outputFile << "random_all-to-all: " << g_random_AllToAll << endl;
		g_outputFile << "all-to-allLength (packets): " << (g_generatorsCount - 1) * g_phases << endl;
		g_outputFile << "all-to-allLength (flits): " << (g_generatorsCount - 1) * g_phases * g_flits_per_packet << endl;
		g_outputFile << "all-to-allPhases: " << g_phases << endl;
		g_outputFile << "all-to-allEndCycle: " << g_cycle << endl;
	}

	//Latency histogram
	g_outputFile << "latencyHistogramMaxLat: " << g_latency_histogram_maxLat << endl;

	//Hops histogram
	g_outputFile << "hopsHistogramMaxHops: " << g_hops_histogram_maxHops << endl << endl << endl << endl;

	g_outputFile << "Flits sent: " << (g_flitCount - g_warmFlitCount) << " (warmup: " << g_warmFlitCount << ")" << endl;
	g_outputFile << "Flits received: " << (g_flitReceivedCount - g_warmFlitReceivedCount) << " (warmup: "
			<< g_warmFlitReceivedCount << ")" << endl;
	long receivedFlitCount = g_flitReceivedCount - g_warmFlitReceivedCount;
	long receivedPacketCount = g_packetReceivedCount - g_warmPacketReceivedCount;
	g_outputFile << "Packets received: " << receivedPacketCount << endl;
	g_outputFile << "Flits misrouted: " << g_misroutedFlitCount << endl;
	g_outputFile << "Flits misrouted percent: " << g_misroutedFlitCount * 100.0 / receivedFlitCount << endl;
	g_outputFile << "Flits local misrouted: " << g_local_misroutedFlitCount << endl;
	g_outputFile << "Flits local misrouted percent: " << g_local_misroutedFlitCount * 100.0 / receivedFlitCount << endl;
	g_outputFile << "Flits global misrouted: " << g_global_misroutedFlitCount << endl;
	g_outputFile << "Flits global misrouted percent: " << g_global_misroutedFlitCount * 100.0 / receivedFlitCount
			<< endl;
	g_outputFile << "Flits sent: " << (g_flitCount - g_warmFlitCount) * g_flitSize << " (warmup: "
			<< g_warmFlitCount * g_flitSize << ")" << endl;
	g_outputFile << "Applied load: "
			<< (float) (1.0 * g_flitCount - g_warmFlitCount) * g_flitSize
					/ (1.0 * g_generatorsCount * (g_cycle - g_warmCycles)) << " phits/(node·cycle)" << endl;
	g_outputFile << "Accepted load: "
			<< (float) 1.0 * receivedFlitCount * g_flitSize / (1.0 * g_generatorsCount * (g_cycle - g_warmCycles))
			<< " phits/(node·cycle)" << endl;
	g_outputFile << "Cycles: " << g_cycle << " (warmup: " << g_warmCycles << ")" << endl;
	g_outputFile << "Total hops: " << g_totalHopCount << endl;
	g_outputFile << "Total local hops: " << g_totalLocalHopCount << endl;
	g_outputFile << "Total global hops: " << g_totalGlobalHopCount << endl;
	g_outputFile << "Total local ring hops: " << g_totalLocalRingHopCount << endl;
	g_outputFile << "Total global ring hops: " << g_totalGlobalRingHopCount << endl;
	g_outputFile << "Total local tree hops: " << g_totalLocalTreeHopCount << endl;
	g_outputFile << "Total global tree hops: " << g_totalGlobalTreeHopCount << endl;
	g_outputFile << "Total subnetwork injections: " << g_totalSubnetworkInjectionsCount << endl;
	g_outputFile << "Total root subnetwork injections: " << g_totalRootSubnetworkInjectionsCount << endl;
	g_outputFile << "Total source subnetwork injections: " << g_totalSourceSubnetworkInjectionsCount << endl;
	g_outputFile << "Total dest subnetwork injections: " << g_totalDestSubnetworkInjectionsCount << endl;
	g_outputFile << "Total local contention: " << g_totalLocalContentionCount << endl;
	g_outputFile << "Total global contention: " << g_totalGlobalContentionCount << endl;
	g_outputFile << "Total local Ring contention: " << g_totalLocalRingContentionCount << endl;
	g_outputFile << "Total global Ring contention: " << g_totalGlobalRingContentionCount << endl;
	g_outputFile << "Total local Tree contention: " << g_totalLocalTreeContentionCount << endl;
	g_outputFile << "Total global Tree contention: " << g_totalGlobalTreeContentionCount << endl;
	g_outputFile << endl;
	g_outputFile << "Average distance: " << g_totalHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average local distance: " << g_totalLocalHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average global distance: " << g_totalGlobalHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average local ring distance: " << g_totalLocalRingHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average global ring distance: " << g_totalGlobalRingHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average local tree distance: " << g_totalLocalTreeHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average global tree distance: " << g_totalGlobalTreeHopCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average subnetwork injections: " << g_totalSubnetworkInjectionsCount * 1.0 / receivedFlitCount
			<< endl;
	g_outputFile << "Average root subnetwork injections: "
			<< g_totalRootSubnetworkInjectionsCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average source subnetwork injections: "
			<< g_totalSourceSubnetworkInjectionsCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average dest subnetwork injections: "
			<< g_totalDestSubnetworkInjectionsCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << endl;
	g_outputFile << "Average local contention: " << g_totalLocalContentionCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average global contention: " << g_totalGlobalContentionCount * 1.0 / receivedFlitCount << endl;
	g_outputFile << "Average local ring contention: " << g_totalLocalRingContentionCount * 1.0 / receivedFlitCount
			<< endl;
	g_outputFile << "Average global ring contention: " << g_totalGlobalRingContentionCount * 1.0 / receivedFlitCount
			<< endl;
	g_outputFile << "Average local tree contention: " << g_totalLocalTreeContentionCount * 1.0 / receivedFlitCount
			<< endl;
	g_outputFile << "Average global tree contention: " << g_totalGlobalTreeContentionCount * 1.0 / receivedFlitCount
			<< endl;
	g_outputFile << endl;
	g_outputFile << "Latency: " << (g_totalLatency - g_warmTotalLatency) << " (warmup: " << g_warmTotalLatency << ")"
			<< endl;
	g_outputFile << "Packet latency: " << (g_totalPacketLatency - g_warmTotalPacketLatency) << " (warmup: "
			<< g_warmTotalPacketLatency << ")" << endl;
	g_outputFile << "Inj. Latency: " << (g_totalInjectionQueueLatency - g_warmTotalInjLatency) << " (warmup: "
			<< g_warmTotalInjLatency << ")" << endl;
	g_outputFile << "Base Latency: " << g_totalBaseLatency << endl;
	g_outputFile << "Average total latency: " << (float) (g_totalLatency - g_warmTotalLatency) / receivedFlitCount
			<< endl;
	g_outputFile << "Average total packet latency: "
			<< (float) (g_totalPacketLatency - g_warmTotalPacketLatency) / receivedPacketCount << endl;
	g_outputFile << "Average inj. latency: "
			<< (float) (g_totalInjectionQueueLatency - g_warmTotalInjLatency) / receivedFlitCount << endl;
	g_outputFile << "Average latency: "
			<< (float) ((g_totalLatency - g_totalInjectionQueueLatency) - (g_warmTotalLatency - g_warmTotalInjLatency))
					/ receivedFlitCount << endl;
	g_outputFile << "Average base Latency: " << (float) (g_totalBaseLatency) / receivedFlitCount << endl;
	//Livelock control
	g_outputFile << "Max_hops: " << g_max_hops << endl;
	g_outputFile << "Max_local_hops: " << g_max_local_hops << endl;
	g_outputFile << "Max_global_hops: " << g_max_global_hops << endl;
	g_outputFile << "Max_local_ring_hops: " << g_max_local_ring_hops << endl;
	g_outputFile << "Max_global_ring_hops: " << g_max_global_ring_hops << endl;
	g_outputFile << "Max_local_tree_hops: " << g_max_local_tree_hops << endl;
	g_outputFile << "Max_global_tree_hops: " << g_max_global_tree_hops << endl;
	g_outputFile << "Max_subnetwork_injections: " << g_max_subnetwork_injections << endl;
	g_outputFile << "Max_root_subnetwork_injections: " << g_max_root_subnetwork_injections << endl;
	g_outputFile << "Max_source_subnetwork_injections: " << g_max_source_subnetwork_injections << endl;
	g_outputFile << "Max_dest_subnetwork_injections: " << g_max_dest_subnetwork_injections << endl;
	g_outputFile << endl;

	g_outputFile
			<< (float) (1.0 * g_flitReceivedCount - g_warmFlitReceivedCount) * g_flitSize
					/ (1.0 * g_generatorsCount * (g_cycle - g_warmCycles)) << endl;
	for (i = 0; i < g_ports; i++) {
		g_outputFile << "Port" << i << " count: " << g_portUseCount[i] << " ("
				<< (float) 100.0 * g_portUseCount[i] * g_flitSize / ((g_cycle - g_warmCycles) * g_switchesCount)
				<< "\%)" << endl;
		g_outputFile << "Port" << i << " Contention count: " << g_portContentionCount[i] << " ("
				<< (float) 100.0 * g_portContentionCount[i] / g_flitSize / ((g_cycle - g_warmCycles) * g_switchesCount)
				<< "\%)" << endl;
		g_outputFile << "Port" << i << " Already Assigned count: " << g_portAlreadyAssignedCount[i] << " ("
				<< (float) 100.0 * g_portAlreadyAssignedCount[i] / g_flitSize
						/ ((g_cycle - g_warmCycles) * g_switchesCount) << "\%)" << endl;
	}

	for (int i = 0; i < g_switchesCount; i++) {
		g_switchesList[i]->setQueueOccupancy();
		for (int vc = 0; vc < g_channels; vc++) {
			LQO[vc] = LQO[vc] + g_switchesList[i]->localQueueOccupancy[vc];
			GQO[vc] = GQO[vc] + g_switchesList[i]->globalQueueOccupancy[vc];
			IQO[vc] = IQO[vc] + g_switchesList[i]->injectionQueueOccupancy[vc];
			LRQO[vc] = LRQO[vc] + g_switchesList[i]->localRingQueueOccupancy[vc];
			GRQO[vc] = GRQO[vc] + g_switchesList[i]->globalRingQueueOccupancy[vc];
			LTQO[vc] = LTQO[vc] + g_switchesList[i]->localTreeQueueOccupancy[vc];
			GTQO[vc] = GTQO[vc] + g_switchesList[i]->globalTreeQueueOccupancy[vc];
		}
	}

	for (int vc = 0; vc < g_channels; vc++) {
		g_outputFile << "VC" << vc << ":" << endl;
		g_outputFile << "Injection queues occupancy: " << IQO[vc] / ((1.0) * (g_cycle - g_warmCycles) * g_switchesCount)
				<< " (" << ((float) 100.0 * IQO[vc] / ((g_cycle - g_warmCycles) * g_switchesCount)) / g_queueGen
				<< "\%)" << endl;
		g_outputFile << "Local queues occupancy: " << LQO[vc] / ((1.0) * (g_cycle - g_warmCycles) * g_switchesCount)
				<< " (" << ((float) 100.0 * LQO[vc] / ((g_cycle - g_warmCycles) * g_switchesCount)) / g_queueSwtch_local
				<< "\%)" << endl;
		g_outputFile << "Global queues occupancy: " << GQO[vc] / ((1.0) * (g_cycle - g_warmCycles) * g_switchesCount)
				<< " ("
				<< ((float) 100.0 * GQO[vc] / ((g_cycle - g_warmCycles) * g_switchesCount)) / g_queueSwtch_global
				<< "\%)" << endl;
		g_outputFile << "Local Ring queues occupancy: "
				<< LRQO[vc] / (((1.0) * g_cycle - g_warmCycles) * g_switchesCount) << " ("
				<< ((float) 100.0 * LRQO[vc] / ((g_cycle - g_warmCycles) * g_localEmbeddedRingSwitchesCount))
						/ g_queueSwtch_local << "\%)" << endl;
		g_outputFile << "Global Ring queues occupancy: "
				<< GRQO[vc] / ((1.0) * (g_cycle - g_warmCycles) * g_switchesCount) << " ("
				<< ((float) 100.0 * GRQO[vc] / ((g_cycle - g_warmCycles) * g_globalEmbeddedRingSwitchesCount))
						/ g_queueSwtch_global << "\%)" << endl;
		g_outputFile << "Local Tree queues occupancy: "
				<< LTQO[vc] / (((1.0) * g_cycle - g_warmCycles) * g_switchesCount) << " ("
				<< ((float) 100.0 * LTQO[vc] / ((g_cycle - g_warmCycles) * g_localEmbeddedTreeSwitchesCount))
						/ g_queueSwtch_local << "\%)" << endl;
		g_outputFile << "Global Tree queues occupancy: "
				<< GTQO[vc] / ((1.0) * (g_cycle - g_warmCycles) * g_switchesCount) << " ("
				<< ((float) 100.0 * GTQO[vc] / ((g_cycle - g_warmCycles) * g_globalEmbeddedTreeSwitchesCount))
						/ g_queueSwtch_global << "\%)" << endl;
	}

	g_outputFile << "Petitions done: " << g_petitions << endl;
	g_outputFile << "Petitions served: " << g_petitions_served << "(" << 100.0 * g_petitions_served / g_petitions
			<< "%)" << endl;
	g_outputFile << "Injection petitions done: " << g_petitionsInj << endl;
	g_outputFile << "Injection petitions served: " << g_petitionsInj_served << "("
			<< 100.0 * g_petitionsInj_served / g_petitionsInj << "%)" << endl << endl;
	g_outputFile << "Max Injections: " << g_maxPacketsInj << endl;
	g_outputFile << "Switch Max Injections: " << g_SWmaxPacketsInj << endl;
	g_outputFile << "Min Injections: " << g_minPacketsInj << endl;
	g_outputFile << "Switch Min Injections: " << g_SWminPacketsInj << endl;
	g_outputFile << "Unbalance coef.: " << (float) 1.0 * g_maxPacketsInj / g_minPacketsInj << endl;

	g_outputFile << endl << endl;

	// group 0, per switch averaged latency
	g_outputFile << "GROUP 0" << endl;
	for (int i = 0; i < g_dA; i++) {
		g_outputFile << "Group0_SW" << i << "_flits: " << g_group0_numFlits[i] << endl;
		if (g_group0_numFlits[i] > 0) {
			g_outputFile << "Group0_SW" << i << "_avgTotalLatency: "
					<< ((double) (g_group0_totalLatency[i])) / ((double) (g_group0_numFlits[i])) << endl;
		}
	}

	// group tree root, per switch averaged latency
	g_outputFile << " " << endl;
	g_outputFile << "TREE ROOT NODE: " << g_treeRoot_node << endl;
	g_outputFile << "TREE ROOT SWITCH: " << int(g_treeRoot_node / g_dP) << endl;
	g_outputFile << "TREE ROOT GROUP: " << g_rootH << " (GroupR)" << endl;
	for (int i = 0; i < g_dA; i++) {
		g_outputFile << "GroupR_SW" << i << "_flits: " << g_groupRoot_numFlits[i] << endl;
		if (g_groupRoot_numFlits[i] > 0) {
			g_outputFile << "GroupR_SW" << i << "_avgTotalLatency: "
					<< ((double) (g_groupRoot_totalLatency[i])) / ((double) (g_groupRoot_numFlits[i])) << endl;
		}
	}

	g_outputFile << endl;

}

void free_memory() {
	int i;

	for (i = 0; i < g_switchesCount; i++) {
		delete g_switchesList[i];
	}
	delete[] g_switchesList;

	for (i = 0; i < g_generatorsCount; i++) {
		delete g_generatorsList[i];
	}
	delete[] g_generatorsList;

	delete[] g_transient_record_latency;
	delete[] g_transient_record_num_flits;
	delete[] g_transient_record_num_misrouted;

	delete[] g_burst_traffic_adv_dist;
	delete[] g_burst_traffic_type;
	delete[] g_burst_traffic_type_percent;

	delete[] g_group0_numFlits;
	delete[] g_group0_totalLatency;
	delete[] g_groupRoot_totalLatency;

	delete[] g_latency_histogram_no_global_misroute;
	delete[] g_latency_histogram_global_misroute_at_injection;
	delete[] g_latency_histogram_other_global_misroute;

	delete[] g_hops_histogram;

	cout << "Memory freed" << endl;
}

void write_transient_output(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;
	int i;

	file_name.append(".transient");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cout << "Can't open the transient output file" << endl;
		exit(-1);
	}
	outputFile << "cycle\tlatency\tnum_packets\tnum_misrouted" << endl;
	for (i = 0; i < g_transient_record_len; i++) {
		outputFile << i - g_transient_record_num_prev_cycles << "\t";
		outputFile << g_transient_record_latency[i] << "\t";
		outputFile << g_transient_record_num_flits[i] << "\t";
		outputFile << g_transient_record_num_misrouted[i] << "\t";
		outputFile << endl;
	}
	outputFile.close();
}

void write_latency_histogram_output(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;
	int i;
	long long total_lat;

	file_name.append(".lat_hist");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cout << "Can't open the transient output file" << endl;
		exit(-1);
	}
	outputFile
			<< "latency\tnum_packets\tpercent\tnum_packets_no_global_misroute\tpercent_no_global_misroute\tnum_packets_global_misroute_at_injection\tpercent_global_misroute_at_injection\tnum_packets_other_global_misroute\tpercent_other_global_misroute"
			<< endl;
	for (i = 0; i < g_latency_histogram_maxLat; i++) {
		outputFile << i << "\t";
		total_lat = g_latency_histogram_no_global_misroute[i] + g_latency_histogram_global_misroute_at_injection[i]
				+ g_latency_histogram_other_global_misroute[i];
		outputFile << total_lat << "\t";
		outputFile << ((double) total_lat) / (g_flitReceivedCount - g_warmFlitReceivedCount) << "\t";
		outputFile << g_latency_histogram_no_global_misroute[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_no_global_misroute[i])
						/ (g_flitReceivedCount - g_warmFlitReceivedCount) << "\t";
		outputFile << g_latency_histogram_global_misroute_at_injection[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_global_misroute_at_injection[i])
						/ (g_flitReceivedCount - g_warmFlitReceivedCount) << "\t";
		outputFile << g_latency_histogram_other_global_misroute[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_other_global_misroute[i])
						/ (g_flitReceivedCount - g_warmFlitReceivedCount) << "\t";
		outputFile << endl;
	}
	outputFile.close();
}

void write_hops_histogram_output(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;
	int i;
	long long num_packets;

	file_name.append(".hops_hist");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cout << "Can't open the hop histogram output file" << endl;
		exit(-1);
	}
	outputFile << "hops\tnum_packets\tpercent" << endl;
	for (i = 0; i < g_hops_histogram_maxHops; i++) {
		outputFile << i << "\t";

		num_packets = g_hops_histogram[i];
		outputFile << num_packets << "\t";
		outputFile << ((double) num_packets) / (g_flitReceivedCount - g_warmFlitReceivedCount);

		outputFile << endl;
	}
	outputFile.close();
}

