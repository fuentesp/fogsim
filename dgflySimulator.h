/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
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

#ifndef torus_sim
#define torus_sim

#include <limits.h>
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
void freeMemory();
void readSwitchType(const char * switch_type, SwitchType * var);
void readTrafficPattern(const char * traffic_name, TrafficType * var);
void readRoutingType(const char * routing_type, RoutingType * var);
void readGlobalMisrouting(const char * global_misrouting, GlobalMisroutingPolicy * var);
void readDeadlockAvoidanceMechanism(const char * d_a, DeadlockAvoidance * var);
void readMisroutingTrigger(const char * d_a, MisroutingTrigger * var);
void readTraceDistribution(const char * d_a, TraceAssignation * var);
void readTraceMap(const char * tracemap_filename);
void buildTraceMap();
#endif
