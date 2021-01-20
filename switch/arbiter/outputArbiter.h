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

#ifndef class_globalarbiter
#define class_globalarbiter

#include "arbiter.h"
#include "lrsArbiter.h"
#include "priorityLrsArbiter.h"
#include "rrArbiter.h"
#include "priorityRrArbiter.h"
#include "ageArbiter.h"
#include "priorityAgeArbiter.h"

using namespace std;

class outputArbiter {
public:
	int label; /* Arbiter ID */
	int *petitions;
	int *nextChannels;
	int *nextPorts;
	int *inputChannels;
	unsigned short *inputCos;
	outputArbiter(int outPortNumber, unsigned short cosLevels, int ports, switchModule *switchM, ArbiterType policy);
	~outputArbiter();
	int action();
	void initPetitions();
	bool checkPort();

private:
	arbiter *arbProtocol;
	switchModule *switchM; /* Associated switch */

	bool attendPetition(int port);
	void updateStatistics(int port);

};

#endif
