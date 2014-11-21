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

#include "configurationFile.h"

configFile::configFile() {
}

configFile::~configFile() {
}

int configFile::suppressSpaces(char *line) {
	int len;
	int i = 0, j = 0;

	len = strlen(line);
	if (len > 0) {
		while (isspace(line[i]) || line[i] == '\t') {
			i++;
		}
		while (i <= len) {
			line[j++] = line[i++];
		}

		i = strlen(line) - 1;
		while (i > 0 && (isspace(line[i]) || line[i] == '\t')) {
			line[i--] = '\0';
		}
		len = strlen(line);
	}
	return len;
}

int configFile::readLine(const char *line, string &section, string &key, vector<string> &values) {
	int len;
	int i = 0, j = 0;
	string value;
	string key_;
	int separator;
	int ret;

	section = key = "";
	len = strlen(line);

	if (line[0] == CONF_SECTION_BEGIN && line[len - 1] == CONF_SECTION_END) {
		section = line + 1;
		section.erase(section.size() - 1, 1);
		ret = SECTION;
	} else {
		key_ = line;
		separator = key_.find(CONF_KEY_ASSIGN, 0);
		if (separator != -1) {       // Pair key_value
			key = key_.substr(0, separator);
			value = key_.substr(separator + 1, key_.size() - separator);
			values.push_back(value);
			ret = KEY;
		} else {                // List
			while (i < len) {
				while (!isspace(line[i]) && line[i] != '\t' && i < len) {
					i++;
				}
				value = (line + j);
				if (i < len) {
					value.erase(i - j, value.size() - (i - j));
				}
				if (j == 0) {
					key = value;
				} else {
					values.push_back(value);
				}
				value.clear();
				j = ++i;
			}
			ret = LIST;
		}
	}
	return ret;
}

int configFile::getListValues(const char *section, const char *key, vector<string> &value) {
	string section_ = section;
	string key_ = key;
	entries *entry;

	if (sectionEntries.count(section_)) {
		entry = sectionEntries[section_];
		if (entry->listEntry.count(key_)) {
			value = entry->listEntry[key_];
		} else {
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}

int configFile::getKeyValue(const char *section, const char *key, string &value) {
	string section_ = section;
	string key_ = key;
	entries *entry;

	if (sectionEntries.count(section_)) {
		entry = sectionEntries[section_];
		if (entry->keyEntry.count(key_)) {
			value = entry->keyEntry[key_];
		} else {
			return -1;
		}
	} else {
		return -1;
	}
	return 0;
}

int configFile::LoadConfig(string fileName) {
	fstream confFile;
	vector < string > lines;
	vector < string > values;
	char line[MAXLINE];
	unsigned int i;
	string sectionName, key;
	entries *sectionEntry = NULL;
	section::iterator iter1;
	keyEntries::iterator iter2;
	listEntries::iterator iter3;

	confFile.open(fileName.c_str(), ios::in);
	if (!confFile) {
		return (-1);
	}

	while (confFile.getline(line, MAXLINE)) {
		if (suppressSpaces(line) > 0 && line[0] != CONF_COMMENT) {
			lines.push_back(line);
		}
	}

	for (i = 0; i < lines.size(); i++) {
		switch (readLine((char *) lines[i].c_str(), sectionName, key, values)) {
			case SECTION:
				sectionEntry = new entries;
				sectionEntries[sectionName] = sectionEntry;
				values.clear();
				break;
			case KEY:
				sectionEntry->keyEntry[key] = values[0];
				values.clear();
				break;
			case LIST:
				sectionEntry->listEntry[key] = values;
				values.clear();
				break;
		}
	}

	lines.clear();
	confFile.clear();
	return 0;
}

void configFile::flushConfig(void) {
	section::iterator iter1;
	listEntries::iterator iter3;
	int i;

	for (iter1 = sectionEntries.begin(); iter1 != sectionEntries.end(); ++iter1) {
		(iter1->second)->keyEntry.clear();
		for (iter3 = iter1->second->listEntry.begin(); iter3 != iter1->second->listEntry.end(); ++iter3) {
			for (i = 0; i < (signed) (iter3->second).size(); i++) {
				(iter3->second)[i].clear();
			}
			(iter3->second).clear();
		}

		(iter1->second)->listEntry.clear();
		delete (iter1->second);
	}
	sectionEntries.clear();
}

