/*

 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2015 University of Cantabria

 FSIN Functional Simulator of Interconnection Networks
 Copyright (2003-2011) J. Miguel-Alonso, J. Navaridas

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

#ifndef _dimemas
#define _dimemas

/**
 * Enumerator that includes all the accepted operation id.
 */
enum op_id {
	CPU = 1,
	SEND = 2,
	RECEIVE = 3,
	COLLECTIVE = 10,
	EVENT = 20,
	FOPEN = 101,
	FREAD = 102,
	FWRITE = 103,
	FSEEK = 104,
	FCLOSE = 105,
	FDUP = 106,
	FUNLINK = 107,
	IOCOLL = 301,
	IOBLOCKNCOLL = 302,
	IOBLOCKCOLL = 303,
	IONBLOCKNCOLLBEGIN = 304,
	IONBLOCKNCOLLEND = 305,
	IONBLOCKCOLLBEGIN = 306,
	IONBLOCKCOLLEND = 307,
	ONESIDEGENOP = 401,
	ONESIDEFENCE = 402,
	ONESIDELOCK = 403,
	ONESIDEPOST = 404,
	LAPIOP = 501
};

/**
 * Enumerator that includes all the accepted definitions.
 */
enum def_t {
	COMMUNICATOR = 1, FILE_IO = 2, OSWINDOW = 3
};

/**
 * Enumerator that includes all the accepted send types.
 */
enum send_type {
	NONE_ = 0, RENDEZVOUS = 1, IMMEDIATE = 2, BOTH = 3
};

/**
 * Enumerator that includes all the accepted reception types.
 */
enum recv_t {
	RECV = 0, IRECV = 1, WAIT = 2
};

/**
 * Enumerator that includes all the accepted collective id.
 */
enum coll_t {
	OP_MPI_Barrier = 0,
	OP_MPI_Bcast = 1,
	OP_MPI_Gather = 2,
	OP_MPI_Gatherv = 3,
	OP_MPI_Scatter = 4,
	OP_MPI_Scatterv = 5,
	OP_MPI_Allgather = 6,
	OP_MPI_Allgatherv = 7,
	OP_MPI_Alltoall = 8,
	OP_MPI_Alltoallv = 9,
	OP_MPI_Reduce = 10,
	OP_MPI_Allreduce = 11,
	OP_MPI_Reduce_Scatter = 12,
	OP_MPI_Scan = 13
};

/**
 * Enumerator that includes some predefined event types.
 */
enum event_type {
	POINT2POINT = 50000001, COLLECTIVES = 50000002, OTHERS = 50000003
};

#define END 0 ///< Event end.
#define End 0 ///< Event end. just in case...
/**
 * Enumerator that includes some predefined point to point event types.
 */
enum p2p_ev_t {
	EV_MPI_Bsend = 33,
	EV_MPI_Bsend_init = 112,
	EV_MPI_Cancel = 40,
	EV_MPI_Recv_init = 116,
	EV_MPI_Send_init = 117,
	EV_MPI_Ibsend = 36,
	EV_MPI_Iprobe = 62,
	EV_MPI_Irecv = 4,
	EV_MPI_Irsend = 38,
	EV_MPI_Isend = 3,
	EV_MPI_Issend = 37,
	EV_MPI_Probe = 61,
	EV_MPI_Recv = 2,
	EV_MPI_Rsend = 35,
	EV_MPI_Rsend_init = 121,
	EV_MPI_Send = 1,
	EV_MPI_Sendrecv = 41,
	EV_MPI_Sendrecv_replace = 42,
	EV_MPI_Ssend = 34,
	EV_MPI_Ssend_init = 122,
	EV_MPI_Wait = 5,
	EV_MPI_Waitall = 6,
	EV_MPI_Waitany = 59,
	EV_MPI_Waitsome = 60
};

/**
 * Enumerator that includes some predefined collective event types.
 */
enum coll_ev_t {
	EV_MPI_Allgather = 17,
	EV_MPI_Allgatherv = 18,
	EV_MPI_Allreduce = 10,
	EV_MPI_Alltoall = 11,
	EV_MPI_Alltoallv = 12,
	EV_MPI_Barrier = 8,
	EV_MPI_Bcast = 7,
	EV_MPI_Gather = 13,
	EV_MPI_Gatherv = 14,
	EV_MPI_Reduce_scatter = 80,
	EV_MPI_Reduce = 9,
	EV_MPI_Scan = 30,
	EV_MPI_Scatter = 15,
	EV_MPI_Scatterv = 16
};

/**
 * Enumerator that includes some other event types.
 */
enum other_ev_t {
	MPI_Op_create = 78,
	MPI_Op_free = 79,
	MPI_Attr_delete = 81,
	MPI_Attr_get = 82,
	MPI_Attr_put = 83,
	MPI_Comm_create = 21,
	MPI_Comm_dup = 22,
	MPI_Comm_free = 25,
	MPI_Comm_group = 24,
	MPI_Comm_rank = 19,
	MPI_Comm_remote_group = 26,
	MPI_Comm_remote_size = 27,
	MPI_Comm_size = 20,
	MPI_Comm_split = 23,
	MPI_Comm_test_inter = 28,
	MPI_Comm_compare = 29,
	MPI_Group_difference = 84,
	MPI_Group_excl = 85,
	MPI_Group_free = 86,
	MPI_Group_incl = 87,
	MPI_Group_intersection = 88,
	MPI_Group_rank = 89,
	MPI_Group_range_excl = 90,
	MPI_Group_range_incl = 91,
	MPI_Group_size = 92,
	MPI_Group_translate_ranks = 93,
	MPI_Group_union = 94,
	MPI_Group_compare = 95,
	MPI_Intercomm_create = 96,
	MPI_Intercomm_merge = 97,
	MPI_Keyval_free = 98,
	MPI_Keyval_create = 99,
	MPI_Abort = 100,
	MPI_Error_class = 101,
	MPI_Errhandler_create = 102,
	MPI_Errhandler_free = 103,
	MPI_Errhandler_get = 104,
	MPI_Error_string = 105,
	MPI_Errhandler_set = 106,
	MPI_Finalize = 32,
	MPI_Get_processor_name = 107,
	MPI_Init = 31,
	MPI_Initialized = 108,
	MPI_Wtick = 109,
	MPI_Wtime = 110,
	MPI_Address = 111,
	MPI_Buffer_attach = 113,
	MPI_Buffer_detach = 114,
	MPI_Request_free = 115,
	MPI_Get_count = 118,
	MPI_Get_elements = 119,
	MPI_Pack = 76,
	MPI_Pack_size = 120,
	MPI_Start = 123,
	MPI_Startall = 124,
	MPI_Test = 39,
	MPI_Testall = 125,
	MPI_Testany = 126,
	MPI_Test_cancelled = 127,
	MPI_Testsome = 128,
	MPI_Type_commit = 129,
	MPI_Type_contiguous = 130,
	MPI_Type_extent = 131,
	MPI_Type_free = 132,
	MPI_Type_hindexed = 133,
	MPI_Type_hvector = 134,
	MPI_Type_indexed = 135,
	MPI_Type_lb = 136,
	MPI_Type_size = 137,
	MPI_Type_struct = 138,
	MPI_Type_ub = 139,
	MPI_Type_vector = 140,
	MPI_Unpack = 77,
	MPI_Cart_coords = 45,
	MPI_Cart_create = 43,
	MPI_Cart_get = 46,
	MPI_Cart_map = 47,
	MPI_Cart_rank = 48,
	MPI_Cart_shift = 44,
	MPI_Cart_sub = 49,
	MPI_Cartdim_get = 50,
	MPI_Dims_create = 51,
	MPI_Graph_get = 52,
	MPI_Graph_map = 53,
	MPI_Graph_neighbours = 54,
	MPI_Graph_create = 55,
	MPI_Grapdims_get = 56,
	MPI_Graph_neighbours_count = 57,
	MPI_Topo_test = 58
};

/**
 * Enumerator that includes all the defined LAPI operations.
 */
enum os_lapi_op_id {
	LAPI_Init = 1, LAPI_End = 2, LAPI_Put = 3, LAPI_Get = 4, LAPI_Fence = 5, LAPI_Barrier = 6, // May be LAPI_Gfence
	LAPI_Alltoall = 7 // May be LAPI_AddrInit
};

#endif
