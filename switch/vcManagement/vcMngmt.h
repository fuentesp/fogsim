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

#ifndef class_vcMngmt
#define class_vcMngmt

#include "../../flit/flitModule.h"

enum class portClass {
	node, local, global, ring, opplocal, oppglobal, qcn
};

class vcMngmt {

public:
	vector<int> petitionVc;
	vector<int> responseVc;
	vector<portClass> typeVc;
	vector<int> globalVc;
	vector<int> localVcSource;
	vector<int> localVcInter;
	vector<int> localVcDest;
	vector<int> globalResVc;
	vector<int> localResVcSource;
	vector<int> localResVcInter;
	vector<int> localResVcDest;
	switchModule *switchM;

	vcMngmt() {
	}
	vcMngmt(vector<portClass> * hopSeq, switchModule * switchM);
	~vcMngmt();
	virtual int nextChannel(int inP, int outP, flitModule * flit);
	portClass portType(int port);
	virtual vector<int> getVcArray(int port);
	vector<int> getSrcVcArray(int port);
	int getHighestVc(portClass portType);
	virtual void checkVcArrayLengths(short minLocalVCs, short minGlobalVCs);
};

#endif
