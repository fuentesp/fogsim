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

#ifndef STEADY_H_
#define STEADY_H_

#include "../../gModule.h"

class steadyTraffic: public gModule {

protected:
	int sourceLabel;
	int pPos;
	int aPos;
	int hPos;

public:
	int rpDestination;
	steadyTraffic(int sourceLabel, int pPos, int aPos, int hPos);
	~steadyTraffic();
	virtual int setDestination(TrafficType type);
	virtual bool isGenerationFinished();
	inline virtual void flitTx() {
	}
	inline virtual void flitRx() {
	}
};

#endif /* STEADY_H_ */
