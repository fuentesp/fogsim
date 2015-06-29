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

#ifndef TRACEGENERATOR_H_
#define TRACEGENERATOR_H_

#include "generatorModule.h"

class traceGenerator: public generatorModule {
private:
	flitModule *saved_packet;
	bool pending_packet;

public:
	event_q events; /* Queue to store trace events  */
	event_l occurs; /* List to store occurred events (one for each message source) */
	traceGenerator(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
			switchModule *switchM);
	~traceGenerator();
	void action();
	void generateFlit();
	bool isGenerationEnded();
	void printHeadEvent();
	int numSkipCycles();
	void consumeCycles(int numCycles);
	void resetEventQueue();
	void insertEvent(event i, int destId);
	void insertOccurredEvent(flitModule *flit);
};

#endif /* TRACEGENERATOR_H_ */
