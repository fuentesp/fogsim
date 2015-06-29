/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
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

#ifndef GLOBAL_H
#define	GLOBAL_H

#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <map>
#include <vector>

using namespace std;

class generatorModule;
class switchModule;

#define DEBUG false

/***
 * General parameters
 */
extern long long g_max_cycles; /*						Maximum number of simulated cycles */
extern long long g_warmup_cycles; /* 					Number of warm-up cycles; in those,
 *														 no general statistics are collected
 *														 (but some specific ones are) */
extern long long g_injection_queue_length; /* 			Injection queue size in phits */
extern long long g_local_queue_length; /* 				Local link queue size in phits */
extern long long g_global_queue_length; /* 				Global link queue size in phits */
extern long long g_out_queue_length; /*					Output queue size in phits (only used
 *														 under InputOutputQueued switch). */
extern int g_ring_injection_bubble; /* 					Deadlock-avoidance bubble to respect when
 *														 injecting packets to ring (preserves
 *														 packet advance through ring) */
extern int g_p_computing_nodes_per_router; /* 			Number of computing nodes ('p') per router */
extern int g_a_routers_per_group; /* 					Number of routers ('a') per group */
extern int g_h_global_ports_per_router; /* 				Number of global ports ('h') per router;
 *														 total number of groups will be a*h+1 */
extern long long g_xbar_delay; /*						Delay when forwarding flit from input to
 *														 output buffers (only with InputOutputQueueing) */
extern long long g_injection_delay; /* 					Delay at injection queues */
extern long long g_local_link_transmission_delay; /*	Flit delay end-to-end of a local link */
extern long long g_global_link_transmission_delay; /*	Flit delay end-to-end of a global link */
extern int g_injection_channels; /*						Number of VCs in injection queues; refers
 *														 amount of generators per computing node */
extern int g_local_link_channels; /* 					Number of VCs in local links */
extern int g_global_link_channels; /* 					Number of VCs in global links */
extern int g_flit_size; /* 								Flit size in phits */
extern int g_packet_size; /* 							Packet size in phits */
extern int g_injection_probability; /* 					Packet injection probability at a generator.
 *														 It ranges between 0 and 100 (it is expressed
 *														 as a percentage). When multiplied by packet
 *														 size and number of generators, gives total
 *														 injection rate within the network */
extern char g_output_file_name[180]; /* 				Results filename */
extern long long g_seed; /* 							Employed seed (to randomize simulations) */
extern int g_allocator_iterations; /* 					Number of (local/global) arbiter iterations
 *														 within an allocation cycle */
extern int g_local_arbiter_speedup; /* 					SpeedUp within local arbiter: number of ports to
 *														 crossbar for every input port. */
extern bool g_issue_parallel_reqs; /*					Used in conjunction with local_arbiter_speedup,
 *														 if set allows to make as many requests as speedup
 *														 in a single allocation iteration */
extern float g_internal_speedup; /*						SpeedUp in router frequency: router allocation
 *														 cycles are conducted faster and more frequently
 *														 than simulation cycles (only with InputOutputQueueing) */
extern bool g_palm_tree_configuration;
extern bool g_transient_stats; /*						Determines if temporal statistics over simulation
 *														 time are tracked or not. Mainly related to transient
 *														 and trace traffic. */

/***
 * General variables
 */
extern long long g_cycle; /* 							Current cycle, tracks amount of simulated cycles */
extern long double g_internal_cycle; /*					Current internal cycle, determines when switch simulation
 *														 is conducted (only profited in InputOutputQueued switch
 *														 with internal speedUp) */
extern int g_iteration; /* 								Global variable to exchange current iteration value
 *														 between allocation operation and petition attendance */
extern int g_local_router_links_offset; /* 				Local links offset among router ports; followed
 *														 port convention is:
 *														 	computing nodes < local links < global links,
 *														 where local links connect a router with the rest
 *														 of routers within the same group, and global
 *														 links connect the router with other groups */
extern int g_global_router_links_offset; /* 			Global links offset among router ports */
extern int g_global_links_per_group; /* 				Convenience variable, tracks amount of global links per
 *														 group (this is, number of groups in the network -1) */
extern int g_number_switches; /* 						Total number of switches in the network */
extern int g_number_generators; /* 						Total number of computing nodes in the network*/
extern int g_channels; /* 								Number of VCs in the network, as a max of local,
 *														 global and injections VCs PLUS those in the escape
 *														 subnetwork, when the latter is embedded */
extern int g_flits_per_packet; /* 						Packet size in flits */
extern ofstream g_output_file; /* 						Results file */
extern generatorModule **g_generators_list; /* 			List of generator modules */
extern switchModule **g_switches_list; /* 				List of router modules */
extern int g_ports; /* 									Number of ports per router */

/***
 * Switch types:
 * -BASE:	input-queued router. Allows to
 * 			have input speedup, with many
 * 			xbar input ports per switch
 * 			input. These input ports can be
 * 			handled in parallel (every
 * 			allocation cycle allows to issue
 * 			many input-output requests) or
 * 			sequentially (only can request
 * 			per input can be made at a time).
 * -IOQ:	Input-Output queued router. It is
 * 			based on Base switch class, but
 * 			with some additions to handle the
 * 			output buffers. These switch can
 * 			have internal speedup (internal
 * 			packet switching goes faster than
 * 			outside the switch).
 */
enum SwitchType {
	BASE_SW, IOQ_SW
};

extern SwitchType g_switch_type;

/***
 * Traffic type
 */
/*
 * Synthetic traffic types:
 * -RANDOM UNIFORM
 * -ADVERSARIAL
 * -ADVERSARIAL RANDOM NODE
 * -LOCAL ADVERSARIAL: all nodes in a router
 * 			send their traffic to nodes in
 * 			the same neighbor router
 * -GROUP ADVERSARIAL: all nodes in a group
 * 			send their traffic to groups
 * 			linked to the same local neighbor
 * 			router (this is roughly equivalent
 * 			to a mixed ADV+1,+2,..+H
 * -ALL-TO-ALL
 * -MIX: COMBINATION OF RANDOM, LOCAL
 * 			ADVERSARIAL & GLOBAL ADVERSARIAL
 * 			(ratios determined by percentage
 * 			values)
 * -TRANSIENT: start with one traffic pattern
 * 			and then switch to another
 * -SINGLE_BURST: defined by a combination of 3
 * 			traffic patterns running in burst
 * 			mode, at highest injection rate
 * 			possible. (It can be redefined to
 * 			admit more than 3 patterns)
 * -BURSTY_UN: behaves like Uniform Random
 * 			but with a burst nature, in which
 * 			instead of randomly selecting a
 * 			destination for each packet,
 * 			generators switch between bursts
 * 			towards the same node.
 * -TRACE: support for trace simulations.
 */
enum TrafficType {
	UN, ADV, ADV_RANDOM_NODE, ADV_LOCAL, ADV_GROUP, ALL2ALL, MIX, TRANSIENT, SINGLE_BURST, BURSTY_UN, TRACE
};

extern TrafficType g_traffic;

/* Adversarial traffic parameters */
extern int g_adv_traffic_distance; /*		Distance to the adverse traffic destination group */
extern int g_adv_traffic_local_distance; /* Distance to the adverse traffic destination router */
/* Auxiliary parameters (employed in many traffic types) */
extern TrafficType *g_phase_traffic_type;
extern int *g_phase_traffic_adv_dist;
extern int *g_phase_traffic_percent;
/* Transient traffic parameters */
extern int g_transient_traffic_cycle;
/* All To All traffic parameters */
extern int g_phases;
extern bool g_random_AllToAll;
/* Single Burst traffic parameters */
extern int g_single_burst_length; /* 		Burst length in flits */
/* Bursty UN traffic parameters */
extern int g_bursty_avg_length; /*			Average burst size in packets */

/***
 * Routing mechanism
 */

/*
 * Misrouting types at port level:
 * -LOCAL
 * -LOCAL_MM
 * -GLOBAL
 * -GLOBAL_MANDATORY
 * -VALIANT (this is not truly at port level, but helps to record statistics)
 * -NONE
 */
enum MisrouteType {
	LOCAL, LOCAL_MM, GLOBAL, GLOBAL_MANDATORY, VALIANT, NONE
};

/*
 * Routing types:
 * -MIN: MINimal routing, chooses minimal path between
 * 		every source-destination pair. Needs DALLY as deadlock
 * 		avoidance mechanism.
 * -MIN_COND: MINimal CONDitional routing, when injecting and
 * 		next link is saturated, diverges traffic obliviously.
 * 		Otherwise, enroutes minimally. Needs DALLY as deadlock
 * 		avoidance mechanism.
 * -VAL: VALiant routing, balances load over the network
 * 		by employing an intermediate, non-minimal node
 * 		when injecting. Needs DALLY as deadlock avoidance
 * 		mechanism.
 * -VAL_ANY: VALiant ANY node routing, same as VAL but
 * 		allowing any node to be selected as intermediate
 * 		misrouting node, instead of restricting to a random
 * 		other-group. BEWARE!, source group nodes are not eligible.
 * -PAR: Progressive Adaptive Routing, ?????
 * 		Needs DALLY as deadlock avoidance mechanism.
 * -UGAL: Universal Globally-Adaptive Load-balanced
 * 		routing, chooses between minimal and Valiant routes
 * 		packet by packet, depending on the state of the
 * 		network. Needs DALLY as deadlock avoidance mechanism.
 * 		UGAL? or UGAL-Local???
 * -PB: PiggyBacking routing, broadcasts remote congestion
 *		information across the group through piggybacking
 *		to select between minimal and misroute paths
 *		(it is equivalent to UGAL Local+Global). Needs DALLY
 *		as deadlock avoidance mechanism.
 * -PB_ANY: PiggyBacking, but following VALiant-ANY procedure of
 * 		considering misroute as done when reached actual Valiant
 * 		node, not group.
 * -OFAR: On-the-Fly Adaptive Routing, adaptive in-transit misrouting.
 *		Decission is taken upon credits from global ports in
 *		current router. Needs a deadlock-free escape
 *		subnetwork as deadlock avoidance mechanism.
 * -RLM: Restricted Local Misrouting, imposes a restriction on local
 *		misrouting possible routes to prevent cyclic
 *		dependencies and thus become deadlock-free. Needs DALLY
 *		as deadlock avoidance mechanism (although it is not used like that).
 * -OLM: Opportunistic Local Misrouting, ???? Needs DALLY as deadlock avoidance mechanism.
 *
 */
enum RoutingType {
	MIN, MIN_COND, VAL, VAL_ANY, PAR, UGAL, PB, PB_ANY, OFAR, RLM, OLM
};

extern RoutingType g_routing;

extern int g_ugal_local_threshold;
extern int g_ugal_global_threshold;
extern int g_piggyback_coef;
extern int g_th_min;

/*
 * VC_misroute restriction:
 * if min output is a global link, prevent misroute when that
 * minimal global output is not congested. Global port is
 * determined to be congested when it exceeds a threshold based
 * on a configurable percentage of mean occupancy & a min.
 * threshold expressed in flits.
 */
extern bool g_vc_misrouting_congested_restriction;
extern int g_vc_misrouting_congested_restriction_coef_percent;
extern int g_vc_misrouting_congested_restriction_th; /* 		VC Misrouting additional congestion
 *																 restriction, expressed in flits */

/*
 * Global Misrouting policies (for those routing mechanisms that allow
 * 		misrouting through global links):
 * -CRG: Current Router Global, global link for misrouting is chosen
 * 		randomly among those in current router.
 * -RRG: Random Router Global, global link is chosen randomly among
 * 		any global link in any router at the source group (this is,
 * 		it selects link towards a group selected at random).
 * -NRG: Neighbor Router Global, a neighbor router in the group is
 * 		randomly chosen, and a global link is randomly chosen among
 * 		those in that router. This explicitly forbids those global
 * 		links in current router (unlike RRG).
 * -MM: Mixed Mode, hybrid policy that applies CRG at injection and
 * 		NRG at transit.
 */
enum GlobalMisroutingPolicy {
	CRG, CRG_L, RRG, RRG_L, NRG, MM, MM_L
};

extern GlobalMisroutingPolicy g_global_misrouting;

/*
 * Misrouting Trigger mechanism (when employing adaptive routing, it triggers
 * 		selection between minimal and non-minimal routes):
 * -CA: Contention Aware,
 * -CGA: ConGestion Aware, estimates contention upon number of
 * 		available credits in a given output port, and triggers
 * 		misrouting when it exceeds a configurable threshold.
 * -HYBRID: mix of CA & CGA, it triggers nonminimal path when
 * 		contention or congestion thresholds are exceeded.
 * -FILTERED: based on CA trigger, it adds an autoregressive
 * 		filter to somooth the variation of contention counters.
 * 		Experimental addition.
 * -DUAL: an improvement over CA trigger, employs TWO counters
 * 		instead of one, to allow discrimination between
 * 		"necessary", unavoidable contention, and "optative",
 * 		nonminimal contention. Experimental addition.
 * -CA_REMOTE: yet another improvement over CA trigger. It employs a set of
 *		partial counters for every router within the group. It
 *		updates its own partial counter the same as done in CA,
 *		but shares it every cycle with the other routers, and
 *		constructs a global counter out of the addition of all
 *		received partial counters. Experimental addition.
 * -HYBRID_REMOTE: mix of HYBRID an CA_REMOTE, keeps the idea of
 * 		triggering misrouting based on credits or counters
 * 		(whichever happens first) but exploiting the scheme of
 * 		partial counters shared amongst all routers of any group.
 * -WEIGTHED_CA: equal to CA, but weighting contention counter
 * 		when considering misrouting port candidates, to make
 * 		low contention port more eager to be selected.
 */
enum MisroutingTrigger {
	CA, CGA, HYBRID, FILTERED, DUAL, CA_REMOTE, HYBRID_REMOTE, WEIGTHED_CA
};

extern MisroutingTrigger g_misrouting_trigger;

//Variable thresholds. It is (roughly) equivalent
// to use congestion aware. If set to 1, sets a variable
// non minimal threshold as a percentage of queue occupancy
// in minimal path. If set to 0, it leaves that threshold
// as an absolute percentage. This happens with both local
// and global misroutes, so it favors misrouting when
// congestion is low, and prevents it when congestion raises.
// When using congestion aware, set to 1. When using
// contention aware only, set to 0.
/*TODO: this needs to be updated! Variable IS equivalent to
 test link saturation, but is used within hybrid models as well*/
extern bool g_relative_threshold;
extern int g_percent_local_threshold;
extern int g_percent_global_threshold;
extern bool g_contention_aware; /*			Contention Aware (CA) Trigger */
extern int g_contention_aware_th;
extern int g_contention_aware_global_th; /* This is used to determine whether to misroute or not
 *											 in the case of CA_REMOTE misrouting trigger */
extern bool g_increaseContentionAtHeader;	//TODO: is it useful to increase contention at header???
extern float g_filtered_contention_regressive_coefficient;

/***
 * Deadlock Avoidance mechanism (used with non- deadlock-free routings)
 */

/*
 * Types of Deadlock Avoidance mechanism:
 * -DALLY: Dally usage of VCs in ascending order when jumping from one link
 * 		to another.
 * -RING: Hamiltonian ring as escape, deadlock-free physical subnetwork.
 * 		It allows packets to advance without incuring in cyclic dependencies,
 * 		but at a cost of reduced throughput. It can be uni or bidirectional.
 * 		Used in combination with OFAR routing.
 * -EMBEDDED RING: same as RING, but embedded in normal transit links (employing
 * 		an exclusive set of VCs).
 * -EMBEDDED TREE: Tree-shaped escape, deadlock-free embedde subnetwork. As RING
 * 		and EMBEDDED RING, is used in combination with OFAR routing.
 */
enum DeadlockAvoidance {
	DALLY, RING, EMBEDDED_RING, EMBEDDED_TREE
};

extern DeadlockAvoidance g_deadlock_avoidance;

extern int g_rings; /* 							Number of rings employed (only for RING and EMBEDDED_RING
 *												 deadlock avoidance methods onlye) */
extern int g_ringDirs; //TODO: Should be turned into enumerate value, with 'NORING', 'UNIDIRECTIONAL_RING', 'BIDIRECTIONAL_RING' possible values
extern int g_ring_ports; /* 					Number of physical dedicated ring ports */
extern bool g_onlyRing2;
extern bool g_forbid_from_inj_queues_to_ring; /* NEVER send packets from inj.queues to the ring NOT CLEAR IF CAN ALSO BE USED WITH TREE ESCAPE SUBNETWORK */
extern int g_restrictLocalCycles;	//Restrict local cycles??
extern int g_globalEmbeddedRingSwitchesCount;
extern int g_localEmbeddedRingSwitchesCount;
extern int g_tree_root_node;
extern int g_tree_root_switch;
extern int g_localEmbeddedTreeSwitchesCount;
extern int g_channels_escape;

/*
 * Congestion Management mechanism (used when deadlock avoidance is done
 * 		through a escape subnetwork):
 * -BCM: Base Congestion Management, employs a bubble. ??
 * -ECM: Escape Congestion Management, employs a threshold value. ??
 */
enum CongestionManagement {
	BCM, ECM
};

extern CongestionManagement g_congestion_management;

extern int g_baseCongestionControl_bub; /*	Control Bubble in BCM */
extern int g_escapeCongestion_th; /* 		Escape Threshold in ECM */

/***
 * (Mis)Routing / Flow-Control Mechanism / Deadlock prevention (all mixed)
 */
//CRG? (CURRENT-ROUTER GLOBAL - Global link is chosen randomly among those in the current router)
extern int g_forceMisrouting; /* Enforce misrouting by any means */
extern bool g_try_just_escape;

/***
 * Statistics variables
 */
/* Latency */
extern long double g_flit_latency;
extern long double g_packet_latency;
extern long double g_injection_queue_latency;
extern long double g_base_latency;
extern long double g_warmup_flit_latency;
extern long double g_warmup_packet_latency;
extern long double g_warmup_injection_latency;
extern float *g_transient_record_latency;
extern float *g_transient_record_injection_latency;
extern long double *g_group0_totalLatency;	//Group 0, per switch average latency
extern long double *g_groupRoot_totalLatency;	//Root group, per switch average latency
//Latency histogram
extern int g_latency_histogram_maxLat;
extern long long * g_latency_histogram_no_global_misroute;
extern long long * g_latency_histogram_global_misroute_at_injection;
extern long long * g_latency_histogram_other_global_misroute;

/* Transmitted and received flits*/
extern long long g_tx_flit_counter;
extern long long g_rx_flit_counter;
extern long long g_attended_flit_counter;
extern long long g_tx_warmup_flit_counter;
extern long long g_rx_warmup_flit_counter;
extern long long g_min_flit_counter[];
extern long long g_global_misrouted_flit_counter[];
extern long long g_global_mandatory_misrouted_flit_counter[];
extern long long g_local_misrouted_flit_counter[];
extern int *g_transient_record_flits;
extern int *g_transient_record_misrouted_flits;
extern long long *g_group0_numFlits;
extern long long *g_groupRoot_numFlits;
extern float *g_transient_net_injection_latency;
extern float *g_transient_net_injection_inj_latency;
extern int *g_transient_net_injection_flits;
extern int *g_transient_net_injection_misrouted_flits;

/* Transmitted and received packets*/
extern long long g_tx_packet_counter;
extern long long g_rx_packet_counter;
extern long long g_tx_warmup_packet_counter;
extern long long g_rx_warmup_packet_counter;
extern long long g_injected_packet_counter;
extern long long g_injected_bursts_counter;

/* Hop counters */
extern long long g_total_hop_counter;
extern long long g_local_hop_counter;
extern long long g_global_hop_counter;
extern long long g_local_ring_hop_counter;
extern long long g_global_ring_hop_counter;
extern long long g_local_tree_hop_counter;
extern long long g_global_tree_hop_counter;
//Livelock control
extern long long g_max_hops;
extern long long g_max_local_hops;
extern long long g_max_global_hops;
extern long long g_max_local_subnetwork_hops;
extern long long g_max_global_subnetwork_hops;
extern long long g_max_local_ring_hops;
extern long long g_max_global_ring_hops;
extern long long g_max_local_tree_hops;
extern long long g_max_global_tree_hops;
extern long long g_max_global_tree_hops;
//Hops histogram
extern int g_hops_histogram_maxHops;
extern long long * g_hops_histogram;

/* Port and VC usage counters */
extern long long g_port_usage_counter[];
extern vector<vector<long long> > g_vc_counter;
extern long long g_port_contention_counter[];
extern long long g_subnetwork_injections_counter;
extern long long g_root_subnetwork_injections_counter;
extern long long g_source_subnetwork_injections_counter;
extern long long g_dest_subnetwork_injections_counter;
extern long double g_local_contention_counter;
extern long double g_global_contention_counter;
extern long double g_local_escape_contention_counter;
extern long double g_global_escape_contention_counter;
extern unsigned int g_petitions;
extern unsigned int g_served_petitions;
extern unsigned int g_injection_petitions;
extern unsigned int g_served_injection_petitions;
extern long long g_max_injection_packets_per_sw;
extern int g_sw_with_max_injection_pkts;
extern long long g_min_injection_packets_per_sw;
extern int g_sw_with_min_injection_pkts;
extern int g_transient_record_len;
extern int g_transient_record_num_cycles;
extern int g_transient_record_num_prev_cycles;
extern int g_AllToAll_generators_finished_count;

extern int g_burst_generators_finished_count;

extern long long g_max_subnetwork_injections;
extern long long g_max_root_subnetwork_injections;
extern long long g_max_source_subnetwork_injections;
extern long long g_max_dest_subnetwork_injections;

/*
 * Traces support
 */
enum TraceAssignation {
	CONSECUTIVE, INTERLEAVED
};
extern int g_num_traces;
extern vector<string> g_trace_file; /*			Trace filename */
extern vector<string> g_pcf_file; /*			PCF filename (used within traces support) */
extern vector<vector<long> > g_event_deadlock;
extern vector<long> g_trace_nodes;
extern vector<int> g_trace_instances;
extern vector<vector<bool> > g_trace_ended;
extern vector<vector<long> > g_trace_end_cycle;
extern TraceAssignation g_trace_distribution;
extern double g_cpu_speed;
extern long g_op_per_cycle;
extern long g_multitask;
extern long g_phit_size;
extern bool g_cutmpi;
extern map<long, long> g_events_map;
struct TraceNodeId {
	int trace_id, trace_node;
};
extern map<int, TraceNodeId> g_gen_2_trace_map;
extern vector<vector<vector<int> > > g_trace_2_gen_map;

#endif	/* GLOBAL_H */

