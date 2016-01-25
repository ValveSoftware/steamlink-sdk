/*
 * Configuration routines.
 *
 * 20150124 ini file parsing by Sam Lantinga
 */

class GError;
class GKeyFile;

enum
{
	G_KEY_FILE_NONE
};

GKeyFile *g_key_file_new();
bool g_key_file_load_from_file(GKeyFile *key_file, const char *filename, int flags, GError **error);
char *g_key_file_get_string(GKeyFile *key_file, const char *section, const char *option, GError **error);
int g_key_file_get_integer(GKeyFile *key_file, const char *section, const char *option, GError **error);
double g_key_file_get_double(GKeyFile *key_file, const char *section, const char *option, GError **error);
void g_key_file_free(GKeyFile *key_file);
