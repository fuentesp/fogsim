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

#ifndef trace_
#define trace_

#include "../global.h"
#include "dimemas.h"
#include "event.h"
#include "generatorModule.h"

#define FILE_TIME 73		///< Default delay for accessing a file.
#define FILE_SCALE 3		///< Default scale for file accesses based on the size.
#define BUFSIZE 131072		///< The size of the buffer,
void read_dimemas(int trace_id, vector<int> instances);
void read_fsin_trc();
void read_alog();
void read_trace(int trace_id, vector<int> instances);

void random_placement();
void consecutive_placement();
void shuffle_placement();
void shift_placement();
void column_placement();
void gaussian_placement();
void quadrant_placement();
void diagonal_placement();
void icube_placement();
void file_placement();
long gcd(long a, long b);
void translate_pcf_file(int trace_id);
void save_event_type(char* event_line);

#endif /* trace_ */
