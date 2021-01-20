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

#include "generator/trace.h"
#include "communicator.h"

int module(int a, int b);
bool parity(int a, int b);
int main(int argc, char *argv[]);
void readConfiguration(int argc, char *argv[]);
void createNetwork();
void action();
void writeOutput();
void writeTransientOutput(char * output_name);
void writeLatencyHistogram(char * output_name);
void writeHopsHistogram(char * output_name);
void writeGeneratorsInjectionProbability(char * output_name);
void writeQcnPortEnruteMinProbability(char * output_name);
void writeQcnPortCongestionValue(char * output_name);
void writeDestMap(char * output_name);
void writeAcorGroup0SwsPacketsBlocked(char * output_name);
void writeAcorGroup0SwsStatus(char * output_name);
void freeMemory();
void readBufferType(const char * buffer_type, BufferType * var);
void readSwitchType(const char * switch_type, SwitchType * var);
void readArbiterType(const char * arbiter_type, ArbiterType * var);
void readTrafficPattern(const char * traffic_name, TrafficType * var);
void readRoutingType(const char * routing_type, RoutingType * var);
void readValiantType(const char * valiant_type, ValiantType * var);
void readValiantMisroutingDestination(const char * v_m_d, valiantMisroutingDestination * var);
void readGlobalMisrouting(const char * global_misrouting, GlobalMisroutingPolicy * var);
void readDeadlockAvoidanceMechanism(const char * d_a, DeadlockAvoidance * var);
void readMisroutingTrigger(const char * m_t, MisroutingTrigger * var);
void readCongestionDetection(const char * c_d, CongestionDetection * var);
void readTraceDistribution(const char * t_d, TraceAssignation * var);
void readVcUsage(const char * v_u, VcUsageType * var);
void readVcAlloc(const char * v_a, VcAllocationMechanism * var);
void readVcInj(const char * v_i, VcInjectionPolicy * var);
void readCongestionManagement(const char * c_m, CongestionManagement * var);
void readQCNSWImplementation(const char * q_s_i, QcnSwImplementation * var);
void readQCNSWPolicy(const char * qcn_sw_pol, QcnSwPolicy * var);
void readACORStateManagement(const char * a_s_m, acorStateManagement * var);
void readTraceMap(const char * tracemap_filename);
void buildTraceMap();
