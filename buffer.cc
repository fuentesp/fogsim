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

#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <fstream>

#include "buffer.h"
#include "gModule.h"
#include "generatorModule.h"
#include "switchModule.h"
#include "flitModule.h"
#include "dgflySimulator.h"

using namespace std;

buffer::buffer(int bufferNumber, int buffCap, int delay, switchModule * sw) {
	this->bufferNumber = bufferNumber;
	this->bufferCapacity = int(buffCap / g_flitSize); //Flits
	this->delay = delay;
	assert(delay >= 0);
	this->bufferEntryCycle = new int[bufferCapacity + 1]; // Range [0... bufferCapacity]
	this->bufferContent = new flitModule *[bufferCapacity + 1];
	this->head = 0;
	this->tail = 0;
	this->escapeBuffer = false;

	this->lastExtractCycle = -10000000;
	this->lastReceiveCycle = -10000000;
	for (int j = 0; j <= bufferCapacity; j++) {
		assert(j >= 0);
		assert(j <= bufferCapacity);
		bufferEntryCycle[j] = -1;
		bufferContent[j] = NULL;
	}

	m_sw = sw;
	m_unLocked = 1;
	m_portLock = -1; //buffer lock by packet in port m_portLock
	m_vcLock = -1; //buffer lock by packet in vc m_vcLock
	m_pktLock = -1; //buffer lock by packet m_pktLock
	m_outPort_currentPkt = -1; // next port for the current packet being sent
	m_nextVC_currentPkt = -1; // next vc for the current packet being sent
	m_currentPkt = -1; // current packet being sent
}

buffer::~buffer() {
	int j;
	for (j = 0; j <= bufferCapacity; j++) {
		assert(j >= 0);
		assert(j <= bufferCapacity);
		delete bufferContent[j];
	}
	delete[] bufferContent;
	delete[] bufferEntryCycle;
}

bool buffer::unLocked() {
	if (m_unLocked) {
		assert(m_portLock == -1);
		assert(m_vcLock == -1);
		assert(m_pktLock == -1);
	} else {
		assert(m_portLock != -1);
		assert(m_vcLock != -1);
		assert(m_pktLock != -1);
	}
	return m_unLocked;
}

bool buffer::extract(flitModule* &flitExtracted) {
	assert(canSendFlit());
	flitExtracted = bufferContent[head];
	bufferEntryCycle[head] = -1;
	bufferContent[head] = NULL;
	this->lastExtractCycle = g_cycle;
	m_sw->messagesInQueuesCounter -= 1;
	return true;
}

void buffer::reorderBuffer() {
	if (lastExtractCycle == g_cycle) {
		head = (head + 1) % (this->bufferCapacity + 1);
	}
}

int buffer::getDelay() const {
	return delay;
}

void buffer::checkFlit(flitModule* &nextFlit) {
	if (bufferEntryCycle[head] != -1) {
		nextFlit = bufferContent[head];
	} else {
		nextFlit = NULL;
	}
}

void buffer::insert(flitModule *flit) {
	assert(this->getSpace() > 0);
	bufferEntryCycle[tail] = g_cycle + delay;
	bufferContent[tail] = flit;
	tail = (tail + 1) % (this->bufferCapacity + 1);
	assert(tail >= 0);
	assert(tail <= bufferCapacity);
	this->lastReceiveCycle = g_cycle + delay;
	m_sw->messagesInQueuesCounter += 1;

}

/*
 * Returns the number of free phits in the buffer
 */
int buffer::getSpace() {
	return (g_flitSize * (bufferCapacity - module((tail - head), (bufferCapacity + 1))));
}

/*
 * Returns the number of occupied phits in the buffer
 */
int buffer::getBufferOccupancy() {
	return (g_flitSize * (module((tail - head), (bufferCapacity + 1))));
}

/*
 * Returns TRUE if the buffer is empty in the current cycle
 */
bool buffer::emptyBuffer() {
	if ((bufferEntryCycle[head] != -1) && (bufferEntryCycle[head] <= g_cycle)) {
		return false;
	}
	return true;
}

/*
 * Checks if buffer has finished sending previous packet [if packet contains more
 * than 1 phit, it takes some time between the 1st and 2nd phits are received].
 */
bool buffer::canSendFlit() {
	if ((this->lastExtractCycle + g_flitSize <= g_cycle) && (bufferEntryCycle[head] <= g_cycle)) {
		return true;
	}
	return false;
}

/*
 * Checks if buffer has finished receiving previous packet.
 */
bool buffer::canReceiveFlit() {
	if (this->lastReceiveCycle + g_flitSize <= g_cycle + delay) {
		return true;
	}
	return false;
}

