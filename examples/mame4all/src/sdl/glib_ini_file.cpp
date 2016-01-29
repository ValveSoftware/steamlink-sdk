/*
 * Configuration routines.
 *
 * 20150124 ini file parsing by Sam Lantinga
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <string>

#include "glib_ini_file.h"


class GError
{
};
static GError s_error;

class GKeyFile
{
public:
	GKeyFile();
	~GKeyFile();

	bool init(const char *filename);
	char *get_string(const char *section, const char *option);

private:
	struct Entry
	{
		std::map<std::string, char *> m_values;
	};
	std::map<std::string, Entry *> m_entries;

	char *m_data;
};


GKeyFile::GKeyFile()
: m_data(NULL)
{
}

GKeyFile::~GKeyFile()
{
	for (std::map<std::string, Entry *>::iterator it = m_entries.begin(); it != m_entries.end(); ++it) {
		delete it->second;
	}
	delete[] m_data;
}

bool GKeyFile::init(const char *filename)
{
	FILE *file = fopen(filename, "r");
	if (!file) {
		return false;
	}

	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	m_data = new char[size+1];
	if (!fread(m_data, size, 1, file)) {
		fclose(file);
		return false;
	}
	m_data[size] = '\0';
	fclose(file);

	Entry *entry = NULL;
	for (char *spot = m_data; *spot; ++spot) {
		if (*spot == '\r' || *spot == '\n') {
			continue;
		}

		if (*spot == '[') {
			char *last = ++spot;
			while (*spot && !isspace(*spot) && *spot != ']') {
				++spot;
			}
			if (*spot != ']') {
				// Unterminated section
				return false;
			}
			*spot = '\0';

			entry = new Entry;
			m_entries[last] = entry;
		} else {
			char *end;
			char *variable = spot;
			while (*spot && *spot != '\r' && *spot != '\n' && *spot != '=') {
				++spot;
			}
			if (!*spot) {
				// end of file
				break;
			}
			if (*spot != '=') {
				continue;
			}
			end = spot;
			while (isspace(*variable) && variable < end) {
				++variable;
			}
			while (isspace(end[-1]) && end > variable) {
				--end;
			}
			*end = '\0';

			char *value = ++spot;
			while (*spot && *spot != '\r' && *spot != '\n') {
				++spot;
			}
			bool eof = (*spot == '\0');
			end = spot;
			while (isspace(*value) && value < end) {
				++value;
			}
			while (isspace(end[-1]) && end > value) {
				--end;
			}
			*end = '\0';

			if (entry && *variable) {
				entry->m_values[variable] = value;
			}
			if (eof) {
				break;
			}
		}
	}
	return true;
}

char *GKeyFile::get_string(const char *section, const char *option)
{
	std::map<std::string, Entry *>::iterator it = m_entries.find(section);
	if (it != m_entries.end()) {
		std::map<std::string, char *>::iterator that = it->second->m_values.find(option);
		if (that != it->second->m_values.end()) {
			return that->second;
		}
	}
	return NULL;
}


GKeyFile *g_key_file_new()
{
	return new GKeyFile;
}

bool g_key_file_load_from_file(GKeyFile *key_file, const char *filename, int flags, GError **error)
{
	if (!key_file->init(filename)) {
		*error = &s_error;
		return false;
	}
	return true;
}

char *g_key_file_get_string(GKeyFile *key_file, const char *section, const char *option, GError **error)
{
	char *str = key_file->get_string(section, option);
	if (!str) {
		*error = &s_error;
	}
	return str;
}

int g_key_file_get_integer(GKeyFile *key_file, const char *section, const char *option, GError **error)
{
	int num = 0;
	char *str = key_file->get_string(section, option);
	if (str) {
		char *end = NULL;
		errno = 0;
		num = strtod(str, &end);
		if (errno || end == str) {
			*error = &s_error;
		}
	}
	return num;
}

double g_key_file_get_double(GKeyFile *key_file, const char *section, const char *option, GError **error)
{
	double num = 0.0;
	char *str = key_file->get_string(section, option);
	if (str) {
		char *end = NULL;
		errno = 0;
		num = strtof(str, &end);
		if (errno || end == str) {
			*error = &s_error;
		}
	}
	return num;
}

void g_key_file_free(GKeyFile *key_file)
{
	delete key_file;
}

