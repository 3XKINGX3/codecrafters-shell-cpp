#include <iostream>
#include <string>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include <sstream>
#include <signal.h>
#include <cstdint>

#include "vfs.hpp"

// ... (все ваши функции остаются без изменений: sighup_handler, check_disk_partitions) ...

// НОВАЯ ФУНКЦИЯ: выполнение команды
int execute_command(const std::string& input) {
    if (input.empty()) return 0;

    // Запись в историю
    const char* home = std::getenv("HOME");
    std::string historyPath = std::string(home) + "/.kubsh_history";
    
    if(!input.empty()) {
        std::ofstream history(historyPath, std::ios::app);
        history << input << "\n";
    }

    // history
    if(input == "history") {
        std::ifstream historyOutput(historyPath);
        std::string line;
        while(std::getline(historyOutput, line)) {
            std::cout << line << "\n";
        }
        return 0;
    }
    
    // \q
    else if (input == "\\q") {
        return 0;  // В режиме -c просто выходим
    }
    
    // \l /dev/sda
    else if (input.substr(0, 3) == "\\l ") {
        std::string device_path = input.substr(3);
        device_path.erase(0, device_path.find_first_not_of(" \t"));
        device_path.erase(device_path.find_last_not_of(" \t") + 1);
        
        if (device_path.empty()) {
            std::cout << "Usage: \\l /dev/device_name (e.g., \\l /dev/sda)\n";
        } else {
            check_disk_partitions(device_path);
        }
        return 0;
    }
    
    // debug
    else if (input.substr(0, 7) == "debug '" && input[input.length() - 1] == '\'') {
        std::cout << input.substr(7, input.length() - 8) << std::endl;
        return 0;
    }
    
    // \e $
    else if (input.substr(0,4) == "\\e $") {
        std::string varName = input.substr(4);
        const char* value = std::getenv(varName.c_str());
        
        if(value != nullptr) {
            std::string valueStr = value;
            bool has_colon = false;
            for (char c : valueStr) {
                if (c == ':') {
                    has_colon = true;
                    break;
                }
            }
            
            if (has_colon) {
                std::string current_part = "";
                for (char c : valueStr) {
                    if (c == ':') {
                        std::cout << current_part << "\n";
                        current_part = "";
                    } else {
                        current_part += c;
                    }
                }
                std::cout << current_part << "\n";
            } else {
                std::cout << valueStr << "\n";
            }
        } else {
            std::cout << varName << ": не найдено\n";
        }
        return 0;
    }
    
    // Внешние команды
    else {
        pid_t pid = fork();
        
        if (pid == 0) {
            std::vector<std::string> tokens;
            std::vector<char*> args;
            std::string token;
            std::istringstream iss(input);
            
            while (iss >> token) {
                tokens.push_back(token);
            }
            
            for (auto& t : tokens) {
                args.push_back(const_cast<char*>(t.c_str()));
            }
            args.push_back(nullptr);
            
            execvp(args[0], args.data());
            
            // Если команда не найдена
            std::cerr << args[0] << ": command not found\n";
            exit(127);  // Важно: 127 для "command not found"
            
        } else if (pid > 0) {
            int status;
            waitpid(pid, &status, 0);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            return 1;
        } else {
            std::cerr << "Failed to create process\n";
            return 1;
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) 
{
    // РЕЖИМ 1: -c (одна команда) - для тестов CodeCrafters
    if (argc > 1 && std::string(argv[1]) == "-c") {
        if (argc > 2) {
            // Важно: не выводим приглашение $ в неинтерактивном режиме
            return execute_command(argv[2]);
        }
        return 0;
    }
    
    // РЕЖИМ 2: Интерактивный режим
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;
    
    fuse_start();
    
    signal(SIGHUP, sighup_handler);
    
    std::string input;
    
    // Выводим приглашение только в интерактивном режиме
    if (isatty(STDIN_FILENO)) {
        std::cout << "$ ";
    }
    
    while (std::getline(std::cin, input)) {
        if (input == "exit") {
            break;
        }
        
        execute_command(input);
        
        // Выводим приглашение только в интерактивном режиме
        if (isatty(STDIN_FILENO)) {
            std::cout << "$ ";
        }
    }
    
    return 0;
}
