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

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>

#include "buffer.h"
#include "gModule.h"
#include "generatorModule.h"
#include "switchModule.h"
#include "flitModule.h"
#include "dgflySimulator.h"

using namespace std;

globalArbiter::globalArbiter(int outPortNumber, int ports) {
	this->ports = ports;
	this->outPortNumber = outPortNumber;
	this->LRSPortList = new int[ports];
	this->olderPortList = new int[ports];
	this->petitions = new int[ports];
	this->nextPorts = new int[ports];
	this->nextChannels = new int[ports];
	this->inputChannels = new int[ports];

	for (int i = 0; i < ports; i++) {
		LRSPortList[i] = i;
	}

	for (int i = 0; i < ports; i++) {
		olderPortList[i] = i;
	}

	for (int i = 0; i < ports; i++) {
		petitions[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		inputChannels[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		nextPorts[i] = 0;
	}
	for (int i = 0; i < ports; i++) {
		nextChannels[i] = 0;
	}
}

globalArbiter::~globalArbiter() {
	delete[] LRSPortList;
	delete[] olderPortList;
	delete[] petitions;
	delete[] nextPorts;
	delete[] nextChannels;
	delete[] inputChannels;

}

bool globalArbiter::reorderLRSPortList(int servedPort) {
	for (int i = 0; i < ports; i++) {
		if (LRSPortList[i] == servedPort) {
			for (int c = i; c < (ports - 1); c++) {
				LRSPortList[c] = LRSPortList[c + 1];
			}
			LRSPortList[ports - 1] = servedPort;
			return true;
		}
	}
	cout << "Served port is not on the ports list. Something is wrong." << endl;
	assert(0);
}

bool globalArbiter::NOTreorderLRSPortList(int servedPort) {
	for (int i = 0; i < ports; i++) {
		if (LRSPortList[i] == servedPort) {
			for (int c = i; c >= 1; c--) {
				LRSPortList[c] = LRSPortList[c - 1];
			}
			LRSPortList[0] = servedPort;
			return true;
		}
	}
	cout << "Served port is not on the ports list. Something is wrong." << endl;
	assert(0);
}
