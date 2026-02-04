#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <filesystem>

class Shell {
private:
    std::unordered_set<std::string> builtIn = {"echo", "exit", "type", "pwd", "cd"};
    std::unordered_set<char> escapedChars = {'\\', '$', '\"', '\n'};
    std::unordered_set<std::string> outRedirect = {">", "1>"};
    std::unordered_set<std::string> outAppend = {">>", "1>>"};
    std::vector<std::string> pathDirs;
    std::string currentDir;
    int stdOut, stdError;
    bool out = false, error = false, append = false;

    std::vector<std::string> tokenizeInput(const std::string& input) {
        std::vector<std::string> tokens;
        std::string token;
        bool singleQuote = false;
        bool doubleQuote = false;

        for (size_t i = 0; i < input.size(); ++i) {
            char c = input[i];

            if (c == '\'' && !doubleQuote) {
                singleQuote = !singleQuote;
            } 
            else if (c == '\"' && !singleQuote) {
                doubleQuote = !doubleQuote;
            } 
            else if (c == '\\' && !singleQuote) {
                if (doubleQuote) {
                    // In double quotes, backslash is only special if followed by $, ", \, or newline
                    if (i + 1 < input.size() && escapedChars.find(input[i + 1]) != escapedChars.end()) {
                        token += input[++i];
                    } else {
                        token += c;
                    }
                } else {
                    // Outside quotes, backslash always escapes the next character
                    if (i + 1 < input.size()) {
                        token += input[++i];
                    }
                }
            } 
            else if (c == ' ' && !singleQuote && !doubleQuote) {
                // Split token on space only if not inside quotes
                if (!token.empty()) {
                    tokens.push_back(token);
                    token.clear();
                }
            } 
            else {
                token += c;
            }
        }

        // Capture the final token after the loop ends
        if (!token.empty()) {
            tokens.push_back(token);
        }

        return tokens;
}

    void handleOutputRedirection(std::string file) {
        int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            // perror("open");
            return;
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    void handleOutputAppend(std::string file) {
        int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd == -1) {
            // perror("open");
            return;
        }

        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    void handleErrorRedirection(std::string file) {
        int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            // perror("open");
            return;
        }

        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    void closeErrorRedirect() {
        dup2(stdError, STDERR_FILENO);
        error = false;
    }

    void closeOutputRedirect() {
        dup2(stdOut, STDOUT_FILENO);
        out = false;
    }

    void closeAppendRedirect() {
        dup2(stdOut, STDOUT_FILENO);
        out = false;
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

        pathDirs.clear();
        initializePath();
        
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
        std::cerr << tokens[1] << ": not found" << std::endl;
    }

    void handleCd(const std::vector<std::string>& tokens) {
        int status = 0;
        if (tokens.size() == 1 || tokens[1][0] == '~') {
            status = chdir(getenv("HOME"));
        } else {
            status = chdir(tokens[1].c_str());
        }

        if (status == -1) {
            std::cerr << tokens[0] << ": " << tokens[1] << ": No such file or directory" << std::endl;
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
            // perror("fork");
            return;
        } else if (pid > 0) {  // Parent
            int status;
            if (waitpid(pid, &status, 0) == -1) {
                // perror("waitpid");
                return;
            }
            
            if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                std::cerr << tokens[0] << ": command not found" << std::endl;
            } else if (WIFSIGNALED(status)) {
                // std::cout << tokens[0] << ": terminated by signal " << WTERMSIG(status) << std::endl;
            } else if (WIFEXITED(status) != 0 && WEXITSTATUS(status) != 0) {
                // std::cout << tokens[0] << ": exit status " << WEXITSTATUS(status) << std::endl;
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

        stdError = dup(STDERR_FILENO);
        stdOut = dup(STDOUT_FILENO);
        if (stdOut == -1 || stdError == -1) perror("dup");
    }

    ~Shell() {
        close(stdOut);
        close(stdError);
    }

    void run() {
        while(true) {
            std::cout << "$ ";
            
            std::string input;
            std::getline(std::cin, input);
            
            std::vector<std::string> tokens = tokenizeInput(input);
            if (tokens.empty()) continue;

            for (size_t i = 0; i < tokens.size(); ++i) {
                if (outRedirect.count(tokens[i])) {
                    handleOutputRedirection(tokens[i+1]);
                    out = true;
                    tokens.erase(tokens.begin() + i, tokens.begin() + i + 2); 
                    i--; // Step back to check the new token at this position
                } else if (outAppend.count(tokens[i])) {
                    handleOutputAppend(tokens[i+1]);
                    append = true;
                    tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
                    i--;
                } else if (tokens[i] == "2>") {
                    handleErrorRedirection(tokens[i+1]);
                    error = true;
                    tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
                    i--;
                }
            }

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

            if (out) closeOutputRedirect();
            if (error) closeErrorRedirect();
            if (append) closeAppendRedirect();
        }
    }
};

int main() {
    Shell shell;
    shell.run();
    return 0;
}