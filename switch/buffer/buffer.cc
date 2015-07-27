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

#include "buffer.h"
#include "../../dgflySimulator.h"
#include "../switchModule.h"

using namespace std;

buffer::buffer(int bufferNumber, int buffCap, float delay) {
	this->bufferNumber = bufferNumber;
	this->bufferCapacity = int(buffCap / g_flit_size);
	this->delay = delay;
	assert(delay >= 0);
	this->bufferEntryCycle = new float[bufferCapacity + 1]; /* Range [0... bufferCapacity] */
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

	m_unLocked = 1;
	m_portLock = -1;
	m_vcLock = -1;
	m_pktLock = -1;
	m_outPort_currentPkt = -1;
	m_nextVC_currentPkt = -1;
	m_currentPkt = -1;
	txLength = 0;
}

buffer::~buffer() {
	for (int j = 0; j <= bufferCapacity; j++) {
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

bool buffer::extract(flitModule* &flitExtracted, float length) {
	assert(canSendFlit());
	flitExtracted = bufferContent[head];
	bufferEntryCycle[head] = -1;
	bufferContent[head] = NULL;
	this->lastExtractCycle = g_internal_cycle;
	txLength = length;
	return true;
}

void buffer::reorderBuffer() {
	if (lastExtractCycle >= g_internal_cycle) {
		head = (head + 1) % (this->bufferCapacity + 1);
	}
}

float buffer::getDelay() const {
	return delay;
}

float buffer::getHeadEntryCycle() {
	return bufferEntryCycle[head];
}

void buffer::checkFlit(flitModule* &nextFlit) {
	if (bufferEntryCycle[head] != -1) {
		assert(bufferContent[head] != NULL);
		nextFlit = bufferContent[head];
	} else {
		nextFlit = NULL;
	}
}

void buffer::insert(flitModule *flit, float length) {
	assert(this->getSpace() > 0);
	bufferEntryCycle[tail] = g_internal_cycle + delay;
	bufferContent[tail] = flit;
	tail = (tail + 1) % (this->bufferCapacity + 1);
	assert(tail >= 0);
	assert(tail <= bufferCapacity);
	this->lastReceiveCycle = g_internal_cycle + length + delay;
}

/*
 * Returns the number of free phits in the buffer
 */
int buffer::getSpace() {
	return (g_flit_size * (bufferCapacity - module((tail - head), (bufferCapacity + 1))));
}

/*
 * Returns the number of occupied phits in the buffer
 */
int buffer::getBufferOccupancy() {
	return (g_flit_size * (module((tail - head), (bufferCapacity + 1))));
}

/*
 * Returns TRUE if the buffer is empty in the current cycle
 */
bool buffer::emptyBuffer() {
	if ((bufferEntryCycle[head] != -1) && (bufferEntryCycle[head] <= g_internal_cycle)) {
		return false;
	}
	return true;
}

/*
 * Returns TRUE if previous flit transmission has ended and
 * reception of current head-of-buffer flit has already started.
 */
bool buffer::canSendFlit() {
	if ((this->lastExtractCycle + txLength <= g_internal_cycle) && (bufferEntryCycle[head] <= g_internal_cycle)) {
		return true;
	}
	return false;
}

/*
 * Returns TRUE if previous flit reception has been fully completed.
 */
bool buffer::canReceiveFlit() {
	if (this->lastReceiveCycle <= g_internal_cycle + delay) {
		return true;
	}
	return false;
}

bool buffer::isBufferSending() {
	if (this->lastExtractCycle + txLength > g_internal_cycle) {
		return true;
	}
	return false;
}
