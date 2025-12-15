#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sstream>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <pwd.h>
#include <sys/wait.h>
#include <fcntl.h>

using namespace std;

namespace fs = std::filesystem;

// FUSE VFS functions
extern "C" int start_users_vfs(const char *mount_point);
extern "C" void stop_users_vfs();

void handle_sighup(int sig) {
    if (sig == SIGHUP) {
        const char* msg = "Configuration reloaded\n";
        write(STDOUT_FILENO, msg, 23);
    }
}

int main() {
    // Initialize FUSE VFS for users directory
    fs::path users_dir = fs::current_path() / "users";
    try {
        if (!fs::exists(users_dir)) {
            fs::create_directory(users_dir);
        }
    } catch (const fs::filesystem_error& e) {
        cerr << "Error creating users directory: " << e.what() << endl;
        return 1;
    }

    // Signal handling
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, NULL);

    // Main shell loop
    string input;
    while (true) {
        // Display prompt
        cout << "$ ";
        cout.flush();

        // Read input
        if (!getline(cin, input)) {
            if (cin.eof()) {
                cout << endl;
                break;
            }
            continue;
        }

        // Trim whitespace
        size_t start = input.find_first_not_of(" \t\n\r");
        if (start == string::npos) {
            continue; // Empty line
        }
        size_t end = input.find_last_not_of(" \t\n\r");
        input = input.substr(start, end - start + 1);

        // Parse command
        istringstream iss(input);
        vector<string> args;
        string arg;
        while (iss >> quoted(arg)) {
            args.push_back(arg);
        }

        if (args.empty()) {
            continue;
        }

        string cmdName = args[0];

        // Handle built-in commands
        if (cmdName == "exit") {
            stop_users_vfs();
            break;
        }
        else if (cmdName == "echo") {
            for (size_t i = 1; i < args.size(); ++i) {
                cout << args[i];
                if (i < args.size() - 1) {
                    cout << " ";
                }
            }
            cout << endl;
        }
        else if (cmdName == "echo" || cmdName == "debug") {
            // This might be a duplicate - check what you need here
            for (size_t i = 1; i < args.size(); ++i) {
                cout << args[i];
                if (i < args.size() - 1) {
                    cout << " ";
                }
            }
            cout << endl;
        }
        else if (cmdName == "type") {
            if (args.size() < 2) {
                cout << "type: missing argument" << endl;
                continue;
            }
            string cmdToCheck = args[1];
            // Check if it's a built-in
            if (cmdToCheck == "exit" || cmdToCheck == "echo" || 
                cmdToCheck == "type" || cmdToCheck == "pwd") {
                cout << cmdToCheck << " is a shell builtin" << endl;
            } else {
                // Check if it's in PATH
                char* path = getenv("PATH");
                if (path) {
                    string pathStr(path);
                    istringstream pathStream(pathStr);
                    string dir;
                    bool found = false;
                    while (getline(pathStream, dir, ':')) {
                        fs::path cmdPath = dir / cmdToCheck;
                        if (fs::exists(cmdPath) && 
                            (fs::status(cmdPath).permissions() & fs::perms::owner_exec) != fs::perms::none) {
                            cout << cmdToCheck << " is " << cmdPath.string() << endl;
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        cout << cmdToCheck << ": not found" << endl;
                    }
                } else {
                    cout << cmdToCheck << ": not found" << endl;
                }
            }
        }
        else if (cmdName == "pwd") {
            cout << fs::current_path().string() << endl;
        }
        else {
            // External command execution
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                vector<char*> argv;
                for (auto& arg : args) {
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);
                
                execvp(argv[0], argv.data());
                
                // If execvp returns, there was an error
                cerr << args[0] << ": command not found" << endl;
                exit(1);
            } else if (pid > 0) {
                // Parent process
                int status;
                waitpid(pid, &status, 0);
            } else {
                cerr << "Failed to fork" << endl;
            }
        }
    }

    return 0;
}

// Check MBR signature function
bool check_mbr_signature(const unsigned char* mbr) {
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        return false;
    }
    return true;
}
