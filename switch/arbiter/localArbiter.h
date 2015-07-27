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

#ifndef class_localarbiter
#define class_localarbiter

#include "../../global.h"
#include "arbiter.h"

using namespace std;

class flitModule;

class localArbiter: public arbiter {
public:
	localArbiter(int inPortNumber, switchModule *switchM);
	~localArbiter();
	bool checkPort();
	bool portCanSendFlit(int port, int vc);

private:
	bool attendPetition(int input_channel);
	bool petitionCondition(int input_port, int input_channel, int outP, int nextP, int nextC, flitModule *flitEx);
	void updateStatistics(int port);
};

#endif
