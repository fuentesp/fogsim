/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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

#ifndef class_cosarbiter
#define class_cosarbiter

#include "arbiter.h"
#include "lrsArbiter.h"
#include "priorityLrsArbiter.h"
#include "rrArbiter.h"
#include "priorityRrArbiter.h"
#include "ageArbiter.h"
#include "priorityAgeArbiter.h"

using namespace std;

class flitModule;

class cosArbiter {
public:
	unsigned short label;
	int input_port;

	cosArbiter(int inPortNumber, unsigned short cos, switchModule *switchM, ArbiterType policy);
	~cosArbiter();
	int action();
	bool portCanSendFlit(int port, unsigned short cos, int vc);

private:
	arbiter *arbProtocol;
	switchModule *switchM; /* Associated switch */

	bool attendPetition(int input_channel);
	bool petitionCondition(int input_port, int input_channel, int outP, int nextP, int nextC, flitModule *flitEx);
};

#endif
