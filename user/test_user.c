/* ==========================================================================
 * AstraOS - User-Space Shell
 * ==========================================================================
 * Interactive command interpreter running in ring 3. Reads from
 * /dev/console, parses commands, and executes built-in operations.
 *
 * Built-in commands:
 *   help      — Show available commands
 *   clear     — Clear the screen
 *   echo      — Print text
 *   cat       — Display file contents
 *   ls        — List directory entries
 *   version   — Show OS version
 *   hostname  — Show system hostname
 *   mem       — Show memory statistics
 *   uptime    — Show system uptime
 *   tasks     — Show active task count
 *   caps      — Show current capabilities
 * ========================================================================== */

/* --- Syscall stubs --- */

static inline unsigned int sc0(unsigned int n)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n));
    return r;
}
static inline unsigned int sc1(unsigned int n, unsigned int a)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a));
    return r;
}
static inline unsigned int sc2(unsigned int n, unsigned int a, unsigned int b)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b));
    return r;
}
static inline unsigned int sc3(unsigned int n, unsigned int a,
                                unsigned int b, unsigned int c)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r)
                      : "a"(n), "b"(a), "c"(b), "d"(c));
    return r;
}
static inline unsigned int sc4(unsigned int n, unsigned int a,
                                unsigned int b, unsigned int c, unsigned int d)
{
    unsigned int r;
    __asm__ volatile ("int $0x80" : "=a"(r)
                      : "a"(n), "b"(a), "c"(b), "d"(c), "S"(d));
    return r;
}

#define SYS_EXIT    0
#define SYS_WRITE   1
#define SYS_GETPID  2
#define SYS_YIELD   3
#define SYS_SLEEP   4
#define SYS_OPEN    5
#define SYS_READ    6
#define SYS_CLOSE   7
#define SYS_STAT    8
#define SYS_READDIR 9
#define SYS_SPAWN   10
#define SYS_WAITPID 11
#define SYS_PIPE    12
#define SYS_KILL    13
#define SYS_GETPPID 14
#define SYS_REBOOT  15

/* --- String utilities --- */

static unsigned int slen(const char *s)
{
    unsigned int l = 0;
    while (s[l]) l++;
    return l;
}

static int sncmp(const char *a, const char *b, unsigned int n)
{
    for (unsigned int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

/* --- I/O helpers --- */

static int console_fd = -1;

static void puts(const char *s)
{
    sc3(SYS_WRITE, 1, (unsigned int)s, slen(s));
}

static void putdec(unsigned int val)
{
    static char dbuf[12];
    if (val == 0) { sc3(SYS_WRITE, 1, (unsigned int)"0", 1); return; }
    int i = 0;
    char tmp[12];
    while (val > 0) { tmp[i++] = '0' + (char)(val % 10); val /= 10; }
    for (int j = 0; j < i; j++) dbuf[j] = tmp[i - 1 - j];
    sc3(SYS_WRITE, 1, (unsigned int)dbuf, (unsigned int)i);
}

static unsigned int readline(char *buf, unsigned int max)
{
    /* Read from console fd — the kernel line discipline handles echo */
    unsigned int n = sc3(SYS_READ, (unsigned int)console_fd, (unsigned int)buf, max - 1);
    /* Strip trailing newline */
    if (n > 0 && buf[n - 1] == '\n')
        n--;
    buf[n] = '\0';
    return n;
}

/* Read a file and print its contents */
static void cat_file(const char *path)
{
    unsigned int fd = sc2(SYS_OPEN, (unsigned int)path, 0);
    if (fd == (unsigned int)-1) {
        puts("cat: cannot open '");
        puts(path);
        puts("'\n");
        return;
    }
    static char fbuf[512];
    unsigned int n;
    while ((n = sc3(SYS_READ, fd, (unsigned int)fbuf, 511)) > 0) {
        sc3(SYS_WRITE, 1, (unsigned int)fbuf, n);
    }
    sc1(SYS_CLOSE, fd);
}

/* --- Command handlers --- */

static void cmd_help(void)
{
    puts("AstraOS Shell Commands:\n");
    puts("  help      Show this help\n");
    puts("  clear     Clear screen\n");
    puts("  echo      Print text\n");
    puts("  cat       Display file contents\n");
    puts("  ls        List directory\n");
    puts("  run       Run a program (e.g. run /bin/hello)\n");
    puts("  version   OS version\n");
    puts("  hostname  System hostname\n");
    puts("  mem       Memory statistics\n");
    puts("  uptime    System uptime\n");
    puts("  tasks     Active task count\n");
    puts("  ps        List running tasks\n");
    puts("  kill      Kill a task (e.g. kill 4)\n");
    puts("  date      Show current date/time\n");
    puts("  sysinfo   System info summary\n");
    puts("  whoami    Show current user info\n");
    puts("  reboot    Reboot the system\n");
    puts("  caps      Show capabilities\n");
}

static void cmd_clear(void)
{
    puts("\f");
}

static void cmd_echo(const char *args)
{
    if (*args)
        puts(args);
    puts("\n");
}

static void cmd_cat(const char *args)
{
    if (!*args) {
        puts("usage: cat <file>\n");
        return;
    }
    cat_file(args);
}

static void cmd_ls(const char *args)
{
    const char *path = (*args) ? args : "/";
    static char name[64];
    unsigned int idx = 0;
    unsigned int n;

    while ((n = sc4(SYS_READDIR, (unsigned int)path, idx,
                    (unsigned int)name, 63)) > 0) {
        puts("  ");
        puts(name);

        /* Check if it's a directory by trying readdir on it */
        static char child_path[256];
        unsigned int plen = slen(path);
        unsigned int nlen = slen(name);

        /* Build child path */
        unsigned int cp = 0;
        for (unsigned int i = 0; i < plen && cp < 254; i++)
            child_path[cp++] = path[i];
        if (cp > 0 && child_path[cp - 1] != '/')
            child_path[cp++] = '/';
        for (unsigned int i = 0; i < nlen && cp < 254; i++)
            child_path[cp++] = name[i];
        child_path[cp] = '\0';

        unsigned int type = 0;
        sc2(SYS_STAT, (unsigned int)child_path, (unsigned int)&type);
        if (type & 0x02)  /* VFS_DIRECTORY */
            puts("/");
        else if (type & 0x04)  /* VFS_CHARDEVICE */
            puts(" [dev]");

        puts("\n");
        idx++;
    }

    if (idx == 0)
        puts("  (empty or not a directory)\n");
}

static void cmd_version(void)
{
    cat_file("/etc/version");
    puts("\n");
}

static void cmd_hostname(void)
{
    cat_file("/etc/hostname");
    puts("\n");
}

static void cmd_mem(void)
{
    cat_file("/proc/meminfo");
}

static void cmd_uptime(void)
{
    cat_file("/proc/uptime");
}

static void cmd_tasks(void)
{
    cat_file("/proc/tasks");
}

static void cmd_ps(void)
{
    cat_file("/proc/tasks");
}

static unsigned int parse_uint(const char *s)
{
    unsigned int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (unsigned int)(*s - '0');
        s++;
    }
    return val;
}

static void cmd_kill(const char *args)
{
    if (!*args) {
        puts("usage: kill <pid>\n");
        return;
    }
    unsigned int pid = parse_uint(args);
    if (pid == 0) {
        puts("kill: invalid pid\n");
        return;
    }
    unsigned int r = sc2(SYS_KILL, pid, 9);  /* SIGKILL */
    if (r == (unsigned int)-1)
        puts("kill: failed\n");
    else {
        puts("killed task ");
        putdec(pid);
        puts("\n");
    }
}

static void cmd_date(void)
{
    cat_file("/dev/rtc");
}

static void cmd_sysinfo(void)
{
    puts("\n");
    puts("        *        ");
    cat_file("/etc/hostname");
    puts("\n");
    puts("       ***       --------\n");
    puts("      *****      OS: ");
    cat_file("/etc/version");
    puts("\n");
    puts("     *******     Arch: i686 (x86)\n");
    puts("    *********    Shell: astra-sh\n");
    puts("   ***********   Uptime: ");
    cat_file("/proc/uptime");
    puts("      *****      Memory: ");
    cat_file("/proc/meminfo");
    puts("     *     *     Date: ");
    cat_file("/dev/rtc");
    puts("\n");
}

static void cmd_whoami(void)
{
    puts("root (pid=");
    putdec(sc0(SYS_GETPID));
    puts(", ppid=");
    putdec(sc0(SYS_GETPPID));
    puts(")\n");
}

static void cmd_reboot(void)
{
    puts("Rebooting...\n");
    sc0(SYS_REBOOT);
}

static void cmd_caps(void)
{
    unsigned int pid = sc0(SYS_GETPID);
    puts("PID: ");
    putdec(pid);
    puts("\nCapabilities: VFS_READ VFS_WRITE DEVICE_ACCESS PROC_CREATE SIGNAL\n");
}

static void cmd_run(const char *args)
{
    if (!*args) {
        puts("usage: run <path>\n");
        return;
    }
    unsigned int tid = sc1(SYS_SPAWN, (unsigned int)args);
    if (tid == (unsigned int)-1) {
        puts("run: failed to spawn '");
        puts(args);
        puts("'\n");
        return;
    }
    puts("[started task ");
    putdec(tid);
    puts("]\n");
    unsigned int exit_code = sc1(SYS_WAITPID, tid);
    puts("[task ");
    putdec(tid);
    puts(" exited with code ");
    putdec(exit_code);
    puts("]\n");
}

/* --- Command dispatch --- */

static void skip_spaces(const char **p)
{
    while (**p == ' ') (*p)++;
}

static void execute(char *line)
{
    const char *p = line;
    skip_spaces(&p);
    if (*p == '\0')
        return;

    /* Extract command name */
    const char *cmd = p;
    while (*p && *p != ' ') p++;

    unsigned int cmd_len = (unsigned int)(p - cmd);
    skip_spaces(&p);
    /* p now points to the arguments */

    if (cmd_len == 4 && sncmp(cmd, "help", 4) == 0)      cmd_help();
    else if (cmd_len == 5 && sncmp(cmd, "clear", 5) == 0) cmd_clear();
    else if (cmd_len == 4 && sncmp(cmd, "echo", 4) == 0)  cmd_echo(p);
    else if (cmd_len == 3 && sncmp(cmd, "cat", 3) == 0)   cmd_cat(p);
    else if (cmd_len == 2 && sncmp(cmd, "ls", 2) == 0)    cmd_ls(p);
    else if (cmd_len == 3 && sncmp(cmd, "run", 3) == 0)  cmd_run(p);
    else if (cmd_len == 2 && sncmp(cmd, "ps", 2) == 0)  cmd_ps();
    else if (cmd_len == 4 && sncmp(cmd, "kill", 4) == 0) cmd_kill(p);
    else if (cmd_len == 7 && sncmp(cmd, "version", 7) == 0) cmd_version();
    else if (cmd_len == 8 && sncmp(cmd, "hostname", 8) == 0) cmd_hostname();
    else if (cmd_len == 3 && sncmp(cmd, "mem", 3) == 0)   cmd_mem();
    else if (cmd_len == 6 && sncmp(cmd, "uptime", 6) == 0) cmd_uptime();
    else if (cmd_len == 5 && sncmp(cmd, "tasks", 5) == 0) cmd_tasks();
    else if (cmd_len == 4 && sncmp(cmd, "date", 4) == 0)  cmd_date();
    else if (cmd_len == 7 && sncmp(cmd, "sysinfo", 7) == 0) cmd_sysinfo();
    else if (cmd_len == 6 && sncmp(cmd, "whoami", 6) == 0) cmd_whoami();
    else if (cmd_len == 6 && sncmp(cmd, "reboot", 6) == 0) cmd_reboot();
    else if (cmd_len == 4 && sncmp(cmd, "caps", 4) == 0)  cmd_caps();
    else {
        puts("unknown command: ");
        /* Print just the command name */
        static char cbuf[64];
        for (unsigned int i = 0; i < cmd_len && i < 63; i++) cbuf[i] = cmd[i];
        cbuf[cmd_len < 63 ? cmd_len : 63] = '\0';
        puts(cbuf);
        puts("\nType 'help' for available commands.\n");
    }
}

/* --- Entry point --- */

void user_main(void)
{
    /* Pre-assign fd 0 (stdin), 1 (stdout), 2 (stderr) to /dev/console.
     * This ensures that file opens get fd >= 3 and don't collide with stdout. */
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 0 = stdin */
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 1 = stdout */
    sc2(SYS_OPEN, (unsigned int)"/dev/console", 0);  /* fd 2 = stderr */
    console_fd = 0;  /* Read from fd 0 (stdin) */

    puts("AstraOS Shell v1.0.0\n");
    puts("Type 'help' for commands.\n\n");

    static char line[256];
    static char last_line[256];
    last_line[0] = '\0';

    while (1) {
        puts("astra> ");
        unsigned int n = readline(line, sizeof(line));
        if (n > 0) {
            /* "!!" recalls last command */
            if (n == 2 && line[0] == '!' && line[1] == '!') {
                if (last_line[0]) {
                    puts(last_line);
                    puts("\n");
                    execute(last_line);
                } else {
                    puts("no previous command\n");
                }
            } else {
                /* Save as last command */
                for (unsigned int i = 0; i <= n; i++)
                    last_line[i] = line[i];
                execute(line);
            }
        }
    }
}
