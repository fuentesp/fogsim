/*
 FOGSim, simulator for interconnection networks.
 http://fuentesp.github.io/fogsim/
 Copyright (C) 2017 University of Cantabria

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
#include <iostream>

ConfigFile::ConfigFile() {
}

ConfigFile::~ConfigFile() {
}

int ConfigFile::suppressSpaces(char *line) {
	int i = 0, j = 0;
	int len = strlen(line);
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

int ConfigFile::readLine(const char *line, string &section, string &key, vector<string> &values) {
	string value;
	string key_;
	int separator;
	int ret;
	int len = strlen(line);

	section = key = "";

	if (line[0] == CONF_SECTION_BEGIN && line[len - 1] == CONF_SECTION_END) {
		section = line + 1;
		section.erase(section.size() - 1, 1);
		ret = SECTION;
	} else {
		key_ = line;
		separator = key_.find(CONF_LIST_ITEM, 0);
		if (separator != -1) {
			/* List of values assigned in-order */
			separator = key_.find(CONF_KEY_ASSIGN);
			key = key_.substr(0, separator);
			key_ = key_.substr(separator + 1);
			while (key_.find(CONF_LIST_ITEM) != -1) {
				separator = key_.find(CONF_LIST_ITEM);
				value = key_.substr(0, separator);
				values.push_back(value);
				key_ = key_.substr(separator + 1);
			}
			value = key_;
			values.push_back(value);
			ret = LIST;
		} else {
			separator = key_.find(CONF_KEY_ASSIGN, 0);
			if (separator != -1) {		// Pair key_value
				key = key_.substr(0, separator);
				value = key_.substr(separator + 1, key_.size() - separator);
				values.push_back(value);
				ret = KEY;
			}
		}
	}

	return ret;
}

int ConfigFile::getListValues(const char *section, const char *key, vector<string> &value) {
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

int ConfigFile::getKeyValue(const char *section, const char *key, string &value) {
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

int ConfigFile::LoadConfig(string fileName) {
	fstream confFile;
	vector < string > lines;
	vector < string > values;
	char line[MAXLINE];
	unsigned int i, j;
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
			case MAP:
				if (sectionEntry->keyEntry.count(key)) {
					values.push_back(sectionEntry->keyEntry[key]);
				}
				sectionEntry->listEntry[key] = values;
				break;
		}
	}

	for (iter1 = sectionEntries.begin(); iter1 != sectionEntries.end(); ++iter1) {
		for (iter2 = iter1->second->keyEntry.begin(); iter2 != iter1->second->keyEntry.end(); ++iter2) {
		}
		for (iter3 = iter1->second->listEntry.begin(); iter3 != iter1->second->listEntry.end(); ++iter3) {
			for (j = 0; j < (iter3->second).size(); j++) {
			}
		}

	}
	lines.clear();
	confFile.clear();
	return 0;
}

void ConfigFile::flushConfig(void) {
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

/***
 * Updates/inserts new key in given section. If section does not
 * exist, will create it. This function is aimed to incorporate
 * new parameters through the command line.
 *
 * MAIN RESTRICTION: this function will only work with simple
 * values (KEY values), NOT with lists. Please take it into
 * consideration.
 */
int ConfigFile::updateKeyValue(const char *section, char *line) {
	/***
	 * Read received line and update/insert key in corresponding section
	 */
	entries *entry = NULL;
	string section_ = section;
	string key;
	string key_;
	string value;
	vector < string > values;
	int separator;
	int type;

	/* Check if a section with the given name exists, and create it otherwise */
	if (sectionEntries.count(section_)) {
		entry = sectionEntries[section_];
	} else {
		entry = new entries;
		sectionEntries[section_] = entry;
	}

	/* Determine entry's name and type (KEY/LIST) */
	key_ = line;
	separator = key_.find(CONF_KEY_ASSIGN, 0);
	if (separator != -1) {       // Pair key_value
		key = key_.substr(0, separator);
		value = key_.substr(separator + 1, key_.size() - separator);
		values.push_back(value);
		type = KEY;
	} else {
		/* Since we can not accept (for the moment) list values, this is an error */
		return -1;
	}

	/* Update/insert new entry. Sanity check: if entry exists,
	 * ensure type suits previous definition */
	switch (type) {
		case KEY:
			if (entry->listEntry.count(key_)) return -1;
			entry->keyEntry[key] = values[0];
			values.clear();
			break;
		case LIST:
			if (entry->keyEntry.count(key_)) return -1;
			entry->listEntry[key] = values;
			values.clear();
			break;
	}

	return 0;
}

int ConfigFile::checkSection(const char *section) {
	string section_ = section;
	return (sectionEntries.count(section_));
}
