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

#ifndef CONFIGURATIONFILE_H
#define	CONFIGURATIONFILE_H

#include <string>
#include <string.h>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
using namespace std;

#define MAXLINE 200
#define CONF_COMMENT '#'
#define CONF_SECTION_BEGIN '['
#define CONF_SECTION_END ']'
#define CONF_KEY_ASSIGN '='

enum {
	SECTION, KEY, LIST,
};

typedef vector<string> listEntry_;
typedef map<string, listEntry_> listEntries;
typedef map<string, string> keyEntries;
typedef struct {
	keyEntries keyEntry;
	listEntries listEntry;
} entries;
typedef map<string, entries *> section;

class configFile {
public:
	configFile();
	~configFile();
	int LoadConfig(string fileName);
	int getListValues(const char *section, const char *key, vector<string> &value);
	int getKeyValue(const char *section, const char *key, string &value);
	void flushConfig(void);

private:
	int suppressSpaces(char *line);
	int readLine(const char *line, string &section, string &key, vector<string> &values);
	section sectionEntries;
};

#endif	/* CONFIGURATIONFILE_H */

