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

#ifndef class_globalarbiter
#define class_globalarbiter

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>

#include "dgflySimulator.h"

using namespace std;

class globalArbiter {
public:
	int ports;
	int outPortNumber;
	int *LRSPortList;
	int *olderPortList;
	int *petitions;
	int *nextChannels;
	int *nextPorts;
	int *inputChannels;
	globalArbiter(int outPortNumber, int ports);
	~globalArbiter();
	bool reorderLRSPortList(int servedChannel);
	bool NOTreorderLRSPortList(int servedChannel);
};

#endif
