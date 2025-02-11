#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

class Shell {
private:
    std::unordered_set<std::string> builtIn = {"echo", "exit", "type", "pwd", "cd"};
    std::vector<std::string> pathDirs;
    std::string currentDir;

    // std::vector<std::string> tokenizeInput(const std::string& input) {
    //     std::stringstream ss(input);
    //     std::vector<std::string> tokens;
    //     std::string token;
        
    //     while (ss >> token) {
    //         tokens.push_back(token);
    //     }
    //     return tokens;
    // }

    std::vector<std::string> tokenizeInput(const std::string& input) {
        bool singleQuote = false, doubleQuote = false;
        std::vector<std::string> tokens;
        std::string token;

        for (size_t i = 0; i < input.size(); i++) {
            char c = input[i];
            if (c == ' ' && !singleQuote && !doubleQuote) {
                if (!token.empty()) tokens.push_back(token);
                token.clear();
            } else if (c == '\'' && !doubleQuote) {
                singleQuote = !singleQuote;
            } else if (c == '\"' && !singleQuote) {
                doubleQuote = !doubleQuote;
            } else if (c == '\\' && !(doubleQuote || singleQuote)) {
                if (i + 1 < input.size()){
                    char esc = input[i + 1];
                    token += esc;
                    ++i;
                }
            } else if(doubleQuote && c == '\'') {
                token += c;
            } else if (c != '\'' && c != '\"') {
                token += c;
            }
        }

        if (!token.empty()) tokens.push_back(token);

        return tokens;
    }

    void handleEcho(const std::vector<std::string>& tokens) {
        if (tokens.size() <= 1) {
            std::cout << std::endl;
            return;
        }

        std::string output = tokens[1];
        for (size_t i = 2; i < tokens.size(); i++) {
            output += " " + tokens[i];
        }
        std::cout << output << std::endl;
    }

    void handleType(const std::vector<std::string>& tokens) {
        if (tokens.size() <= 1) return;
        
        if (builtIn.find(tokens[1]) != builtIn.end()) {
            std::cout << tokens[1] << " is a shell builtin" << std::endl;
            return;
        }

        for (const std::string& dir : pathDirs) {
            std::string fullPath = dir + "/" + tokens[1];
            if (access(fullPath.c_str(), X_OK) == 0) {
                std::cout << tokens[1] << " is " << fullPath << std::endl;
                return;
            }
        }
        std::cout << tokens[1] << ": not found" << std::endl;
    }

    void handleCd(const std::vector<std::string>& tokens) {
        int status = 0;
        if (tokens.size() == 1 || tokens[1][0] == '~') {
            status = chdir(getenv("HOME"));
        } else {
            status = chdir(tokens[1].c_str());
        }

        if (status == -1) {
            std::cout << tokens[0] << ": " << tokens[1] << ": No such file or directory" << std::endl;
        } else {
            currentDir = std::filesystem::current_path();
        }
    }

    void executeExternalCommand(const std::vector<std::string>& tokens) {
        std::vector<char*> args(tokens.size() + 1);
        for(size_t i = 0; i < tokens.size(); i++) {
            args[i] = const_cast<char*>(strdup(tokens[i].c_str()));
        }
        args[tokens.size()] = nullptr;
        
        pid_t pid = fork();
        
        if (pid < 0) { 
            perror("fork");
            return;
        } else if (pid > 0) {  // Parent
            int status;
            if (waitpid(pid, &status, 0) == -1) {
                perror("waitpid");
                return;
            }
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                std::cerr << tokens[0] << ": command not found" << std::endl;
            } else if (WIFSIGNALED(status)) {
                std::cout << tokens[0] << ": terminated by signal " << WTERMSIG(status) << std::endl;
            } else if (WIFEXITED(status) != 0 && WEXITSTATUS(status) != 0) {
                std::cout << tokens[0] << ": exit status " << WEXITSTATUS(status) << std::endl;
            }
        } else if (pid == 0) {  // Child    
            execvp(tokens[0].c_str(), args.data());
            // std::cerr << "Execution failed for command: " << tokens[0] << std::endl;
            // perror("execvp");
            exit(127);
        }

        for (size_t i = 0; i < tokens.size(); i++) {
            free(args[i]);
        }
    }

    void initializePath() {
        char* env = std::getenv("PATH");
        if (!env) {
            std::cerr << "ERROR: PATH environnment variable not set" << std::endl;
            return;
        }
        std::stringstream senv(env);
        std::string path;

        while (std::getline(senv, path, ':')) {
            pathDirs.push_back(path);
        }
    }

public:
    Shell() {
        std::cout << std::unitbuf;
        std::cerr << std::unitbuf;
        
        initializePath();
        currentDir = std::filesystem::current_path();
    }

    void run() {
        while(true) {
            std::cout << "$ ";
            
            std::string input;
            std::getline(std::cin, input);
            
            std::vector<std::string> tokens = tokenizeInput(input);
            if (tokens.empty()) continue;

            if (tokens[0] == "exit") {
                break;
            } else if (tokens[0] == "echo") {
                handleEcho(tokens);
            } else if (tokens[0] == "type") {
                handleType(tokens);
            } else if (tokens[0] == "pwd") {
                std::cout << currentDir << std::endl;
            } else if (tokens[0] == "cd") {
                handleCd(tokens);
            } else {
                executeExternalCommand(tokens);
            }
        }
    }
};

int main() {
    Shell shell;
    shell.run();
    return 0;
}