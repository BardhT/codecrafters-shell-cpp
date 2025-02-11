#include <cstdlib>
#include <iostream>
#include <ostream>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <unistd.h>

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
    std::string command;
    ss >> command;
    
    if(command == "exit") {
      break;
    } else if (command == "echo") {
      std::string output = ss.str();
      std::cout << output.substr(5) << std::endl;
      continue;
    } else if (command == "type") {
      std::string subcommand;
      ss >> subcommand;
      if (builtIn.find(subcommand) != builtIn.end()) {
        std::cout << subcommand << " is a shell builtin" << std::endl;
        continue;
      } else {
        // Check the PATH directories
        bool path = false;
        for (std::string s : envs) {
          std::string full = s + "/" + subcommand;
          if (access(full.c_str(), X_OK) == 0) {
            std::cout << subcommand << " is " << full << std::endl;
            path = true;
            break;
          }
        }
        if(!path) {
          std::cout << subcommand << ": not found" << std::endl;
        }
        continue;
      }
    }

    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}