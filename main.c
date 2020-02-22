#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_XOPEN
#include <time.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#define DAY_SECS (24 * 60 * 60)
#define BRIMLEY_SECS (15000 * DAY_SECS)

typedef enum {
    OK = 0,
    ERR_UNKNOWN = 1,
    ERR_CMD_MALFORMED,
    ERR_CONFIG_FILE_PATH_CREATE_FAIL,
    ERR_CONFIG_FILE_NOT_FOUND,
    ERR_CONFIG_FILE_DATE_INVALID,
} ErrorCode;

// Parsed config file.
typedef struct {
    struct tm birth_date;
} Config;

// Get the BCL as a time_t.
time_t bcl_for(Config *c) {
    time_t birth = mktime(&c->birth_date);
    return birth + BRIMLEY_SECS;
}

// Get the time-to-BCL as a time_t.
time_t diff_for(Config *c) {
    time_t bcl = bcl_for(c);
    time_t now;
    time(&now);
    return bcl - now;
}

// Read the config file at a given path.
int read_config_file(const char *path, Config *config) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "Config file not found: %s\n", path);
        fprintf(stderr, "Consider running `bcl set`\n");
        return ERR_CONFIG_FILE_NOT_FOUND;
    }
    char buf[11];
    size_t r = fread(buf, 1, 10, f);
    if (r != 10) {
        fprintf(stderr,
                "Config file %s contains invalid date (expected YYYY-MM-DD)\n",
                path);
        fprintf(stderr, "Consider running `bcl set`\n");
        return ERR_CONFIG_FILE_DATE_INVALID;
    }
    fclose(f);
    struct tm ltm = {0};
    char *v = strptime(buf, "%Y-%m-%d", &ltm);
    if (v == NULL) {
        fprintf(
            stderr,
            "Config file %s contains invalid date: %s (expected YYYY-MM-DD)\n",
            path, buf);
        fprintf(stderr, "Consider running `bcl set`\n");
        return ERR_CONFIG_FILE_DATE_INVALID;
    }
    config->birth_date = ltm;
    return OK;
}

// Get user config path.
int get_config_path(char *buf, size_t buf_sz) {
    struct passwd *pw = getpwuid(getuid());
    int i = snprintf(buf, buf_sz, "%s/.config/bcl", pw->pw_dir);
    if (i < 0) {
        return ERR_CONFIG_FILE_PATH_CREATE_FAIL;
    }
    return OK;
}

// Load config from user config path into the passed config file.
int load_config(Config *c) {
    char buf[256];
    int status = get_config_path(buf, sizeof buf);
    if (status != OK) {
        return status;
    }
    return read_config_file(buf, c);
}

// `bcl` invocation.
int cmd_bcl() {
    Config c;
    int status = load_config(&c);
    if (status != OK) {
        return status;
    }
    const time_t diff = diff_for(&c);
    const size_t days = diff / DAY_SECS;
    const size_t rem = (diff % DAY_SECS);
    const size_t hours = rem / 60 / 60;
    const size_t minutes = (rem / 60) % 60;
    const size_t seconds = rem % 60;
    printf("%zu days %zu hours %zu minutes %zu seconds\n", days, hours, minutes,
           seconds);
    return 0;
}

// `bcl when` invocation.
int cmd_when() {
    Config c;
    int status = load_config(&c);
    if (status != OK) {
        return status;
    }
    time_t bcl = bcl_for(&c);
    struct tm *bcl_tm = gmtime(&bcl);
    char buf[256];
    strftime(buf, 256, "%A, %d %B %Y", bcl_tm);
    printf("%s\n", buf);
    return 0;
}

// `bcl set` invocation - write or overwrite config file with new date.
int cmd_set(const char *birth_date) {
    struct tm ltm = {0};
    char *v = strptime(birth_date, "%Y-%m-%d", &ltm);
    if (v == NULL) {
        fprintf(stderr, "%s is not a valid date (expected YYYY-MM-DD)\n",
                birth_date);
        return ERR_CONFIG_FILE_DATE_INVALID;
    }

    char buf[256];
    int i = get_config_path(buf, sizeof buf);
    if (i < 0) {
        fprintf(stderr, "Failed to get config path\n");
        return ERR_CONFIG_FILE_PATH_CREATE_FAIL;
    }
    FILE *f = fopen(buf, "w");
    if (f == NULL) {
        return ERR_CONFIG_FILE_NOT_FOUND;
    }
    fprintf(f, "%s", birth_date);
    fclose(f);
    printf("Wrote '%s' to %s\n", birth_date, buf);
    return OK;
}

// `bcl unset` incovation - delete config file.
int cmd_unset() {
    char buf[256];
    int i = get_config_path(buf, sizeof buf);
    if (i < 0) {
        fprintf(stderr, "Failed to get config path\n");
        return -1;
    }
    remove(buf);
    printf("Deleted %s\n", buf);
    return OK;
}

// Print usage text, return passed ErrorCode.
int usage(ErrorCode retval) {
    printf("bcl: Print time remaining until Brimley-Cocoon Line\n");
    printf("Calculation is made relative to a date stored in ~/.config/bcl in "
           "the form 'YYYY-MM-DD'.\n");
    printf("\n");
    printf("Subcommands:\n");
    printf("    help - Print this text\n");
    printf("    set - Set DOB in config file\n");
    printf("    unset - Clear DOB, deleting config file\n");
    printf("    when - Print date of Brimley-Cocoon Line\n");
    return retval;
}

int handle_cmd(int argc, const char **argv) {
    switch (argc) {
    case 1:
        return cmd_bcl();
    case 2: {
        const char *cmd = argv[1];
        if (strcmp(cmd, "when") == 0) {
            return cmd_when();
        }
        if (strcmp(cmd, "unset") == 0) {
            return cmd_unset();
        }
        if (strcmp(cmd, "set") == 0) {
            printf("Usage: bcl set YYYY-MM-DD\n");
            return ERR_CMD_MALFORMED;
        }
        if (strcmp(cmd, "help") == 0) {
            return usage(OK);
        }
    }
    // fallthrough
    case 3: {
        if (strcmp(argv[1], "set") == 0) {
            return cmd_set(argv[2]);
        }
    }
    // fallthrough
    default:
        return usage(ERR_CMD_MALFORMED);
    }
}

int main(int argc, const char **argv) {
    switch (handle_cmd(argc, argv)) {
    case OK:
        return EXIT_SUCCESS;
    default:
        return EXIT_FAILURE;
    }
}
