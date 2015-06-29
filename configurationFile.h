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

#ifndef CONFIGURATIONFILE_H
#define	CONFIGURATIONFILE_H

#include <string.h>
#include <map>
#include <vector>
#include <fstream>
using namespace std;

#define MAXLINE 200
#define CONF_COMMENT '#'
#define CONF_SECTION_BEGIN '['
#define CONF_SECTION_END ']'
#define CONF_KEY_ASSIGN '='
#define CONF_LIST_BEGIN '('
#define CONF_LIST_END ')'
#define CONF_LIST_ITEM ','

enum {
	SECTION, KEY, LIST, MAP,
};

typedef vector<string> listEntry_;
typedef map<string, listEntry_> listEntries;
typedef map<string, string> keyEntries;
typedef struct {
	keyEntries keyEntry;
	listEntries listEntry;
} entries;
typedef map<string, entries *> section;

class ConfigFile {
public:
	ConfigFile();
	~ConfigFile();

	int LoadConfig(string fileName);
	int getListValues(const char *section, const char *key, vector<string> &value);
	int getKeyValue(const char *section, const char *key, string &value);
	int updateKeyValue(const char *section, char *line);
	int checkSection(const char *section);
	void flushConfig(void);

private:
	int suppressSpaces(char *line);
	int readLine(const char *line, string &section, string &key, vector<string> &values);
	section sectionEntries;
};

#endif	/* CONFIGURATIONFILE_H */

