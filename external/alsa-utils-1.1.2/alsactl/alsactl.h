extern int debugflag;
extern int force_restore;
extern int ignore_nocards;
extern int do_lock;
extern int use_syslog;
extern char *command;
extern char *statefile;
extern char *lockfile;

void info_(const char *fcn, long line, const char *fmt, ...);
void error_(const char *fcn, long line, const char *fmt, ...);
void cerror_(const char *fcn, long line, int cond, const char *fmt, ...);
void dbg_(const char *fcn, long line, const char *fmt, ...);

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
#define info(...) do { info_(__FUNCTION__, __LINE__, __VA_ARGS__); } while (0)
#define error(...) do { error_(__FUNCTION__, __LINE__, __VA_ARGS__); } while (0)
#define cerror(cond, ...) do { cerror_(__FUNCTION__, __LINE__, (cond) != 0, __VA_ARGS__); } while (0)
#define dbg(...) do { dbg_(__FUNCTION__, __LINE__, __VA_ARGS__); } while (0)
#else
#define info(args...) do { info_(__FUNCTION__, __LINE__, ##args); }  while (0)
#define error(args...) do { error_(__FUNCTION__, __LINE__, ##args); }  while (0)
#define cerror(cond, ...) do { error_(__FUNCTION__, __LINE__, (cond) != 0, ##args); } while (0)
#define dbg(args...) do { dbg_(__FUNCTION__, __LINE__, ##args); }  while (0)
#endif	

int init(const char *file, const char *cardname);
int state_lock(const char *file, int timeout);
int state_unlock(int fd, const char *file);
int save_state(const char *file, const char *cardname);
int load_state(const char *file, const char *initfile, const char *cardname,
	       int do_init);
int power(const char *argv[], int argc);
int monitor(const char *name);
int state_daemon(const char *file, const char *cardname, int period,
		 const char *pidfile);
int state_daemon_kill(const char *pidfile, const char *cmd);

/* utils */

int file_map(const char *filename, char **buf, size_t *bufsize);
void file_unmap(void *buf, size_t bufsize);
size_t line_width(const char *buf, size_t bufsize, size_t pos);
void initfailed(int cardnumber, const char *reason, int exitcode);

static inline int hextodigit(int c)
{
        if (c >= '0' && c <= '9')
                c -= '0';
        else if (c >= 'a' && c <= 'f')
                c = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
                c = c - 'A' + 10;
        else
                return -1;
        return c;
}

#define ARRAY_SIZE(a) (sizeof (a) / sizeof (a)[0])
