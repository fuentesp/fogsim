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

#ifndef class_baseRouting
#define class_baseRouting

#include "../global.h"
#include "../dgflySimulator.h"
#include "../switch/switchModule.h"
#include "../flit/flitModule.h"
#include "../switch/vcManagement/vcMngmt.h"
#include "../switch/vcManagement/flexVc.h"
#include "../switch/vcManagement/tbFlexVc.h"

class switchModule;
class flitModule;

struct candidate {
	int port;
	int vc;
	int neighPort;
};

class baseRouting {
protected:
	switchModule *switchM;

	bool misrouteCondition(flitModule * flit, int prev_outP, int prev_nextC);
	bool misrouteCandidate(flitModule * flit, int inPort, int inVC, int minOutPort, int minOutVC, int &selectedPort,
			int &selectedVC, MisrouteType &misroute_type);
public:
	int portCount; /* Number of ports in switch */
	switchModule **neighList; /* SWITCH associated to each PORT of the current switch */
	int *neighPort; /* PORT NUMBER associated to each PORT of the current switch */
	int *tableSwOut, *tableGroupOut, *tableInRing1, *tableOutRing1, *tableInRing2, *tableOutRing2, *tableInTree,
			*tableOutTree;
	bool *** globalLinkCongested; /* Employed under congested restriction to determine wether to misroute or not */
	vcMngmt* vcM;//TODO: move into protected class, once all routing mechanisms inherit from baseRouting and qcnVcMngmt is addressed properly

	baseRouting(switchModule *switchM);
	~baseRouting();
	void initialize(switchModule* swList[]);
	int minInputPort(int dest);
	int minOutputPort(int dest);
	bool escapePort(int port);
	void updateCongestionStatusGlobalLinks();
	bool validMisroutePort(flitModule * flit, int outP, int nextC, double threshold, MisrouteType misroute);
	virtual struct candidate enroute(flitModule * flit, int inPort, int inVC) = 0;
	int hopsToDest(flitModule * flit, int outP);
	int hopsToDest(int destination);
	void setValNode(flitModule * flit);

private:
	void setMinTables();
	void setRingTables();
	void setTreeTables();
	void findNeighbors(switchModule* swList[]);
	void visitNeighbors();
	bool misrouteCandidate();
	virtual MisrouteType misrouteType(int inPort, int inVC, flitModule * flit, int minOutPort, int minOutVC) = 0;
	virtual int nominateCandidates(flitModule * flit, int inPort, int minOutP, double threshold, MisrouteType &misroute,
			int* &candidates_port, int* &candidates_VC) = 0;
};

#endif
