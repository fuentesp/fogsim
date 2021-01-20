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

#ifndef ALL2ALL_H_
#define ALL2ALL_H_

#include "steady.h"

class all2allTraffic: public steadyTraffic {

protected:
	int flits_tx_count;
	int flits_rx_count;
	int phase_count;
	int dest_pointer;
	int *dests_array;
	int prev_dest;

	bool isPhaseSent();
	bool isPhaseRx();

public:
	all2allTraffic(int sourceLabel, int pPos, int aPos, int hPos);
	~all2allTraffic();
	int setDestination(TrafficType type);
	bool isGenerationFinished();
	void flitTx();
	void flitRx();
};

#endif /* ALL2ALL_H_ */
