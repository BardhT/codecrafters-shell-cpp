#include <iostream>
#include <ostream>
#include <sstream>

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
    }    
    std::cout << input << ": command not found" << std::endl;
  }

  return 0;
}
