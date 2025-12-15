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
            fs::create_directories(users_dir);
        }
    } catch (...) {}
    
    start_users_vfs(users_dir.string().c_str());

    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_sighup;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGHUP, &sa, NULL);

    cout << unitbuf;    
    string input;
   
    const char* home_dir = getenv("HOME");
    string history_file;
   
    if(home_dir !=nullptr) {
        history_file = string(home_dir) + "/kubsh_history";
    } else {
        history_file = "/kubsh_history";
    }

    while(true) {
        if (isatty(STDIN_FILENO)) {
            cout << "$ ";
        }
        
        if(!getline(cin, input)) {
            if (cin.eof()) break;
            cin.clear();
            continue;
        }

        // Выход
        if(input == "\\q") {
            stop_users_vfs();
            return 0;
        }

        // Парсинг аргументов (ОДИН раз!)
        vector<string> args;
        string current_arg;
        bool in_quotes = false;
        
        for(char c : input) {
            if(c == '\'') {
                in_quotes = !in_quotes;
            } else if(c == ' ' && !in_quotes) {
                if(!current_arg.empty()) {
                    args.push_back(current_arg);
                    current_arg.clear();
                }
            } else {
                current_arg += c;
            }
        }
        if(!current_arg.empty()) {
            args.push_back(current_arg);
        }

        if (args.empty()) continue;
        
        string cmdName = args[0];

        // Сохраняем в историю (кроме специальных команд)
        if(!input.empty() && input != "history") {
            ofstream file(history_file, ios::app);
            file << input << endl;
        }

        // Builtin команды
        if (cmdName == "history") {
            ifstream read_file(history_file);
            string line;
            while(getline(read_file, line)) {
                cout << " " << line << endl;
            }
            continue;
        } 
        else if (cmdName == "echo" || cmdName == "debug") {
            for(size_t i = 1; i < args.size(); ++i) {
                cout << args[i] << (i == args.size() - 1 ? "" : " ");
            }
            cout << endl;
            continue;
        }
        else if (cmdName == "\\e") {
            if (args.size() == 1) {
                // Вывести все переменные окружения
                extern char **environ;
                for (char **env = environ; *env != nullptr; env++) {
                    cout << *env << endl;
                }
            } else if (args.size() > 1) {
                string var = args[1];
                if (var.size() > 0 && var[0] == '$') {
                    var = var.substr(1);
                }
                const char* env_val = getenv(var.c_str());
                if (env_val) {
                    string val = env_val;
                    size_t start = 0;
                    size_t end = val.find(':');
                    while (end != string::npos) {
                        cout << val.substr(start, end - start) << endl;
                        start = end + 1;
                        end = val.find(':', start);
                    }
                    cout << val.substr(start) << endl;
                }
            }
            continue;
        }
        else if (cmdName == "\\l") {
            if (args.size() < 2) {
                cerr << "Usage: \\l <device>" << endl;
            } else {
                string device = args[1];
                ifstream dev_file(device, ios::binary);
                if (!dev_file) {
                    cerr << "Failed to open device: " << device << endl;
                } else {
                    uint8_t mbr[512];
                    if (!dev_file.read(reinterpret_cast<char*>(mbr), 512)) {
                        cerr << "Failed to read MBR" << endl;
                    } else {
                        if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
                            cerr << "Invalid MBR signature" << endl;
                        } else {
                            cout << "Partition table for " << device << ":" << endl;
                            cout << "Boot Start LBA Size Type" << endl;
                            for (int i = 0; i < 4; ++i) {
                                int offset = 446 + i * 16;
                                uint8_t status = mbr[offset];
                                uint8_t type = mbr[offset + 4];
                                uint32_t lba_start = *reinterpret_cast<uint32_t*>(&mbr[offset + 8]);
                                uint32_t size = *reinterpret_cast<uint32_t*>(&mbr[offset + 12]);
                                
                                cout << (status == 0x80 ? "*" : " ") << "    "
                                     << setw(10) << lba_start << " "
                                     << setw(10) << size << " "
                                     << hex << setw(2) << setfill('0') << (int)type << dec << setfill(' ') << endl;
                            }
                        }
                    }
                }
            }
            continue;
        }
        
        // ВСЕ остальные команды - внешние (fork/exec)
        pid_t pid = fork();
        if (pid == 0) {
            // Дочерний процесс
            vector<char*> c_args;
            for(const auto& arg : args) {
                c_args.push_back(const_cast<char*>(arg.c_str()));
            }
            c_args.push_back(nullptr);
            
            execvp(c_args[0], c_args.data());
            
            // Если execvp вернул ошибку
            cerr << input << ": command not found" << endl;
            exit(127);  // Важно: 127 для "command not found"
        } 
        else if (pid > 0) {
            // Родительский процесс
            int status;
            waitpid(pid, &status, 0);
        } 
        else {
            cerr << "Fork failed" << endl;
        }
    }
    
    stop_users_vfs();
    return 0;
}
