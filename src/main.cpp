#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>

std::unordered_set<std::string> builtIn = {"echo", "exit", "type"};

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    // Uncomment this block to pass the first stage
    std::cout << "$ ";

    // Extract enviroment
    char* env = std::getenv("PATH");
    std::stringstream senv(env);

    // Parse environment
    std::vector<std::string> envs;
    std::string e;

    while (std::getline(senv, e, ':')) {
      envs.push_back(e);
    }
    
    // std::string input;
    std::string input;
    std::getline(std::cin, input);
    std::stringstream ss(input);

    // Tokenize and validate input
    std::vector<std::string> tokens;
    std::string token;
    while (ss >> token) {
      tokens.push_back(token);
    }
    if (tokens.empty()) continue;
    
    if(tokens[0] == "exit") {
      break;
    } else if (tokens[0] == "echo") {
      std::string output;
      
      if (tokens.size() > 1) output += tokens[1];
      else {
        std::cout << std::endl;
        continue;
      }

      for (size_t i = 2; i < tokens.size(); i++){
        output += " " + tokens[i];
      }

      std::cout << output << std::endl;
      continue;
    } else if (tokens[0] == "type") {
      if (tokens.size() <= 1) continue;
      if (builtIn.find(tokens[1]) != builtIn.end()) {
        std::cout << tokens[1] << " is a shell builtin" << std::endl;
        continue;
      } else {
        // Check the PATH directories
        bool path = false;
        for (std::string s : envs) {
          std::string full = s + "/" + tokens[1];
          if (access(full.c_str(), X_OK) == 0) {
            std::cout << tokens[1] << " is " << full << std::endl;
            path = true;
            break;
          }
        }
        if(!path) {
          std::cout << tokens[1] << ": not found" << std::endl;
        }
        continue;
      }
    } else {
      pid_t pid = fork();

      if (pid > 0) {  // Parent
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)){
          if (WEXITSTATUS(status) == 127) {
            std::cout << input << ": command not found" << std::endl;
          }
          continue;
        }
      } else if (pid == 0) {  // Child
        char* args[tokens.size() + 1];

        for(size_t i = 0; i < tokens.size(); i++) {
          args[i] = (char*)tokens[i].c_str();
        }
        args[tokens.size()] = nullptr;
        execvp(tokens[0].c_str(), args);
        exit(127);
      }
    }

    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}