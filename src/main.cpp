#include <iostream>
#include <ostream>
#include <sstream>
#include <unordered_set>

std::unordered_set<std::string> builtIn = {"echo", "exit", "type"};

int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while(true) {
    // Uncomment this block to pass the first stage
    std::cout << "$ ";
    
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
      } else{
        std::cout << subcommand << ": command not found" << std::endl;
        continue;
      }
    }

    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}
