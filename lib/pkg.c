#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_LINE 1024
#define MAX_VALUE 512
#define MAX_PATH 1024

static void trim_newline(char *s) {
    s[strcspn(s, "\r\n")] = '\0';
}

static void lstrip(char **s) {
    while (**s == ' ' || **s == '\t') {
        (*s)++;
    }
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int dir_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int read_key_value(const char *file, const char *key, char *out, size_t out_size) {
    FILE *fp = fopen(file, "r");
    if (!fp) return 1;

    char line[MAX_LINE];
    size_t key_len = strlen(key);

    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);

        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            strncpy(out, line + key_len + 1, out_size - 1);
            out[out_size - 1] = '\0';
            fclose(fp);
            return 0;
        }
    }

    fclose(fp);
    return 1;
}

static int read_api_url(char *out, size_t out_size) {
    return read_key_value("trainlurgy_server.conf", "api", out, out_size);
}

static int read_saved_api_key(char *out, size_t out_size) {
    FILE *fp = fopen("api_response.json", "r");
    if (!fp) return 1;

    char data[4096];
    size_t n = fread(data, 1, sizeof(data) - 1, fp);
    fclose(fp);
    data[n] = '\0';

    char *api_pos = strstr(data, "\"API\"");
    if (!api_pos) return 1;

    char *colon = strchr(api_pos, ':');
    if (!colon) return 1;

    char *first_quote = strchr(colon, '"');
    if (!first_quote) return 1;
    first_quote++;

    char *second_quote = strchr(first_quote, '"');
    if (!second_quote) return 1;

    size_t len = (size_t)(second_quote - first_quote);
    if (len >= out_size) len = out_size - 1;

    strncpy(out, first_quote, len);
    out[len] = '\0';

    if (strcmp(out, "wrongcreds") == 0) return 1;
    return 0;
}

static void pkg_help(void) {
    printf("TrainLurgy Package Manager\n");
    printf("Commands:\n");
    printf("  tlpkg help\n");
    printf("  tlpkg list\n");
    printf("  tlpkg login <name> <pass>\n");
    printf("  tlpkg build <directory_or_.>\n");
    printf("  tlpkg publish <dist_directory_or_.>\n");
    printf("  tlpkg install <module>\n");
    printf("  tlpkg remove <module>\n");
    printf("\n");
    printf("Build examples:\n");
    printf("  tlpkg build ./hello_pkg\n");
    printf("  tlpkg build .\n");
    printf("\n");
    printf("Publish examples:\n");
    printf("  tlpkg publish ./hello_pkg/dist\n");
    printf("  tlpkg publish .\n");
}

static void pkg_list(void) {
    printf("Installed packages:\n");
    if (!dir_exists("packages")) {
        printf("  packages directory not found\n");
        return;
    }
    system("ls packages 2>/dev/null");
}

static void pkg_login(const char *user, const char *pass) {
    char api_url[MAX_VALUE];

    if (read_api_url(api_url, sizeof(api_url)) != 0) {
        printf("No registry connected.\n");
        printf("Run: connect <server>\n");
        return;
    }

    printf("Logging into registry...\n");

    char cmd[2048];
    snprintf(
        cmd,
        sizeof(cmd),
        "curl -s -X POST %s/auth/api "
        "-H \"Content-Type: application/json\" "
        "-d '{\"name\":\"%s\",\"pass\":\"%s\"}' "
        "-o api_response.json",
        api_url, user, pass
    );

    int result = system(cmd);

    if (result != 0) {
        printf("Login request failed.\n");
        return;
    }

    if (!file_exists("api_response.json")) {
        printf("Login response file was not created.\n");
        return;
    }

    printf("Server response saved to api_response.json\n");
}

static void pkg_build(const char *package_dir) {
    char meta_path[MAX_PATH];
    char name[MAX_VALUE];
    char version[MAX_VALUE];
    char main_file[MAX_VALUE];
    char dist_dir[MAX_PATH];
    char output_file[MAX_PATH];
    char cmd[4096];

    snprintf(meta_path, sizeof(meta_path), "%s/tlpkg.meta", package_dir);

    if (!file_exists(meta_path)) {
        printf("Missing tlpkg.meta in: %s\n", package_dir);
        return;
    }

    if (read_key_value(meta_path, "name", name, sizeof(name)) != 0) {
        printf("Missing name= in %s\n", meta_path);
        return;
    }

    if (read_key_value(meta_path, "version", version, sizeof(version)) != 0) {
        printf("Missing version= in %s\n", meta_path);
        return;
    }

    if (read_key_value(meta_path, "main", main_file, sizeof(main_file)) != 0) {
        strncpy(main_file, "main.tl", sizeof(main_file) - 1);
        main_file[sizeof(main_file) - 1] = '\0';
    }

    snprintf(dist_dir, sizeof(dist_dir), "%s/dist", package_dir);
    snprintf(output_file, sizeof(output_file), "%s/%s.tlpkg", dist_dir, name);

    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", dist_dir);
    system(cmd);

    printf("Building package from %s\n", package_dir);

    snprintf(
        cmd,
        sizeof(cmd),
        "sh -c 'cd \"%s\" && tar --exclude=\"./dist\" -czf \"dist/%s.tlpkg\" .'",
        package_dir, name
    );

    if (system(cmd) != 0) {
        printf("Build failed.\n");
        return;
    }

    printf("Package built: %s\n", output_file);
}

static int read_dist_meta(const char *dist_dir, char *name, size_t name_size, char *version, size_t version_size) {
    char meta_path[MAX_PATH];

    snprintf(meta_path, sizeof(meta_path), "%s/tlpkg.meta", dist_dir);
    if (file_exists(meta_path)) {
        if (read_key_value(meta_path, "name", name, name_size) != 0) return 1;
        if (read_key_value(meta_path, "version", version, version_size) != 0) return 1;
        return 0;
    }

    snprintf(meta_path, sizeof(meta_path), "%s/../tlpkg.meta", dist_dir);
    if (file_exists(meta_path)) {
        if (read_key_value(meta_path, "name", name, name_size) != 0) return 1;
        if (read_key_value(meta_path, "version", version, version_size) != 0) return 1;
        return 0;
    }

    return 1;
}

static void pkg_publish(const char *dist_dir) {
    char api_url[MAX_VALUE];
    char api_key[MAX_VALUE];
    char name[MAX_VALUE];
    char version[MAX_VALUE];
    char package_file[MAX_PATH];
    char meta_file[MAX_PATH];
    char cmd[4096];

    if (read_api_url(api_url, sizeof(api_url)) != 0) {
        printf("No registry connected.\n");
        printf("Run: connect <server>\n");
        return;
    }

    if (read_saved_api_key(api_key, sizeof(api_key)) != 0) {
        printf("No valid API key found.\n");
        printf("Run: tlpkg login <name> <pass>\n");
        return;
    }

    if (read_dist_meta(dist_dir, name, sizeof(name), version, sizeof(version)) != 0) {
        printf("Could not find package metadata in dist folder: %s\n", dist_dir);
        return;
    }

    snprintf(package_file, sizeof(package_file), "%s/%s.tlpkg", dist_dir, name);
    snprintf(meta_file, sizeof(meta_file), "%s/tlpkg.meta", dist_dir);

    if (!file_exists(package_file)) {
        printf("Package file not found: %s\n", package_file);
        return;
    }

    if (!file_exists(meta_file)) {
        char parent_meta[MAX_PATH];
        snprintf(parent_meta, sizeof(parent_meta), "%s/../tlpkg.meta", dist_dir);
        if (file_exists(parent_meta)) {
            strncpy(meta_file, parent_meta, sizeof(meta_file) - 1);
            meta_file[sizeof(meta_file) - 1] = '\0';
        } else {
            printf("Metadata file not found in dist or parent package dir.\n");
            return;
        }
    }

    printf("Publishing package: %s\n", package_file);

    snprintf(
        cmd,
        sizeof(cmd),
        "curl -s -X POST %s/publish "
        "-H \"X-TLPKG-API-Key: %s\" "
        "-F \"package=@%s\" "
        "-F \"meta=@%s\"",
        api_url, api_key, package_file, meta_file
    );

    system(cmd);
    printf("\n");
}

static void pkg_install(const char *module) {
    char api_url[512];
    char cmd[1024];

    if (read_api_url(api_url, sizeof(api_url)) != 0) {
        printf("No registry connected.\n");
        return;
    }

    printf("Installing %s...\n", module);

    snprintf(
        cmd,
        sizeof(cmd),
        "mkdir -p packages && "
        "curl -f -s -o %s.tlpkg %s/download/%s.tlpkg && "
        "mkdir -p packages/%s && "
        "tar -xzf %s.tlpkg -C packages/%s && "
        "rm -f %s.tlpkg",
        module,
        api_url,
        module,
        module,
        module,
        module,
        module
    );

    if (system(cmd) != 0) {
        printf("Install failed.\n");
        return;
    }

    printf("Installed %s into packages/%s\n", module, module);
}

static void pkg_remove(const char *module) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -rf \"packages/%s\"", module);
    system(cmd);
    printf("Removed package: %s\n", module);
}

int tlpkg_command(const char *args) {
    char buf[MAX_LINE];
    strncpy(buf, args ? args : "", sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *p = buf;
    lstrip(&p);

    char *cmd = strtok(p, " ");
    if (!cmd) {
        pkg_help();
        return 0;
    }

    if (strcmp(cmd, "help") == 0) {
        pkg_help();
    }
    else if (strcmp(cmd, "list") == 0) {
        pkg_list();
    }
    else if (strcmp(cmd, "login") == 0) {
        char *user = strtok(NULL, " ");
        char *pass = strtok(NULL, "");

        if (!user || !pass) {
            printf("Usage: tlpkg login <name> <pass>\n");
            return 1;
        }

        lstrip(&pass);
        pkg_login(user, pass);
    }
    else if (strcmp(cmd, "build") == 0) {
        char *path = strtok(NULL, "");
        if (!path) {
            printf("Usage: tlpkg build <directory_or_.>\n");
            return 1;
        }

        lstrip(&path);
        pkg_build(path);
    }
    else if (strcmp(cmd, "publish") == 0) {
        char *path = strtok(NULL, "");
        if (!path) {
            printf("Usage: tlpkg publish <dist_directory_or_.>\n");
            return 1;
        }

        lstrip(&path);
        pkg_publish(path);
    }
    else if (strcmp(cmd, "install") == 0) {
        char *module = strtok(NULL, "");
        if (!module) {
            printf("Usage: tlpkg install <module>\n");
            return 1;
        }

        lstrip(&module);
        pkg_install(module);
    }
    else if (strcmp(cmd, "remove") == 0) {
        char *module = strtok(NULL, "");
        if (!module) {
            printf("Usage: tlpkg remove <module>\n");
            return 1;
        }

        lstrip(&module);
        pkg_remove(module);
    }
    else {
        printf("unknown tlpkg command: %s\n", cmd);
        return 1;
    }

    return 0;
}
