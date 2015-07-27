/**
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

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <vector>
#include <string.h>
#include <fstream>

#include "../global.h"
#include "dimemas.h"
#include "trace.h"
#include "event.h"
#include "generatorModule.h"
#include "../communicator.h"

typedef struct {
	long root;
	long type;
	long collective_comm_id;
	long communicator_id;
} mpi_collective_comm_t;

/**
 * The trace reader dispatcher selects the format type and calls to the correct trace read.
 *
 * The selection reads the first character in the file. This could be: '#' for dimemas, 
 * 'c', 's' or 'r' for fsin trc, and '-' for alog (in complete trace the header is "-1",
 * or in filtered trace could be "-101" / "-102"). This is a very naive decision, so we
 * probably have to change this, but for the moment it works.
 * 
 *@see read_dimemas
 *@see read_fsin_trc
 *@see read_alog
 */
void read_trace(int trace_id, vector<int> instances) {
	FILE * ftrc;
	char c;
	cout << ">>read_trace: " << g_trace_file[trace_id] << endl;

	if ((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) == NULL) {
		cout << "Trace file not found in current directory" << endl;
	}
	assert((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) != NULL);
	c = (char) fgetc(ftrc);
	fclose(ftrc);

	switch (c) {
		case '#':
			read_dimemas(trace_id, instances);
			break;
		case -1:
			cout << "Reading empty file\n" << endl;
			break;
		default:
			cout << "Cannot understand this trace format" << endl;
			break;
	}
	cout << "<<read_trace" << endl;
}

/**
 * Reads a trace from a dimemas file.
 *
 * Read a trace from a dimemas file whose name is in global variable #g_trace_file
 * It only consideres events for CPU and point to point operations. File I/O could be
 * considered as a cpu event if FILEIO is defined.
 */
void read_dimemas(int trace_id, vector<int> instances) {
	FILE * ftrc;
	char buffer[BUFSIZE];
	long n; ///< The number of nodes is read here.
	char sep[] = ":"; ///< Dimemas record separator.
	char tsep[] = "(),"; ///< Separators to get the task info.

	char *op_id; ///< Operation id.
	long type, ev_type; ///< Global operation id(for collectives), point-to-point type.
	long task_id; ///< Task id.
	long th_id; ///< Thread id.
	long t_id; ///< The other task id. (sender or receiver)
	long comm_id = -1; ///< The communicator id.

	double cpu_burst; ///< CPU time(in seconds).
	long size; ///< Message size(in bytes). 
	long tag; ///< Tag of the MPI operation.
	long comm; ///< Communicator id.

	long rtask_id; ///< Root task in collectives.
	long rth_id; ///< Root thread in collectives.
	long bsent; ///< Bytes sent in collectives.
	long brecv; ///< Bytes received in collectives.
	event ev; ///< The read event.
	long x;
	long value;
	long root;
	enum coll_ev_t mpi_task = (enum coll_ev_t) -1;
	long mpi_type = -1, mpi_sendsize = -1, mpi_recvsize = -1, mpi_communicator = -1, mpi_isroot = 0;
	const long mpilist_allocated = 65536;

	mpi_collective_comm_t mpilist_aux;
	map<long, vector<mpi_collective_comm_t> > mpilist_map; // Each communicator has it list of collectives
	map<long, long> mpilist_comm_to_offset;
	map<long, communicator*> communicator_map;

	cout << ">>read_dimemas" << endl;

	if ((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) == NULL)
		cout << "Trace file not found in current directory" << endl;
	assert((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) != NULL);
	while (fgets(buffer, BUFSIZE, ftrc) != NULL) {
		op_id = strtok(buffer, sep);

		if (!strcmp(op_id, "#DIMEMAS")) {
			/* Sanity check: determine if number of trace nodes upon trace header equals
			 * number of trace nodes as given by the parameter in config file. */
			strtok(NULL, sep);
			strtok(NULL, sep);
			if (atoi(strtok(NULL, "(")) != g_trace_nodes[trace_id]) {
				cout << "Number of nodes in trace doesn't match spec in config file!!" << endl;
				assert(0);
			}
		} else if (!strcmp(op_id, "d")) {
			// Si la linea de la traza es una definicion de: communicator, I/O file, OSWINDOW
			type = atol(strtok(NULL, sep));
			switch (type) {
				case COMMUNICATOR:
					comm_id = atol(strtok(NULL, sep));
					assert(comm_id != -1);
					assert(communicator_map.count(comm_id) == 0);
					communicator_map[comm_id] = new communicator();
					communicator_map[comm_id]->id = comm_id;
					communicator_map[comm_id]->size = atol(strtok(NULL, sep));
#if DEBUG
					cout << "comm_id" << communicator_map[comm_id]->id << endl;
					cout << "comm_size" << communicator_map[comm_id]->size << endl;
#endif
					for (long pocess_count = 0; pocess_count < communicator_map[comm_id]->size; pocess_count++) {
						long proc = atol(strtok(NULL, sep));
						communicator_map[comm_id]->process[pocess_count] = proc;
#if DEBUG
						cout << "Proc: " << communicator_map[comm_id]->process[pocess_count] << endl;
#endif

					}

					break;
				case FILE_IO:
					cerr << "Not implemented definition" << endl;
					assert(0);
					break;
				case OSWINDOW:
					cerr << "Not implemented definition" << endl;
					assert(0);
					break;
				default:
					cerr << "Wrong definition" << endl;
					assert(0);
			}
		}

		if (atol(op_id) != EVENT) continue;
		task_id = atol(strtok(NULL, sep)); //We have the task id.
		th_id = atol(strtok(NULL, sep)); //We have the thread id.
		ev_type = atol(strtok(NULL, sep));
		type = g_events_map[ev_type];
#if DEBUG
		cout << "ev_type:" << ev_type << endl;
		cout << "type" << type << endl;
#endif
		value = atol(strtok(NULL, sep));
		switch (type) {
			case 50100003:		//   Root in MPI Global OP
				if (value != 1) cerr << "root flag != 1" << endl;
				assert(value == 1);
				mpilist_aux.root = task_id;
				break;
			case 50100004:			//   Communicator in MPI Global OP
#if DEBUG
			cout << "First Pass with communicator!=1" << endl;
			cout << "Comunicator=" << value << endl;
#endif
				mpilist_aux.communicator_id = value;
				break;
			case 50000002:			//    MPI Collective Comm
				if (value == 0) {
					//END
					//Put the aux structure into the comm list

					long comm_id = mpilist_aux.communicator_id;
#if DEBUG
					cout << "END" << endl;
					cout << "communicator_id:" << comm_id << endl;
#endif

					if (mpilist_map.count(comm_id) == 0) {
						assert(mpilist_comm_to_offset.count(comm_id) == 0);
					}
#if DEBUG
					cout << "communicator_id count:" << mpilist_map.count(comm_id) << endl;
#endif
					//Check if the coll. array for the current communicator exists and if not, create it with -1 values
					if (mpilist_map.count(comm_id) <= 0) {
#if DEBUG
						cout << "communicator_id count:" << mpilist_map.count(comm_id) << endl;
#endif
						mpilist_map[comm_id].resize(mpilist_allocated);
						for (x = 0; x < mpilist_allocated; x++) {
							(mpilist_map[comm_id])[x].root = -1;
							(mpilist_map[comm_id])[x].type = -1;
						}
					}

					if (mpilist_map[comm_id][mpilist_comm_to_offset[comm_id]].type == -1) {
						mpilist_map[comm_id][mpilist_comm_to_offset[comm_id]].collective_comm_id =
								mpilist_comm_to_offset[comm_id];
						mpilist_map[comm_id][mpilist_comm_to_offset[comm_id]].root = mpilist_aux.root;
						mpilist_map[comm_id][mpilist_comm_to_offset[comm_id]].type = mpilist_aux.type;
						mpilist_aux.type = -1; //For the next iter, to find it available
						mpilist_comm_to_offset[comm_id]++; //This comm offset needs to be increased.

						if (mpilist_comm_to_offset[comm_id] >= mpilist_allocated) cerr << "MPI overflow!" << endl;
						assert(mpilist_comm_to_offset[comm_id] < mpilist_allocated);
					}

				} else {
#if DEBUG
					cout << "START" << endl;
#endif
					//START
					if (mpi_task != task_id) {
						mpi_task = (enum coll_ev_t) task_id;
						mpilist_comm_to_offset.clear(); //Remove the offsets for the new process to check the already exisiting comms.
					}
					mpilist_aux.type = value;
				}
				break;
			default:
				//Nothing to do
				break;
		}
	}
	fclose(ftrc);

	mpilist_comm_to_offset.clear();
	mpi_task = (enum coll_ev_t) -1;

	//Normal pass
	if ((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) == NULL)
		cerr << "Trace file not found in current directory" << endl;
	assert((ftrc = fopen(g_trace_file[trace_id].c_str(), "r")) != NULL);

	if (fgets(buffer, BUFSIZE, ftrc) == NULL) cerr << "Error accessing trace file" << endl;
	if (strncmp("#DIMEMAS", buffer, 8)) cerr << "Header line is missing, maybe not a dimemas file" << endl;
	assert(!strncmp("#DIMEMAS", buffer, 8));

	strtok(buffer, sep); // Drops the #DIMEMAS.
	strtok(NULL, sep); // Drops trace_name.
	strtok(NULL, sep); // Offsets are dropped here.
	n = atol(strtok(NULL, tsep)); // task info has diferent separators
	if (n > g_trace_nodes[trace_id]) {
		cout << "n=" << n << " g_trace_nodes=" << g_trace_nodes[trace_id] << endl;
		cout << "There are not enough nodes for running this trace" << endl;
	}
	assert(n <= g_trace_nodes[trace_id]);

	bool tracestop[n]; //Used if g_cutmpi to stop at MPI_Finalize (indexed by task_id)
	memset(tracestop, 0, sizeof(tracestop)); //Default to false
	while (fgets(buffer, BUFSIZE, ftrc) != NULL) {
		op_id = strtok(buffer, sep);
		if (!strcmp(op_id, "s")) // Offset
			// As we parse the whole file, it is not important for us.
			continue;

		if (!strcmp(op_id, "d")) { // Definitions.
			type = atol(strtok(NULL, sep));
			switch (type) {
				case COMMUNICATOR:
					// Not implemented yet.
					break;
				case FILE_IO:
					// It doesn't care about what files are accessed during the execution of the traces.
					// It doesn't even if the FILEIO is active.
					break;
				case OSWINDOW:
					// We could treat the one side windows as MPI communicators.
					break;
				default:
					cerr << "Wrong definition" << endl;
					assert(0);
			}
			continue;
		}
		task_id = atol(strtok(NULL, sep)); //We have the task id.
		if (task_id > n || task_id < 0) cerr << "Task id not defined: Aborting" << endl;
		assert(!(task_id > n || task_id < 0));
		if (tracestop[task_id]) continue;
		th_id = atol(strtok(NULL, sep)); //We have the thread id.
		switch (atol(op_id)) {
			case CPU:
				cpu_burst = atof(strtok(NULL, sep)); //We have the time taken by the CPU.
				ev.type = COMPUTATION;
				ev.length = (long) ceil(0.001 + ((double) cpu_burst * g_cpu_speed) / g_op_per_cycle); // Computation time.
				ev.count = 0; // Elapsed time.
				ev.mpitype = (enum coll_ev_t) 0;
				if (task_id < g_trace_nodes[trace_id] && task_id >= 0) {
					ev.pid = task_id;
					ins_event(g_generators_list, trace_id, instances, task_id, ev);
				} else {
					cerr << "Adding cpu event into a non defined CPU. Task_id = " << task_id << endl;
					assert(0);
				}
				break;

			case SEND:
				t_id = atol(strtok(NULL, sep)); //We have the destination task id.
				if (t_id > g_number_generators * g_multitask || t_id < 0)
					cerr << "Destination task id is not defined: Aborting" << endl;
				assert(!(t_id > g_number_generators * g_multitask || t_id < 0));
				size = atol(strtok(NULL, sep)); //We have the size.
				tag = atol(strtok(NULL, sep)); //We have the tag.
				comm = atol(strtok(NULL, sep)); //We have the communicator id.
				type = atol(strtok(NULL, sep)); //We have the send type (I, B, S or -).
				switch (type) {
					case NONE_: // This should be Bsend (buffered)
					case RENDEZVOUS: // This should be Ssend (synchronized)
					case IMMEDIATE: // This should be Isend (inmediate)
					case BOTH: // This should be Issend(inmediate & synchronized)
						ev.type = SENDING;
						if (task_id != t_id) { // Valid event
							ev.task = tag; // Type of message
							ev.length = size; // Length of message
							ev.count = 0; // Packets sent or received
							if (ev.length == 0) ev.length = 1;
							ev.length = (long) ceil((double) ev.length / (g_packet_size * g_phit_size));
							ev.mpitype = (enum coll_ev_t) 0;
							if (task_id < g_trace_nodes[trace_id] && t_id < g_trace_nodes[trace_id] && task_id >= 0
									&& t_id >= 0) {
								ev.pid = t_id;
								ins_event(g_generators_list, trace_id, instances, task_id, ev);
							} else {
								cerr << "Adding comm event into a non defined CPU" << endl;
								assert(0);
							}
						}
						break;
					default:
						cout << "WARNING: There is an Unexpected Send type" << type << "!!!\n" << endl;
						continue;
				}
				break;

			case RECEIVE:
				t_id = atol(strtok(NULL, sep)); //We have the source task id.
				if (t_id > g_number_generators || t_id < 0)
					cerr << "Source task id:" << t_id << " is not defined: Aborting. nProcs=" << g_number_generators
							<< endl;
				assert(!(t_id > g_number_generators/* *g_multitask */|| t_id < 0));
				size = atol(strtok(NULL, sep)); //We have the size.
				tag = atol(strtok(NULL, sep)); //We have the tag.
				comm = atol(strtok(NULL, sep)); //We have the communicator id.
				type = atol(strtok(NULL, sep)); //We have the recv type (Recv, Irecv or Wait).
				switch (type) {
					case IRECV: // This is not useful for us.
						break;
						// A reception and a wait is the same for us.
					case RECV:
					case WAIT:
						ev.type = RECEPTION;
						if (t_id != task_id) { // Valid event
							ev.task = tag; // Type of message
							ev.length = size; // Length of message
							ev.count = 0; // Packets sent or received
							if (ev.length == 0) ev.length = 1;
							ev.length = (long) ceil((double) ev.length / (g_packet_size * g_phit_size));
							ev.mpitype = (enum coll_ev_t) 0;
							if (task_id < g_trace_nodes[trace_id] && t_id < g_trace_nodes[trace_id] && task_id >= 0
									&& t_id >= 0) {
								ev.pid = t_id;
								ins_event(g_generators_list, trace_id, instances, task_id, ev);
							} else {
								cerr << "Adding comm event into a non defined CPU" << endl;
								assert(0);
							}
						}
						break;
					default:
						cout << "WARNING: There is an Unexpected Send type" << type << "!!!\n" << endl;
						continue;
				}
				break;

			case COLLECTIVE:
				type = atol(strtok(NULL, sep)); //We have the global operation id.
				comm = atol(strtok(NULL, sep)); //We have the communicator id.
				rtask_id = atol(strtok(NULL, sep)); //We have the root task_id.
				rth_id = atol(strtok(NULL, sep)); //We have the root thread_id.
				bsent = atol(strtok(NULL, sep)); //We have the sent byte count.
				brecv = atol(strtok(NULL, sep)); //We have the received byte count.
				switch (type) {
					case OP_MPI_Barrier:
						break;
					case OP_MPI_Bcast:
						break;
					case OP_MPI_Gather:
						break;
					case OP_MPI_Gatherv:
						break;
					case OP_MPI_Scatter:
						break;
					case OP_MPI_Scatterv:
						break;
					case OP_MPI_Allgather:
						break;
					case OP_MPI_Allgatherv:
						break;
					case OP_MPI_Alltoall:
						break;
					case OP_MPI_Alltoallv:
						break;
					case OP_MPI_Reduce:
						break;
					case OP_MPI_Allreduce:
						break;
					case OP_MPI_Reduce_Scatter:
						break;
					case OP_MPI_Scan:
						break;
					default:
						cout << "WARNING: There is an Unexpected Collective type!!!\n" << endl;
						continue;
				}
				break;

			case EVENT: {
				if (th_id != 0) cout << "Warning: unknown th_id field" << th_id << endl;
				ev_type = atol(strtok(NULL, sep));
				type = g_events_map[ev_type];
#if DEBUG
				cout << "ev_type:" << ev_type << endl;
				cout << "ype:" << type << endl;
#endif

				value = atol(strtok(NULL, sep));
				switch (type)			//Are this values defined somewhere? or are them dynamic?
				{
					case 50100001:			//   Send Size in MPI Global OP
						if (task_id != mpi_task) cerr << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_sendsize = value;
						if (mpi_sendsize == 0) mpi_sendsize = 1;
#if DEBUG
						cout << "MPI sendsize = " << mpi_sendsize << ",  g_packet_size" << g_packet_size
						<< ",  g_phit_size" << g_phit_size << endl;
#endif
						mpi_sendsize = (long) ceil((double) mpi_sendsize / (g_packet_size * g_phit_size));
						break;
					case 50100002:				//   Recv Size in MPI Global OP
						if (task_id != mpi_task) cerr << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_recvsize = value;
						if (mpi_recvsize == 0) mpi_recvsize = 1;
						mpi_recvsize = (long) ceil((double) mpi_recvsize / (g_packet_size * g_phit_size));
						break;
					case 50100003:				//   Root in MPI Global OP
						if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						if (value != 1) cerr << "root value != 1\n" << endl;
						assert(value == 1);
						mpi_isroot = value;
						break;
					case 50100004:				//   Communicator in MPI Global OP
						if (task_id != mpi_task) cerr << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_communicator = value;
						break;
					case 50000001:				//    POINT2POINT
						//Already managed by the normal records
						break;
					case 50000002:				//    MPI Collective Comm
#if DEBUG
					cout << "MPI Collective Communication" << type << ":" << value << "task_id=" << task_id << endl;
#endif
						if (value && mpi_type != -1)
							cerr << "Previous MPI communication have not been released\n" << endl;
						assert(!(value && mpi_type != -1));
						//coll_ev_t
						switch (value) {
							case 0:
								//Execute call
#if DEBUG
								cout << "Executing" << mpi_type << " in task=" << mpi_task << endl;
#endif
								if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
								if (mpi_sendsize == -1) {
									cout << "Undefined send size\n" << endl;
									mpi_sendsize = 1;
								}
								if (mpi_recvsize == -1) {
									cout << "Undefined recv size\n" << endl;
									mpi_recvsize = 1;
								}
								if (mpi_communicator == -1) cout << "Unknown communicator\n" << endl;
#if DEBUG
								cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
								cout << "mpi_communicator=" << mpi_communicator << endl;
								cout << "map size" << mpilist_map.size() << endl;
								cout << "saved size" << mpilist_map[mpi_communicator].size() << endl;
								cout << "saved type"
								<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
								<< endl;
#endif
								if (mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
										!= mpi_type) {
									cout << "mpilist[" << mpilist_comm_to_offset[mpi_communicator] << " ]="
											<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
											<< "root="
											<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].root
											<< endl;
									cout << "Incorrect MPI type in list" << endl;
								}
								assert((task_id == mpi_task));
								assert(mpi_sendsize != -1);
								assert(mpi_recvsize != -1);
								assert(mpi_communicator != -1);
								assert(
										mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
												== mpi_type);
								root = mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].root;
								switch (mpi_type) {
									case EV_MPI_Bcast:					//7
										if (mpi_isroot) {
											ev.type = SENDING;
											ev.task = 0; //Events don't have tags
											ev.length = mpi_sendsize;
											ev.count = 0; // Packets sent or received
											ev.mpitype = (enum coll_ev_t) mpi_type;
											for (t_id = 0; t_id < communicator_map[mpi_communicator]->size; t_id++) {
												ev.pid = communicator_map[mpi_communicator]->process[t_id];
												ins_event(g_generators_list, trace_id, instances, task_id, ev);
											}
										} else {
											//root is unknown, we wait for any Bcast
											ev.type = RECEPTION;
											ev.task = 0; //Events don't have tags
											ev.length = mpi_recvsize;
											ev.count = 0; // Packets sent or received
											ev.pid = -1;
											ev.mpitype = (enum coll_ev_t) mpi_type;
											ev.pid = root;
											ins_event(g_generators_list, trace_id, instances, task_id, ev);
										}
										break;
									case EV_MPI_Alltoall: //11
									case EV_MPI_Barrier: //8
									case EV_MPI_Allreduce: //10
									case EV_MPI_Alltoallv: //12
										if (mpi_isroot) cerr << "A root in Alltoall\n" << endl;
										assert(!mpi_isroot);
										ev.task = 0; //Events don't have tags
										ev.count = 0; // Packets sent or received
										ev.mpitype = (enum coll_ev_t) mpi_type;
										if (mpi_type == EV_MPI_Alltoallv) {
											//We cannot do anything better...
											mpi_sendsize = mpi_recvsize = 1;
										}
										long stride;
										ev.length = mpi_sendsize;
#if DEBUG
										cout << "Even length = " << ev.length << endl;
#endif
										for (stride = 1; stride < communicator_map[mpi_communicator]->size; stride++) {
											long g = gcd(stride, communicator_map[mpi_communicator]->size);
											if ((task_id / g) & 1)
												t_id = (task_id + communicator_map[mpi_communicator]->size - stride)
														% communicator_map[mpi_communicator]->size;
											else
												t_id = (task_id + stride) % communicator_map[mpi_communicator]->size;
											ev.pid = communicator_map[mpi_communicator]->process[t_id];
											ev.type = SENDING;
											ins_event(g_generators_list, trace_id, instances, task_id, ev);
											ev.type = RECEPTION;
											ins_event(g_generators_list, trace_id, instances, task_id, ev);
										}
										break;
									case EV_MPI_Reduce:								//9
										if (mpi_isroot) {
											ev.type = RECEPTION;
											ev.task = 0; //Events don't have tags
											ev.length = mpi_recvsize;
											ev.count = 0; // Packets sent or received
											ev.mpitype = (enum coll_ev_t) mpi_type;
											for (t_id = 0; t_id < communicator_map[mpi_communicator]->size; t_id++) {
												if (communicator_map[mpi_communicator]->process[t_id] == task_id)
													continue;
												ev.pid = communicator_map[mpi_communicator]->process[t_id];
												ins_event(g_generators_list, trace_id, instances, task_id, ev);
											}
										} else {
											ev.type = SENDING;
											ev.task = 0; //Events don't have tags
											ev.length = mpi_sendsize;
											ev.count = 0; // Packets sent or received
											ev.pid = -1;
											ev.mpitype = (enum coll_ev_t) mpi_type;
											ev.pid = root;
											ins_event(g_generators_list, trace_id, instances, task_id, ev);
										}
										break;
									default:
										cerr << "Implement me!" << endl;
										assert(0);
										break;
								}
								//clean
								mpi_type = -1;
								mpi_sendsize = -1;
								mpi_recvsize = -1;
								mpi_isroot = 0;
								mpilist_comm_to_offset[mpi_communicator]++;
#if DEBUG
								cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
#endif
								mpi_communicator = -1;
								break;
							case EV_MPI_Bcast:
							case EV_MPI_Alltoall:
							case EV_MPI_Barrier:
							case EV_MPI_Reduce:
							case EV_MPI_Allreduce:
							case EV_MPI_Alltoallv:
								if (mpi_task != task_id) {
									mpi_task = (enum coll_ev_t) task_id;
									mpilist_comm_to_offset.clear();
#if DEBUG
									cout << "mpi_task=" << mpi_task << " task_id=" << task_id << endl;
									cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
#endif
								}
								mpi_type = value;
								break;
							default:
								cerr << "Undefined MPI Collective Communication " << value << endl;
								assert(0);
						}
						break;
					case 50000003:					//    MPI Other
#if DEBUG
					cout << "MPI Other " << type << ":" << value << endl;
#endif
						switch (value) {
							case 0:					//Event end
								break;
							case MPI_Init:
								if (g_cutmpi) {
									//flush event queue
#if DEBUG
									cout << "MPI_Init: Flushing " << i << endl;
#endif
									clear_event(g_generators_list, trace_id, instances, task_id);
								}
								break;
							case MPI_Comm_size:
#if DEBUG
								cout << "MPI_Comm_size Ignored " << endl;
#endif
								break;
							case MPI_Comm_rank:
#if DEBUG
								cout << "MPI_Comm_rank Ignored " << endl;
#endif
								break;
							case MPI_Finalize:
								if (g_cutmpi) {
									tracestop[task_id] = true;
								}
								break;
							case MPI_Comm_split:
#if DEBUG
								cout << "MPI_Comm_split ignored " << endl;
#endif
								break;
							case MPI_Comm_dup:
#if DEBUG
								cout << "MPI_Comm_dup ignored " << endl;
#endif
								break;
							case MPI_Comm_create:
#if DEBUG
								cout << "MPI_Comm_created ignored " << endl;
#endif
								break;
							default:
								cerr << "Undefined MPI Other " << value << endl;
								assert(0);
						}
						break;
					case 0:
					case 40000001:					//   Application
					case 40000003:					//  Flushing Traces
					case 40000004:					//  I/O Read
					case 40000005:					//  I/O Write
					case 40000018:					//    Tracing mode:
					case 45000000:					//   User time used
					case 45000001:					//   System time used
					case 45000006:					//   Soft page faults
					case 45000007:					//   Hard page faults
					case 45000014:					//   Voluntary context switches
					case 45000015:					//   Involuntary context switches
					case 42009999:					//   Active hardware counter set
					case 42000050:					//   Instr completed (PAPI_TOT_INS)
					case 42000052:					//   FP instructions (PAPI_FP_INS
					case 42000059:					//   Total cycles (PAPI_TOT_CYC)
					case 42000000:					//   L1D cache misses (PAPI_L1_DCM)
					case 70000001:					//   Caller at level 1
					case 70000002:					//   Caller at level 2
					case 70000003:					//   Caller at level 3
					case 80000001:					//   Caller line at level 1
					case 80000002:					//   Caller line at level 2
					case 80000003:					//   Caller line at level 3
						break;
					default:
						cerr << "Unknown Event type n" << type << endl;
						assert(0);
				}
				// This will be useful to generate paraver output files.
				break;
			}

				// IO events could be treated as CPU or NETWORK events.
			case FREAD:
			case FWRITE:
#ifdef FILEIO
				cout << "FILEIO not implemented!!!!" << endl;
				assert(1);
				strtok( NULL, sep); // File Descriptor is dropped here.
				strtok( NULL, sep);// Required size is dropped here.
				cpu_burst=atol(strtok( NULL, sep));//We have the size.
				ev.type=COMPUTATION;
				ev.length=(long)ceil((FILE_TIME+(FILE_SCALE*cpu_burst)/g_op_per_cycle));// Computation time.
				ev.count=0;// Elapsed time.
				if (task_id<g_trace_nodes && task_id>=0) {
					ev.pid=task_id;
					ins_event(&g_generators_list[task_id]->events,ev);
				} else {
					cerr << "Adding cpu event into a non defined CPU" << endl;
					assert(0);
				}
#endif
				break;

			case FOPEN:
			case FSEEK:
			case FCLOSE:
			case FDUP:
			case FUNLINK:
#ifdef FILEIO
				cout << "FILEIO not implemented!!!!" << endl;
				ev.type=COMPUTATION;
				ev.length=(long)ceil((FILE_TIME)/g_op_per_cycle); // Computation time.
				ev.count=0;// Elapsed time.
				if (task_id<g_trace_nodes && task_id>=0) {
					for (inst=0; inst<trace_instances; inst++) {
						ev.pid=translation[task_id][inst].router;
						ev.taskid=translation[task_id][inst].task;
						ins_event(&network[ev.pid].events[ev.taskid], ev); // Add event to its node event queue
					}
				} else {
					cerr << "Adding cpu event into a non defined CPU" << endl;
					assert(0);
				}
#endif

				break;
			case IOCOLL:
				break;
			case IOBLOCKNCOLL:
				break;
			case IOBLOCKCOLL:
				break;
			case IONBLOCKNCOLLBEGIN:
				break;
			case IONBLOCKNCOLLEND:
				break;
			case IONBLOCKCOLLBEGIN:
				break;
			case IONBLOCKCOLLEND:
				break;
			case ONESIDEGENOP:
				break;
			case ONESIDEFENCE:
				break;
			case ONESIDELOCK:
				break;
			case ONESIDEPOST:
				break;
			case LAPIOP:
				/// These are communication with different semantic values of the MPI. In study...
#ifdef LAPI
				type=atol(strtok( NULL, sep)); //We have the LAPI operation.
				strtok( NULL, sep);//Handler dropped here.
				t_id=atol(strtok( NULL, sep));//We have the destination task id.
				if ( t_id>=g_trace_nodes || t_id <0 ) {
					cerr << "Destination task id is not defined (" << task_id << "): Aborting!!!" << endl;
					return -1;
				}

				size=atol(strtok( NULL, sep)); //We have the size.

				switch (type) {
					case LAPI_Init:
					case LAPI_End:
					break;
					case LAPI_Put:
					break;
					case LAPI_Get:
					break;
					case LAPI_Fence: // Could be a Wait
					break;
					case LAPI_Barrier:// Could be a Barrier
					break;
					case LAPI_Alltoall:// Could be an Alltoall
					break;
					default:
					cout << "Undefined LAPI operation: " << endl;
					break;
				}
#endif
				break;
			default:
				cout << "WARNING: There is an Unexpected operation!!!\n" << endl;
		}
	}
	fclose(ftrc);

	for (int j = 0; j < instances.size(); j++) {
		bool trace_empty = true;
		for (int k = 0; k < g_trace_nodes[trace_id]; k++) {
			assert(g_trace_2_gen_map[trace_id][k][j] < g_number_generators); // Sanity check
			if (!g_generators_list[g_trace_2_gen_map[trace_id][k][j]]->isGenerationEnded()) {
				trace_empty = false;
				break;
			}
		}
		if (trace_empty)
			cerr << "Error when loading trace " << trace_id << ", instance " << j << ": event queues are empty!"
					<< endl;
		assert(!trace_empty); // Sanity check, to ensure trace instance has been properly loaded
	}

	cout << "<<read_dimemas" << endl;
}

/**
 * Places the tasks in a random way.
 */
/*void random_placement(){
 long i, j, d;

 for (i=0; i<g_number_generators; i++)
 {
 //network[i].source=INDEPENDENT_SOURCE;
 network[i].source=0;
 }

 for (i=0; i<g_trace_nodes; i++)
 for (j=0; j<trace_instances; j++) {
 do{
 d=rand()%g_number_generators;
 //} while (network[d].source!=INDEPENDENT_SOURCE);
 } while (network[d].source==g_multitask);
 translation[i][j].router=d;
 translation[i][j].task=network[d].source;
 network[d].source++;
 }
 for (i=0; i<g_number_generators; i++)
 network[i].source=OTHER_SOURCE;
 }*/

/**
 * Places the tasks consecutively (node 0 in switch 0 port 0; node 1 in switch 0 port 1, and so on). same as row
 */
/*void consecutive_placement(){
 long i, j, k, d;

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;
 for (j=0; j<trace_instances; j++){
 k=(j*g_trace_nodes);
 for (i=0; i<g_trace_nodes; i++){
 d=i+k;
 translation[i][j].router=d/g_multitask;
 translation[i][j].task=d%g_multitask;
 //cout << "(" << i << "," << j << ")->(" << translation[i][j].router << "," << translation[i][j].task << ")\n" << endl;
 network[d].source=OTHER_SOURCE;
 }
 }
 }
 */
/**
 * Places the tasks alternatively one in each switch (node 0 in switch 0 port 0; node 1 in switch 1 port 0, and so on). (for tree and indirect cube )
 */
/*
 void shuffle_placement(){
 long i, j, k, d;

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 for (j=0; j<trace_instances; j++){
 k=(j*g_trace_nodes);
 for (i=0; i<g_trace_nodes; i++){
 d=((i+k)/(g_number_generators/stDown))+((i+k)%(g_number_generators/stDown))*stDown;
 translation[i][j].router=d;
 translation[i][j].task=0;
 network[d].source=OTHER_SOURCE;
 }
 }
 }
 */
/**
 * Places the tasks consecutively but shifting a given number of places (node 0 in switch 0 port 0; node 1 in switch 0 port 1, and so on).
 */
/*
 void shift_placement(){
 long i, j, k, d;

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 for (j=0; j<trace_instances; j++){
 k=(j*g_trace_nodes);
 for (i=0; i<g_trace_nodes; i++){
 d=(i+k+shift)%g_number_generators;
 translation[i][j].router=d;
 translation[i][j].task=0;
 network[d].source=OTHER_SOURCE;
 }
 }
 }
 */
/**
 * Places the tasks consecutively in a cube column.
 * For 2D (Hence using nodes_x)
 */
/*
 void column_placement(){
 long i, j, k, d,t;
 long s,x,y;

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 for (j=0; j<trace_instances; j++){
 k=(j*g_trace_nodes);
 for (i=0; i<g_trace_nodes; i++){
 //d=((i+k)/nodes_x)+((i+k)%nodes_x)*nodes_x;
 s=i+k;
 x=s%nodes_x;
 y=s/nodes_x;
 t=y%g_multitask;
 d=(y/g_multitask)*nodes_x+x;
 translation[i][j].router=d;
 translation[i][j].task=t;
 network[d].source=OTHER_SOURCE;
 }
 }
 }
 */
/**
 * Places the tasks doing module by a+ai, i.e. (a,a)=0
 * For 2D (Hence using nodes_x)
 */
/*
 void gaussian_placement(){
 long i, j, k, d,t,gt=0;
 long s,x,y,a;

 //if(nodes_x&1)panic("In gaussian_placement nodes_x must be even");
 if(nodes_x&1)cout << "In gaussian_placement nodes_x must be even" << endl;
 assert(!(nodes_x&1));
 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 for (j=0; j<trace_instances; j++){
 k=(j*g_trace_nodes);
 for (i=0; i<g_trace_nodes; i++){
 //d=((i+k)/nodes_x)+((i+k)%nodes_x)*nodes_x;
 s=i+k;
 x=s%nodes_x;
 y=s/nodes_x;
 a=nodes_x/2;
 t=y/a;
 if(t>gt)gt=t;
 x=(x+t*a)%nodes_x;
 y=y-t*a;
 d=y*nodes_x+x;
 translation[i][j].router=d;
 translation[i][j].task=t;
 network[d].source=OTHER_SOURCE;
 }
 }
 if(gt+1!=g_multitask)
 {
 cout << "gt+1=" << gt+1 << "g_multitask=%ld\n" << endl;
 //panic("In gaussian_placement tk does not match g_multitask");
 cout << "In gaussian_placement tk does not match g_multitask" << endl;
 assert(0);
 }
 }
 */
/**
 * Places the tasks in quadrants, isolated subtori, half the size in each dimension.
 *
 * 2 q. for 1-D, 4 q. for 2-D and 8 q. for 3-D.
 */
/*
 void quadrant_placement() {
 long i, j, k, d;
 long n1, n2=1, n3=1; // nodes/2 in each dimension
 long c1, c2, c3; // coordenates in each dimension of the base placement.
 
 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 n1=nodes_x/2;
 if (ndim>1)
 n2=nodes_y/2;
 if (ndim>2)
 n3=nodes_z/2;

 for (j=0; j<trace_instances; j++)
 for (i=0; i<g_trace_nodes; i++){
 c1=(i%n1) + ((j%2)*n1);
 c2=((i/n1)%n2) + (((j/2)%2)*n2);
 c3=((i/(n1*n2))%n3) + (((j/4)%2)*n3);
 d=c1 + (c2*nodes_x)+(c3*nodes_x*nodes_y);
 translation[i][j].router=d;
 translation[i][j].task=0;
 network[d].source=OTHER_SOURCE;
 }
 }
 */

/**
 * Places the tasks in a diagonal. only 1 instance and 2D torus.
 */
/*
 void diagonal_placement() {
 long i,x,y;
 x=y=0;

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 for (i=0; i<nodes_x*nodes_y; i++){
 translation[(nodes_x*y)+x][0].router=i;
 translation[(nodes_x*y)+x][0].task=0;
 network[i].source=OTHER_SOURCE;
 y++;
 if (y == nodes_y){
 y=x+1;
 x=nodes_x-1;
 }
 else{
 x--;
 if (x < 0){
 x=y;
 y=0;
 }
 }
 }
 }
 */
/**
 * Places the tasks in an indirect cube, keeping a submesh attached to each switch.
 */
/*
 void icube_placement() {
 long i, j, t, d;
 long tnodes_x=nodes_x*pnodes_x, // total nodes in the X dimension of the virtual mesh
 tnodes_y=nodes_y*pnodes_y, // total nodes in the Y dimension of the virtual mesh
 tnodes_z=nodes_z*pnodes_z; // total nodes in the Z dimension of the virtual mesh
 long tx,ty,tz; // coordenates in the virtual mesh.
 long sx,sy,sz; // switch coordenates in each dimension.
 long nx,ny,nz; // coordenates in each dimension of the node attached to the switch.

 for (i=0; i<g_number_generators; i++)
 network[i].source=INDEPENDENT_SOURCE;

 cout << trace_instances << " x " << g_trace_nodes << endl;
 cout << "virtual  " << tnodes_x << " x "<< tnodes_y << " x "<< tnodes_z << endl;
 cout << "virtual  " << nodes_x << " x "<< nodes_y << " x "<< nodes_z << endl;
 cout << "virtual  " << pnodes_x << " x "<< pnodes_y << " x "<< pnodes_z << endl;

 for (j=0; j<trace_instances; j++)
 for (i=0; i<g_trace_nodes; i++){
 t=(j*g_trace_nodes)+i;  // could be done directly with a single for, however it has been done in two fors for the sake of simplicity

 tx=t%tnodes_x;
 ty=(t/tnodes_x)%tnodes_y;
 tz=t/(tnodes_x*tnodes_y);
 
 sx=tx/pnodes_x;
 nx=tx%pnodes_x;
 sy=ty/pnodes_y;
 ny=ty%pnodes_y;
 sz=tz/pnodes_z;
 nz=tz%pnodes_z;
 d=((sx+(nodes_x*(sy+(nodes_y*sz))))*nodes_per_switch)+(nx+(pnodes_x*(ny+(pnodes_y*nz))));
 translation[i][j].router=d;
 translation[i][j].task=0;
 cout << "(" << tx << "," << ty << "," << tz << ")    " << i << ", " << j << " -> " << d << endl;
 network[d].source=OTHER_SOURCE;
 }
 }
 */
/**
 * Places the tasks as defined in #placefile.
 * The format of this file is very simple:
 * node_id, task_id, instance_id
 * separated by one or more of the following characters: '<spc>' '<tab>' '_' ',' '.' ':' '-' '>'
 */
/*
 void file_placement() {
 FILE * fp;
 char buffer[512];
 long node,task,inst;
 char sep[]=",.:_-> \t";

 for (node=0; node<g_number_generators; node++)
 network[node].source=INDEPENDENT_SOURCE;

 for (task=0; task<g_trace_nodes; task++)
 for (inst=0; inst<trace_instances; inst++)
 {
 translation[task][inst].router=-1;
 translation[task][inst].task=-1;
 }
 //if((fp = fopen(placefile, "r")) == NULL)
 //panic("Placement file not found");
 if((fp = fopen(placefile, "r")) == NULL)
 cout << "Placement file not found" << endl;
 assert((fp = fopen(placefile, "r")) != NULL);

 while(fgets(buffer, 512, fp) != NULL) {
 if(buffer[0] != '\n' && buffer[0] != '#') {
 if(buffer[strlen(buffer) - 1] == '\n')
 buffer[strlen(buffer) - 1] = '\0';
 node = atol(strtok( buffer, sep));
 task = atol(strtok( NULL, sep));
 inst = atol(strtok( NULL, sep));
 if (translation[task][inst].router!=-1)
 cout << "Warning: task " << task << "." << inst << " is being redefined\n" << endl;
 if (network[node].source==OTHER_SOURCE)
 cout << "Warning: node " << node << " is being redefined\n" << endl;
 translation[task][inst].router=node;
 translation[task][inst].task=0;
 network[node].source=OTHER_SOURCE;
 }
 }
 }
 */
/**
 * Runs simulation using a trace file as workload.
 *
 * In this mode, simulation are running until all the events in the nodes queues are done.
 * It prints partial results each pinterval simulation cycles & calculates global 
 * queues states for global congestion control.
 *
 * @see read_trace() 
 * @see init_functions
 * @see run_network
 */
/*void run_network_trc() {
 g_event_deadlock=0;
 do {
 long i,ti;
 g_event_deadlock++;
 data_movement(TRUE);//can set g_event_deadlock=0
 if(global_q_u)g_event_deadlock=0;
 if(g_event_deadlock>10000)
 {
 cout << "EVENT DEADLOCK detected at cycle " << sim_clock << endl;
 for (i=0; i<NUMNODES; i++) {
 for(ti=0;ti<g_multitask;ti++)
 {
 if(event_empty(&network[i].events[ti]))
 {
 cout << "[" << i << "," << ti << "] empty\n" << endl;
 }
 else
 {
 event e = head_event(&network[i].events[ti]);
 cout << "[" << i << "," << ti << "] " << (char)e.type << " (" << e.pid << "," << e.taskid <<  ") flag=" << e.task << " length=" << e.length << " count=" << e.count << " mpitype=" << e.mpitype << endl;
 }
 }
 }
 //panic("EVENT DEADLOCK!!");
 cout << "EVENT DEADLOCK!!" << endl;
 assert(0);
 }
 sim_clock++;

 if ((pheaders > 0) && (sim_clock % pinterval == 0)) {
 print_partials();
 }
 if ((sim_clock%update_period) == 0) {
 global_q_u = global_q_u_current;
 global_q_u_current = injected_count - rcvd_count;
 }
 go_on=FALSE;
 for (i=0; i<NUMNODES && !go_on; i++) {
 for(ti=0;ti<g_multitask;ti++)
 {
 if (!event_empty (&network[i].events[ti]) && network[i].source==OTHER_SOURCE) {
 go_on=TRUE;
 break;
 }
 }
 }
 } while (go_on);
 print_partials();
 save_batch_results();
 reset_stats();
 }*/

long gcd(long a, long b) {
	long c;
	while (a) {
		//a, b = b%a, a
		c = a;
		a = b % a;
		b = c;
	}
	return b;
}

void translate_pcf_file(int trace_id) {
	fstream pcfFile;
	char line[256];

	pcfFile.open(g_pcf_file[trace_id].c_str(), ios::in);
	if (!pcfFile) {
		cerr << "Error, not pcf file" << endl;
		exit(-1);
	}

	bool ignore = true;
	int line_number = 0;
	while (pcfFile.getline(line, 256)) {
#if DEBUG
		cout << "Reading line number " << line_number << endl;
#endif
		string line_field = "";
		if (ignore) {
			line_field.append(line, 0, 10);
			if (strcmp(line_field.c_str(), "EVENT_TYPE") == 0) ignore = false;
		} else {
			line_field.append(line, 0, 6);
			if ((strcmp(line_field.c_str(), "") == 0) || (strcmp(line_field.c_str(), "VALUES") == 0)) {
				ignore = true;
			} else {
				if (!(strcmp(line, "EVENT_TYPE") == 0)) save_event_type(line);
			}
		}
		line_number++;
	}
}

void save_event_type(char* event_line) {
	char * identificator;
	char * event;
	char * event_line2;
	long id;

	event_line2 = strtok(event_line, " ");
#if DEBUG
	cout << "event_line: " << event_line << endl;
	cout << "event_line2: " << event_line2 << endl;
#endif
	event_line2 = strtok(NULL, " ");
	identificator = event_line2;
	id = atol(identificator);
#if DEBUG
	cout << "indentificator: " << identificator << endl;
	cout << "id: " << id << endl;
#endif
	event_line2 = strtok(NULL, "\n");
	event = event_line2;
#if DEBUG
	cout << "event: " << event << endl;
#endif

	if ((strcmp(event, "   Send Size in MPI Global OP") == 0) || (strcmp(event, "  MPI Global OP send size") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100001));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if ((strcmp(event, "   Recv Size in MPI Global OP") == 0)
			|| (strcmp(event, "  MPI Global OP recv size") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100002));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if ((strcmp(event, "   Root in MPI Global OP") == 0) || (strcmp(event, "  MPI Global OP is root?") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100003));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if ((strcmp(event, "   Communicator in MPI Global OP") == 0)
			|| (strcmp(event, "  MPI Global OP communicator") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100004));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if (strcmp(event, "   MPI Point-to-point") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000001));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if (strcmp(event, "   MPI Collective Comm") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000002));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else if (strcmp(event, "   MPI Other") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000003));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
#endif
	} else {
		g_events_map.insert(pair<long, long>(id, 0));
		assert(g_events_map.count(id) == 1);
#if DEBUG
		cout << "Event 0!!!!!!!" << endl;
		cout << "g_events_map[id]=" << g_events_map[id] << " (id " << id << ")" << endl;
#endif
	}
#if DEBUG
	cout << "g_events_map[id]=" << g_events_map.find(id)->second << endl;
	cout << "events_map now contains " << (int) g_events_map.size() << " elements." << endl << endl;
#endif
}
