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

#ifndef torus_sim
#define torus_sim

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>
#include <vector>
#include "buffer.h"
#include "gModule.h"
#include "generatorModule.h"
#include "switchModule.h"
#include "flitModule.h"
#include "localArbiter.h"
#include "globalArbiter.h"
#include "configurationFile.h"
#include "global.h"
#include "trace.h"

using namespace std;

int module(int a, int b);
bool parity(int a, int b);
int main(int argc, char *argv[]);
void read_configuration(char * filename);
void create_network();
void run_cycles();
void write_output();
void write_transient_output(char * output_name);
void write_latency_histogram_output(char * output_name);
void write_hops_histogram_output(char * output_name);
void free_memory();

#endif
