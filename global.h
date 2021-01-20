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

#ifndef GLOBAL_H
#define	GLOBAL_H

#include <stdlib.h>
#include <fstream>
#include <assert.h>
#include <map>
#include <vector>
#include <limits.h>
#include <random>
#include <set>

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
extern int g_print_cycles; /*						Number of cycles between printing
 *														 temporary stats to the stdout. */
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
extern int g_local_res_channels; /*						Number of VCs in local links reserved for responses
 *														 (exclusive for reactive traffic patterns)*/
extern int g_global_res_channels; /*					Number of VCs in global links reserved for responses
 *														 (exclusive for reactive traffic patterns)*/
extern int g_segregated_flows; /*						Number of segregated traffic flows at output
 *														 ports; currently only used to segregate petitions
 *														 from responses at the consumption nodes. */
extern int g_flit_size; /* 								Flit size in phits */
extern int g_packet_size; /* 							Packet size in phits */
extern float g_injection_probability; /* 				Packet injection probability at a generator.
 *														 It ranges between 0 and 100 (it is expressed
 *														 as a percentage). When multiplied by packet
 *														 size and number of generators, gives total
 *														 injection rate within the network */
extern char *g_output_file_name; /* 					Results filename */
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
extern unsigned short g_cos_levels; /*					Number of Class of Service levels - Ethernet 802.1q */
extern bool g_print_hists; /*							Chooses whether to print latency and injection
 *														 histograms or not. */

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
extern default_random_engine g_reng; /*					Default random engine for any given distribution */

enum PortType {
	IN, OUT
};

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
 * Buffer types:
 * -SEPARATED:                 default option, it employs one
 *                                             buffer per port and VC.
 * -SHARED:   shared memory space between those
 *                                             VCs allocated in the same port. Each
 *                                             VC has a reserved part of the memory
 *                                             which can not be allocated to other
 *                                             VCs, and there is a shared pool of
 *                                             memory where packets from all VCs
 *                                             can be hosted. Code implementation
 *                                             considers separated buffer queues
 *                                             but establishes additional
 *                                             restrictions not to overcome total
 *                                             buffer space.
 */
enum BufferType {
	SEPARATED, DYNAMIC
};

/*
 * Flit types:
 * - PETITION:	petition packet for reactive traffic
 * - RESPONSE:	default option
 * - CNM:              congestion notification message
 * - SIGNAL:	end-of-message-dispatching signal (for Graph500 synthetic traffic model)
 * - ALLREDUCE: allreduce message (for Graph500 synthetic traffic model)
 */
enum FlitType {
	PETITION, RESPONSE, CNM, SIGNAL, ALLREDUCE
};

extern BufferType g_buffer_type;
extern int g_local_queue_reserved; /* Reserved queue space when using
 *                                                                             shared buffers for local queues */
extern int g_global_queue_reserved; /* Reserved queue space when using
 *                                                                             shared buffers for global queues */

/***
 * Arbiter types:
 * -RR:						Round-Robin.
 * -PrioRR:					Priority RR, gives priority to certain ports over others.
 * -LRS:					Least Recently Served, default option, it serves
 * 							 first the port which has been most time without
 * 							 being attended.
 * -PrioLRS:				Priority LRS, default option for output arbiters.
 * 							 It gives priority to certain ports over others,
 * 							 keeping them higher in the attendance order list
 * 							 regardless of the time they were last served.
 * 							 Useful to attend transit traffic before new injections,
 * 							 reducing the effective injection rate under
 * 							 congestion situations.
 * -AGE:					AGE arbiter, gives priority to ports whose head-of-
 * 							 -line packet has the oldest injection timestamp.
 * -PrioAGE:				Priority Age Arbiter, has two sets of ports with
 * 							 different priorities, upon the port priority given
 * 							 by the user, and ports are ordered through their
 * 							 timestamp among each set.
 */
enum ArbiterType {
	RR, PrioRR, LRS, PrioLRS, AGE, PrioAGE
};
extern ArbiterType g_input_arbiter_type;
extern ArbiterType g_output_arbiter_type;

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
 * -ADVERSARIAL Consecutive: all nodes in a
 * 			group send their traffic to groups
 * 			linked to the same local neighbor
 * 			router (this is roughly equivalent
 * 			to a mixed ADV+1,+2,..+H)
 * Oversubscribed ADVERSARIAL (oADV): every
 * 			router with the same offset within
 * 			its group target the same node.
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
 * -RANDOM UNIFORM REACTIVE
 * -ADVERSARIAL RANDOM NODE REACTIVE:
 * 			identical to ADV_RANDOM_NODE, but
 * 			triggering a response per every
 * 			petition received at a node.
 * -GRAPH500: Syntechic Traffic model of the Graph500 Communications
 * -HOTREGION: g_percent_traffic_to_congest % of the traffic is sent to the
 * 			first g_percent_nodes_into_region % of the destinations; the
 * 			rest is sent UN.
 * -HOTSPOT: g_percent_traffic_to_congest % of the traffic is sent to node g_hostspop_node;
 * 			the rest is sent UN.
 * -RANDOM PERMUTATION: each node sends to a selected unique destination during all simulation. The
 * permutation does not imply bidirectional assigment of each pair of nodes.
 */
enum TrafficType {
	UN,
	ADV,
	ADV_RANDOM_NODE,
	ADV_LOCAL,
	ADVc,
	oADV,
	ALL2ALL,
	MIX,
	TRANSIENT,
	SINGLE_BURST,
	BURSTY_UN,
	TRACE,
	UN_RCTV,
	ADV_RANDOM_NODE_RCTV,
	BURSTY_UN_RCTV,
	GRAPH500,
	HOTREGION,
	HOTREGION_RCTV,
	HOTSPOT,
	RANDOMPERMUTATION,
	RANDOMPERMUTATION_RCTV
};

extern TrafficType g_traffic;
extern bool g_reactive_traffic; /*			Triggered by the traffic type, determines if petitions trigger a response */

/* Adversarial traffic parameters */
extern int g_adv_traffic_distance; /*		Distance to the adverse traffic destination group */
extern int g_adv_traffic_local_distance; /* Distance to the adverse traffic destination router */
/* Auxiliary parameters (employed in many traffic types) */
extern TrafficType *g_phase_traffic_type;
extern int *g_phase_traffic_adv_dist;
extern float *g_phase_traffic_probability;
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
/* Reactive traffic parameters */
extern int g_max_petitions_on_flight; /*		Max number of petitions that can be issued without receiving their
 *												response  (-1 for infinite)*/
/* Graph 500 traffic variables */
extern int g_graph_coalescing_size; /*		Number of queries by message */
extern vector<int> g_graph_nodes_cap_mod; /*		Nodes with capabilities modified */
extern int g_graph_cap_mod_factor; /*		Capability modification factor (0..200] */
extern float g_graph_query_time; /*			Query consumption and generation time */
extern int g_graph_scale; /*				Base 2 log of the number of vertices in the graph*/
extern int g_graph_edgefactor; /*			Half of the average vertex degree */
extern vector<int> g_graph_tree_level; /*	Number of levels */
extern lognormal_distribution<float> g_graph_lognormal;
extern vector<int> g_graph_root_node; /*	Root node */
extern vector<int> g_graph_root_degree; /*	Number of edges connected to the root vertex */
extern vector<long long> g_graph_queries_remain; /*	Queries to send, initialized to maximum for the whole network */
extern vector<long long> g_graph_queries_rem_minus_means; /* Queries remaining minus means accumulated during levels */
extern long long ***g_graph_p2pmess_node2node; /* Number of p2p messages from node to node by stage */
extern int g_graph_max_levels; /*			Maximum number of levels */
extern long long g_graph_p2pmess; /*		P2P messages sent during execution of Graph500 simulation */
/*												response  (-1 for infinite)*/
/* End-point congestion traffic parameters */
extern int g_percent_traffic_to_congest; /* The percentage of the total traffic which is sent to congested hosts */
/* Hot-region traffic parameters */
extern float g_percent_nodes_into_region; /* The percentage of the total nodes to consider as a hot-region */
/* Hot-spot traffic parameters */
extern int g_hotspot_node; /* Node to consider as a hot-spot */
/* Random permutation auxiliar variable */
extern vector<int> g_available_generators;

/***
 * Routing mechanism
 */

/*
 * Misrouting types at port level:
 * -LOCAL
 * -LOCAL_MM use in VALIANT for local hop in src_group if destination sw is on source group
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
 * -OBL: OBLivious non-minimal routing, chooses a
 * 		random path across an intermediate group linked to
 * 		the source router or a neighbor router in the source
 * 		group, depending on the global misrouting policy.
 * -ACOR: Adaptive Congestion Oblivious Routing, chooses a random path following
 *              the next sequence:
 *              - chooses a random group linked to the source router
 *              - chooses a random node in a group linked to a neighbord router in the source group
 *          Needs DALLY as deadlock avoidance mechanism.
 *          Needs a _RECOMP ValiantType.
 *          Uses a restricted version of Global Misrouting policy on each case.
 * -PB_ACOR: PiggyBacking using ACOR for the non-minimal path.
 * -SRC_ADP: SouRCe ADaPtive non-minimal routing,
 * 		selects between the minimal and a non-minimal path
 * 		at injection based on the occupancy of remote queues
 * 		(in a congestion-based PiggyBackin-alike misrouting
 * 		trigger). Misrouting path depends on the global
 * 		misrouting policy in use.
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
 * -CAR: Continuosly Adaptive Routing, similar to OLM but with simpler
 * 		misrouting decisions: it decides between a (set at injection) Valiant
 * 		route and the minimal path either at injection, at the source group
 * 		after a minimal local hop, or at the intermediate group before reaching
 * 		Valiant node. Decision is based on the occupancy of the minimal and
 * 		nonminimal queues and two parameters, a factor and a threshold.
 */
enum RoutingType {
	MIN, MIN_COND, VAL, VAL_ANY, OBL, ACOR, PB_ACOR, SRC_ADP, PAR, UGAL, PB, PB_ANY, OFAR, RLM, OLM, CAR
};

extern RoutingType g_routing;

/***
 * Type of Valiant routing (VALIANT and VALIANT_LOCAL misrouting types)
 * - FULL: intermediate sw can be in source and destination group
 * - *SRCEXC: intermediate sw can not be in source group (legacy default)
 * - DSTEXC: intermediate sw can not be in destination group (not implemented)
 * - SRCDSTEXC: intermediate sw could not be in source or destination group (not implemented)
 * -          *_RECOMP: allow the recomputation of the intermediate sw
 */
enum ValiantType {
	FULL, SRCEXC, DSTEXC, SRCDSTEXC,
        FULL_RECOMP, SRCEXC_RECOMP, DSTEXC_RECOMP, SRCDSTEXC_RECOMP
};

extern ValiantType g_valiant_type;

/***
 * Type of Valiant misrouting destination used in OBL routing
 * - GROUP : same as VAL
 * - *SWITCH : same as VAL_ANY
 */
enum valiantMisroutingDestination { GROUP, NODE };

extern valiantMisroutingDestination g_valiant_misrouting_destination;

extern int g_ugal_local_threshold;
extern int g_ugal_global_threshold;
extern int g_piggyback_coef;
extern int g_th_min;
extern bool g_reset_val; /*	Recalculate VAL node with SRC_ADP/OBL routing at injection if at the previous cycle
 *									has not advanced through non-minimal route */

/***
 * Status of Adaptive Congestion Oblivious Routing for each packet/switch
 * - None: flit just generated - only used for packets, switch starts on CRGLGr
 * - CRGLGr: Current Router Global Limited Group - misrouted hops: 1 global
 * - CRGLSw: Current Router Global Limited Switch - misrouted hops: 1 global + 1 local
 * - RRGLSw: Random Router Global Limited Switch - misrouted hops: 1 local + 1 global + 1 local
 */
enum acorState {None, CRGLGr, CRGLSw, RRGLSw};

/***
 * ACOR state management per:
 * - PACKET.*: the state of each packet is controlled individually
 * - *SWITCH.*: each switch has a minimum ACOR state and each packet takes this as a minimum. To
 * change the status of the switch, a hysteresis cycle is used. Note than a packet could be 
 * routed with a status bigger than the minimum state imposed by switch.
 * .* can be the following options:
 * -- *CGCSRS: CRGLGr <--> CRGLSw <--> RRGLSw
 * -- CGRS: CRGLGr <--> RRGLSw
 * -- CSRS: CRGLSw <--> RRGLSw
 */
enum acorStateManagement {PACKETCGCSRS, PACKETCGRS, PACKETCSRS, SWITCHCGCSRS, SWITCHCGRS, SWITCHCSRS};

extern acorStateManagement g_acor_state_management;

/***
 * Parameters to control ACOR per switch management hysteresis cycle
 * - acor_hysteresis_cycle_duration_cycles : Duration of each hysteresis cycle on switch cycles
 * - acor_inc_state_first_th_packets : Minimum number of packets blocked during the hysteresis cycle to 
 * trigger the change to second status.
 * - acor_dec_state_first_th_packets : Maximum number of packets blocked during the hysteresis cycle to
 * trigger the change from second status to first.
 * - acor_inc_state_second_th_packets : Minimum number of packets blocked during the hysteresis cycle to 
 * trigger the change from second status to third.
 * - acor_dec_state_second_th_packets : Maximum number of packets blocked during the hysteresis cycle to
 * trigger the change from third status to second.
 */
extern int g_acor_hysteresis_cycle_duration_cycles;
extern int g_acor_inc_state_first_th_packets;
extern int g_acor_dec_state_first_th_packets;
extern int g_acor_inc_state_second_th_packets;
extern int g_acor_dec_state_second_th_packets;

/*
 * VC usage:
 * -Base: former VC usage (as in Marina's PhD. thesis), reusing
 * 		VC labelling for local and global channels (i.e., a hop
 * 		from a local link with VC 0 to a global link keeps VC 0).
 * -Flexible: new VC usage, allowing every hop to choose within
 * 		a range of possible VCs (rule to choose is low occupancy
 * 		first), keeping a possible increasing VC path, whereas
 * 		followed or not. Further detailed in flexible_routing.h
 * 		This routing can have different allocation mechanisms.
 * -Table-based FlexVC: identical to Flexible in concept, but determining the VC based on tables rather than
 * 		algorithms.
 */
enum VcUsageType {
	BASE, FLEXIBLE, TBFLEX
};
extern VcUsageType g_vc_usage;

enum VcAllocationMechanism {
	HIGHEST_VC, LOWEST_VC, LOWEST_OCCUPANCY, RANDOM_VC
};
extern VcAllocationMechanism g_vc_alloc;

/*
 * Injection VC policy:
 * -DEST: DESTination-based, divides the injection VCs by the
 * 		number of destinations in the network, so that each
 * 		injection VC has an even (or close to) share of the
 * 		injections. Allocation is conducted via a modulo
 * 		operation. This alleviates HOL-Blocking.
 * -RAND: RANDom allocation, it selects any random VC whose
 * 		corresponding buffer has space to store the flit.
 * 		This potentially increases HOL-Blocking but slightly
 * 		increases the injection rate.
 */
enum VcInjectionPolicy {
	DEST, RAND
};

extern VcInjectionPolicy g_vc_injection;

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
 * -CRG_L: CRG if destination group is not the same as source, in this case,
 *         selects one random local to make a misrouting hop.
 * -RRG: Random Router Global, global link is chosen randomly among
 * 		any global link in any router at the source group (this is,
 * 		it selects link towards a group selected at random).
 * -RRG_L: RRG if destination group is not the same as source, in this case,
 *         select one random local to make a misrouting hop.
 * -NRG: Neighbor Router Global, a neighbor router in the group is
 * 		randomly chosen, and a global link is randomly chosen among
 * 		those in that router. This explicitly forbids those global
 * 		links in current router (unlike RRG).
 * -MM: Mixed Mode, hybrid policy that applies CRG at injection and
 * 		NRG at transit.
 */
enum GlobalMisroutingPolicy {
	CRG, CRG_L, RRG, RRG_L, NRG, NRG_L, MM, MM_L
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
 * -WEIGHTED_CA: equal to CA, but weighting contention counter
 * 		when considering misrouting port candidates, to make
 * 		low contention port more eager to be selected.
 */
enum MisroutingTrigger {
	CA, CGA, HYBRID, FILTERED, DUAL, CA_REMOTE, HYBRID_REMOTE, WEIGHTED_CA
};

extern MisroutingTrigger g_misrouting_trigger;

/*
 * Congestion detection policy:
 * -PER_PORT: checks total credit occupancy for the port.
 * -PER_VC:	measures credit occupancy for the VC buffer.
 * -PER_PORT_MIN: checks only the credit occupancy of the
 * 		port corresponding to minimally routed packets
 * 		(excluding those packets that have been misrouted
 * 		at some point of their path).
 * -PER_VC_MIN: identical to PER_PORT_MIN, but considering
 * 		the VC buffer instead of the whole port.
 * -PER_GROUP: checks those VCs that can be used for
 *		packets at their source group.
 * -HISTORY_WINDOW: tracks the congestion values for the
 * 		last previous cycles, a la Cascade.
 * -HISTORY_WINDOW_AVG: same as HISTORY_WINDOW, but comparing
 * 		the occupancy in the minimal queue against the
 * 		average of all the ports in the current router
 */
enum CongestionDetection {
	PER_PORT, PER_VC, PER_PORT_MIN, PER_VC_MIN, PER_GROUP, HISTORY_WINDOW, HISTORY_WINDOW_AVG
};

extern CongestionDetection g_congestion_detection;

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
extern int g_contention_aware_local_th; /*	Optionally employed in base CA implementation, to
 *											alleviate contention detection problems under
 *											certain traffic patterns. */
extern int g_contention_aware_global_th; /* This is used to determine whether to misroute or not
 *											 in the case of CA_REMOTE misrouting trigger */
extern bool g_increaseContentionAtHeader;	//TODO: is it useful to increase contention at header???
extern float g_filtered_contention_regressive_coefficient;
extern float g_car_misroute_factor; /*		Factor for misrouting decision in flexOppRouting */
extern float g_car_misroute_th; /*			Threshold (in flits) for misrouting decision in flexOppRouting */
extern int g_local_window_size; /*			Size of the congestion window, when HISTORY_WINDOW detection is used */
extern int g_global_window_size;

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
 * 		through a escape subnetwork and for 802.1Qau):
 * -BCM: Base Congestion Management, employs a bubble. ??
 * -ECM: Escape Congestion Management, employs a threshold value. ??
 * -PAUSE: Per priority flow control mechanism (employs credits for determine the status of ports)
 * -QCNSW: Congestion management which leverages QCN messages for change routing - Reaction point in switches.
 */
enum CongestionManagement {
	BCM, ECM, PAUSE, QCNSW
};

extern CongestionManagement g_congestion_management;

extern int g_baseCongestionControl_bub; /*	Control Bubble in BCM */
extern int g_escapeCongestion_th; /* 		Escape Threshold in ECM */

/* Quantized Congestion Notification - 802.1Qau */
extern float g_qcn_gd; /* Multiplied times the QCN field reveived in a CNM to decrease target rate */
extern int g_qcn_bc_limit; /* Byte count limit for one cycle */
extern float g_qcn_timer_period; /* Time limit for one cycle */
extern float g_qcn_r_ai; /* The rate, in % of injection probability, used to increase target rate in the AI phase */
extern float g_qcn_r_hai; /* The rate, in % of injection probability, used to increase target rate in the HAI phase */
extern int g_qcn_fast_recovery_th; /* Number of cycles in fast recovery phase */
extern float g_qcn_min_rate; /* The minimun rate accepted for rate limiters, expressed as % of injection probability */
extern unsigned int g_qcn_q_eq; /* The reference point of a queue. QCN aims to keep the queue occupancy at this level */
extern float g_qcn_w; /* Weight to be given to the change in queue length in the calculation of feedback value */
extern float g_qcn_c; /* The speed of a link where a rate limiter is installed expressed as % of injection probability  */
extern long long g_qcn_queue_length; /* QCN link queue size in phits */
extern unsigned int g_qcn_cp_sampling_interval; /* Switch occupancy sampling interval - Occupancy sampling for terabit cee switches */
extern int g_qcn_port; /* Port for sending CNM - last port of switch */
extern int g_qcn_th1; /* avg + th1 set start point at which the min probability is increased */
extern int g_qcn_th2; /* avg + th2 set start point at which the min probability is reduced */
extern int g_qcn_cnms_percent; /* % of QCN CNMs sent */
extern float ***g_qcn_g0_port_enroute_min_prob; /* PortEnruteMinProb for switches of group zero during whole simulation */
extern bool g_qcn_transient_stats; /* Enable or disable qcn transient stats for group 0 */
extern int ***g_qcn_g0_port_congestion; /* Fb value ports of switches of group zero during whole simulation */

/*
 * QCNSW alternative implementations:
 * -QCNSWBASE: Reaction point in switch which receives the notification
 * -QCNSWOUT: Reaction point in switch and sampling in output queues
 * -QCNSWFBCOMP: QCNSWBASE + feedback comparison between ports
 * -QCNSWOUTFBCOMP: QCNSWOUT + feedback comparison between ports
 * -QCNSWSELF: CP notifies to other node and reuse the same notification for adapt its routing table
 * -QCNSWCOMPLETE: QCNSWBASE + QCNSWSELF + QCNSEFBCOMP
 */
enum QcnSwImplementation {
	QCNSWBASE, QCNSWOUT, QCNSWFBCOMP, QCNSWOUTFBCOMP, QCNSWSELF, QCNSWCOMPLETE
};

extern QcnSwImplementation g_qcn_implementation;

/*
 * QCNSW policy to manipulate probability:
 * -AIMD : Additive increase - Multiplicative decrease (default)
 * -AIAD : Additive increase - Additive decrease
 * -MIMD : Multiplicative increase - Multiplicative decrease
 * -MIAD : Multiplicative decrease - Additive decrease
 */
enum QcnSwPolicy {
    AIMD, AIAD, MIMD, MIAD
};

extern QcnSwPolicy g_qcn_policy;

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
extern long double g_response_latency;
extern float *g_transient_record_latency;
extern float *g_transient_record_injection_latency;
extern long double *g_group0_totalLatency;	//Group 0, per switch average latency
extern long double *g_groupRoot_totalLatency;	//Root group, per switch average latency
//Latency histogram
extern int g_latency_histogram_maxLat;
extern vector<long long> g_latency_histogram_no_global_misroute;
extern vector<long long> g_latency_histogram_global_misroute_at_injection;
extern vector<long long> g_latency_histogram_other_global_misroute;

/* Transmitted and received flits*/
extern long long g_tx_flit_counter;
extern long long g_tx_cnmFlit_counter;
extern long long g_tx_flit_counter_printC;
extern long long g_rx_flit_counter;
extern long long g_rx_cnmFlit_counter;
extern long long g_rx_acorState_counter[];
extern long long g_rx_flit_counter_printC;
extern long long g_attended_flit_counter;
extern long long g_tx_warmup_flit_counter;
extern long long g_tx_warmup_cnmFlit_counter;
extern long long g_rx_warmup_flit_counter;
extern long long g_rx_warmup_cnmFlit_counter;
extern long long g_rx_warmup_acorState_counter[];
extern long long g_response_counter;
extern long long g_response_warmup_counter;
extern long long g_nonminimal_counter;
extern long long g_nonminimal_warmup_counter;
extern long long g_nonminimal_inj;
extern long long g_nonminimal_warmup_inj;
extern long long g_nonminimal_src;
extern long long g_nonminimal_warmup_src;
extern long long g_nonminimal_int;
extern long long g_nonminimal_warmup_int;
extern long long g_min_flit_counter[];
extern long long g_global_misrouted_flit_counter[];
extern long long g_global_mandatory_misrouted_flit_counter[];
extern long long g_local_misrouted_flit_counter[];
extern int *g_transient_record_flits;
extern int *g_transient_record_misrouted_flits;
extern long long ***g_group0_numFlits;
extern int **g_acor_group0_sws_packets_blocked;
extern int **g_acor_group0_sws_status;
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
extern long long g_max_injection_cnmPackets_per_sw;
extern int g_sw_with_max_injection_pkts;
extern int g_sw_with_max_injection_cnmPkts;
extern long long g_min_injection_packets_per_sw;
extern long long g_min_injection_cnmPackets_per_sw;
extern int g_sw_with_min_injection_pkts;
extern int g_sw_with_min_injection_cnmPkts;
extern int g_transient_record_len;
extern int g_transient_record_num_cycles;
extern int g_transient_record_num_prev_cycles;
extern int g_AllToAll_generators_finished_count;

extern int g_burst_generators_finished_count;

extern long long g_max_subnetwork_injections;
extern long long g_max_root_subnetwork_injections;
extern long long g_max_source_subnetwork_injections;
extern long long g_max_dest_subnetwork_injections;

extern set<int> g_verbose_switches;
extern int g_verbose_cycles;

/*
 * Traces support
 */
enum TraceAssignation {
	CONSECUTIVE, INTERLEAVED, RANDOM
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

