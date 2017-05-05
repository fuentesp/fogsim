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

#include "dgflySimulator.h"
#include "global.h"
#include "configurationFile.h"
#include "generator/generatorModule.h"
#include "generator/burstGenerator.h"
#include "generator/traceGenerator.h"
#include "generator/graph500Generator.h"
#include "switch/ioqSwitchModule.h"
#include <math.h>
#include <sstream>
#include <iomanip>
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
	int i, j;

	/* Print back command from prompt; useful for cluster use */
	for (i = 0; i < argc; i++) {
		cout << argv[i] << " ";
	}
	cout << endl;

	/* Read parameters in config file */
	readConfiguration(argc, argv);
	srand(g_seed);
	g_reng.seed(g_seed);

	/* Open output file */
	g_output_file.open(g_output_file_name, ios::out);
	if (!g_output_file) {
		cerr << "Can't open the output file" << g_output_file_name << endl;
		exit(-1);
	}
	g_output_file.close();

	createNetwork();

#if DEBUG
	cout << "Network has been created" << endl;
#endif

	/* Read/translate trace-related files */
	if (g_traffic == TRACE) {
		for (i = 0; i < g_num_traces; i++) {
			/* read_trace() receives a vector of trace instances to
			 * load. For first initialization, all trace instances
			 * must be load at once. */
			translate_pcf_file(i);

			vector<int> instances;
			for (j = 0; j < g_trace_instances[i]; j++)
				instances.push_back(j);
			read_trace(i, instances);
		}
	}

	/* Run simulation */
	action();

	/* Write results into output file */
	for (i = 0; i < g_number_switches; i++) {
		if (g_switches_list[i]->packetsInj > g_max_injection_packets_per_sw) {
			g_max_injection_packets_per_sw = g_switches_list[i]->packetsInj;
			g_sw_with_max_injection_pkts = g_switches_list[i]->label;
		}
	}
	g_min_injection_packets_per_sw = g_max_injection_packets_per_sw;
	for (i = 0; i < g_number_switches; i++) {
		if (g_switches_list[i]->packetsInj < g_min_injection_packets_per_sw) {
			g_min_injection_packets_per_sw = g_switches_list[i]->packetsInj;
			g_sw_with_min_injection_pkts = g_switches_list[i]->label;
		}
	}
	assert(g_min_injection_packets_per_sw <= g_max_injection_packets_per_sw);

	cout << "Write output" << endl;
	writeOutput();

	if (g_print_hists) {
		writeLatencyHistogram(g_output_file_name);
		writeHopsHistogram(g_output_file_name);
		writeGeneratorsInjectionProbability(g_output_file_name);
	}
	if (g_transient_stats) writeTransientOutput(g_output_file_name);

	freeMemory();

	cout << "Simulation finished" << endl;

	return 0;
}

/* 
 * Reads config parameters from config file.
 * - Asserted parameters are required.
 * -'If-ed' parameters are optional (see default value in global.cc)
 */
void readConfiguration(int argc, char *argv[]) {
	char * filename = argv[1];
	int i, j, total_trace_nodes;
	string value;
	vector<string> list_values;
	ConfigFile config;

	/* Open configuration file */
	if (config.LoadConfig(filename) < 0) {
		cerr << "Can't read the configuration file: " << filename << endl;
		exit(-1);
	}

	/* Config file parameters are loaded up in a map, to be read.
	 * We check for parameters in the command line, and overwrite
	 * those in config file if necessary. */
	for (i = 2; i < argc; i++) {
		if (config.updateKeyValue("CONFIG", argv[i]) != 0) {
			cerr << endl << "Argument " << argv[i] << " doesn't follow prompt convention! Remember that first argument"
					" must be output file, and following arguments must be preceded by argument name and"
					" '=' symbol [no space in between]" << endl;
			exit(-1);
		}
	}

	/* Initialize auxiliar parameters */
	g_phase_traffic_adv_dist = new int[3];
	g_phase_traffic_type = new TrafficType[3];
	g_phase_traffic_percent = new int[3];
	for (i = 0; i < 3; i++) {
		g_phase_traffic_adv_dist[i] = 1;
		g_phase_traffic_type[i] = UN;
		g_phase_traffic_percent[i] = 0;
	}

	/* READ PARAMETERS */

	/* Necessary parameters (must be present in ALL cases) */
	assert(config.getKeyValue("CONFIG", "P", value) == 0);
	g_p_computing_nodes_per_router = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "A", value) == 0);
	g_a_routers_per_group = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "H", value) == 0);
	g_h_global_ports_per_router = atoi(value.c_str());

	g_number_switches = (g_a_routers_per_group * g_h_global_ports_per_router + 1) * g_a_routers_per_group;
	g_number_generators = g_number_switches * g_p_computing_nodes_per_router;

	assert(config.getKeyValue("CONFIG", "MaxCycles", value) == 0);
	g_max_cycles = atoi(value.c_str());
	if (config.getKeyValue("CONFIG", "WarmupCycles", value) == 0)
		g_warmup_cycles = atoi(value.c_str());
	else
		g_warmup_cycles = g_max_cycles;

	if (config.getKeyValue("CONFIG", "PrintCycles", value) == 0) g_print_cycles = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "ArbiterIterations", value) == 0);
	g_allocator_iterations = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "InjectionQueueLength", value) == 0);
	g_injection_queue_length = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "LocalQueueLength", value) == 0);
	g_local_queue_length = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "GlobalQueueLength", value) == 0);
	g_global_queue_length = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "Probability", value) == 0);
	g_injection_probability = atof(value.c_str());

	assert(config.getKeyValue("CONFIG", "Seed", value) == 0);
	g_seed = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "OutputFileName", value) == 0);
	g_output_file_name = new char[value.length() + 1];
	strcpy(g_output_file_name, value.c_str());

	assert(config.getKeyValue("CONFIG", "LocalLinkTransmissionDelay", value) == 0);
	g_local_link_transmission_delay = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "GlobalLinkTransmissionDelay", value) == 0);
	g_global_link_transmission_delay = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "InjectionDelay", value) == 0);
	g_injection_delay = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "PacketSize", value) == 0);
	g_packet_size = atoi(value.c_str());
	assert(g_packet_size > 0);

	assert(config.getKeyValue("CONFIG", "FlitSize", value) == 0);
	g_flit_size = atoi(value.c_str());
	assert(g_flit_size > 0);

	g_flits_per_packet = g_packet_size / g_flit_size;
	assert(g_flits_per_packet % 1 == 0);

	assert(config.getKeyValue("CONFIG", "GlobalChannels", value) == 0);
	g_global_link_channels = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "LocalLinkChannels", value) == 0);
	g_local_link_channels = atoi(value.c_str());

	assert(config.getKeyValue("CONFIG", "InjectionChannels", value) == 0);
	g_injection_channels = atoi(value.c_str());

	if (config.getKeyValue("CONFIG", "VcInjectionPolicy", value) == 0) {
		readVcInj(value.c_str(), &g_vc_injection);
	}

	if (config.getKeyValue("CONFIG", "VcUsage", value) == 0) {
		readVcUsage(value.c_str(), &g_vc_usage);
	}

	g_channels = (g_injection_channels > g_local_link_channels) ? g_injection_channels : g_local_link_channels;
	if (g_global_link_channels > g_channels) g_channels = g_global_link_channels;
	if (g_vc_usage == FLEXIBLE) {
		g_channels = g_local_link_channels + g_global_link_channels;
		if (config.getKeyValue("CONFIG", "VcAllocation", value) == 0) readVcAlloc(value.c_str(), &g_vc_alloc);
	}

	assert(config.getKeyValue("CONFIG", "PalmTreeConfiguration", value) == 0);
	g_palm_tree_configuration = atoi(value.c_str());

	if (config.getKeyValue("CONFIG", "InputArbiter", value) == 0) {
		readArbiterType(value.c_str(), &g_input_arbiter_type);
	}
	if (config.getKeyValue("CONFIG", "OutputArbiter", value) == 0) {
		readArbiterType(value.c_str(), &g_output_arbiter_type);
	}
	if (config.getKeyValue("CONFIG", "TransitPriority", value) == 0) {
		cerr
				<< "WARNING: deprecated parameter: TransitPriority. This parameter should be replaced by a LRS/PrioLRS OutputArbiter parameter."
				<< endl;
		if (atoi(value.c_str()) == 1) {
			g_output_arbiter_type = PrioLRS;
		} else {
			g_output_arbiter_type = LRS;
		}
	}

	/* Switch type */
	if (config.getKeyValue("CONFIG", "Switch", value) == 0) {
		readSwitchType(value.c_str(), &g_switch_type);

		if (g_switch_type == IOQ_SW) {
			assert(config.getKeyValue("CONFIG", "XbarDelay", value) == 0);
			g_xbar_delay = atoi(value.c_str());

			assert(config.getKeyValue("CONFIG", "OutQueueLength", value) == 0);
			g_out_queue_length = atoi(value.c_str());

			if (config.getKeyValue("CONFIG", "InternalSpeedup", value) == 0) {
				g_internal_speedup = strtod(value.c_str(), NULL);
			}

			if (config.getKeyValue("CONFIG", "NumberSegregatedTrafficFlows", value) == 0) {
				g_segregated_flows = atoi(value.c_str());
				assert(g_segregated_flows >= 1 && g_segregated_flows < 3); /* Number of flows
				 needs to be equal or greater than 1; currently only up to 2 flows are supported */
			}
		}
	}

	/* Buffer type */
	if (config.getKeyValue("CONFIG", "Buffer", value) == 0) {
		readBufferType(value.c_str(), &g_buffer_type);

		if (g_buffer_type == DYNAMIC) {
			assert(config.getKeyValue("CONFIG", "ReservedBufferLocal", value) == 0);
			g_local_queue_reserved = atoi(value.c_str());
			if (g_local_queue_reserved > (g_local_queue_length / g_flit_size / g_local_link_channels))
				cerr << "ERROR! with DYNAMIC buffers; for a local link buffer size of " << g_local_queue_length
						<< " phits and " << g_local_link_channels << " vcs, maximum reserved memory must be <= "
						<< (g_local_queue_length / g_flit_size / g_local_link_channels) << " pkts (input value: "
						<< g_local_queue_reserved << ")" << endl;
			assert(g_local_queue_reserved <= (g_local_queue_length / g_flit_size / g_local_link_channels));

			assert(config.getKeyValue("CONFIG", "ReservedBufferGlobal", value) == 0);
			g_global_queue_reserved = atoi(value.c_str());
			if (g_global_queue_reserved > (g_global_queue_length / g_flit_size / g_global_link_channels))
				cerr << "ERROR! with DYNAMIC buffers; for a local link buffer size of " << g_global_queue_length
						<< " phits and " << g_global_link_channels << " vcs, maximum reserved memory must be <= "
						<< (g_global_queue_length / g_flit_size / g_global_link_channels) << " pkts (input value: "
						<< g_global_queue_reserved << ")" << endl;
			assert(g_global_queue_reserved <= (g_global_queue_length / g_flit_size / g_global_link_channels));
		}
	}

	/* Class of service levels */
	if (config.getKeyValue("CONFIG", "CosLevels", value) == 0) {
		g_cos_levels = atoi(value.c_str());
		assert(g_cos_levels > 0 && g_cos_levels < 9);
	}

	/* Traffic pattern */
	assert(config.getKeyValue("CONFIG", "Traffic", value) == 0);
	readTrafficPattern(value.c_str(), &g_traffic);
	switch (g_traffic) {
		case UN_RCTV:
			g_reactive_traffic = true;
			g_traffic = UN;
			break;
		case ADV_RANDOM_NODE_RCTV:
			g_reactive_traffic = true;
			g_traffic = ADV_RANDOM_NODE;
			break;
		case BURSTY_UN_RCTV:
			g_reactive_traffic = true;
			g_traffic = BURSTY_UN;
			break;
	}

	/* Reactive traffic patterns need to have initial injection probability halved because
	 * every petition triggers a response packet, effectively doubling the amount of communications.
	 * Also the VCs must be evenly split between the two ways, making up for an even number of
	 * VCs in local and global links. */
	if (g_reactive_traffic) {
		g_injection_probability /= 2;
		assert(g_routing != OFAR); /* Currently OFAR does not support reactive traffic due to the use of VCs that is hard-coded into the OFAR class */
		/* With Flexible VC usage, we need to set the number of VCs reserved for the responses */
		if (g_vc_usage == FLEXIBLE) {
			assert(config.getKeyValue("CONFIG", "LocalResChannels", value) == 0);
			g_local_res_channels = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "GlobalResChannels", value) == 0);
			g_global_res_channels = atoi(value.c_str());
		} else {
			assert((g_local_link_channels % 2) == 0);
			assert((g_global_link_channels % 2) == 0);
		}
		if (config.getKeyValue("CONFIG", "MaxPetitionsOnFlight", value) == 0) {
			g_max_petitions_on_flight = atoi(value.c_str());
			assert(g_max_petitions_on_flight > 0);
		}
	}

	switch (g_traffic) {
		case ADV:
		case ADV_RANDOM_NODE:
			assert(config.getKeyValue("CONFIG", "AdvTrafficDistance", value) == 0);
			g_adv_traffic_distance = atoi(value.c_str());
			break;
		case ADV_LOCAL:
			assert(config.getKeyValue("CONFIG", "AdvTrafficLocalDistance", value) == 0);
			g_adv_traffic_local_distance = atoi(value.c_str());
			break;
		case ADVc:
			break;
		case MIX:
			assert(config.getKeyValue("CONFIG", "AdvTrafficDistance", value) == 0);
			g_adv_traffic_distance = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "AdvTrafficLocalDistance", value) == 0);
			g_adv_traffic_local_distance = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "randomTrafficPercent", value) == 0);
			g_phase_traffic_percent[0] = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "localAdvTrafficPercent", value) == 0);
			g_phase_traffic_percent[1] = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "globalAdvTrafficPercent", value) == 0);
			g_phase_traffic_percent[2] = atoi(value.c_str());
			assert(g_phase_traffic_percent[0] + g_phase_traffic_percent[1] + g_phase_traffic_percent[2] == 100);
			break;
		case TRANSIENT:
			g_transient_stats = true;
			assert(config.getKeyValue("CONFIG", "transientTrafficFirstPattern", value) == 0);
			readTrafficPattern(value.c_str(), &g_phase_traffic_type[0]);
			if (config.getKeyValue("CONFIG", "AdvTrafficDistance", value) == 0) {
				g_phase_traffic_adv_dist[0] = atoi(value.c_str());
			}
			assert(config.getKeyValue("CONFIG", "transientTrafficSecondPattern", value) == 0);
			readTrafficPattern(value.c_str(), &g_phase_traffic_type[1]);
			if (config.getKeyValue("CONFIG", "transientTrafficNextDist", value) == 0) {
				g_phase_traffic_adv_dist[1] = atoi(value.c_str()); //Shouldn't this be an obliged parameter? (since its value is taken when reading 2nd traffic pattern
			}
			assert(config.getKeyValue("CONFIG", "transientRecordCycles", value) == 0);
			g_transient_record_num_cycles = atoi(value.c_str());
			assert(g_transient_record_num_cycles > 0);
			assert(config.getKeyValue("CONFIG", "transientTrafficCycle", value) == 0);
			g_transient_traffic_cycle = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "transientRecordPrevCycles", value) == 0);
			g_transient_record_num_prev_cycles = atoi(value.c_str());
			assert(g_transient_record_num_prev_cycles > 0);
			g_transient_record_len = g_transient_record_num_cycles + g_transient_record_num_prev_cycles;
			break;
		case ALL2ALL:
			g_injection_probability = 100;
			g_warmup_cycles = 0;
			g_max_cycles = 10000000;
			assert(config.getKeyValue("CONFIG", "phases", value) == 0);
			g_phases = atoi(value.c_str());
			assert(g_phases > 0);
			if (config.getKeyValue("CONFIG", "random_all-to-all", value) == 0) {
				g_random_AllToAll = atoi(value.c_str());
			}
			g_single_burst_length = (g_number_generators - 1) * g_phases * g_flits_per_packet;
			break;
		case SINGLE_BURST:
			g_injection_probability = 100;
			g_warmup_cycles = 0;
			g_max_cycles = 1000000;
			assert(config.getKeyValue("CONFIG", "burstLength", value) == 0);
			g_single_burst_length = atoi(value.c_str());
			g_single_burst_length = g_single_burst_length * g_flits_per_packet;
			assert(config.getKeyValue("CONFIG", "burstTrafficType1", value) == 0);
			readTrafficPattern(value.c_str(), &g_phase_traffic_type[0]);
			assert(config.getKeyValue("CONFIG", "burstTrafficTypePercent1", value) == 0);
			g_phase_traffic_percent[0] = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "burstTrafficAdvDist1", value) == 0) {
				g_phase_traffic_adv_dist[0] = atoi(value.c_str());
			}
			assert(config.getKeyValue("CONFIG", "burstTrafficType2", value) == 0);
			readTrafficPattern(value.c_str(), &g_phase_traffic_type[1]);
			assert(config.getKeyValue("CONFIG", "burstTrafficTypePercent2", value) == 0);
			g_phase_traffic_percent[1] = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "burstTrafficAdvDist2", value) == 0) {
				g_phase_traffic_adv_dist[1] = atoi(value.c_str());
			}
			assert(config.getKeyValue("CONFIG", "burstTrafficType3", value) == 0);
			readTrafficPattern(value.c_str(), &g_phase_traffic_type[2]);
			assert(config.getKeyValue("CONFIG", "burstTrafficTypePercent3", value) == 0);
			g_phase_traffic_percent[2] = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "burstTrafficAdvDist3", value) == 0) {
				g_phase_traffic_adv_dist[2] = atoi(value.c_str());
			}
			assert(g_phase_traffic_percent[0] + g_phase_traffic_percent[1] + g_phase_traffic_percent[2] == 100);
			break;
		case BURSTY_UN:
			assert(config.getKeyValue("CONFIG", "avgBurstLength", value) == 0);
			g_bursty_avg_length = atoi(value.c_str());
			break;
		case GRAPH500:
			assert(g_deadlock_avoidance == DALLY && g_flit_size == g_packet_size);
			assert(config.getKeyValue("CONFIG", "GraphCoalescingSize", value) == 0);
			g_graph_coalescing_size = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "GraphQueryTime", value) == 0) {
				g_graph_query_time = atof(value.c_str());
				g_injection_probability = (100.0 * g_flit_size) / (g_graph_query_time * g_graph_coalescing_size);
			} else
				g_graph_query_time = (100.0 * g_flit_size) / (g_injection_probability * g_graph_coalescing_size);
			if (config.getListValues("CONFIG", "GraphNodesCapMod", list_values) == 0) {
				assert(int(list_values.size()) <= g_number_generators);
				for (i = 0; i < list_values.size(); i++) {
					assert(atoi(list_values[i].c_str()) < g_number_generators);
					g_graph_nodes_cap_mod.push_back(atoi(list_values[i].c_str()));
				}
			} else if (config.getKeyValue("CONFIG", "GraphNodesCapMod", value) == 0) {
				assert(atoi(value.c_str()) < g_number_generators);
				g_graph_nodes_cap_mod.push_back(atoi(value.c_str()));
			}
			if (g_graph_nodes_cap_mod.size() > 0) {
				assert(config.getKeyValue("CONFIG", "GraphCapModFactor", value) == 0);
				g_graph_cap_mod_factor = atoi(value.c_str());
				assert(g_graph_cap_mod_factor > 0 && g_graph_cap_mod_factor <= 200);
			}
			assert(config.getKeyValue("CONFIG", "GraphScale", value) == 0);
			g_graph_scale = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "GraphEdgefactor", value) == 0);
			g_graph_edgefactor = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "GraphMaxLevels", value) == 0) g_graph_max_levels = atoi(value.c_str());
			/* Reused variables from trace generators: if specified, use the number of
			 * processes used to run the benchmark and the number of benchmark instances.
			 * By default consider that the benchmark execution employs the whole system. */
			g_num_traces = 1;
			if (config.getKeyValue("CONFIG", "GraphProcesses", value) == 0)
				g_trace_nodes.push_back(atol(value.c_str()));
			else
				g_trace_nodes.push_back(g_number_generators);
			if (config.getKeyValue("CONFIG", "GraphInstances", value) == 0)
				g_trace_instances.push_back(atoi(value.c_str()));
			else
				g_trace_instances.push_back(1);
			assert(g_trace_nodes[0] * g_trace_instances[0] <= g_number_generators);
			/* 2 ways of specifying benchmark layout: through a trace map
			 * file, and selecting a preset trace distribution */
			if (config.getKeyValue("CONFIG", "TraceMap", value) == 0) {
				readTraceMap(value.c_str());
			} else {
				if (config.checkSection("TRACE_MAP")) {
					readTraceMap(filename);
				} else {
					if (config.getKeyValue("CONFIG", "TraceDistribution", value) == 0)
						readTraceDistribution(value.c_str(), &g_trace_distribution);
					assert(g_trace_instances.size() != 0); // Sanity check; value was previously collected
					buildTraceMap();
				}
			}
			if (config.getKeyValue("CONFIG", "GraphRootDegree", value) == 0)
				for (i = 0; i < g_trace_instances[0]; i++)
					g_graph_root_degree.push_back(atoi(value.c_str()));
			else {
				/* If a root vertex connectivity is not provided, determine it through a lognormal distribution */
				lognormal_distribution<float> g_graph_lognormal(
						log(pow(0.3604, g_graph_scale)) + 1.0661704 * g_graph_scale, 0.079313065 * g_graph_scale);
				for (i = 0; i < g_trace_instances[0]; i++)
					g_graph_root_degree.push_back(ceil(g_graph_lognormal(g_reng)));
			}
			for (i = 0; i < g_trace_instances[0]; i++) {
				g_graph_tree_level.push_back(0);
				/* Choose a random node of the current instance as root */
				g_graph_root_node.push_back(
						g_trace_2_gen_map[0][rand() / (int) (((unsigned) RAND_MAX + 1) / (g_trace_nodes[0]))][i]);
				/* Upper bound of the p2p queries */
				g_graph_queries_remain.push_back(
						pow(2, g_graph_scale + 1) * g_graph_edgefactor * ((g_trace_nodes[0] - 1.0) / g_trace_nodes[0]));
				g_graph_queries_rem_minus_means.push_back(
						g_graph_queries_remain[i]
								- ceil(g_graph_root_degree[i] * (g_trace_nodes[0] - 1.0) / g_trace_nodes[0]));
			}
#if DEBUG	/* These statistics are not computed in a release compilation because they eat too much memory */
			g_graph_p2pmess_node2node = new long long **[g_graph_max_levels];
			for (int l = 0; l < g_graph_max_levels; l++) {
				g_graph_p2pmess_node2node[l] = new long long *[g_number_generators];
				for (i = 0; i < g_number_generators; i++) {
					g_graph_p2pmess_node2node[l][i] = new long long[g_number_generators];
					for (int z = 0; z < g_number_generators; z++)
					g_graph_p2pmess_node2node[l][i][z] = 0;
				}
			}
#endif
			break;
		case TRACE:
			/* TRACE simulations need to be REDEFINED!! to use a map of assignments */

			assert(config.getKeyValue("CONFIG", "numTraces", value) == 0);
			g_num_traces = atoi(value.c_str());
			assert(g_num_traces > 0); //Current code doesn't support more than two trace files

			if (config.getKeyValue("CONFIG", "trackTempStats", value) == 0) {
				g_transient_stats = atoi(value.c_str());
				assert(config.getKeyValue("CONFIG", "transientRecordCycles", value) == 0);
				g_transient_record_num_cycles = atoi(value.c_str());
				assert(g_transient_record_num_cycles > 0);
				assert(config.getKeyValue("CONFIG", "transientRecordPrevCycles", value) == 0);
				g_transient_record_num_prev_cycles = atoi(value.c_str());
				assert(g_transient_record_num_prev_cycles > 0);
				g_transient_record_len = g_transient_record_num_cycles + g_transient_record_num_prev_cycles;
			}

			/* Depending on the number of different traces employed, one or other function
			 * will be used to determine variable values. */
			if (g_num_traces == 1) {
				assert(config.getKeyValue("CONFIG", "trcFile", value) == 0);
				g_trace_file.push_back(value);

				assert(config.getKeyValue("CONFIG", "trace_nodes", value) == 0);
				g_trace_nodes.push_back(atol(value.c_str()));

				assert(config.getKeyValue("CONFIG", "pcfFile", value) == 0);
				g_pcf_file.push_back(value);

				if (config.getKeyValue("CONFIG", "trace_copies", value) == 0) {
					g_trace_instances.push_back(atoi(value.c_str()));
				} else {
					g_trace_instances.push_back(1);
				}
			} else {
				assert(config.getListValues("CONFIG", "trcFile", list_values) == 0);
				assert(int(list_values.size()) == g_num_traces); // Sanity check
				for (i = 0; i < g_num_traces; i++)
					g_trace_file.push_back(list_values[i]);

				assert(config.getListValues("CONFIG", "trace_nodes", list_values) == 0);
				assert(int(list_values.size()) == g_num_traces); // Sanity check
				for (i = 0; i < g_num_traces; i++)
					g_trace_nodes.push_back(atol(list_values[i].c_str()));

				assert(config.getListValues("CONFIG", "pcfFile", list_values) == 0);
				assert(int(list_values.size()) == g_num_traces); // Sanity check
				for (i = 0; i < g_num_traces; i++)
					g_pcf_file.push_back(list_values[i]);

				if (config.getListValues("CONFIG", "trace_copies", list_values) == 0) {
					assert(int(list_values.size()) == g_num_traces); // Sanity check
					for (i = 0; i < g_num_traces; i++)
						g_trace_instances.push_back(atoi(list_values[i].c_str()));
				} else {
					for (i = 0; i < g_num_traces; i++)
						g_trace_instances.push_back(1);
				}

			}

			/* 2 ways of specifying trace layout: through a trace map
			 * file, and selecting a preset trace distribution */
			if (config.getKeyValue("CONFIG", "TraceMap", value) == 0) {
				readTraceMap(value.c_str());
			} else {
				if (config.checkSection("TRACE_MAP")) {
					readTraceMap(filename);
				} else {
					assert(config.getKeyValue("CONFIG", "trace_distribution", value) == 0);
					readTraceDistribution(value.c_str(), &g_trace_distribution);
					assert(g_trace_instances.size() != 0); // Sanity check; value was previously collected
					buildTraceMap();
				}
			}

			/* Sanity check: can not have more trace source ids than generators in the network */
			total_trace_nodes = 0;
			for (i = 0; i < g_num_traces; i++)
				total_trace_nodes += g_trace_nodes[i] * g_trace_instances[i];
			assert(total_trace_nodes <= g_number_generators);

			if (config.getKeyValue("CONFIG", "cpu_speed", value) == 0) {
				g_cpu_speed = atof(value.c_str());
			}

			if (config.getKeyValue("CONFIG", "op_per_cycle", value) == 0) {
				g_op_per_cycle = atol(value.c_str());
			}

			if (config.getKeyValue("CONFIG", "cutmpi", value) == 0) {
				g_cutmpi = atol(value.c_str());
			}

			if (config.getKeyValue("CONFIG", "phit_size", value) == 0) {
				g_phit_size = atol(value.c_str());
			}

			break;
		default:
			break;
	}

	/* Deadlock avoidance & Routing */
	assert(config.getKeyValue("CONFIG", "deadlock_avoidance", value) == 0);
	readDeadlockAvoidanceMechanism(value.c_str(), &g_deadlock_avoidance);
	switch (g_deadlock_avoidance) {
		case DALLY:
			/* Dally usage of VCs - deadlock avoidance */
			break;
		case RING:
			assert(config.getKeyValue("CONFIG", "rings", value) == 0);
			g_rings = atoi(value.c_str());
			assert(g_rings >= 1);
			assert(config.getKeyValue("CONFIG", "RingInjectionBubble", value) == 0);
			g_ring_injection_bubble = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "ringDirs", value) == 0);
			g_ringDirs = atoi(value.c_str());
			assert(g_ringDirs == 0 || g_ringDirs == 1 || g_ringDirs == 2);
			if (g_rings > 1) {
				assert(config.getKeyValue("CONFIG", "onlyRing2", value) == 0);
				g_onlyRing2 = atoi(value.c_str());
			}
			g_ring_ports = 2 * g_rings; /* Ring dedicated physical ports */
			assert(g_channels_escape == 0); // Sanity check
			if (config.getKeyValue("CONFIG", "forbid_from_injQueues_to_ring", value) == 0) {
				g_forbid_from_inj_queues_to_ring = atoi(value.c_str());
			}
			break;
		case EMBEDDED_RING:
			assert(config.getKeyValue("CONFIG", "rings", value) == 0);
			g_rings = atoi(value.c_str());
			assert(g_rings >= 1);
			if (g_rings > 1) {
				assert(config.getKeyValue("CONFIG", "onlyRing2", value) == 0);
				g_onlyRing2 = atoi(value.c_str());
			}
			assert(config.getKeyValue("CONFIG", "RingInjectionBubble", value) == 0);
			g_ring_injection_bubble = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "channels_ring", value) == 0);
			g_channels_escape = atoi(value.c_str());
			// Sanity checks
			assert(g_palm_tree_configuration == 1);
			assert(g_channels_escape > 0);
			if (g_injection_channels < g_local_link_channels + g_channels_escape) {
				/* Update number of VCs */
				g_channels = g_local_link_channels + g_channels_escape;
			}
			if (config.getKeyValue("CONFIG", "forbid_from_injQueues_to_ring", value) == 0) {
				g_forbid_from_inj_queues_to_ring = atoi(value.c_str());
			}
			break;
		case EMBEDDED_TREE:
			assert(config.getKeyValue("CONFIG", "channels_tree", value) == 0);
			g_channels_escape = atoi(value.c_str());
			// Sanity checks
			assert(g_palm_tree_configuration == 1);
			assert(g_channels_escape > 0);
			if (g_injection_channels < g_local_link_channels + g_channels_escape) {
				/* Update number of VCs */
				g_channels = g_local_link_channels + g_channels_escape;
			}
			break;
		default:
			cerr << "ERROR: NO DEADLOCK AVOIDANCE MECHANISM HAS BEEN FOUND" << endl;
			exit(0);
	}

	if (g_deadlock_avoidance == RING || g_deadlock_avoidance == EMBEDDED_RING
			|| g_deadlock_avoidance == EMBEDDED_TREE) {
		/* Ring and tree escape subnetworks are only defined for its
		 * usage with global network palm tree disposal, not with the
		 * "traditional" distribution */
		assert(g_palm_tree_configuration == 1);
		/* If deadlock avoidance mechanism is a escape subnetwork,
		 a congestion management mechanism can be used */
		if (config.getKeyValue("CONFIG", "CongestionManagement", value) == 0) {
			if (strcmp(value.c_str(), "BCM") == 0) {
				g_congestion_management = BCM;
				assert(config.getKeyValue("CONFIG", "baseCongestionControl_bub", value) == 0);
				g_baseCongestionControl_bub = atoi(value.c_str());
			} else if (strcmp(value.c_str(), "ECM") == 0) {
				g_congestion_management = ECM;
				assert(config.getKeyValue("CONFIG", "escapeCongestion_th", value) == 0);
				g_escapeCongestion_th = atoi(value.c_str());
				assert((g_escapeCongestion_th >= 0) && (g_escapeCongestion_th <= 100));
			} else {
				cerr << "ERROR: UNRECOGNISED CONGESTION MANAGEMENT MECHANISM!" << endl;
				exit(0);
			}
		}
	}

	if (config.getKeyValue("CONFIG", "GlobalMisroutingPolicy", value) == 0) {
		readGlobalMisrouting(value.c_str(), &g_global_misrouting);
	}

	assert(config.getKeyValue("CONFIG", "routing", value) == 0);
	readRoutingType(value.c_str(), &g_routing);
	switch (g_routing) {
		case MIN:
			assert(g_deadlock_avoidance == DALLY);
			break;
		case MIN_COND:
			assert(g_deadlock_avoidance == DALLY);
			break;
		case VAL:
		case VAL_ANY:
			assert(g_deadlock_avoidance == DALLY);
			break;
		case OBL:
			assert(g_deadlock_avoidance == DALLY);
			if (config.getKeyValue("CONFIG", "ResetValNode", value) == 0)
				g_reset_val = atoi(value.c_str());
			break;
		case PAR:
			assert(g_deadlock_avoidance == DALLY);
			assert(config.getKeyValue("CONFIG", "MisroutingTrigger", value) == 0);
			break;
		case UGAL:
			assert(g_deadlock_avoidance == DALLY);
			assert(config.getKeyValue("CONFIG", "ugal_global_threshold", value) == 0);
			g_ugal_global_threshold = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "ugal_local_threshold", value) == 0);
			g_ugal_local_threshold = atoi(value.c_str());
			break;
		case SRC_ADP:
			assert(config.getKeyValue("CONFIG", "CongestionDetection", value) == 0);
			readCongestionDetection(value.c_str(), &g_congestion_detection);
			if (config.getKeyValue("CONFIG", "ResetValNode", value) == 0)
				g_reset_val = atoi(value.c_str());
		case PB:
		case PB_ANY:
			assert(g_deadlock_avoidance == DALLY);
			assert(config.getKeyValue("CONFIG", "CongestionDetection", value) == 0);
			readCongestionDetection(value.c_str(), &g_congestion_detection);
			assert(config.getKeyValue("CONFIG", "piggybacking_coef", value) == 0);
			g_piggyback_coef = atoi(value.c_str());
			assert(g_piggyback_coef > 0);
			if (config.getKeyValue("CONFIG", "ugal_global_threshold", value) == 0)
				g_ugal_global_threshold = atoi(value.c_str());
			if (config.getKeyValue("CONFIG", "ugal_local_threshold", value) == 0)
				g_ugal_local_threshold = atoi(value.c_str());
			break;
		case RLM:
			assert(g_deadlock_avoidance == DALLY);
			assert(config.getKeyValue("CONFIG", "MisroutingTrigger", value) == 0); //We need to have a misrouting trigger defined
			break;
		case OLM:
			assert(g_deadlock_avoidance == DALLY);
			assert(config.getKeyValue("CONFIG", "MisroutingTrigger", value) == 0);
			assert(config.getKeyValue("CONFIG", "CongestionDetection", value) == 0);
			readCongestionDetection(value.c_str(), &g_congestion_detection);
			break;
		case OFAR:
			assert(
					g_deadlock_avoidance == RING || g_deadlock_avoidance == EMBEDDED_RING
							|| g_deadlock_avoidance == EMBEDDED_TREE);
			assert(config.getKeyValue("CONFIG", "RestrictLocalCycles", value) == 0);
			g_restrictLocalCycles = atoi(value.c_str());
			assert(config.getKeyValue("CONFIG", "MisroutingTrigger", value) == 0);
			if (config.getKeyValue("CONFIG", "OFAR_misrouting_local", value) == 0) {
			}
			break;
		default: /* An unrecognized routing type should not be used */
			assert(0);
			break;
	}

	if (config.getKeyValue("CONFIG", "MisroutingTrigger", value) == 0) {
		readMisroutingTrigger(value.c_str(), &g_misrouting_trigger);
		switch (g_misrouting_trigger) {
			case CGA:
				g_relative_threshold = 1;
				g_vc_misrouting_congested_restriction = 1;
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionCoefPercent", value) == 0);
				g_vc_misrouting_congested_restriction_coef_percent = atoi(value.c_str());
				assert(g_vc_misrouting_congested_restriction_coef_percent >= 0);
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionThreshold", value) == 0);
				g_vc_misrouting_congested_restriction_th = atoi(value.c_str());
				break;
			case HYBRID: /* Hybrid (contention OR congestion aware): needs both parameters for CA and CGA thresholds */
				g_relative_threshold = 1;
				g_vc_misrouting_congested_restriction = 1;
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionCoefPercent", value) == 0);
				g_vc_misrouting_congested_restriction_coef_percent = atoi(value.c_str());
				assert(g_vc_misrouting_congested_restriction_coef_percent >= 0);
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionThreshold", value) == 0);
				g_vc_misrouting_congested_restriction_th = atoi(value.c_str());
			case DUAL:
			case WEIGTHED_CA:
			case CA:
				g_contention_aware = 1;
				assert(g_routing == PAR || g_routing == RLM || g_routing == OLM);
				assert(config.getKeyValue("CONFIG", "ContentionThreshold", value) == 0);
				g_contention_aware_th = atoi(value.c_str());
				assert(g_contention_aware_th >= 0);
				if (config.getKeyValue("CONFIG", "ContentionLocalThreshold", value) == 0) {
					g_contention_aware_local_th = atoi(value.c_str());
					assert(g_contention_aware_local_th >= 0);
				}
				if (config.getKeyValue("CONFIG", "increaseContentionAtHeader", value) == 0) {
					g_increaseContentionAtHeader = atoi(value.c_str());
				}
				break;
			case FILTERED:
				g_contention_aware = 1;
				assert(g_routing == PAR || g_routing == RLM || g_routing == OLM);
				assert(config.getKeyValue("CONFIG", "ContentionThreshold", value) == 0);
				g_contention_aware_th = atoi(value.c_str());
				assert(config.getKeyValue("CONFIG", "FilteredContentionCoefficient", value) == 0);
				g_filtered_contention_regressive_coefficient = atof(value.c_str());
				assert(g_contention_aware_th >= 0);
				if (config.getKeyValue("CONFIG", "increaseContentionAtHeader", value) == 0) {
					g_increaseContentionAtHeader = atoi(value.c_str());
				}
				break;
			case CA_REMOTE:
				g_contention_aware = 1;
				assert(g_routing == PAR || g_routing == RLM || g_routing == OLM);
				assert(config.getKeyValue("CONFIG", "ContentionThreshold", value) == 0);
				g_contention_aware_th = atoi(value.c_str());
				assert(config.getKeyValue("CONFIG", "ContentionGlobalThreshold", value) == 0);
				g_contention_aware_global_th = atoi(value.c_str());
				assert(g_contention_aware_th >= 0);
				assert(g_contention_aware_global_th >= 0);
				if (config.getKeyValue("CONFIG", "increaseContentionAtHeader", value) == 0) {
					g_increaseContentionAtHeader = atoi(value.c_str());
				}
				break;
			case HYBRID_REMOTE:
				g_relative_threshold = 1;
				g_vc_misrouting_congested_restriction = 1;
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionCoefPercent", value) == 0);
				g_vc_misrouting_congested_restriction_coef_percent = atoi(value.c_str());
				assert(g_vc_misrouting_congested_restriction_coef_percent >= 0);
				assert(config.getKeyValue("CONFIG", "VcMisroutingCongestedRestrictionThreshold", value) == 0);
				g_vc_misrouting_congested_restriction_th = atoi(value.c_str());
				g_contention_aware = 1;
				assert(g_routing == PAR || g_routing == RLM || g_routing == OLM);
				assert(config.getKeyValue("CONFIG", "ContentionThreshold", value) == 0);
				g_contention_aware_th = atoi(value.c_str());
				assert(g_contention_aware_th >= 0);
				if (config.getKeyValue("CONFIG", "increaseContentionAtHeader", value) == 0) {
					g_increaseContentionAtHeader = atoi(value.c_str());
				}
				assert(config.getKeyValue("CONFIG", "ContentionGlobalThreshold", value) == 0);
				g_contention_aware_global_th = atoi(value.c_str());
				assert(g_contention_aware_global_th >= 0);
				break;
		}
	}

	// Determine (if proceeds) local and global misrouting percentage thresholds
	if (g_routing == OLM || g_routing == RLM || g_routing == PAR || g_routing == OFAR) {
		switch (g_misrouting_trigger) {
			case CGA:
			case HYBRID:
			case HYBRID_REMOTE:
				assert(config.getKeyValue("CONFIG", "PercentLocalThreshold", value) == 0);
				g_percent_local_threshold = atoi(value.c_str());
				assert(g_percent_local_threshold >= 0 && g_percent_local_threshold <= 100);
				assert(config.getKeyValue("CONFIG", "PercentGlobalThreshold", value) == 0);
				g_percent_global_threshold = atoi(value.c_str());
				assert(g_percent_global_threshold >= 0 && g_percent_global_threshold <= 100);
				assert(config.getKeyValue("CONFIG", "ThMin", value) == 0);
				g_th_min = atoi(value.c_str());
				assert(g_th_min >= 0 && g_th_min <= 100);
				assert(config.getKeyValue("CONFIG", "CongestionDetection", value) == 0);
				readCongestionDetection(value.c_str(), &g_congestion_detection);
				break;
			default:
				break;
		}
	}

	/* Optional parameters (can be avoided) */

	if (config.getKeyValue("CONFIG", "PrintHists", value) == 0) g_print_hists = atoi(value.c_str());

	if (config.getKeyValue("CONFIG", "forceMisrouting", value) == 0) {
		g_forceMisrouting = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "try_just_escape", value) == 0) {
		g_try_just_escape = atoi(value.c_str());
	}

	if (config.getKeyValue("CONFIG", "CrossbarPortsPerInPort", value) == 0) {
		g_local_arbiter_speedup = atoi(value.c_str());
		assert(config.getKeyValue("CONFIG", "IssueParallelReqs", value) == 0);
		g_issue_parallel_reqs = atoi(value.c_str());
	}

	if (config.getListValues("CONFIG", "VerboseSwitches", list_values) == 0) {
		for (i = 0; i < list_values.size(); i++)
			g_verbose_switches.insert(atoi(list_values[i].c_str()));
		if (config.getKeyValue("CONFIG", "VerboseCycles", value) == 0)
			g_verbose_cycles = atoi(value.c_str());
	}

	/* All config values have been loaded, let config descriptor be removed */
	config.flushConfig();
}

void createNetwork() {
	int i, j, k;
	string swtch = "switcha_b";
	string gnrtr = "generatora_b_c";

	g_ports = g_p_computing_nodes_per_router + g_a_routers_per_group - 1 + g_h_global_ports_per_router;
	g_global_links_per_group = g_a_routers_per_group * g_h_global_ports_per_router;

	if (g_deadlock_avoidance == EMBEDDED_TREE) {
		g_tree_root_node = rand() % g_number_switches; /* We used this node to route packets to the root switch */
		cout << "treeRoot_node=" << g_tree_root_node << endl;
		cout << "treeRoot_switch=" << int(g_tree_root_node / g_p_computing_nodes_per_router) << endl;
		g_tree_root_switch = int(g_tree_root_node / (g_a_routers_per_group * g_p_computing_nodes_per_router));
	}

	g_generators_list = new generatorModule*[g_number_generators];
	g_switches_list = new switchModule*[g_number_switches];

	if (g_rings != 0) { /* Physical Ring */
		if (g_deadlock_avoidance == RING) g_ports = g_ports + 2;
		g_globalEmbeddedRingSwitchesCount = 2 * (g_a_routers_per_group * g_h_global_ports_per_router + 1); /* 2 switches per group dedicate one global links to the embedded ring */
		g_localEmbeddedRingSwitchesCount = g_number_switches; /* Every switch in the group dedicates 1 or 2 local links to the embedded ring */
	}

	g_latency_histogram_no_global_misroute.insert(g_latency_histogram_no_global_misroute.begin(),
			g_latency_histogram_maxLat, 0);
	g_latency_histogram_global_misroute_at_injection.insert(g_latency_histogram_global_misroute_at_injection.begin(),
			g_latency_histogram_maxLat, 0);
	g_latency_histogram_other_global_misroute.insert(g_latency_histogram_other_global_misroute.begin(),
			g_latency_histogram_maxLat, 0);

	g_hops_histogram = new long long[g_hops_histogram_maxHops];

	for (i = 0; i < g_hops_histogram_maxHops; i++) {
		g_hops_histogram[i] = 0;
	}

	g_local_router_links_offset = g_p_computing_nodes_per_router;
	g_global_router_links_offset = g_local_router_links_offset + g_a_routers_per_group - 1;

	g_group0_numFlits = new long long**[g_a_routers_per_group];
	g_group0_totalLatency = new long double[g_a_routers_per_group];
	for (i = 0; i < g_a_routers_per_group; i++) {
		g_group0_numFlits[i] = new long long*[g_p_computing_nodes_per_router];
		for (int p = 0; p < g_p_computing_nodes_per_router; p++) {
			g_group0_numFlits[i][p] = new long long[2];
			g_group0_numFlits[i][p][0] = 0;
			g_group0_numFlits[i][p][1] = 0;
		}
		g_group0_totalLatency[i] = 0;
	}

	g_groupRoot_numFlits = new long long[g_a_routers_per_group];
	g_groupRoot_totalLatency = new long double[g_a_routers_per_group];
	for (i = 0; i < g_a_routers_per_group; i++) {
		g_groupRoot_numFlits[i] = 0;
		g_groupRoot_totalLatency[i] = 0;
	}

	for (i = 0; i < g_ports; i++) {
		g_port_usage_counter[i] = 0;
		g_port_contention_counter[i] = 0;
	}

	switch (g_vc_usage) {
		case BASE:
			for (i = 0; i < (g_a_routers_per_group - 1); i++) {
				g_vc_counter.push_back(vector<long long>(g_local_link_channels, 0));
			}
			for (i = 0; i < g_h_global_ports_per_router; i++) {
				g_vc_counter.push_back(vector<long long>(g_global_link_channels, 0));
			}
			break;
		case FLEXIBLE:
			for (i = 0; i < (g_a_routers_per_group - 1 + g_h_global_ports_per_router); i++) {
				g_vc_counter.push_back(vector<long long>(g_local_link_channels + g_global_link_channels, 0));
			}
			break;
		default:
			assert(0);
			break;
	}

	cout << "switchModule size: " << sizeof(switchModule) << endl;
	cout << "generatorModule size: " << sizeof(generatorModule) << endl;

	if (g_vc_usage == FLEXIBLE) g_channels = g_local_link_channels + g_global_link_channels;
	for (i = 0; i < ((g_h_global_ports_per_router * g_a_routers_per_group) + 1); i++) {
		for (j = 0; j < (g_a_routers_per_group); j++) {
			try {
				switch (g_switch_type) {
					case BASE_SW:
						g_switches_list[j + i * g_a_routers_per_group] = new switchModule("switch",
								(j + i * g_a_routers_per_group), j, i, g_ports, g_channels);
						break;
					case IOQ_SW:
						g_switches_list[j + i * g_a_routers_per_group] = new ioqSwitchModule("switch",
								(j + i * g_a_routers_per_group), j, i, g_ports, g_channels);
						break;
					default:
						cerr << "Unknown switch type" << endl;
						exit(0);
				}
			} catch (exception& e) {
				g_output_file << "Error making switchModule, i= " << i << ", j=" << j;
				cerr << "Error making switchModule, i= " << i << ", j=" << j;
				exit(0);
			}
			for (k = 0; k < g_p_computing_nodes_per_router; k++) {
				try {
					switch (g_traffic) {
						case TRACE:
							g_generators_list[(k + j * g_p_computing_nodes_per_router
									+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group))] =
									new traceGenerator(1, "generator",
											(k + j * g_p_computing_nodes_per_router
													+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group)), k,
											j, i, g_switches_list[(j + i * g_a_routers_per_group)]);
							break;
						case BURSTY_UN:
							g_generators_list[(k + j * g_p_computing_nodes_per_router
									+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group))] =
									new burstGenerator(1, "generator",
											(k + j * g_p_computing_nodes_per_router
													+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group)), k,
											j, i, g_switches_list[(j + i * g_a_routers_per_group)]);
							break;
						case GRAPH500:
							g_generators_list[(k + j * g_p_computing_nodes_per_router
									+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group))] =
									new graph500Generator(1, "graph500",
											(k + j * g_p_computing_nodes_per_router
													+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group)), k,
											j, i, g_switches_list[(j + i * g_a_routers_per_group)]);
							break;
						default:
							g_generators_list[(k + j * g_p_computing_nodes_per_router
									+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group))] =
									new generatorModule(1, "generator",
											(k + j * g_p_computing_nodes_per_router
													+ i * (g_p_computing_nodes_per_router * g_a_routers_per_group)), k,
											j, i, g_switches_list[(j + i * g_a_routers_per_group)]);
					}
				} catch (exception& e) {
					g_output_file << "Error making generatorModule, i= " << i << ", j=" << j << ", k=" << k;
					cerr << "Error making generatorModule, i= " << i << ", j=" << j << ", k=" << k;
					exit(0);
				}
			}
		}
	}

	for (i = 0; i < g_number_switches; i++) {
		g_switches_list[i]->routing->initialize(g_switches_list);
		g_switches_list[i]->resetCredits();
	}

	//transient stats variables
	//initialize records
	if (g_transient_stats) {
		g_transient_record_latency = new float[g_transient_record_len];
		g_transient_record_injection_latency = new float[g_transient_record_len];
		g_transient_record_flits = new int[g_transient_record_len];
		g_transient_record_misrouted_flits = new int[g_transient_record_len];
		g_transient_net_injection_latency = new float[g_transient_record_len];
		g_transient_net_injection_inj_latency = new float[g_transient_record_len];
		g_transient_net_injection_flits = new int[g_transient_record_len];
		g_transient_net_injection_misrouted_flits = new int[g_transient_record_len];

		for (i = 0; i < g_transient_record_len; i++) {
			g_transient_record_latency[i] = 0;
			g_transient_record_injection_latency[i] = 0;
			g_transient_record_flits[i] = 0;
			g_transient_record_misrouted_flits[i] = 0;
			g_transient_net_injection_latency[i] = 0;
			g_transient_net_injection_inj_latency[i] = 0;
			g_transient_net_injection_flits[i] = 0;
			g_transient_net_injection_misrouted_flits[i] = 0;
		}
	}

	//initialize trace-related variables
	if (g_traffic == TRACE) {
		for (i = 0; i < g_num_traces; i++) {
			/* Initiate trace_ended map */
			g_trace_ended.push_back(vector<bool>(g_trace_instances[i], false));
			assert(g_trace_ended[i].size() == g_trace_instances[i]); // Sanity check
			/* Initiate event_deadlock and trace_end_cycle vectors: one entry per every trace instance */
			g_event_deadlock.push_back(vector<long>(g_trace_instances[i], 0));
			assert(g_event_deadlock[i].size() == g_trace_instances[i]); // Sanity check
			g_trace_end_cycle.push_back(vector<long>(g_trace_instances[i], -1));
			assert(g_trace_end_cycle[i].size() == g_trace_instances[i]); // Sanity check
		}
	}
}

void action() {
	int i, j, print_cycle, totalSwitchSpace, totalSwitchFreeSpace, flitWaitingCount;
	bool trace_ended;

	totalSwitchSpace = 0;
	totalSwitchFreeSpace = 0;
	flitWaitingCount = 0;
	g_warmup_flit_latency = 0;
	g_warmup_packet_latency = 0;
	g_warmup_injection_latency = 0;

	/* WARMUP execution [only for synthetic traffic] */
	if (g_traffic != TRACE && g_traffic != GRAPH500) {
		for (g_cycle = 0; g_cycle < g_warmup_cycles; g_cycle++) {
			if (g_switch_type == BASE_SW) g_internal_cycle = g_cycle;
			for (i = 0; i < g_number_switches; i++) {
				if (g_congestion_management == ECM) g_switches_list[i]->escapeCongested();
				assert(g_switches_list[i]->messagesInQueuesCounter >= 0);
				if (g_switches_list[i]->messagesInQueuesCounter >= 1) g_switches_list[i]->action();
			}
			for (i = 0; i < g_number_generators; i++) {
				g_generators_list[i]->action();
			}
			if (g_cycle % g_print_cycles == 0) {
				cout << "cycle:" << setfill(' ') << setw(8) << g_cycle << "\tMessages sent:" << setfill(' ') << setw(12)
						<< g_tx_flit_counter << "\tMessages received:" << setfill(' ') << setw(12) << g_rx_flit_counter
						<< endl;
			}
		}
	}

	/* Reset hop and contention counters */
	g_total_hop_counter = 0;
	g_local_hop_counter = 0;
	g_global_hop_counter = 0;
	g_local_ring_hop_counter = 0;
	g_global_ring_hop_counter = 0;
	g_local_tree_hop_counter = 0;
	g_global_tree_hop_counter = 0;
	g_local_contention_counter = 0;
	g_global_contention_counter = 0;
	g_local_escape_contention_counter = 0;
	g_global_escape_contention_counter = 0;
	for (i = 0; i < g_ports; i++) {
		g_port_usage_counter[i] = 0;
		g_port_contention_counter[i] = 0;
	}
	for (i = 0; i < (g_a_routers_per_group - 1); i++) {
		for (int j = 0; j < g_local_link_channels; j++)
			g_vc_counter[i][j] = 0;
	}
	for (i = 0; i < g_h_global_ports_per_router; i++) {
		for (int j = 0; j < g_global_link_channels; j++)
			g_vc_counter[i][j] = 0;
	}

	for (i = 0; i < g_number_switches; i++) {
		totalSwitchSpace = g_switches_list[i]->getTotalCapacity();
		totalSwitchFreeSpace = g_switches_list[i]->getTotalFreeSpace();
		flitWaitingCount += totalSwitchSpace - totalSwitchFreeSpace;
		g_switches_list[i]->resetQueueOccupancy();
	}
	g_tx_warmup_flit_counter = g_tx_flit_counter;
	g_rx_warmup_flit_counter = g_rx_flit_counter;
	g_tx_warmup_packet_counter = g_tx_packet_counter;
	g_rx_warmup_packet_counter = g_rx_packet_counter;
	g_warmup_flit_latency = g_flit_latency;
	g_warmup_packet_latency = g_packet_latency;
	g_warmup_injection_latency = g_injection_queue_latency;
	g_response_warmup_counter = g_response_counter;
	g_nonminimal_warmup_counter = g_nonminimal_counter;
	g_nonminimal_warmup_inj = g_nonminimal_inj;
	g_nonminimal_warmup_src = g_nonminimal_src;
	g_nonminimal_warmup_int = g_nonminimal_int;
	cout << "=== Cycle:" << g_cycle << "\tWarmup Flits Sent: " << setw(12) << g_tx_warmup_flit_counter
			<< "\tWarmup Flits Received: " << setw(12) << g_rx_warmup_flit_counter << " ===" << endl;

	/* Reset petition statistics */
	g_petitions = 0;
	g_served_petitions = 0;
	g_injection_petitions = 0;
	g_served_injection_petitions = 0;

	/* Simulation AFTER warmup */
	if (g_traffic != TRACE && g_traffic != GRAPH500) {
		for (; g_cycle < (g_max_cycles + g_warmup_cycles); g_cycle++) {
			if (g_switch_type == BASE_SW) g_internal_cycle = g_cycle;
			for (i = 0; i < g_number_switches; i++) {
				if (g_congestion_management == ECM) g_switches_list[i]->escapeCongested();
				assert(g_switches_list[i]->messagesInQueuesCounter >= 0);
				if (g_switches_list[i]->messagesInQueuesCounter >= 1) {
					g_switches_list[i]->action();
				}
			}
			for (i = 0; i < g_number_generators; i++) {
				if (!g_generators_list[i]->switchM->escapeNetworkCongested) g_generators_list[i]->action();
			}
			if (g_cycle % g_print_cycles == 0) {
				cout << "cycle:" << setfill(' ') << setw(8) << g_cycle << "\tMessages sent:" << setfill(' ') << setw(12)
						<< g_tx_flit_counter << "\tMessages received:" << setfill(' ') << setw(12) << g_rx_flit_counter
						<< endl;
			}

			/* BURST and ALL2ALL patterns end when all messages have been received */
			if (g_traffic == SINGLE_BURST && g_burst_generators_finished_count >= g_number_generators) break;
			if (g_traffic == ALL2ALL && g_AllToAll_generators_finished_count >= g_number_generators) break;
		}
	} else if (g_traffic == GRAPH500) { // Graph500 Traffic Model
		bool graph_model_ended = false;
		bool graph_all_level_end;

		for (g_cycle = 0; !graph_model_ended; g_cycle++) {
			if (g_switch_type == BASE_SW) g_internal_cycle = g_cycle;
			// Switches action
			for (i = 0; i < g_number_switches; i++) {
				assert(g_switches_list[i]->messagesInQueuesCounter >= 0);
				if (g_switches_list[i]->messagesInQueuesCounter >= 1) g_switches_list[i]->action();
			}
			// Compute nodes action
			for (i = 0; i < g_number_generators; i++)
				g_generators_list[i]->action();
			// Print information
			if (g_cycle % g_print_cycles == 0)
				cout << "cycle:" << setfill(' ') << setw(8) << g_cycle << "\tMessages sent:" << setfill(' ') << setw(12)
						<< g_tx_flit_counter << "\tMessages received:" << setfill(' ') << setw(12) << g_rx_flit_counter
						<< endl;
			/* For each instance, check if all compute nodes are in LEVELEND state. End instance if remaining
			 * queries are lower than 10% of total queries; increase tree level and start new level computation
			 * otherwise. End simulation when all instances have finished. */
			graph_model_ended = true;
			for (i = 0; i < g_trace_instances[0]; i++) {
				graph_all_level_end = true;
				for (j = 0; j < g_trace_nodes[0] && graph_all_level_end; j++)
					graph_all_level_end =
							((graph500Generator *) g_generators_list[g_trace_2_gen_map[0][j][i]])->getState()
									== GraphCNState::LEVELEND;
				// Evaluate end simulation or increase level
				if (!graph_all_level_end)
					graph_model_ended = false;
				else {
					if (not (g_graph_queries_remain[i] <= 0 || (g_graph_tree_level[i] + 1) >= g_graph_max_levels
							|| (g_graph_tree_level[i] >= 5
									&& g_graph_queries_remain[i]
											<= 0.1
													* (pow(2, g_graph_scale + 1) * g_graph_edgefactor
															* ((g_trace_nodes[0] - 1.0) / g_trace_nodes[0]))))) {
						g_graph_tree_level[i]++;
						for (j = 0; j < g_trace_nodes[0]; j++)
							((graph500Generator *) g_generators_list[g_trace_2_gen_map[0][j][i]])->setState(
									GraphCNState::LEVELSTART);
						graph_model_ended = false;
					}
				}
			}
		}
	} else { //  TRACE TRAFFIC
		g_warmup_cycles = 0;
		bool go_on = true;
		int gen;
		long l, min_length = LONG_MAX;
		event e;
		for (g_cycle = 0; true; g_cycle++) {
			bool normal = true;
			if (g_switch_type == BASE_SW) g_internal_cycle = g_cycle;
			if (g_tx_flit_counter == g_rx_flit_counter && go_on) {
				for (gen = 0; gen < g_number_generators; gen++) {
					l = g_generators_list[gen]->numSkipCycles();
					if (l < 0) {
						continue;
					} else if (l < min_length) {
						min_length = l;
						if (min_length == 0) break;
						/* Determine which trace instance is generator assigned to. */
						TraceNodeId trace_node = g_gen_2_trace_map[gen];
						for (j = 0; j < g_trace_instances[trace_node.trace_id]; j++) {
							if (g_trace_2_gen_map[trace_node.trace_id][trace_node.trace_node][j] == gen) break;
						}
						assert(j < g_trace_instances[trace_node.trace_id]);
						g_event_deadlock[trace_node.trace_id][j] = 0;
						assert(g_trace_end_cycle[0].size() == g_trace_instances[0]);
					}
				}
				if (min_length > 1 && min_length != LONG_MAX) {
					normal = false;
					cout << "cycle:" << g_cycle << " skipping " << min_length << " cycles" << endl;
					min_length--;
					g_cycle += min_length - 1;
					for (gen = 0; gen < g_number_generators; gen++) {
						g_generators_list[gen]->consumeCycles(min_length);
					}
				}
			}

			if (normal) {
				for (i = 0; i < g_event_deadlock.size(); i++) {
					for (j = 0; j < g_event_deadlock[i].size(); j++) {
						if (g_event_deadlock[i][j] > 2000000) {
							cerr << "EVENT DEADLOCK detected at cycle " << g_cycle << " in trace " << i << ", instance "
									<< j << endl;
							for (int k = 0; k < g_trace_nodes[i]; k++) {
								gen = g_trace_2_gen_map[i][k][j];
								cerr << "Node " << k << " -> gen " << gen << endl;
								g_generators_list[gen]->printHeadEvent();
							}
							for (int k = 0; k < g_number_generators; k++) {
								g_generators_list[k]->printHeadEvent();
							}
							cerr << "EVENT DEADLOCK!!" << endl;
							assert(0);
						}
					}
				}

				if (go_on == false) {
					break;
				}
				for (i = 0; i < g_event_deadlock.size(); i++) {
					for (j = 0; j < g_event_deadlock[i].size(); j++) {
						g_event_deadlock[i][j]++;
					}
				}
				for (i = 0; i < g_number_switches; i++) {

					assert(g_switches_list[i]->messagesInQueuesCounter >= 0);
					if (g_switches_list[i]->messagesInQueuesCounter >= 1) {
						g_switches_list[i]->action();
					} else {
						if (g_routing == PB || g_routing == PB_ANY || g_routing == SRC_ADP) {
							g_switches_list[i]->updateReadPb();
						}
						if (g_contention_aware) {
							g_switches_list[i]->m_ca_handler.readIncomingCAFlits();
						}
					}
				}
				for (i = 0; i < g_number_generators; i++) {
					g_generators_list[i]->action();
				}

				/* Determine if trace instance has ended, checking every generator
				 * associated with it. If trace has finished, update end status
				 * and reload trace. If all traces have finished, stop simulation. */
				go_on = false;
				for (i = 0; i < g_num_traces; i++) {
					for (j = 0; j < g_trace_instances[i]; j++) {
						trace_ended = true;
						for (int k = 0; k < g_trace_nodes[i]; k++) {
							assert(g_trace_2_gen_map[i][k][j] < g_number_generators); /* Sanity check: number of trace nodes has to be below number of available generators */
							if (!g_generators_list[g_trace_2_gen_map[i][k][j]]->isGenerationEnded()) {
								trace_ended = false;
								break;
							}
						}
						if (trace_ended) {
							if (!g_trace_ended[i][j]) g_trace_end_cycle[i][j] = g_cycle; // Only track first time every trace instance ends
							g_trace_ended[i][j] = trace_ended; // Update trace instance status
							cout << "Trace id " << i << ", instance " << j << " has ended on cycle " << g_cycle << endl;
							vector<int> aux_vector(1, j); // Trace instance has to be introduced as a vector
							read_trace(i, aux_vector); // Refill trace event queue
						}
					}
				}
				trace_ended = true;
				for (i = 0; i < g_num_traces; i++) {
					assert(g_trace_ended[i].size() == g_trace_instances[i]); // Sanity check
					for (j = 0; j < g_trace_instances[i]; j++) {
						trace_ended &= g_trace_ended[i][j];
					}
				}
				/* All trace instances have finished once or more times: simulation must be ended at this point */
				go_on = trace_ended ? false : true;

				if (g_cycle % g_print_cycles == 0) {
					cout << "cycle:" << g_cycle << "   Messages sent:" << g_tx_flit_counter << "   Messages received:"
							<< g_rx_flit_counter << endl;
				}
			}
		}
	}

	flitWaitingCount = 0;
	/* stats */
	for (i = 0; i < g_number_switches; i++) {
		totalSwitchSpace = g_switches_list[i]->getTotalCapacity();
		totalSwitchFreeSpace = g_switches_list[i]->getTotalFreeSpace();
		flitWaitingCount = flitWaitingCount + (totalSwitchSpace - totalSwitchFreeSpace);
	}
}

void writeOutput() {
	float IQO[g_channels], GRQO[g_channels], LRQO[g_channels], GTQO[g_channels], LTQO[g_channels], GQO[g_channels],
			LQO[g_channels], OQO = 0;
	int i;

	/* Open output file */
	g_output_file.open(g_output_file_name, ios::out);
	if (!g_output_file) {
		cerr << "Can't open the output file" << g_output_file_name << endl;
		exit(-1);
	}

	for (int vc = 0; vc < g_channels; vc++) {
		LQO[vc] = 0;
		GQO[vc] = 0;
		IQO[vc] = 0;
		LRQO[vc] = 0;
		GRQO[vc] = 0;
		LTQO[vc] = 0;
		GTQO[vc] = 0;
	}

	if (g_reactive_traffic) {
		g_injection_probability *= 2;
		for (int i = 0; i < g_number_generators; i++) {
			g_generators_list[i]->sum_injection_probability *= 2;
		}
		switch (g_traffic) {
			case UN:
				g_traffic = UN_RCTV;
				break;
			case ADV_RANDOM_NODE:
				g_traffic = ADV_RANDOM_NODE_RCTV;
				break;
			case BURSTY_UN:
				g_traffic = BURSTY_UN_RCTV;
				break;
		}
	}

	g_output_file << "PARAMETERS" << endl;
	g_output_file << "Total Number Of Cycles: " << g_cycle << endl;
	g_output_file << "Max Cycles: " << g_max_cycles << endl;
	g_output_file << "H: " << g_h_global_ports_per_router << endl;
	g_output_file << "P: " << g_p_computing_nodes_per_router << endl;
	g_output_file << "A: " << g_a_routers_per_group << endl;

	switch (g_switch_type) {
		case BASE_SW:
			g_output_file << "Switch: BASE" << endl;
			break;
		case IOQ_SW:
			g_output_file << "Switch: IOQ" << endl;
			g_output_file << "Xbar Delay: " << g_xbar_delay << endl;
			g_output_file << "Out Queue Length: " << g_out_queue_length << endl;
			g_output_file << "Internal SpeedUp: " << g_internal_speedup << endl;
			g_output_file << "Number Segregated Traffic Flows: " << g_segregated_flows << endl;
			break;
	}

	switch (g_input_arbiter_type) {
		case LRS:
			g_output_file << "InputArbiter: LRS" << endl;
			break;
		case PrioLRS:
			g_output_file << "InputArbiter: PrioLRS (priority of transit over injection)" << endl;
			break;
		case RR:
			g_output_file << "InputArbiter: RoundRobin" << endl;
			break;
		case PrioRR:
			g_output_file << "InputArbiter: PrioRoundRobin (priority of transit over injection)" << endl;
			break;
		case AGE:
			g_output_file << "InputArbiter: AgeArbiter" << endl;
			break;
		case PrioAGE:
			g_output_file << "InputArbiter: PrioAgeArbiter (priority of transit over injection)" << endl;
			break;
	}
	switch (g_output_arbiter_type) {
		case LRS:
			g_output_file << "OutputArbiter: LRS" << endl;
			break;
		case PrioLRS:
			g_output_file << "OutputArbiter: PrioLRS" << endl;
			break;
		case RR:
			g_output_file << "OutputArbiter: RoundRobin" << endl;
			break;
		case PrioRR:
			g_output_file << "OutputArbiter: PrioRoundRobin" << endl;
			break;
		case AGE:
			g_output_file << "OutputArbiter: AgeArbiter" << endl;
			break;
		case PrioAGE:
			g_output_file << "OutputArbiter: PrioAgeArbiter" << endl;
			break;
	}
	g_output_file << "Arbiter Iterations: " << g_allocator_iterations << endl;
	g_output_file << "Injection Delay: " << g_injection_delay << endl;
	g_output_file << "Local Link Delay: " << g_local_link_transmission_delay << endl;
	g_output_file << "Global Link Delay: " << g_global_link_transmission_delay << endl;
	g_output_file << "Packet Size: " << g_packet_size << " phits" << endl;
	g_output_file << "Flit Size: " << g_flit_size << " phits" << endl;
	g_output_file << "Packet Flit Size: " << g_flits_per_packet << " flits" << endl;
	g_output_file << "Generator Queue Length: " << g_injection_queue_length << endl;
	switch (g_buffer_type) {
		case SEPARATED:
			g_output_file << "Buffer: SEPARATED" << endl;
			g_output_file << "Local Queue Length: " << g_local_queue_length << " (phits per vc)" << endl;
			g_output_file << "Global Queue Length: " << g_global_queue_length << " (phits per vc)" << endl;
			g_output_file << "Total Local Buffer: " << g_local_queue_length * g_local_link_channels
					<< " (phits per port)" << endl;
			g_output_file << "Total Global Buffer: " << g_global_queue_length * g_global_link_channels
					<< " (phits per port)" << endl;
			break;
		case DYNAMIC:
			g_output_file << "Buffer: DYNAMIC" << endl;
			g_output_file << "Local Queue Length: " << g_local_queue_length << " (phits per port)" << endl;
			g_output_file << "Global Queue Length: " << g_global_queue_length << " (phits per port)" << endl;
			g_output_file << "Reserved Local Queue: " << g_local_queue_reserved << " (pkts) " << endl;
			g_output_file << "Reserved Global Queue: " << g_global_queue_reserved << " (pkts) " << endl;
			g_output_file << "Total Local Buffer: " << g_local_queue_length << " (phits per port)" << endl;
			g_output_file << "Total Global Buffer: " << g_global_queue_length << " (phits per port)" << endl;
			break;
		default:
			assert(0);
			break;
	}
	g_output_file << "VCs: " << g_channels << endl;
	g_output_file << "Injection VCs: " << g_injection_channels << endl;
	g_output_file << "Local Link VCs: " << g_local_link_channels << endl;
	g_output_file << "Global Link VCs: " << g_global_link_channels << endl;
	g_output_file << "Seed: " << g_seed << endl;
	g_output_file << "Palm Tree Configuration: " << g_palm_tree_configuration << endl << endl << endl;
	g_output_file << "Latency Histogram Max Lat: " << g_latency_histogram_maxLat << endl;
	g_output_file << "Hops Histogram Max Hops: " << g_hops_histogram_maxHops << endl;
	if (g_local_arbiter_speedup > 0) g_output_file << "Input Speedup: " << g_local_arbiter_speedup << endl;
	if (g_issue_parallel_reqs) g_output_file << "Parallel Req Issuing: 1" << endl;
	if (g_cos_levels > 1) g_output_file << "CoS Levels: " << g_cos_levels << endl;

	//Traffic pattern employed
	g_output_file << "Injection Probability: " << g_injection_probability << endl;

	switch (g_traffic) {
		case UN:
			g_output_file << "Traffic: UN" << endl;
			break;
		case ADV:
			g_output_file << "Traffic: ADV" << endl;
			g_output_file << "Adversarial Traffic Distance: " << g_adv_traffic_distance << endl;
			break;
		case ADV_RANDOM_NODE:
			g_output_file << "Traffic: ADV_RANDOM_NODE" << endl;
			g_output_file << "Adversarial Traffic Distance: " << g_adv_traffic_distance << endl;
			break;
		case ADV_LOCAL:
			g_output_file << "Traffic: ADV_LOCAL" << endl;
			g_output_file << "Adversarial Traffic Distance: " << g_adv_traffic_distance << endl;
			break;
		case ADVc:
			g_output_file << "Traffic: ADVc" << endl;
			break;
		case ALL2ALL:
			g_output_file << "Traffic: ALL2ALL" << endl;
			if (g_random_AllToAll)
				g_output_file << "All2All Random" << endl;
			else
				g_output_file << "All2All Naive" << endl;
			g_output_file << "All2All Length: " << (g_number_generators - 1) * g_phases << " packets" << endl;
			g_output_file << "All2All Length: " << (g_number_generators - 1) * g_phases * g_flits_per_packet << " flits"
					<< endl;
			g_output_file << "All2All Phases: " << g_phases << endl;
			g_output_file << "All2All End Cycle: " << g_cycle << endl;
			assert(g_cycle < g_max_cycles); // If assert fails, it means simulation was unable to complete phase
			break;
		case MIX:
			g_output_file << "Traffic: MIX" << endl;
			g_output_file << "Adversarial Traffic Distance: " << g_adv_traffic_distance << endl;
			g_output_file << "Adversarial Local Traffic Distance: " << g_adv_traffic_local_distance << endl;
			g_output_file << "Random Traffic Percent: " << g_phase_traffic_percent[0] << endl;
			g_output_file << "Adv Local Traffic Percent: " << g_phase_traffic_percent[1] << endl;
			g_output_file << "Adv Global Traffic Percent: " << g_phase_traffic_percent[2] << endl;
			break;
		case SINGLE_BURST:
			g_output_file << "Traffic: SINGLE_BURST" << endl;
			g_output_file << "Burst Length: " << (g_single_burst_length / g_flits_per_packet) << " packets" << endl;
			g_output_file << "Burst Length: " << g_single_burst_length << " flits" << endl;
			for (i = 0; i < 3; i++) {
				switch (g_phase_traffic_type[i]) {
					case UN:
						g_output_file << "Traffic Type " << i << ": UN" << endl;
						break;
					case ADV:
						g_output_file << "Traffic Type " << i << ": ADV" << endl;
						g_output_file << "Traffic " << i << " Adv Dist: " << g_phase_traffic_adv_dist[i] << endl;
						break;
					case ADV_RANDOM_NODE:
						g_output_file << "Traffic Type " << i << ": ADV_RANDOM_NODE" << endl;
						g_output_file << "Traffic " << i << " Adv Dist: " << g_phase_traffic_adv_dist[i] << endl;
						break;
					default:
						g_output_file << "Traffic Type " << i << ": " << g_phase_traffic_type[i] << endl;
						g_output_file << "Traffic " << i << " Adv Dist: " << g_phase_traffic_adv_dist[i] << endl;
				}
				g_output_file << "Type " << i << " Percent: " << g_phase_traffic_percent[i] << endl;
			}
			g_output_file << "Burst End Cycle: " << g_cycle << endl;
			assert(g_cycle < g_max_cycles); // If assert fails, it means simulation was unable to complete burst delivery
			break;
		case TRANSIENT:
			g_output_file << "Traffic: TRANSIENT" << endl;
			g_output_file << "Transient First Pattern: " << g_phase_traffic_type[0] << endl;
			g_output_file << "First Pattern Adv Distance: " << g_phase_traffic_adv_dist[0];
			g_output_file << "First Pattern End Cycle: " << g_transient_traffic_cycle << endl;
			g_output_file << "Transient Second Pattern: " << g_phase_traffic_type[1] << endl;
			g_output_file << "Second Pattern Adv Distance: " << g_phase_traffic_adv_dist[1] << endl;
			g_output_file << "Transient Record Cycles: " << g_transient_record_num_cycles << endl;
			g_output_file << "Transient Record Prev Cycles: " << g_transient_record_num_prev_cycles << endl;
			break;
		case BURSTY_UN:
			g_output_file << "Traffic: BURSTY_UN" << endl;
			g_output_file << "Avg burst length: " << g_bursty_avg_length << endl;
			g_output_file << "Effective avg burst length: "
					<< (double) g_injected_packet_counter / g_injected_bursts_counter << endl;
			break;
		case TRACE:
			g_output_file << "Traffic: TRACE" << endl;
			for (i = 0; i < g_num_traces; i++) {
				g_output_file << "Trace " << i << " filename: " << g_trace_file[i] << endl;
			}
			for (i = 0; i < g_num_traces; i++) {
				for (int j = 0; j < g_trace_instances[i]; j++) {
					assert(g_trace_end_cycle[i][j] >= 0);
					g_output_file << "Trace " << i << " instance " << j << " End Cycle: " << g_trace_end_cycle[i][j]
							<< endl;
				}
			}
			break;
		case UN_RCTV:
			g_output_file << "Traffic: UN_RCTV" << endl;
			g_output_file << "Max Petitions On Flight: " << g_max_petitions_on_flight << endl;
			break;
		case ADV_RANDOM_NODE_RCTV:
			g_output_file << "Traffic: ADV_RANDOM_NODE_RCTV" << endl;
			g_output_file << "Adversarial Traffic Distance: " << g_adv_traffic_distance << endl;
			g_output_file << "Max Petitions On Flight: " << g_max_petitions_on_flight << endl;
			break;
		case BURSTY_UN_RCTV:
			g_output_file << "Traffic: BURSTY_UN_RCTV" << endl;
			g_output_file << "Avg burst length: " << g_bursty_avg_length << endl;
			g_output_file << "Effective avg burst length: "
					<< (double) g_injected_packet_counter / g_injected_bursts_counter << endl;
			g_output_file << "Max Petitions On Flight: " << g_max_petitions_on_flight << endl;
			break;
		case GRAPH500:
			g_output_file << "Traffic: GRAPH500" << endl;
			g_output_file << "Coalescing Size: " << g_graph_coalescing_size << endl;
			g_output_file << "Query Time: " << g_graph_query_time << endl;
			if (g_graph_nodes_cap_mod.size() > 0) {
				g_output_file << "Nodes Capabilities Modified: ";
				for (int i = 0; i < g_graph_nodes_cap_mod.size(); i++)
					g_output_file << g_graph_nodes_cap_mod[i] << " ";
				g_output_file << endl;
				g_output_file << "Capabilities Modified Factor: " << g_graph_cap_mod_factor << endl;
				g_output_file << "Injection Probability Modified: "
						<< g_injection_probability * (g_graph_cap_mod_factor / 100.0) << endl;
				g_output_file << "Query Time Modified: " << g_graph_query_time / (g_graph_cap_mod_factor / 100.0)
						<< endl;
			}
			g_output_file << "Scale: " << g_graph_scale << endl;
			g_output_file << "Edgefactor: " << g_graph_edgefactor << endl;
			g_output_file << "Root Node: " << g_graph_root_node[0];
			for (int i = 1; i < g_trace_instances[0]; i++)
				g_output_file << ", " << g_graph_root_node[i];
			g_output_file << endl;
			g_output_file << "Root Degree: " << g_graph_root_degree[0];
			for (int i = 1; i < g_trace_instances[0]; i++)
				g_output_file << ", " << g_graph_root_degree[i];
			g_output_file << endl;
			g_output_file << "Parallel Instances: " << g_trace_instances[0] << endl;
			g_output_file << "Processes: " << g_trace_nodes[0] << endl;
			g_output_file << "P2P Queries sent: "
					<< pow(2, g_graph_scale + 1) * g_graph_edgefactor * ((g_trace_nodes[0] - 1.0) / g_trace_nodes[0])
							- g_graph_queries_remain[0] << " ("
					<< 100
							* (1
									- g_graph_queries_remain[0]
											/ (pow(2, g_graph_scale + 1) * g_graph_edgefactor * (g_trace_nodes[0] - 1.0)
													/ g_trace_nodes[0])) << "%)";
			for (int i = 1; i < g_trace_instances[0]; i++)
				g_output_file << ", "
						<< pow(2, g_graph_scale + 1) * g_graph_edgefactor
								* ((g_trace_nodes[0] - 1.0) / g_trace_nodes[0]) - g_graph_queries_remain[i] << " ("
						<< 100
								* (1
										- g_graph_queries_remain[i]
												/ (pow(2, g_graph_scale + 1) * g_graph_edgefactor
														* (g_trace_nodes[0] - 1.0) / g_trace_nodes[0])) << "%)";
			g_output_file << endl;
			g_output_file << "P2P Messages sent: " << g_graph_p2pmess << endl;
			g_output_file << "Levels: " << g_graph_tree_level[0] + 1;
			for (int i = 1; i < g_trace_instances[0]; i++)
				g_output_file << ", " << g_graph_tree_level[i];
			g_output_file << endl;
			break;
		default:
			g_output_file << "Traffic: UNKNOWN!!!! (" << g_traffic << ")" << endl;
	}
	g_output_file << endl;

	switch (g_deadlock_avoidance) {
		case DALLY:
			g_output_file << "Deadlock Avoidance Mechanism: Dally" << endl;
			break;
		case EMBEDDED_TREE:
			g_output_file << "Deadlock Avoidance Mechanism: Embedded Tree" << endl;
			g_output_file << "VCs Tree: " << g_channels_escape << endl;
			break;
		case EMBEDDED_RING:
			g_output_file << "Deadlock Avoidance Mechanism: Embedded Ring" << endl;
			g_output_file << "VCs Ring: " << g_channels_escape << endl;
			g_output_file << "Rings: " << g_rings << endl;
			g_output_file << "Ring Dirs: " << g_ringDirs << endl;
			g_output_file << "Ring Injection Bubble: " << g_ring_injection_bubble << endl;
			g_output_file << "Forbid Injection To Ring: " << g_forbid_from_inj_queues_to_ring << endl;
			break;
		case RING:
			g_output_file << "Deadlock Avoidance Mechanism: Ring" << endl;
			g_output_file << "Rings: " << g_rings << endl;
			g_output_file << "Ring Dirs: " << g_ringDirs << endl;
			g_output_file << "Ring Injection Bubble: " << g_ring_injection_bubble << endl;
			g_output_file << "onlyRing2: " << g_onlyRing2 << endl;
			g_output_file << "Forbid Injection To Ring: " << g_forbid_from_inj_queues_to_ring << endl;
			break;
	}
	if (g_deadlock_avoidance == EMBEDDED_TREE || g_deadlock_avoidance == EMBEDDED_RING
			|| g_deadlock_avoidance == RING) {
		switch (g_congestion_management) {
			case BCM:
				g_output_file << "Congestion Management: Base" << endl;
				g_output_file << "Congestion Bubble: " << g_baseCongestionControl_bub << endl;
				break;
			case ECM:
				g_output_file << "Congestion Management: Escape" << endl;
				g_output_file << "Congestion Threshold: " << g_escapeCongestion_th << endl;
				break;
		}
	}

	switch (g_routing) {
		case MIN:
			g_output_file << "Routing: MIN" << endl;
			break;
		case MIN_COND:
			g_output_file << "Routing: MIN_COND" << endl;
			break;
		case VAL:
			g_output_file << "Routing: VAL" << endl;
			break;
		case VAL_ANY:
			g_output_file << "Routing: VAL_ANY" << endl;
			break;
		case OBL:
			g_output_file << "Routing: OBL" << endl;
			g_output_file << "Reset VAL node: " << g_reset_val << endl;
			break;
		case SRC_ADP:
			g_output_file << "Routing: SRC_ADP" << endl;
			g_output_file << "Reset VAL node: " << g_reset_val << endl;
			g_output_file << "Piggybacking Coef: " << g_piggyback_coef << endl;
			g_output_file << "UGAL Local Threshold: " << g_ugal_local_threshold << endl;
			g_output_file << "UGAL Global Threshold: " << g_ugal_global_threshold << endl;
			break;
		case PAR:
			g_output_file << "Routing: PAR" << endl;
			break;
		case UGAL:
			g_output_file << "Routing: UGAL" << endl;
			g_output_file << "UGAL Local Threshold: " << g_ugal_local_threshold << endl;
			g_output_file << "UGAL Global Threshold: " << g_ugal_global_threshold << endl;
			break;
		case PB:
			g_output_file << "Routing: PB" << endl;
			g_output_file << "Piggybacking Coef: " << g_piggyback_coef << endl;
			g_output_file << "UGAL Local Threshold: " << g_ugal_local_threshold << endl;
			g_output_file << "UGAL Global Threshold: " << g_ugal_global_threshold << endl;
			break;
		case PB_ANY:
			g_output_file << "Routing: PB_ANY" << endl;
			g_output_file << "Piggybacking Coef: " << g_piggyback_coef << endl;
			g_output_file << "UGAL Local Threshold: " << g_ugal_local_threshold << endl;
			g_output_file << "UGAL Global Threshold: " << g_ugal_global_threshold << endl;
			break;
		case RLM:
			g_output_file << "Routing: RLM" << endl;
			g_output_file << "Local Threshold Percent: " << g_percent_local_threshold << endl;
			g_output_file << "Global Threshold Percent: " << g_percent_global_threshold << endl;
			g_output_file << "Threshold Min: " << g_th_min << endl;
			break;
		case OLM:
			g_output_file << "Routing: OLM" << endl;
			break;
		case OFAR:
			g_output_file << "Routing: OFAR" << endl;
			g_output_file << "Local Threshold Percent: " << g_percent_local_threshold << endl;
			g_output_file << "Global Threshold Percent: " << g_percent_global_threshold << endl;
			g_output_file << "Threshold Min: " << g_th_min << endl;
			g_output_file << "Restrict Local Cycles: " << g_restrictLocalCycles << endl;
			break;
	}

	switch (g_vc_usage) {
		case BASE:
			g_output_file << "VC Usage: BASE" << endl;
			break;
		case FLEXIBLE:
			g_output_file << "VC Usage: FLEXIBLE" << endl;
			switch (g_vc_alloc) {
				case HIGHEST_VC:
					g_output_file << "VC Allocation: HIGHEST_VC" << endl;
					break;
				case LOWEST_VC:
					g_output_file << "VC Allocation: LOWEST_VC" << endl;
					break;
				case LOWEST_OCCUPANCY:
					g_output_file << "VC Allocation: CONGESTION_BASED" << endl;
					break;
				case RANDOM_VC:
					g_output_file << "VC Allocation: RANDOM" << endl;
					break;
			}
			break;
	}

	switch (g_routing) {
		case OBL:
		case OFAR:
		case OLM:
		case PAR:
		case RLM:
		case SRC_ADP:
			switch (g_global_misrouting) {
				case CRG:
					g_output_file << "Global Misrouting: CRG" << endl;
					break;
				case CRG_L:
					g_output_file << "Global Misrouting: CRG_L" << endl;
					break;
				case RRG:
					g_output_file << "Global Misrouting: RRG" << endl;
					break;
				case RRG_L:
					g_output_file << "Global Misrouting: RRG_L" << endl;
					break;
				case MM:
					g_output_file << "Global Misrouting: MM" << endl;
					break;
				case MM_L:
					g_output_file << "Global Misrouting: MM_L" << endl;
					break;
				default:
					g_output_file << "Global Misrouting: Unknown (" << g_global_misrouting << ")" << endl;
					break;
			}
			switch (g_congestion_detection) {
				case PER_PORT:
					g_output_file << "Congestion Detection: PER_PORT" << endl;
					break;
				case PER_VC:
					g_output_file << "Congestion Detection: PER_VC" << endl;
					break;
				case PER_PORT_MIN:
					g_output_file << "Congestion Detection: PER_PORT_MIN" << endl;
					break;
				case PER_VC_MIN:
					g_output_file << "Congestion Detection: PER_VC_MIN" << endl;
					break;
			}
	}

	switch (g_misrouting_trigger) {
		case CGA:
			g_output_file << "Misrouting Trigger: CGA" << endl;
			g_output_file << "Local Threshold Percent: " << g_percent_local_threshold << endl;
			g_output_file << "Global Threshold Percent: " << g_percent_global_threshold << endl;
			g_output_file << "Threshold Min: " << g_th_min << endl;
			g_output_file << "Congestion Misrouting Restriction: " << g_vc_misrouting_congested_restriction << endl;
			g_output_file << "Restriction Coef Percent: " << g_vc_misrouting_congested_restriction_coef_percent << endl;
			g_output_file << "Restriction Threshold: " << g_vc_misrouting_congested_restriction_th << endl;
			break;
		case HYBRID:
			g_output_file << "Misrouting Trigger: HYBRID" << endl;
			g_output_file << "Local Threshold Percent: " << g_percent_local_threshold << endl;
			g_output_file << "Global Threshold Percent: " << g_percent_global_threshold << endl;
			g_output_file << "Congestion Misrouting Restriction: " << g_vc_misrouting_congested_restriction << endl;
			g_output_file << "Restriction Coef Percent: " << g_vc_misrouting_congested_restriction_coef_percent << endl;
			g_output_file << "Restriction Threshold: " << g_vc_misrouting_congested_restriction_th << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case DUAL:
			g_output_file << "Misrouting Trigger: DUAL CA" << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case CA:
			g_output_file << "Misrouting Trigger: CA" << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			if (g_contention_aware_local_th >= 0)
				g_output_file << "Contention Local Threshold: " << g_contention_aware_local_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case WEIGTHED_CA:
			g_output_file << "Misrouting Trigger: WEIGTHED_CA" << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case FILTERED:
			g_output_file << "Misrouting Trigger: FILTERED" << endl;
			g_output_file << "Alpha coefficient: " << g_filtered_contention_regressive_coefficient << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case CA_REMOTE:
			g_output_file << "Misrouting Trigger: ECtN" << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Contention Global Threshold: " << g_contention_aware_global_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			break;
		case HYBRID_REMOTE:
			g_output_file << "Misrouting Trigger: HYBRID ECtN" << endl;
			g_output_file << "Contention Threshold: " << g_contention_aware_th << endl;
			g_output_file << "Contention Global Threshold: " << g_contention_aware_global_th << endl;
			g_output_file << "Increase Contention At Header: " << g_increaseContentionAtHeader << endl;
			g_output_file << "Local Threshold Percent: " << g_percent_local_threshold << endl;
			g_output_file << "Global Threshold Percent: " << g_percent_global_threshold << endl;
			g_output_file << "Congestion Misrouting Restriction: " << g_vc_misrouting_congested_restriction << endl;
			g_output_file << "Restriction Coef Percent: " << g_vc_misrouting_congested_restriction_coef_percent << endl;
			g_output_file << "Restriction Threshold: " << g_vc_misrouting_congested_restriction_th << endl;
			break;
	}

	g_output_file << "Force Misrouting: " << g_forceMisrouting << endl;
	g_output_file << endl << endl;

	/* Simulation statistics */
	g_output_file << "STATISTICS" << endl;
	g_output_file << "Phits Sent: " << (g_tx_flit_counter - g_tx_warmup_flit_counter) * g_flit_size << " (warmup: "
			<< g_tx_warmup_flit_counter * g_flit_size << ")" << endl;
	g_output_file << "Flits Sent: " << (g_tx_flit_counter - g_tx_warmup_flit_counter) << " (warmup: "
			<< g_tx_warmup_flit_counter << ")" << endl;
	g_output_file << "Flits Received: " << (g_rx_flit_counter - g_rx_warmup_flit_counter) << " (warmup: "
			<< g_rx_warmup_flit_counter << ")" << endl;
	long receivedFlitCount = g_rx_flit_counter - g_rx_warmup_flit_counter;
	long receivedPacketCount = g_rx_packet_counter - g_rx_warmup_packet_counter;
	g_output_file << "Packets Received: " << receivedPacketCount << endl;
	if (g_reactive_traffic)
		g_output_file << "Responses Received: " << (g_response_counter - g_response_warmup_counter) << endl;
	g_output_file << "Non Minimally Routed Packets Received: " << (g_nonminimal_counter - g_nonminimal_warmup_counter)
			<< endl;
	g_output_file << "InjectionMisrouted Packets Received: " << (g_nonminimal_inj - g_nonminimal_warmup_inj) << endl;
	g_output_file << "SrcGroupMisrouted Packets Received: " << (g_nonminimal_src - g_nonminimal_warmup_src) << endl;
	g_output_file << "IntGroupMisrouted Packets Received: " << (g_nonminimal_int - g_nonminimal_warmup_int) << endl;
	for (i = 0; i < g_allocator_iterations; i++) {
		g_output_file << "Flits Misrouted Iter " << i << ": "
				<< (g_local_misrouted_flit_counter[i] + g_global_misrouted_flit_counter[i]) << " ("
				<< (g_local_misrouted_flit_counter[i] + g_global_misrouted_flit_counter[i]) * 100.0
						/ g_attended_flit_counter << "%)" << endl;
		g_output_file << "Flits Local Misrouted Iter " << i << ": " << g_local_misrouted_flit_counter[i] << " ("
				<< g_local_misrouted_flit_counter[i] * 100.0 / g_attended_flit_counter << "%)" << endl;
		g_output_file << "Flits Global Misrouted Iter " << i << ": " << g_global_misrouted_flit_counter[i] << " ("
				<< g_global_misrouted_flit_counter[i] * 100.0 / g_attended_flit_counter << "%)" << endl;
		g_output_file << "Flits Global Mandatory Misrouted Iter " << i << ": "
				<< g_global_mandatory_misrouted_flit_counter[i] << " ("
				<< g_global_mandatory_misrouted_flit_counter[i] * 100.0 / g_attended_flit_counter << "%)" << endl;
		g_output_file << "Flits MIN Routed Iter " << i << ": " << g_min_flit_counter[i] << " ("
				<< g_min_flit_counter[i] * 100.0 / g_attended_flit_counter << "%)" << endl;
	}
	g_output_file << endl;
	g_output_file << "Applied Load: "
			<< (float) (1.0 * g_tx_flit_counter - g_tx_warmup_flit_counter) * g_flit_size
					/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	g_output_file << "Accepted Load: "
			<< (float) 1.0 * receivedFlitCount * g_flit_size / (1.0 * g_number_generators * (g_cycle - g_warmup_cycles))
			<< " phits/(node·cycle)" << endl;
	if (g_reactive_traffic) {
		g_output_file << "Petition Accepted Load: "
				<< (float) 1.0 * (receivedFlitCount - g_response_counter + g_response_warmup_counter) * g_flit_size
						/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
		g_output_file << "Response Accepted Load: "
				<< (float) 1.0 * (g_response_counter - g_response_warmup_counter) * g_flit_size
						/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	}
	g_output_file << "NonMinimal Accepted Load: "
			<< (float) 1.0 * (g_nonminimal_counter - g_nonminimal_warmup_counter) * g_flit_size
					/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	g_output_file << "InjectionMisrouted Accepted Load: "
			<< (float) 1.0 * (g_nonminimal_inj - g_nonminimal_warmup_inj) * g_flit_size
					/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	g_output_file << "SrcGroupMisrouted Accepted Load: "
			<< (float) 1.0 * (g_nonminimal_src - g_nonminimal_warmup_src) * g_flit_size
					/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	g_output_file << "IntGroupMisrouted Accepted Load: "
			<< (float) 1.0 * (g_nonminimal_int - g_nonminimal_warmup_int) * g_flit_size
					/ (1.0 * g_number_generators * (g_cycle - g_warmup_cycles)) << " phits/(node·cycle)" << endl;
	g_output_file << "Cycles: " << g_cycle << " (warmup: " << g_warmup_cycles << ")" << endl;
	g_output_file << "Total Hops: " << g_total_hop_counter << endl;
	g_output_file << "Local Hops: " << g_local_hop_counter << endl;
	g_output_file << "Global Hops: " << g_global_hop_counter << endl;
	switch (g_deadlock_avoidance) {
		case EMBEDDED_TREE:
			g_output_file << "Local Tree Hops: " << g_local_tree_hop_counter << endl;
			g_output_file << "Global Tree Hops: " << g_global_tree_hop_counter << endl;
			g_output_file << "Local Contention: " << g_local_contention_counter << endl;
			g_output_file << "Global Contention: " << g_global_contention_counter << endl;
			g_output_file << "Local Tree Contention: " << g_local_escape_contention_counter << endl;
			g_output_file << "Global Tree Contention: " << g_global_escape_contention_counter << endl;
			break;
		case RING:
		case EMBEDDED_RING:
			g_output_file << "Local Ring Hops: " << g_local_ring_hop_counter << endl;
			g_output_file << "Global Ring Hops: " << g_global_ring_hop_counter << endl;
			g_output_file << "Local Contention: " << g_local_contention_counter << endl;
			g_output_file << "Global Contention: " << g_global_contention_counter << endl;
			g_output_file << "Local Ring Contention: " << g_local_escape_contention_counter << endl;
			g_output_file << "Global Ring Contention: " << g_global_escape_contention_counter << endl;
			break;
		default:
			g_output_file << "Local Contention: " << g_local_contention_counter << endl;
			g_output_file << "Global Contention: " << g_global_contention_counter << endl;
			break;
	}

	if (g_deadlock_avoidance != DALLY) {
		g_output_file << "Subnetwork Injections: " << g_subnetwork_injections_counter << endl;
		g_output_file << "Root Subnetwork Injections: " << g_root_subnetwork_injections_counter << endl;
		g_output_file << "Source Subnetwork Injections: " << g_source_subnetwork_injections_counter << endl;
		g_output_file << "Dest Subnetwork Injections: " << g_dest_subnetwork_injections_counter << endl;
	}

	g_output_file << "Average Distance: " << g_total_hop_counter * 1.0 / receivedFlitCount << endl;
	g_output_file << "Average Local Distance: " << g_local_hop_counter * 1.0 / receivedFlitCount << endl;
	g_output_file << "Average Global Distance: " << g_global_hop_counter * 1.0 / receivedFlitCount << endl;
	switch (g_deadlock_avoidance) {
		case RING:
		case EMBEDDED_RING:
			g_output_file << "Average Local Ring Distance: " << g_local_ring_hop_counter * 1.0 / receivedFlitCount
					<< endl;
			g_output_file << "Average Global Ring Distance: " << g_global_ring_hop_counter * 1.0 / receivedFlitCount
					<< endl;
			break;
		case EMBEDDED_TREE:
			g_output_file << "Average Local Tree Distance: " << g_local_tree_hop_counter * 1.0 / receivedFlitCount
					<< endl;
			g_output_file << "Average Global Tree Distance: " << g_global_tree_hop_counter * 1.0 / receivedFlitCount
					<< endl;
			break;
	}
	if (g_deadlock_avoidance != DALLY) {
		g_output_file << "Average Subnetwork Injections: " << g_subnetwork_injections_counter * 1.0 / receivedFlitCount
				<< endl;
		g_output_file << "Average Root Subnetwork Injections: "
				<< g_root_subnetwork_injections_counter * 1.0 / receivedFlitCount << endl;
		g_output_file << "Average Source Subnetwork Injections: "
				<< g_source_subnetwork_injections_counter * 1.0 / receivedFlitCount << endl;
		g_output_file << "Average Dest Subnetwork Injections: "
				<< g_dest_subnetwork_injections_counter * 1.0 / receivedFlitCount << endl;
	}
	g_output_file << "Average Local Contention: " << g_local_contention_counter / receivedFlitCount << endl;
	g_output_file << "Average Global Contention: " << g_global_contention_counter / receivedFlitCount << endl;
	switch (g_deadlock_avoidance) {
		case RING:
		case EMBEDDED_RING:
			g_output_file << "Average Local Ring Contention: " << g_local_escape_contention_counter / receivedFlitCount
					<< endl;
			g_output_file << "Average Global Ring Contention: "
					<< g_global_escape_contention_counter / receivedFlitCount << endl;
			break;
		case EMBEDDED_TREE:
			g_output_file << "Average Local Tree Contention: " << g_local_escape_contention_counter / receivedFlitCount
					<< endl;
			g_output_file << "Average Global Tree Contention: "
					<< g_global_escape_contention_counter / receivedFlitCount << endl;
			break;
	}
	g_output_file << endl;
	g_output_file << "Latency: " << (g_flit_latency - g_warmup_flit_latency) << " (warmup: " << g_warmup_flit_latency
			<< ")" << endl;
	g_output_file << "Packet latency: " << (g_packet_latency - g_warmup_packet_latency) << " (warmup: "
			<< g_warmup_packet_latency << ")" << endl;
	g_output_file << "Inj Latency: " << (g_injection_queue_latency - g_warmup_injection_latency) << " (warmup: "
			<< g_warmup_injection_latency << ")" << endl;
	g_output_file << "Base Latency: " << g_base_latency << endl;
	g_output_file << "Average Total Latency: " << (g_flit_latency - g_warmup_flit_latency) / receivedFlitCount << endl;
	for (i = 0; i < g_latency_histogram_maxLat; i++) {
		if (g_latency_histogram_no_global_misroute[i] + g_latency_histogram_global_misroute_at_injection[i]
				+ g_latency_histogram_other_global_misroute[i] > 0) {
			g_output_file << "Min Total Latency: " << i << endl;
			break;
		}
	}
	g_output_file << "Max Total Latency: " << g_latency_histogram_maxLat - 1 << endl;
	g_output_file << "Average Total Packet Latency: "
			<< (g_packet_latency - g_warmup_packet_latency) / receivedPacketCount << endl;
	g_output_file << "Average Inj Latency: "
			<< (g_injection_queue_latency - g_warmup_injection_latency) / receivedFlitCount << endl;
	g_output_file << "Average Latency: "
			<< ((g_flit_latency - g_injection_queue_latency) - (g_warmup_flit_latency - g_warmup_injection_latency))
					/ receivedFlitCount << endl;
	g_output_file << "Average Base Latency: " << g_base_latency / receivedFlitCount << endl;
	if (g_reactive_traffic)
		g_output_file << "Average Response Latency: " << g_response_latency / g_response_counter << endl;

	//Livelock control
	g_output_file << "Max Hops: " << g_max_hops << endl;
	g_output_file << "Max Local Hops: " << g_max_local_hops << endl;
	g_output_file << "Max Global Hops: " << g_max_global_hops << endl;
	switch (g_deadlock_avoidance) {
		case EMBEDDED_TREE:
			g_output_file << "Max Local Tree Hops: " << g_max_local_subnetwork_hops << endl;
			g_output_file << "Max Global Tree Hops: " << g_max_global_subnetwork_hops << endl;
			break;
		case RING:
		case EMBEDDED_RING:
			g_output_file << "Max Local Ring Hops: " << g_max_local_subnetwork_hops << endl;
			g_output_file << "Max Global Ring Hops: " << g_max_global_subnetwork_hops << endl;
			break;
		default:
			break;
	}
	if (g_deadlock_avoidance != DALLY) {
		g_output_file << "Max Subnetwork Injections: " << g_max_subnetwork_injections << endl;
		g_output_file << "Max Root Subnetwork Injections: " << g_max_root_subnetwork_injections << endl;
		g_output_file << "Max Source Subnetwork Injections: " << g_max_source_subnetwork_injections << endl;
		g_output_file << "Max Dest Subnetwork Injections: " << g_max_dest_subnetwork_injections << endl;
	}
	g_output_file << endl;

	g_output_file << "PORT COUNTERS" << endl;
	for (i = 0; i < g_p_computing_nodes_per_router; i++) {
		g_output_file << "Port" << i << " Count: " << g_port_usage_counter[i] << " ("
				<< (float) 100.0 * g_port_usage_counter[i] * g_flit_size
						/ ((g_cycle - g_warmup_cycles) * g_number_switches) << "\%)" << endl;
	}
	for (i = g_p_computing_nodes_per_router; i < g_ports; i++) {
		g_output_file << "Port" << i << " Count: " << g_port_usage_counter[i] << " ("
				<< (float) 100.0 * g_port_usage_counter[i] * g_flit_size
						/ ((g_cycle - g_warmup_cycles) * g_number_switches) << "\%)" << endl;
		g_output_file << "Port" << i << " Contention Count: " << g_port_contention_counter[i] << " ("
				<< (float) 100.0 * g_port_contention_counter[i] / g_flit_size
						/ ((g_cycle - g_warmup_cycles) * g_number_switches) << "\%)" << endl;
	}
	g_output_file << endl << "VC/PORT COUNTERS" << endl;
	g_output_file << "LOCAL LINKS" << endl;
	for (i = 0; i < (g_a_routers_per_group - 1); i++) {
		for (int j = 0; j < g_local_link_channels; j++) {
			g_output_file << "VC" << j << ", Port " << i << " Count: " << g_vc_counter[i][j] << " ("
					<< (float) 100.0 * g_vc_counter[i][j] * g_flit_size
							/ ((g_cycle - g_warmup_cycles) * g_number_switches) << "\%)" << endl;
		}
	}
	g_output_file << "GLOBAL LINKS" << endl;
	for (i = (g_a_routers_per_group - 1); i < (g_h_global_ports_per_router + g_a_routers_per_group - 1); i++) {
		for (int j = 0; j < g_global_link_channels; j++)
			g_output_file << "VC" << j << ", Port " << i << " Count: " << g_vc_counter[i][j] << " ("
					<< (float) 100.0 * g_vc_counter[i][j] * g_flit_size
							/ ((g_cycle - g_warmup_cycles) * g_number_switches) << "\%)" << endl;
	}
	g_output_file << endl;

	for (int i = 0; i < g_number_switches; i++) {
		g_switches_list[i]->setQueueOccupancy();
		for (int vc = 0; vc < g_injection_channels; vc++) {
			IQO[vc] += g_switches_list[i]->injectionQueueOccupancy[vc];
		}
		for (int vc = 0; vc < g_channels; vc++) {
			LQO[vc] += g_switches_list[i]->localQueueOccupancy[vc];
			if (g_deadlock_avoidance == EMBEDDED_TREE)
				LTQO[vc] += g_switches_list[i]->localEscapeQueueOccupancy[vc];
			else if (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == RING) LRQO[vc] +=
					g_switches_list[i]->localEscapeQueueOccupancy[vc];
		}
		for (int vc = 0; vc < g_channels; vc++) {
			GQO[vc] += g_switches_list[i]->globalQueueOccupancy[vc];
			if (g_deadlock_avoidance == EMBEDDED_TREE)
				GTQO[vc] += g_switches_list[i]->globalEscapeQueueOccupancy[vc];
			else if (g_deadlock_avoidance == EMBEDDED_RING || g_deadlock_avoidance == RING) GRQO[vc] +=
					g_switches_list[i]->globalEscapeQueueOccupancy[vc];
		}
		OQO += g_switches_list[i]->outputQueueOccupancy;
	}

	for (int vc = 0; vc < g_channels; vc++) {
		g_output_file << "VC" << vc << ":" << endl;
		g_output_file << "Injection Queues Occupancy: "
				<< IQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
				<< ((float) 100.0 * IQO[vc] / ((g_cycle - g_warmup_cycles) * g_number_switches))
						/ g_injection_queue_length << "\%)" << endl;
		switch (g_buffer_type) {
			case SEPARATED:
				g_output_file << "Local Queues Occupancy: "
						<< LQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * LQO[vc] / ((g_cycle - g_warmup_cycles) * g_number_switches))
								/ g_local_queue_length << "\%)" << endl;
				g_output_file << "Global Queues Occupancy: "
						<< GQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * GQO[vc] / ((g_cycle - g_warmup_cycles) * g_number_switches))
								/ g_global_queue_length << "\%)" << endl;
				break;
			case DYNAMIC:
				g_output_file << "Local Queues Occupancy: "
						<< LQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * LQO[vc] / ((g_cycle - g_warmup_cycles) * g_number_switches))
								/ (g_local_queue_length / g_local_link_channels) << "\%)" << endl;
				g_output_file << "Global Queues Occupancy: "
						<< GQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * GQO[vc] / ((g_cycle - g_warmup_cycles) * g_number_switches))
								/ (g_global_queue_length / g_global_link_channels) << "\%)" << endl;
				break;
		}

		switch (g_deadlock_avoidance) {
			case EMBEDDED_TREE:
				g_output_file << "Local Tree Queues Occupancy: "
						<< LTQO[vc] / (((1.0) * g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * LTQO[vc] / ((g_cycle - g_warmup_cycles) * g_localEmbeddedTreeSwitchesCount))
								/ g_local_queue_length << "\%)" << endl;
				break;
			case RING:
			case EMBEDDED_RING:
				g_output_file << "Local Ring Queues Occupancy: "
						<< LRQO[vc] / (((1.0) * g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * LRQO[vc] / ((g_cycle - g_warmup_cycles) * g_localEmbeddedRingSwitchesCount))
								/ g_local_queue_length << "\%)" << endl;
				g_output_file << "Global Ring Queues Occupancy: "
						<< GRQO[vc] / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
						<< ((float) 100.0 * GRQO[vc] / ((g_cycle - g_warmup_cycles) * g_globalEmbeddedRingSwitchesCount))
								/ g_global_queue_length << "\%)" << endl;
				break;
		}
	}
	g_output_file << endl;
	if (g_switch_type == IOQ_SW) {
		g_output_file << "Consumption Output Queues Occupancy: "
				<< OQO / ((1.0) * (g_cycle - g_warmup_cycles) * g_number_switches) << " ("
				<< ((float) 100.0 * OQO / ((g_cycle - g_warmup_cycles) * g_number_switches)) / g_out_queue_length
				<< "\%)" << endl << endl;
	}
	g_output_file << "Petitions Done: " << g_petitions << endl;
	g_output_file << "Petitions Served: " << g_served_petitions << "(" << 100.0 * g_served_petitions / g_petitions
			<< "%)" << endl;
	g_output_file << "Injection Petitions Done: " << g_injection_petitions << endl;
	g_output_file << "Injection Petitions Served: " << g_served_injection_petitions << "("
			<< 100.0 * g_served_injection_petitions / g_injection_petitions << "%)" << endl;

	g_output_file << endl;
	g_output_file << "Max Injections: " << g_max_injection_packets_per_sw << endl;
	g_output_file << "Switch With Max Injections: " << g_sw_with_max_injection_pkts << endl;
	g_output_file << "Min Injections: " << g_min_injection_packets_per_sw << endl;
	g_output_file << "Switch With Min Injections: " << g_sw_with_min_injection_pkts << endl;
	g_output_file << "Unbalance Quotient: "
			<< (float) 1.0 * g_max_injection_packets_per_sw / g_min_injection_packets_per_sw << endl;
	double sum = 0, sqsum = 0;
	for (i = 0; i < g_number_switches; i++) {
		sum += g_switches_list[i]->packetsInj;
		sqsum += pow(g_switches_list[i]->packetsInj, 2);
	}
	double mean = sum / g_number_switches;
	double stdev = sqrt(sqsum / g_number_switches - pow(mean, 2));
	g_output_file << "Unfairness Quotient (stdev/avg): " << stdev / mean << endl << endl;

	/* Group 0 and tree root group (if deadlock avoidance mechanism is an embedded tree)
	 * latency statistics per switch. */
	long long *g_group0_numFlits_aux = new long long[g_a_routers_per_group];
	for (int a = 0; a < g_a_routers_per_group; a++) {
		g_group0_numFlits_aux[a] = 0;
		for (int p = 0; p < g_p_computing_nodes_per_router; p++)
			g_group0_numFlits_aux[a] += g_group0_numFlits[a][p][0] + g_group0_numFlits[a][p][1];
	}

	g_output_file << "GROUP 0" << endl;
	for (int i = 0; i < g_a_routers_per_group; i++) {
		g_output_file << "Group0 SW" << i << " Flits: " << g_group0_numFlits_aux[i] << endl;
		if (g_group0_numFlits_aux[i] > 0) {
			g_output_file << "Group0 SW" << i << " AvgTotalLatency: "
					<< (g_group0_totalLatency[i]) / ((double) (g_group0_numFlits_aux[i])) << endl;
		}
		for (int p = 0; p < g_p_computing_nodes_per_router; p++)
			if (g_group0_numFlits[i][p][0] > 0 || g_group0_numFlits[i][p][1] > 0) {
				g_output_file << "Group0 SW" << i << " Node" << p << " MIN: " << g_group0_numFlits[i][p][0] << endl;
				g_output_file << "Group0 SW" << i << " Node" << p << " NON-MIN: " << g_group0_numFlits[i][p][1] << endl;
			}

	}
	delete[] g_group0_numFlits_aux;
	if (g_deadlock_avoidance == EMBEDDED_TREE) {
		g_output_file << " " << endl;
		g_output_file << "TREE ROOT NODE: " << g_tree_root_node << endl;
		g_output_file << "TREE ROOT SWITCH: " << int(g_tree_root_node / g_p_computing_nodes_per_router) << endl;
		g_output_file << "TREE ROOT GROUP: " << g_tree_root_switch << " (GroupR)" << endl;
		for (int i = 0; i < g_a_routers_per_group; i++) {
			g_output_file << "GroupR SW" << i << " Flits: " << g_groupRoot_numFlits[i] << endl;
			if (g_groupRoot_numFlits[i] > 0) {
				g_output_file << "GroupR SW" << i << " AvgTotalLatency: "
						<< (g_groupRoot_totalLatency[i]) / ((double) (g_groupRoot_numFlits[i])) << endl;
			}
		}
	}

	g_output_file << endl;
	g_output_file.close();
}

void freeMemory() {
	int i;

	for (i = 0; i < g_number_switches; i++) {
		delete g_switches_list[i];
	}
	delete[] g_switches_list;

	for (i = 0; i < g_number_generators; i++) {
		delete g_generators_list[i];
	}
	delete[] g_generators_list;

	if (g_transient_stats) {
		delete[] g_transient_record_latency;
		delete[] g_transient_record_injection_latency;
		delete[] g_transient_record_flits;
		delete[] g_transient_record_misrouted_flits;
		delete[] g_transient_net_injection_latency;
		delete[] g_transient_net_injection_inj_latency;
		delete[] g_transient_net_injection_flits;
		delete[] g_transient_net_injection_misrouted_flits;
	}
	delete[] g_phase_traffic_adv_dist;
	delete[] g_phase_traffic_type;
	delete[] g_phase_traffic_percent;

	for (i = 0; i < g_a_routers_per_group; i++) {
		for (int p = 0; p < g_p_computing_nodes_per_router; p++)
			delete[] g_group0_numFlits[i][p];
		delete[] g_group0_numFlits[i];
	}
	delete[] g_group0_numFlits;
	delete[] g_group0_totalLatency;
	delete[] g_groupRoot_numFlits;
	delete[] g_groupRoot_totalLatency;

	g_latency_histogram_no_global_misroute.clear();
	g_latency_histogram_global_misroute_at_injection.clear();
	g_latency_histogram_other_global_misroute.clear();

	delete[] g_hops_histogram;
	delete[] g_output_file_name;

	cout << "Freed memory" << endl;
}

void writeTransientOutput(char * output_name) {

	string file_name(output_name);
	ofstream outputFile;
	int i;

	file_name.append(".transient");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cerr << "Can't open the transient output file" << endl;
		exit(-1);
	}

	outputFile << "cycle\tinj_cycle\tlatency\tnum_packets\tnum_misrouted\tinj_latency" << endl;
	for (i = 0; i < g_transient_record_len; i++) {
		/* Remove those records that don't host any packet activity,
		 * for transient output file economy and simplicity reasons. */
		if (i == 0 || g_transient_record_flits[i] != 0) {
			outputFile << i + (g_warmup_cycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles)
					<< "\t";
			outputFile << i - g_transient_record_num_prev_cycles << "\t";
			outputFile << g_transient_record_latency[i] << "\t";
			outputFile << g_transient_record_flits[i] << "\t";
			outputFile << g_transient_record_misrouted_flits[i] << "\t";
			outputFile << g_transient_record_injection_latency[i] << "\t";
			outputFile << endl;
		}
	}

	outputFile.close();

	file_name.append("_injection");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cerr << "Can't open the injection transient output file" << endl;
		exit(-1);
	}

	outputFile << "injCycle\tcycle\tlatency\tnum_packets\tnum_misrouted\ttotal_latency" << endl;
	for (i = 0; i < g_transient_record_len; i++) {
		outputFile << i + (g_warmup_cycles + g_transient_traffic_cycle - g_transient_record_num_prev_cycles) << "\t";
		outputFile << i - g_transient_record_num_prev_cycles << "\t";
		outputFile << g_transient_net_injection_latency[i] - g_transient_net_injection_inj_latency[i] << "\t";
		outputFile << g_transient_net_injection_flits[i] << "\t";
		outputFile << g_transient_net_injection_misrouted_flits[i] << "\t";
		outputFile << g_transient_net_injection_latency[i] << "\t";
		outputFile << endl;
	}
	outputFile.close();
}

void writeLatencyHistogram(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;
	int i;
	long long total_lat;

	file_name.append(".lat_hist");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cerr << "Can't open the latency histogram output file" << endl;
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
		outputFile << ((double) total_lat) / (g_rx_flit_counter - g_rx_warmup_flit_counter) << "\t";
		outputFile << g_latency_histogram_no_global_misroute[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_no_global_misroute[i]) / (g_rx_flit_counter - g_rx_warmup_flit_counter)
				<< "\t";
		outputFile << g_latency_histogram_global_misroute_at_injection[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_global_misroute_at_injection[i])
						/ (g_rx_flit_counter - g_rx_warmup_flit_counter) << "\t";
		outputFile << g_latency_histogram_other_global_misroute[i] << "\t";
		outputFile
				<< ((double) g_latency_histogram_other_global_misroute[i])
						/ (g_rx_flit_counter - g_rx_warmup_flit_counter) << "\t";
		outputFile << endl;
	}
	outputFile.close();
}

void writeHopsHistogram(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;
	int i;
	long long num_packets;

	file_name.append(".hops_hist");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cerr << "Can't open the hop histogram output file" << endl;
		exit(-1);
	}
	outputFile << "hops\tnum_packets\tpercent" << endl;
	for (i = 0; i < g_hops_histogram_maxHops; i++) {
		outputFile << i << "\t";

		num_packets = g_hops_histogram[i];
		outputFile << num_packets << "\t";
		outputFile << ((double) num_packets) / (g_rx_flit_counter - g_rx_warmup_flit_counter);

		outputFile << endl;
	}
	outputFile.close();
}

void writeGeneratorsInjectionProbability(char * output_name) {
	string file_name(output_name);
	ofstream outputFile;

	file_name.append(".generatorsInjectionProbability");
	outputFile.open(file_name.c_str(), ios::out);
	if (!outputFile) {
		cerr << "Can't open the generators injection probability output file" << endl;
		exit(-1);
	}

	outputFile << "generator\tinjectionProbability" << endl;

	for (int i = 0; i < g_number_generators; i++) {
		float averageInjectionProbability = g_generators_list[i]->sum_injection_probability / g_cycle;
		if (averageInjectionProbability != g_injection_probability)
			outputFile << i << "\t" << averageInjectionProbability << endl;
	}
	outputFile.close();
}

void readTraceMap(const char * tracemap_filename) {
	int generator, node, trace, instances;
	vector<string> values;
	ConfigFile map;

	/* Open configuration file */
	if (map.LoadConfig(tracemap_filename) < 0) {
		cerr << "Can't read the trace_map file" << endl;
		exit(-1);
	}

	/* Initiate trace2gen map */
	for (int i = 0; i < g_num_traces; i++) {
		vector<vector<int> > aux; // Aux vector
		for (int j = 0; j < g_trace_nodes[i]; j++) {
			aux.push_back(vector<int>()); // Add an empty vector (will be later increased by every gen mapped to the trace node)
		}
		g_trace_2_gen_map.push_back(aux);
	}

	/* Load trace layout into a map */
	for (generator = 0; generator < g_number_generators; generator++) {
		stringstream strs;
		strs << generator;
		if (map.getListValues("TRACE_MAP", strs.str().c_str(), values) == 0) {
			trace = atoi(values[0].c_str());
			assert(trace >= 0 && trace < g_num_traces); // Sanity check
			node = atoi(values[1].c_str());
			assert(node >= 0 && node < g_trace_nodes[trace]); // Sanity check
			g_trace_2_gen_map[trace][node].push_back(generator);
		}
	}

	/* Update number of instances */
	for (trace = 0; trace < g_num_traces; trace++) {
		instances = g_trace_2_gen_map[trace][0].size();
		for (node = 1; node < g_trace_nodes[trace]; node++) {
#if DEBUG
			cout << "Traces instances for trace " << trace << " don't match! first node has " << instances
			<< " copies, node " << node << " has " << g_trace_2_gen_map[trace][node].size() << " copies"
			<< endl;
#endif
			assert(instances == g_trace_2_gen_map[trace][node].size()); // Sanity check: # of instances must be same for all nodes in a trace
		}
		g_trace_instances.push_back(instances);
	}
}

/* Constructs trace layout/map (linking every trace node to a generator)
 * upon input specs such as trace distribution, number of traces and number
 * of instances. Writes layout to a file, to keep a record of it (and/or
 * reuse it for future simulations).
 */
void buildTraceMap() {
	int i, j, trace, instance, node, generator;
	string fileName(g_output_file_name);
	ofstream traceMapFile;
	assert(g_num_traces > 0);
	assert(g_trace_nodes.size() == g_num_traces && g_trace_instances.size() == g_num_traces);

	/* Open trace map file and write section header */
	fileName.append(".traceMap");
	traceMapFile.open(fileName.c_str(), ios::out);
	traceMapFile << "[TRACE_MAP]" << endl;

	/* Initiate trace2gen map */
	for (i = 0; i < g_num_traces; i++) {
		vector<vector<int> > aux; // Aux vector
		for (j = 0; j < g_trace_nodes[i]; j++) {
			aux.push_back(vector<int>()); // Add an empty vector (will be later increased by every gen mapped to the trace node)
		}
		g_trace_2_gen_map.push_back(aux);
	}

	/* Associate every trace node with a generator */
	for (trace = 0; trace < g_num_traces; trace++) {
		for (node = 0; node < g_trace_nodes[trace]; node++) {
			for (instance = 0; instance < g_trace_instances[trace]; instance++) {
				generator = 0;
				switch (g_trace_distribution) {
					case CONSECUTIVE:
						for (i = 0; i < trace; i++) {
							generator += g_trace_instances[i] * g_trace_nodes[i];
						}
						generator += node + instance * g_trace_nodes[trace];
						break;
					case INTERLEAVED:
						for (i = 0; i < g_num_traces; i++) {
							if (g_trace_nodes[i] < node)
								generator += g_trace_nodes[i] * g_trace_instances[i];
							else
								generator += node * g_trace_instances[i];
						}
						for (i = 0; i < trace; i++) {
							if (g_trace_nodes[i] > node) generator += g_trace_instances[i];
						}
						generator += instance;
						break;
					case RANDOM:
						do
							generator = rand() % g_number_generators;
						while (g_gen_2_trace_map.count(generator) == 1);
						break;
					default:
						assert(0);
						break;
				}
				assert(generator >= 0 && generator < g_number_generators); // Sanity check
				g_trace_2_gen_map[trace][node].push_back(generator);
				assert(g_gen_2_trace_map.insert(pair<int, TraceNodeId>(generator, { trace, node })).second == true);
			}
		}
	}

	/* Save trace map to file */
	for (generator = 0; generator < g_number_generators; generator++) {
		if (g_gen_2_trace_map.count(generator) > 0) {
			traceMapFile << generator << "=" << g_gen_2_trace_map[generator].trace_id << ","
					<< g_gen_2_trace_map[generator].trace_node << endl;
		}
	}

	traceMapFile.close();
}

#define READ_ENUM(VAR, VALUE) if (strcmp(VAR, #VALUE) == 0) *var = VALUE;

void readBufferType(const char * buffer_type, BufferType * var) {
	READ_ENUM(buffer_type, SEPARATED) else
	READ_ENUM(buffer_type, DYNAMIC) else {
		cerr << "ERROR: UNRECOGNISED BUFFER TYPE! " << endl;
	}
}

void readSwitchType(const char * switch_type, SwitchType * var) {
	READ_ENUM(switch_type, BASE_SW) else
	READ_ENUM(switch_type, IOQ_SW) else {
		cerr << "ERROR: UNRECOGNISED SWITCH TYPE!" << endl;
		exit(0);
	}
}

void readArbiterType(const char * arbiter_type, ArbiterType * var) {
	READ_ENUM(arbiter_type, LRS) else
	READ_ENUM(arbiter_type, PrioLRS) else
	READ_ENUM(arbiter_type, RR) else
	READ_ENUM(arbiter_type, PrioRR) else
	READ_ENUM(arbiter_type, AGE) else
	READ_ENUM(arbiter_type, PrioAGE) else {
		cerr << "ERROR: UNRECOGNISED ARBITER TYPE! " << arbiter_type << endl;
	}
}

void readTrafficPattern(const char * traffic_name, TrafficType * var) {
	READ_ENUM(traffic_name, UN) else
	READ_ENUM(traffic_name, ADV) else
	READ_ENUM(traffic_name, ADV_RANDOM_NODE) else
	READ_ENUM(traffic_name, ADV_LOCAL) else
	READ_ENUM(traffic_name, ADVc) else
	READ_ENUM(traffic_name, SINGLE_BURST) else
	READ_ENUM(traffic_name, ALL2ALL) else
	READ_ENUM(traffic_name, MIX) else
	READ_ENUM(traffic_name, TRANSIENT) else
	READ_ENUM(traffic_name, BURSTY_UN) else
	READ_ENUM(traffic_name, TRACE) else
	READ_ENUM(traffic_name, UN_RCTV) else
	READ_ENUM(traffic_name, ADV_RANDOM_NODE_RCTV) else
	READ_ENUM(traffic_name, GRAPH500) else
	READ_ENUM(traffic_name, BURSTY_UN_RCTV) else {
		cerr << "ERROR: UNRECOGNISED TRAFFIC TYPE!" << traffic_name << endl;
		exit(0);
	}
}

void readRoutingType(const char * routing_type, RoutingType * var) {
	READ_ENUM(routing_type, MIN) else
	READ_ENUM(routing_type, MIN_COND) else
	READ_ENUM(routing_type, VAL) else
	READ_ENUM(routing_type, VAL_ANY) else
	READ_ENUM(routing_type, OBL) else
	READ_ENUM(routing_type, SRC_ADP) else
	READ_ENUM(routing_type, PAR) else
	READ_ENUM(routing_type, UGAL) else
	READ_ENUM(routing_type, PB) else
	READ_ENUM(routing_type, PB_ANY) else
	READ_ENUM(routing_type, OFAR) else
	READ_ENUM(routing_type, RLM) else
	READ_ENUM(routing_type, OLM) else {
		cerr << "ERROR: UNRECOGNISED ROUTING TYPE!" << endl;
		exit(0);
	}
}

void readGlobalMisrouting(const char * global_misrouting, GlobalMisroutingPolicy * var) {
	READ_ENUM(global_misrouting, CRG) else
	READ_ENUM(global_misrouting, CRG_L) else
	READ_ENUM(global_misrouting, NRG) else
	READ_ENUM(global_misrouting, NRG_L) else
	READ_ENUM(global_misrouting, RRG) else
	READ_ENUM(global_misrouting, RRG_L) else
	READ_ENUM(global_misrouting, MM) else
	READ_ENUM(global_misrouting, MM_L) else {
		cerr << "ERROR: UNRECOGNISED GLOBAL MISROUTING POLICY!" << endl;
		exit(0);
	}
}

void readDeadlockAvoidanceMechanism(const char * d_a, DeadlockAvoidance * var) {
	READ_ENUM(d_a, DALLY) else
	READ_ENUM(d_a, RING) else
	READ_ENUM(d_a, EMBEDDED_RING) else
	READ_ENUM(d_a, EMBEDDED_TREE) else {
		cerr << "ERROR: UNRECOGNISED DEADLOCK AVOIDANCE MECHANISM!" << endl;
		exit(0);
	}
}

void readMisroutingTrigger(const char * d_a, MisroutingTrigger * var) {
	READ_ENUM(d_a, CA) else
	READ_ENUM(d_a, WEIGTHED_CA) else
	READ_ENUM(d_a, FILTERED) else
	READ_ENUM(d_a, CA_REMOTE) else
	READ_ENUM(d_a, DUAL) else
	READ_ENUM(d_a, CGA) else
	READ_ENUM(d_a, HYBRID) else
	READ_ENUM(d_a, HYBRID_REMOTE) else {
		cerr << "ERROR: UNRECOGNISED MISROUTING TRIGGER MECHANISM!" << endl;
		exit(0);
	}
}

void readCongestionDetection(const char * d_a, CongestionDetection * var) {
	READ_ENUM(d_a, PER_PORT) else
	READ_ENUM(d_a, PER_VC) else
	READ_ENUM(d_a, PER_PORT_MIN) else
	READ_ENUM(d_a, PER_VC_MIN) else {
		cerr << "ERROR: UNRECOGNISED CONGESTION MANAGEMENT POLICY!: " << d_a << endl;
	}
}

void readTraceDistribution(const char * d_a, TraceAssignation * var) {
	READ_ENUM(d_a, RANDOM) else
	READ_ENUM(d_a, CONSECUTIVE) else
	READ_ENUM(d_a, INTERLEAVED) else {
		cerr << "ERROR: UNRECOGNISED TRACE DISTRIBUTION!" << endl;
		exit(0);
	}
}

void readVcUsage(const char * d_a, VcUsageType * var) {
	READ_ENUM(d_a, BASE) else
	READ_ENUM(d_a, FLEXIBLE) else {
		cerr << "ERROR: UNRECOGNISED VC USAGE!" << endl;
		exit(0);
	}
}

void readVcAlloc(const char * d_a, VcAllocationMechanism * var) {
	READ_ENUM(d_a, HIGHEST_VC) else
	READ_ENUM(d_a, LOWEST_VC) else
	READ_ENUM(d_a, LOWEST_OCCUPANCY) else
	READ_ENUM(d_a, RANDOM_VC) else {
		cerr << "ERROR: UNRECOGNISED VC ALLOCATION MECHANISM!" << endl;
		exit(0);
	}
}

void readVcInj(const char * d_a, VcInjectionPolicy * var) {
	READ_ENUM(d_a, DEST) else
	READ_ENUM(d_a, RAND) else {
		cerr << "ERROR: UNRECOGNISED VC INJECTION POLICY!" << endl;
		exit(0);
	}
}
