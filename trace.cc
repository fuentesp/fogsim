/**
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2014 University of Cantabria

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

#include <fstream>

#include "global.h"
#include "dimemas.h"
#include "trace.h"
#include "event.h"
#include "generatorModule.h"
#include "communicator.h"
#include <string.h>

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
void read_trace() {
	FILE * ftrc;
	char c;
	cout << ">>read_trace\n" << endl;

	if ((ftrc = fopen(g_trcFile, "r")) == NULL) {
		cout << "g_trcFile" << endl;
		cout << "Trace file not found in current directory" << endl;
	}
	assert((ftrc = fopen(g_trcFile, "r")) != NULL);
	c = (char) fgetc(ftrc);
	fclose(ftrc);

	switch (c) {
		case '#':
			read_dimemas();
			break;
		case -1:
			cout << "Reading empty file\n" << endl;
			break;
		default:
			cout << "Cannot understand this trace format" << endl;
			break;
	}
	cout << "<<read_trace\n" << endl;
}

/**
 * Reads a trace from a dimemas file.
 *
 * Read a trace from a dimemas file whose name is in global variable #g_trcFile
 * It only consideres events for CPU and point to point operations. File I/O could be
 * considered as a cpu event if FILEIO is defined.
 */
void read_dimemas() {
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
	long ti, x, y;
	long value;
	long root;
	enum coll_ev_t mpi_task = (enum coll_ev_t) -1;
	long mpi_type = -1, mpi_sendsize = -1, mpi_recvsize = -1, mpi_communicator = -1, mpi_isroot = 0;
	const long mpilist_allocated = 65536;

	mpi_collective_comm_t mpilist_aux;
	map<long, vector<mpi_collective_comm_t> > mpilist_map; // Each communicator has it list of collectives
	map<long, long> mpilist_comm_to_offset;
	long mpilistsize = 0;
	long definition_type = 0;
	map<long, communicator*> communicator_map;

	cout << ">>read_dimemas\n" << endl;

	if ((ftrc = fopen(g_trcFile, "r")) == NULL) cout << "Trace file not found in current directory" << endl;
	assert((ftrc = fopen(g_trcFile, "r")) != NULL);
	while (fgets(buffer, BUFSIZE, ftrc) != NULL) {
		op_id = strtok(buffer, sep);

		if (!strcmp(op_id, "d")) { // If trace line is a definition for: communicator, I/O file, OSWINDOW
			type = atol(strtok(NULL, sep));
			switch (type) {
				case COMMUNICATOR:
					new communicator();
					comm_id = atol(strtok(NULL, sep));
					cout << "comm_id" << comm_id << endl;
					assert(comm_id != -1);
					communicator_map[comm_id] = new communicator();
					cout << "communicator created" << endl;
					communicator_map[comm_id]->id = comm_id;
					cout << "comm_id" << communicator_map[comm_id]->id << endl;
					communicator_map[comm_id]->size = atol(strtok(NULL, sep));
					cout << "comm_size" << communicator_map[comm_id]->size << endl;
					for (long pocess_count = 0; pocess_count < communicator_map[comm_id]->size; pocess_count++) {
						long proc = atol(strtok(NULL, sep));
						communicator_map[comm_id]->process[pocess_count] = proc;
						cout << "Proc: " << communicator_map[comm_id]->process[pocess_count] << endl;
					}
					break;
				case FILE_IO:
					cout << "Not implemented definition" << endl;
					assert(0);
					break;
				case OSWINDOW:
					cout << "Not implemented definition" << endl;
					assert(0);
					break;
				default:
					cout << "Wrong definition" << endl;
					assert(0);
			}
		}

		if (atol(op_id) != EVENT) continue;
		task_id = atol(strtok(NULL, sep)); //We have the task id.
		th_id = atol(strtok(NULL, sep)); //We have the thread id.
		ev_type = atol(strtok(NULL, sep));
		cout << "ev_type:" << ev_type << endl;
		type = g_events_map[ev_type];
		cout << "type" << type << endl;
		value = atol(strtok(NULL, sep));
		switch (type) {
			case 50100003:		//   Root in MPI Global OP
				if (value != 1) cout << "root flag != 1" << endl;
				assert(value == 1);
				mpilist_aux.root = task_id;
				break;
			case 50100004:			//   Communicator in MPI Global OP
				cout << "First Pass with communicator!=1" << endl;
				cout << "Comunicator=" << value << endl;
				mpilist_aux.communicator_id = value;
				break;
			case 50000002:			//    MPI Collective Comm
				if (value == 0) {
					cout << "END" << endl;
					//END
					//Put the aux structure into the comm list
					long comm_id = mpilist_aux.communicator_id;
					cout << "communicator_id:" << endl;

					if (mpilist_map.count(comm_id) == 0) {
						assert(mpilist_comm_to_offset.count(comm_id) == 0);
					}
					cout << "communicator_id count:" << mpilist_map.count(comm_id) << endl;
					//Check if the coll. array for the current communicator exists and if not, create it with -1 values
					if (mpilist_map.count(comm_id) <= 0) {
						cout << "communicator_id count:" << mpilist_map.count(comm_id) << endl;
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

						if (mpilist_comm_to_offset[comm_id] >= mpilist_allocated) cout << "MPI overflow!" << endl;
						assert(mpilist_comm_to_offset[comm_id] < mpilist_allocated);
					}

				} else {
					cout << "START" << endl;
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
	if ((ftrc = fopen(g_trcFile, "r")) == NULL) cout << "Trace file not found in current directory" << endl;
	assert((ftrc = fopen(g_trcFile, "r")) != NULL);

	fgets(buffer, BUFSIZE, ftrc);
	if (strncmp("#DIMEMAS", buffer, 8)) cout << "Header line is missing, maybe not a dimemas file" << endl;
	assert(!strncmp("#DIMEMAS", buffer, 8));

	strtok(buffer, sep); // Drops the #DIMEMAS.
	strtok(NULL, sep); // Drops trace_name.
	strtok(NULL, sep); // Offsets are dropped here.
	n = atol(strtok(NULL, tsep)); // task info has diferent separators
	if (n > g_trace_nodes) {
		cout << "n=" << n << " g_trace_nodes=" << g_trace_nodes << endl;
		cout << "There are not enough nodes for running this trace" << endl;
	}
	assert(n <= g_trace_nodes);

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
					cout << "Wrong definition" << endl;
					assert(0);
			}
			continue;
		}
		task_id = atol(strtok(NULL, sep)); //We have the task id.
		if (task_id > n || task_id < 0) cout << "Task id not defined: Aborting" << endl;
		assert(!(task_id > n || task_id < 0));
		if (tracestop[task_id]) continue;
		th_id = atol(strtok(NULL, sep)); //We have the thread id.
		switch (atol(op_id)) {
			case CPU:
				cpu_burst = atof(strtok(NULL, sep)); //We have the time taken by the CPU.
				ev.type = COMPUTATION;
				//FIXME: count rounding errors
				ev.length = (long) ceil(0.001 + ((double) cpu_burst * g_cpuSpeed) / g_op_per_cycle); // Computation time.
				ev.count = 0; // Elapsed time.
				ev.mpitype = (enum coll_ev_t) 0;
				if (task_id < g_trace_nodes && task_id >= 0) {
					ev.pid = task_id;
					ins_event(&g_generatorsList[task_id]->events, ev);
				} else {
					cout << "Adding cpu event into a non defined CPU. Task_id = " << task_id << endl;
					assert(0);
				}
				break;

			case SEND:
				t_id = atol(strtok(NULL, sep)); //We have the destination task id.
				if (t_id > g_generatorsCount * g_multitask || t_id < 0)
					cout << "Destination task id is not defined: Aborting" << endl;
				assert(!(t_id > g_generatorsCount * g_multitask || t_id < 0));
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
							ev.length = (long) ceil((double) ev.length / (g_packetSize * g_phit_size));
							ev.mpitype = (enum coll_ev_t) 0;
							if (task_id < g_trace_nodes && t_id < g_trace_nodes && task_id >= 0 && t_id >= 0) {
								ev.pid = t_id;
								ins_event(&g_generatorsList[task_id]->events, ev);
							} else {
								cout << "Adding comm event into a non defined CPU" << endl;
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
				if (t_id > g_generatorsCount || t_id < 0)
					cout << "Source task id:" << t_id << " is not defined: Aborting. nProcs=" << g_generatorsCount
							<< endl;
				assert(!(t_id > g_generatorsCount/* *g_multitask */|| t_id < 0));
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
							ev.length = (long) ceil((double) ev.length / (g_packetSize * g_phit_size));
							ev.mpitype = (enum coll_ev_t) 0;
							if (task_id < g_trace_nodes && t_id < g_trace_nodes && task_id >= 0 && t_id >= 0) {
								ev.pid = t_id;
								ins_event(&g_generatorsList[task_id]->events, ev);
							} else {
								cout << "Adding comm event into a non defined CPU" << endl;
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
				cout << "ev_type:" << ev_type << endl;
				type = g_events_map[ev_type];
				cout << "ype:" << type << endl;

				value = atol(strtok(NULL, sep));
				switch (type)			//Are this values defined somewhere? or are them dynamic?
				{
					case 50100001:			//   Send Size in MPI Global OP
						if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_sendsize = value;
						if (mpi_sendsize == 0) mpi_sendsize = 1;
						cout << "MPI sendsize = " << mpi_sendsize << ",  g_packetSize" << g_packetSize
								<< ",  g_phit_size" << g_phit_size << endl;
						mpi_sendsize = (long) ceil((double) mpi_sendsize / (g_packetSize * g_phit_size));
						break;
					case 50100002:				//   Recv Size in MPI Global OP
						if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_recvsize = value;
						if (mpi_recvsize == 0) mpi_recvsize = 1;
						mpi_recvsize = (long) ceil((double) mpi_recvsize / (g_packetSize * g_phit_size));
						break;
					case 50100003:				//   Root in MPI Global OP
						if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						if (value != 1) cout << "root value != 1\n" << endl;
						assert(value == 1);
						mpi_isroot = value;
						break;
					case 50100004:				//   Communicator in MPI Global OP
						if (task_id != mpi_task) cout << "Changed task in the middle of a collective\n" << endl;
						assert(task_id == mpi_task);
						mpi_communicator = value;
						break;
					case 50000001:				//    POINT2POINT
						//Already managed by the normal records
						break;
					case 50000002:				//    MPI Collective Comm
						cout << "MPI Collective Communication" << type << ":" << value << "task_id=" << task_id << endl;
						if (value && mpi_type != -1)
							cout << "Previous MPI communication have not been released\n" << endl;
						assert(!(value && mpi_type != -1));
						//coll_ev_t
						switch (value) {
							case 0:
								//Execute call
								cout << "Executing" << mpi_type << " in task=" << mpi_task << endl;
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
								cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
								cout << "mpi_communicator=" << mpi_communicator << endl;
								cout << "map size" << mpilist_map.size() << endl;
								cout << "saved size" << mpilist_map[mpi_communicator].size() << endl;
								cout << "saved type"
										<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
										<< endl;
								if (mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
										!= mpi_type) {
									cout << "mpilist[" << mpilist_comm_to_offset[mpi_communicator] << " ]="
											<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].type
											<< "root="
											<< mpilist_map[mpi_communicator][mpilist_comm_to_offset[mpi_communicator]].root
											<< endl;
									cout << "Incorrect MPI type in list" << endl;
								}
								cout << "a" << endl;
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
												ins_event(&g_generatorsList[task_id]->events, ev);
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
											ins_event(&g_generatorsList[task_id]->events, ev);
										}
										break;
									case EV_MPI_Alltoall: //11
									case EV_MPI_Barrier: //8
									case EV_MPI_Allreduce: //10
									case EV_MPI_Alltoallv: //12
										if (mpi_isroot) cout << "A root in Alltoall\n" << endl;
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
										cout << "Even length = " << ev.length << endl;
										for (stride = 1; stride < communicator_map[mpi_communicator]->size; stride++) {
											long g = gcd(stride, communicator_map[mpi_communicator]->size);
											if ((task_id / g) & 1)
												t_id = (task_id + communicator_map[mpi_communicator]->size - stride)
														% communicator_map[mpi_communicator]->size;
											else
												t_id = (task_id + stride) % communicator_map[mpi_communicator]->size;
											ev.pid = communicator_map[mpi_communicator]->process[t_id];
											ev.type = SENDING;
											ins_event(&g_generatorsList[task_id]->events, ev);
											ev.type = RECEPTION;
											ins_event(&g_generatorsList[task_id]->events, ev);
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
												ins_event(&g_generatorsList[task_id]->events, ev);
											}
										} else {
											ev.type = SENDING;
											ev.task = 0; //Events don't have tags
											ev.length = mpi_sendsize;
											ev.count = 0; // Packets sent or received
											ev.pid = -1;
											ev.mpitype = (enum coll_ev_t) mpi_type;
											ev.pid = root;
											ins_event(&g_generatorsList[task_id]->events, ev);
										}
										break;
									default:
										cout << "Implement me!" << endl;
										assert(0);
										break;
								}
								//clean
								mpi_type = -1;
								mpi_sendsize = -1;
								mpi_recvsize = -1;
								mpi_isroot = 0;
								cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
								mpilist_comm_to_offset[mpi_communicator]++;
								cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
								mpi_communicator = -1;
								break;
							case EV_MPI_Bcast:
							case EV_MPI_Alltoall:
							case EV_MPI_Barrier:
							case EV_MPI_Reduce:
							case EV_MPI_Allreduce:
							case EV_MPI_Alltoallv:
								if (mpi_task != task_id) {
									cout << "mpi_task=" << mpi_task << " task_id=" << task_id << endl;
									mpi_task = (enum coll_ev_t) task_id;
									cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
									mpilist_comm_to_offset.clear();
									cout << "mpi_id=" << mpilist_comm_to_offset[mpi_communicator] << endl;
								}
								mpi_type = value;
								break;
							default:
								cout << "Undefined MPI Collective Communication " << value << endl;
								assert(0);
						}
						break;
					case 50000003:					//    MPI Other
						cout << "MPI Other " << type << ":" << value << endl;
						switch (value) {
							case 0:					//Event end
								break;
							case MPI_Init:
								if (g_cutmpi) {
									//flush event queue
									cout << "MPI_Init: Flushing" << endl;
									clear_event(&g_generatorsList[task_id]->events);
								}
								break;
							case MPI_Comm_size:
								cout << "MPI_Comm_size Ignored " << endl;
								break;
							case MPI_Comm_rank:
								cout << "MPI_Comm_rank Ignored " << endl;
								break;
							case MPI_Finalize:
								if (g_cutmpi) {
									tracestop[task_id] = true;
								}
								break;
							case MPI_Comm_split:
								cout << "MPI_Comm_split ignored " << endl;
								break;
							case MPI_Comm_dup:
								cout << "MPI_Comm_dup ignored " << endl;
								break;
							case MPI_Comm_create:
								cout << "MPI_Comm_created ignored " << endl;
								break;
							default:
								cout << "Undefined MPI Other " << value << endl;
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
						cout << "Unknown Event type n" << type << endl;
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
					ins_event(&g_generatorsList[task_id]->events,ev);
				}
				else
				cout << "Adding cpu event into a non defined CPU" << endl;
				assert(0);
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
				if (task_id<g_trace_nodes && task_id>=0)
				for (inst=0; inst<trace_instances; inst++) {
					ev.pid=translation[task_id][inst].router;
					ev.taskid=translation[task_id][inst].task;
					ins_event(&network[ev.pid].events[ev.taskid], ev); // Add event to its node event queue
				}
				else
				cout << "Adding cpu event into a non defined CPU" << endl;
				assert(0);
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
					cout << "Destination task id is not defined (" << task_id << "): Aborting!!!" << endl;
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
	cout << "<<read_dimemas\n" << endl;
}

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

void translate_pcf_file() {
	fstream pcfFile;
	char line[256];

	pcfFile.open(g_pcfFile, ios::in);
	if (!pcfFile) {
		cout << "Error, not pcf file" << endl;
		exit(-1);
	}

	bool ignore = true;
	int line_number = 0;
	while (pcfFile.getline(line, 256)) {
		cout << "Reading line number " << line_number << endl;
		if (ignore) {
			if (strcmp(line, "EVENT_TYPE") == 0) ignore = false;
		} else {
			if ((strcmp(line, "") == 0) || (strcmp(line, "VALUES") == 0)) {
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
	char * check;
	char * event_line2;
	long id;

	event_line2 = strtok(event_line, " ");
	cout << "event_line: " << event_line << endl;
	cout << "event_line2: " << event_line2 << endl;
	event_line2 = strtok(NULL, " ");
	identificator = event_line2;
	id = atol(identificator);
	cout << "indentificator: " << identificator << endl;
	cout << "id: " << id << endl;
	event_line2 = strtok(NULL, "\n");
	event = event_line2;
	cout << "event: " << event << endl;
	assert(g_events_map.count(id) == 0);

	if ((strcmp(event, "   Send Size in MPI Global OP") == 0) || (strcmp(event, "  MPI Global OP send size") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100001));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if ((strcmp(event, "   Recv Size in MPI Global OP") == 0)
			|| (strcmp(event, "  MPI Global OP recv size") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100002));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if ((strcmp(event, "   Root in MPI Global OP") == 0) || (strcmp(event, "  MPI Global OP is root?") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100003));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if ((strcmp(event, "   Communicator in MPI Global OP") == 0)
			|| (strcmp(event, "  MPI Global OP communicator") == 0)) {
		g_events_map.insert(pair<long, long>(id, 50100004));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if (strcmp(event, "   MPI Point-to-point") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000001));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if (strcmp(event, "   MPI Collective Comm") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000002));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else if (strcmp(event, "   MPI Other") == 0) {
		g_events_map.insert(pair<long, long>(id, 50000003));
		assert(g_events_map.count(id) == 1);
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	} else {
		g_events_map.insert(pair<long, long>(id, 0));
		assert(g_events_map.count(id) == 1);
		cout << "Event 0!!!!!!!" << endl;
		cout << "g_events_map[id]=" << g_events_map[id] << endl;
	}
	cout << "g_events_map[id]=" << g_events_map.find(id)->second << endl;
	cout << "events_map now contains " << (int) g_events_map.size() << " elements." << endl << endl;
}
