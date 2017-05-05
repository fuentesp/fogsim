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

#ifndef class_io_switch
#define class_io_switch

#include "switchModule.h"
#include "port/dynBufBufferedOutPort.h"

using namespace std;

class ioqSwitchModule: public switchModule {
private:
	int *lastCheckedOutBuffer;
	long double m_speedup_interval;
	long double m_internal_cycle;
	void updateOutputBuffer(int port);
	void sendFlit(int input_port, unsigned short cos, int input_channel, int outP, int nextP, int nextC);
	void txFlit(int port, flitModule * flit);
	bool nextPortCanReceiveFlit(int port);
	int getCredits(int port, unsigned short cos, int channel);
	int getCreditsOccupancy(int port, unsigned short cos, int channel, int buffer = 0);
	bool checkConsumePort(int port, flitModule* flit);
	void printSwitchStatus();

public:
	ioqSwitchModule(string name, int label, int aPos, int hPos, int ports, int vcCount);
	~ioqSwitchModule();
	void action();
};

#endif
