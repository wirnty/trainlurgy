#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 1024
#define MAX_VARS 100
#define MAX_NAME 64
#define MAX_VALUE 256

typedef struct {
    char name[MAX_NAME];
    char value[MAX_VALUE];
} Variable;

static Variable vars[MAX_VARS];
static int var_count = 0;

int tlpkg_command(const char *args);
int run_file(const char *filename);
int start_repl(void);

static void run_line(char *line);

static int find_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return i;
    }
    return -1;
}

static void set_var(const char *name, const char *value) {
    int idx = find_var(name);

    if (idx != -1) {
        strncpy(vars[idx].value, value, MAX_VALUE - 1);
        vars[idx].value[MAX_VALUE - 1] = '\0';
        return;
    }

    if (var_count >= MAX_VARS) {
        printf("Too many variables\n");
        return;
    }

    strncpy(vars[var_count].name, name, MAX_NAME - 1);
    vars[var_count].name[MAX_NAME - 1] = '\0';

    strncpy(vars[var_count].value, value, MAX_VALUE - 1);
    vars[var_count].value[MAX_VALUE - 1] = '\0';

    var_count++;
}

static const char *get_var(const char *name) {
    int idx = find_var(name);

    if (idx != -1)
        return vars[idx].value;

    return "";
}

static void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

static void lstrip(char **s) {
    while (**s == ' ' || **s == '\t')
        (*s)++;
}

static void expand_vars(const char *input, char *output) {
    int oi = 0;

    for (int i = 0; input[i] && oi < 511; i++) {
        if (input[i] == '$') {
            char name[64];
            int ni = 0;

            i++;

            while (input[i] && (isalnum((unsigned char)input[i]) || input[i] == '_')) {
                if (ni < 63)
                    name[ni++] = input[i];
                i++;
            }

            name[ni] = 0;
            i--;

            const char *val = get_var(name);

            for (int j = 0; val[j] && oi < 511; j++)
                output[oi++] = val[j];
        } else {
            output[oi++] = input[i];
        }
    }

    output[oi] = 0;
}

static void connect_server(const char *url) {
    char cmd[512];

    printf("Connecting to %s\n", url);

    snprintf(cmd, sizeof(cmd),
        "curl -s %s/server.txt -o server.txt", url);

    if (system(cmd) != 0) {
        printf("Failed to download server.txt\n");
        return;
    }

    FILE *f = fopen("server.txt", "r");

    if (!f) {
        printf("Invalid registry\n");
        return;
    }

    printf("Registry configuration:\n");

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), f)) {
        printf("%s", line);
    }

    fclose(f);

    FILE *save = fopen("trainlurgy_server.conf", "w");
    f = fopen("server.txt", "r");

    if (!save || !f) {
        if (save) fclose(save);
        if (f) fclose(f);
        printf("Failed to save trainlurgy_server.conf\n");
        return;
    }

    while (fgets(line, sizeof(line), f))
        fputs(line, save);

    fclose(f);
    fclose(save);

    printf("\nConnected to package server.\n");
}

static void print_help(void) {
    printf("TrainLurgy commands:\n");
    printf("print <text>\n");
    printf("set <name> <value>\n");
    printf("get <name>\n");
    printf("input <name>\n");
    printf("add <name> <num>\n");
    printf("catch <file.tl>\n");
    printf("use <module>\n");
    printf("tlpkg <command>\n");
    printf("connect <server>\n");
    printf("help\n");
    printf("exit\n");
}

int run_file(const char *filename) {
    FILE *fp = fopen(filename, "r");

    if (!fp) {
        printf("Cannot open file: %s\n", filename);
        return 1;
    }

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);

        if (line[0] == 0 || line[0] == '#')
            continue;

        run_line(line);
    }

    fclose(fp);
    return 0;
}

static void run_line(char *line) {
    char original[MAX_LINE];
    strncpy(original, line, sizeof(original) - 1);
    original[sizeof(original) - 1] = '\0';

    char *cmd = strtok(line, " ");

    if (!cmd)
        return;

    if (!strcmp(cmd, "print")) {
        char *rest = original + 5;
        lstrip(&rest);

        char out[512];
        expand_vars(rest, out);

        printf("%s\n", out);
    }

    else if (!strcmp(cmd, "set")) {
        char *name = strtok(NULL, " ");
        char *val = strtok(NULL, "");

        if (!name || !val)
            return;

        lstrip(&val);
        set_var(name, val);
    }

    else if (!strcmp(cmd, "get")) {
        char *name = strtok(NULL, " ");

        if (!name)
            return;

        printf("%s\n", get_var(name));
    }

    else if (!strcmp(cmd, "input")) {
        char *name = strtok(NULL, " ");

        if (!name) {
            printf("Usage: input <variable>\n");
            return;
        }

        char buffer[MAX_VALUE];

        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin))
            return;

        trim_newline(buffer);
        set_var(name, buffer);
    }

    else if (!strcmp(cmd, "add")) {
        char *name = strtok(NULL, " ");
        char *num = strtok(NULL, " ");

        if (!name || !num)
            return;

        int result = atoi(get_var(name)) + atoi(num);

        char buf[64];
        snprintf(buf, sizeof(buf), "%d", result);

        set_var(name, buf);
    }

    else if (!strcmp(cmd, "catch")) {
        char *file = strtok(NULL, "");

        if (!file)
            return;

        lstrip(&file);
        run_file(file);
    }

    else if (!strcmp(cmd, "use")) {
        char *name = strtok(NULL, " ");

        if (!name)
            return;

        char path[256];
        snprintf(path, sizeof(path), "packages/%s/main.tl", name);

        run_file(path);
    }

    else if (!strcmp(cmd, "connect")) {
        char *url = strtok(NULL, " ");

        if (!url) {
            printf("Usage: connect <server>\n");
            return;
        }

        connect_server(url);
    }

    else if (!strcmp(cmd, "tlpkg")) {
        char *args = strtok(NULL, "");

        if (!args)
            args = "help";

        tlpkg_command(args);
    }

    else if (!strcmp(cmd, "help")) {
        print_help();
    }

    else if (!strcmp(cmd, "exit")) {
        exit(0);
    }

    else {
        printf("Unknown command: %s\n", cmd);
    }
}

int start_repl(void) {
    char line[MAX_LINE];

    printf("TrainLurgy 0.1\n");
    printf("Type 'help' for commands\n");

    while (1) {
        printf(">>> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin))
            break;

        trim_newline(line);

        if (line[0] == 0)
            continue;

        run_line(line);
    }

    return 0;
}
