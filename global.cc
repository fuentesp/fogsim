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

#include "global.h"

class generatorModule;
class switchModule;

using namespace std;

/* General parameters */
long long g_max_cycles = 10000; /*						Maximum number of simulated cycles */
long long g_warmup_cycles = g_max_cycles; /* 			Number of warm-up cycles; in those,
 *														 no general statistics are collected
 *														 (but some specific ones are) */
int g_print_cycles = 100; /*						Number of cycles between printing
 *														 temporary stats to the stdout. */
long long g_injection_queue_length = 999999; /* 		Injection queue size in phits */
long long g_local_queue_length = 10; /* 				Local link queue size in phits */
long long g_global_queue_length = 100; /* 				Global link queue size in phits */
long long g_out_queue_length = 10; /*					Output queue size in phits (only used
 *														 under InputOutputQueued switch). */
int g_ring_injection_bubble = 1; /* 					Deadlock-avoidance bubble to respect when
 *														 injecting packets to ring (preserves
 *														 packet advance through ring) */
int g_p_computing_nodes_per_router = 4; /* 				Number of computing nodes ('p') per router */
int g_a_routers_per_group = 4; /* 						Number of routers ('a') per group */
int g_h_global_ports_per_router = 1; /* 				Number of global ports ('h') per router;
 *														 total number of groups will be a*h+1 */
long long g_xbar_delay = 1; /*							Delay when forwarding flit from input to
 *														 output buffers (only with InputOutputQueueing) */
long long g_injection_delay = 1; /* 					Delay at injection queues */
long long g_local_link_transmission_delay = 1; /*		Flit delay end-to-end of a local link */
long long g_global_link_transmission_delay = 1; /*		Flit delay end-to-end of a global link */
int g_injection_channels = 1; /*						Number of VCs in injection queues; refers
 *														 amount of generators per computing node */
int g_local_link_channels = 1; /* 						Number of VCs in local links */
int g_global_link_channels = 1; /* 						Number of VCs in global links */
int g_local_res_channels = 0; /*						Number of VCs in local links reserved for responses
 *														 (exclusive for reactive traffic patterns)*/
int g_global_res_channels = 0; /*						Number of VCs in global links reserved for responses
 *														 (exclusive for reactive traffic patterns)*/
int g_segregated_flows = 1; /*							Number of segregated traffic flows at output
 *														 ports; currently only used to segregate petitions
 *														 from responses at the consumption nodes. */
int g_flit_size = 1; /* 								Flit size in phits */
int g_packet_size = 1; /* 								Packet size in phits */
float g_injection_probability = 1; /* 					Packet injection probability at a generator.
 *														 It ranges between 0 and 100 (it is expressed
 *														 as a percentage). When multiplied by packet
 *														 size and number of generators, gives total
 *														 injection rate within the network */
char *g_output_file_name; /* 							Results filename */
long long g_seed = 1; /* 								Employed seed (to randomize simulations) */
int g_allocator_iterations = 3; /* 						Number of (local/global) arbiter iterations
 *														 within an allocation cycle */
int g_local_arbiter_speedup = 1; /* 					SpeedUp within local arbiter: number of ports to
 *														 crossbar for every input port. */
bool g_issue_parallel_reqs = true; /*					Used in conjunction with local_arbiter_speedup,
 *														 if set allows to make as many requests as speedup
 *														 in a single allocation iteration */
float g_internal_speedup = 1.0; /*						SpeedUp in router frequency: router allocation
 *														 cycles are conducted faster and more frequently
 *														 than simulation cycles (only with InputOutputQueueing) */
bool g_palm_tree_configuration = 0;
bool g_transient_stats = false; /*						Determines if temporal statistics over simulation
 *														 time are tracked or not. Mainly related to transient
 *														 and trace traffic. */
unsigned short g_cos_levels = 1; /*						Number of Class of Service levels - Ethernet 802.1q */
bool g_print_hists = false; /*							Chooses whether to print latency and injection
 *														 histograms or not. */

/* General variables */
long long g_cycle = 0; /* 								Current cycle, tracks amount of simulated cycles */
long double g_internal_cycle = 0.0; /*					Current internal cycle, determines when switch simulation
 *														 is conducted (only profited in InputOutputQueued switch
 *														 with internal speedUp) */
int g_iteration = 0; /* 								Global variable to exchange current iteration value
 *														 between allocation operation and petition attendance */
int g_local_router_links_offset = 0; /* 				Local links offset among router ports; followed
 *														 port convention is:
 *														 	computing nodes < local links < global links,
 *														 where local links connect a router with the rest
 *														 of routers within the same group, and global
 *														 links connect the router with other groups */
int g_global_router_links_offset = 0; /* 				Global links offset among router ports */
int g_global_links_per_group = g_a_routers_per_group * g_h_global_ports_per_router; /* Convenience variable,
 *																					 tracks amount of global
 *																					 links per group (this is,
 *														 							 number of groups
 *														 							 in the network -1) */
int g_number_switches = (g_a_routers_per_group * g_h_global_ports_per_router + 1) * g_a_routers_per_group; /* Total number of switches in the network */
int g_number_generators = g_number_switches * g_p_computing_nodes_per_router; /* Total number of computing
 *																				  nodes in the network */
int g_channels = 1; /* 									Number of VCs in the network, as a max of local,
 *														 global and injections VCs PLUS those in the escape
 *														 subnetwork, when the latter is embedded */
int g_flits_per_packet = 0; /* 							Packet size in flits */
ofstream g_output_file; /* 								Results file */
generatorModule **g_generators_list; /* 				List of generator modules */
switchModule **g_switches_list; /* 						List of router modules */
int g_ports = g_p_computing_nodes_per_router + g_a_routers_per_group - 1 + g_h_global_ports_per_router; /*	Number of ports per router */
default_random_engine g_reng;

/* Switch type */
SwitchType g_switch_type = BASE_SW;

/* Buffer type */
BufferType g_buffer_type = SEPARATED;
int g_local_queue_reserved = 1;
int g_global_queue_reserved = 1;

/* Arbiter type */
ArbiterType g_input_arbiter_type = LRS;
ArbiterType g_output_arbiter_type = PrioLRS;

/* Traffic type */
TrafficType g_traffic = UN; /* 				Traffic pattern */
bool g_reactive_traffic = false; /*			Reactive traffic */
/* Adversarial traffic parameters */
int g_adv_traffic_distance = 1; /*			Distance to the adverse traffic destination group */
int g_adv_traffic_local_distance = 1; /*	Distance to the adverse traffic destination router */
/* Auxiliary parameters (employed in many traffic types) */
TrafficType *g_phase_traffic_type;
int *g_phase_traffic_adv_dist;
int *g_phase_traffic_percent;
/* Transient traffic parameters */
int g_transient_traffic_cycle = 0;
/* All To All traffic parameters */
int g_phases = -1;
bool g_random_AllToAll = 0;
/* Burst traffic parameters */
int g_single_burst_length = 100; /* 			Burst length in flits */
/* Bursty UN traffic parameters */
int g_bursty_avg_length = 1; /*					Average burst size in packets */
/* Reactive traffic parameters */
int g_max_petitions_on_flight = -1;
/* Graph 500 traffic parameters */
int g_graph_coalescing_size = 256;
vector<int> g_graph_nodes_cap_mod;
int g_graph_cap_mod_factor = 100;
vector<int> g_graph_root_node;
vector<int> g_graph_root_degree;
float g_graph_query_time;
int g_graph_scale;
int g_graph_edgefactor;
vector<int> g_graph_tree_level;
vector<long long> g_graph_queries_remain;
vector<long long> g_graph_queries_rem_minus_means;
long long ***g_graph_p2pmess_node2node;
int g_graph_max_levels = 9;
long long g_graph_p2pmess = 0;

/* Routing */
RoutingType g_routing = MIN;
VcUsageType g_vc_usage = BASE;
VcAllocationMechanism g_vc_alloc = LOWEST_OCCUPANCY;
VcInjectionPolicy g_vc_injection = DEST;
int g_ugal_local_threshold = 0;
int g_ugal_global_threshold = 5;
int g_piggyback_coef = 200;
int g_th_min = 0;
bool g_reset_val = false;
bool g_vc_misrouting_congested_restriction = false;
int g_vc_misrouting_congested_restriction_coef_percent = 150;
int g_vc_misrouting_congested_restriction_th = 5;
GlobalMisroutingPolicy g_global_misrouting = MM_L;
MisroutingTrigger g_misrouting_trigger = CGA;
CongestionDetection g_congestion_detection = PER_PORT;
bool g_relative_threshold = 0;
int g_percent_local_threshold = 100;
int g_percent_global_threshold = 100;
bool g_contention_aware = false;
int g_contention_aware_th = 3;
int g_contention_aware_local_th = -1;
int g_contention_aware_global_th = 8;
bool g_increaseContentionAtHeader = true;
float g_filtered_contention_regressive_coefficient = 0.5;
DeadlockAvoidance g_deadlock_avoidance = DALLY;
int g_rings = 0;
int g_ringDirs = 0;
int g_ring_ports = 0;
bool g_onlyRing2 = 0;
bool g_forbid_from_inj_queues_to_ring = 1;
int g_restrictLocalCycles = 0;
int g_globalEmbeddedRingSwitchesCount = -1;
int g_localEmbeddedRingSwitchesCount = -1;
int g_tree_root_node = -1;
int g_tree_root_switch = -1;
int g_localEmbeddedTreeSwitchesCount = -1;
int g_channels_escape = 0;
CongestionManagement g_congestion_management = BCM;
int g_baseCongestionControl_bub = 0;
int g_escapeCongestion_th = 100;
int g_forceMisrouting = 0;
bool g_try_just_escape = 0;

/* Statistics variables */
long double g_flit_latency = 0;
long double g_packet_latency = 0;
long double g_injection_queue_latency = 0;
long double g_base_latency = 0;
long double g_warmup_flit_latency;
long double g_warmup_packet_latency;
long double g_warmup_injection_latency;
long double g_response_latency;
float *g_transient_record_latency;
float *g_transient_record_injection_latency;
long double *g_group0_totalLatency;
long double *g_groupRoot_totalLatency;
int g_latency_histogram_maxLat = 1;
vector<long long> g_latency_histogram_no_global_misroute;
vector<long long> g_latency_histogram_global_misroute_at_injection;
vector<long long> g_latency_histogram_other_global_misroute;
long long g_tx_flit_counter = 0;
long long g_rx_flit_counter = 0;
long long g_attended_flit_counter = 0;
long long g_tx_warmup_flit_counter = 0;
long long g_rx_warmup_flit_counter = 0;
long long g_response_counter = 0;
long long g_response_warmup_counter = 0;
long long g_nonminimal_counter = 0;
long long g_nonminimal_warmup_counter = 0;
long long g_nonminimal_inj = 0;
long long g_nonminimal_warmup_inj = 0;
long long g_nonminimal_src = 0;
long long g_nonminimal_warmup_src = 0;
long long g_nonminimal_int = 0;
long long g_nonminimal_warmup_int = 0;
long long g_min_flit_counter[4] = { 0 };
long long g_global_misrouted_flit_counter[4] = { 0 };
long long g_global_mandatory_misrouted_flit_counter[4] = { 0 };
long long g_local_misrouted_flit_counter[4] = { 0 };
int *g_transient_record_flits;
int *g_transient_record_misrouted_flits;
long long ***g_group0_numFlits;
long long *g_groupRoot_numFlits;
float *g_transient_net_injection_latency;
float *g_transient_net_injection_inj_latency;
int *g_transient_net_injection_flits;
int *g_transient_net_injection_misrouted_flits;
long long g_tx_packet_counter = 0;
long long g_rx_packet_counter = 0;
long long g_tx_warmup_packet_counter = 0;
long long g_rx_warmup_packet_counter = 0;
long long g_injected_packet_counter = 0;
long long g_injected_bursts_counter = 0;
long long g_total_hop_counter = 0;
long long g_local_hop_counter = 0;
long long g_global_hop_counter = 0;
long long g_local_ring_hop_counter = 0;
long long g_global_ring_hop_counter = 0;
long long g_local_tree_hop_counter = 0;
long long g_global_tree_hop_counter = 0;
long long g_max_hops = 0;
long long g_max_local_hops = 0;
long long g_max_global_hops = 0;
long long g_max_local_subnetwork_hops = 0;
long long g_max_global_subnetwork_hops = 0;
long long g_max_local_ring_hops = 0;
long long g_max_global_ring_hops = 0;
long long g_max_local_tree_hops = 0;
long long g_max_global_tree_hops = 0;
int g_hops_histogram_maxHops = 200;
long long * g_hops_histogram;
long long g_port_usage_counter[100];
vector<vector<long long> > g_vc_counter;
long long g_port_contention_counter[100];
long long g_subnetwork_injections_counter = 0;
long long g_root_subnetwork_injections_counter = 0;
long long g_source_subnetwork_injections_counter = 0;
long long g_dest_subnetwork_injections_counter = 0;
long double g_local_contention_counter = 0;
long double g_global_contention_counter = 0;
long double g_local_escape_contention_counter = 0;
long double g_global_escape_contention_counter = 0;
unsigned int g_petitions = 0;
unsigned int g_served_petitions = 0;
unsigned int g_injection_petitions = 0;
unsigned int g_served_injection_petitions = 0;
long long g_max_injection_packets_per_sw = 0;
int g_sw_with_max_injection_pkts = -1;
long long g_min_injection_packets_per_sw = 0;
int g_sw_with_min_injection_pkts = -1;
int g_transient_record_len = 10100;
int g_transient_record_num_cycles = 10000;
int g_transient_record_num_prev_cycles = 100;
int g_AllToAll_generators_finished_count = 0;
int g_burst_generators_finished_count = 0;
long long g_max_subnetwork_injections = 0;
long long g_max_root_subnetwork_injections = 0;
long long g_max_source_subnetwork_injections = 0;
long long g_max_dest_subnetwork_injections = 0;
set<int> g_verbose_switches;
int g_verbose_cycles = 5;

/* Traces support */
int g_num_traces = 0;
vector<string> g_trace_file;
vector<string> g_pcf_file;
vector<vector<long> > g_event_deadlock;
vector<long> g_trace_nodes;
vector<int> g_trace_instances;
vector<vector<bool> > g_trace_ended;
vector<vector<long> > g_trace_end_cycle;
TraceAssignation g_trace_distribution = CONSECUTIVE;
double g_cpu_speed = 1e9;
long g_op_per_cycle = 50;
long g_multitask = 1;
long g_phit_size = 4; /* Phit size in bytes, for trace messages translation into simulator packets */
bool g_cutmpi = 1;
map<long, long> g_events_map;
map<int, TraceNodeId> g_gen_2_trace_map;
vector<vector<vector<int> > > g_trace_2_gen_map;
