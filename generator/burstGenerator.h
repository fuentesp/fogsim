/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
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

#ifndef BURSTGENERATOR_H_
#define BURSTGENERATOR_H_

#include "generatorModule.h"

class burstGenerator: public generatorModule {
protected:
	/* Change-of-state probabilities: determine the rate of start/stop
	 * injecting traffic, and the average burst length. */
	double pOff2On;
	double pOn2Off;
	bool injecting;
	int prevDest;
	int curBurstLength;

public:
	burstGenerator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
			switchModule *switchM);
	~burstGenerator();
	void generateFlit();
};

#endif /* BURSTGENERATOR_H_ */
