int run_file(const char *filename);
int start_repl(void);

int main(int argc, char *argv[]) {
    if (argc == 2) {
        return run_file(argv[1]);
    }

    return start_repl();
}
