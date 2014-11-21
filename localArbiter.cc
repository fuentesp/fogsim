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

localArbiter::localArbiter(int inPortNumber) {
	this->inPortNumber = inPortNumber;
	this->LRSChannelList = new int[g_channels];
	this->olderChannelList = new int[g_channels];
	for (int i = 0; i < g_channels; i++) {
		this->LRSChannelList[i] = i;
	}
	for (int i = 0; i < g_channels; i++) {
		this->olderChannelList[i] = i;
	}
}

localArbiter::~localArbiter() {
	delete[] LRSChannelList;
	delete[] olderChannelList;
}

bool localArbiter::reorderLRSChannelList(int servedChannel) {
	for (int i = 0; i < g_channels; i++) {
		if (this->LRSChannelList[i] == servedChannel) {
			for (int c = i; c < (g_channels - 1); c++) {
				this->LRSChannelList[c] = this->LRSChannelList[c + 1];
			}
			this->LRSChannelList[g_channels - 1] = servedChannel;
			return true;
		}
	}
	cout << "Served channel is not on the ports list. Something is wrong." << endl;
	assert(0);
}

bool localArbiter::NOTreorderLRSChannelList(int servedChannel) {
	for (int i = 0; i < g_channels; i++) {
		if (this->LRSChannelList[i] == servedChannel) {
			for (int c = i; c >= 1; c--) {
				this->LRSChannelList[c] = this->LRSChannelList[c - 1];
			}
			this->LRSChannelList[0] = servedChannel;
			return true;
		}
	}
	cout << "Served channel is not on the ports list. Something is wrong." << endl;
	assert(0);
}
