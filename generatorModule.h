/*
 FOGSim, simulator for interconnection networks.
 https://code.google.com/p/fogsim/
 Copyright (C) 2014 University of Cantabria

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

#ifndef class_generatorAdapt
#define class_generatorAdapt

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "switchModule.h"
#include "event.h"
#include "flitModule.h"
#include "global.h"
#include "gModule.h"
using namespace std;

class generatorModule: public gModule {
private:
	int interArrivalTime;
	int lastTimeSent;
	int Count, in;
	int pPos;
	int aPos;
	int hPos;
	flitModule *flit;
	flitModule *saved_packet;
	bool pending_packet;
	int m_valiantLabel;
	int m_assignedRing;
	int m_min_path;
	int m_val_path;
	int m_injVC;
	int m_flits_sent_count;
	int m_flits_received_count;
	int m_packets_sent_count;
	int m_packets_received_count;
	int m_flitSeq;
	int destLabel;
	int destSwitch;
	bool m_flit_created;
	long long m_packet_id;
	int m_packet_in_cycle;
	int m_phase;
	int m_dest_pointer;
	int *m_dests_array;

public:
	switchModule *switchM;
	int sourceLabel;
	event_q events; ///< A Queue with events to occur
	event_l occurs; ///< Lists with occurred events (one for each messsage source)
	generatorModule(int interArrivalTime, string name, int sourceLabel, int pPos, int aPos, int hPos,
			switchModule *switchM);
	~generatorModule();
	void action();
	bool isAllToAllSent();
	bool isAllToAllReceived();
	bool isPhaseSent();
	bool isPhaseReceived();
	void AllToAllFlitSent();
	void AllToAllFlitReceived();
	void AllToAllPacketSent();
	void AllToAllPacketReceived();
	bool isBurstSent();
	bool isBurstReceived();
	void burstFlitSent();
	void burstFlitReceived();
	void burstPacketSent();
	void burstPacketReceived();
	void traceTrafficGeneration();
	void syntheticTrafficGeneration();
};

#endif
