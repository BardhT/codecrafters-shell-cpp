#pragma once
#include "Trie.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <readline/history.h>
#include <readline/readline.h>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

class Shell {
private:
  std::unordered_set<std::string> builtIn = {"echo", "exit", "type", "pwd",
                                             "cd"};
  std::unordered_set<char> escapedChars = {'\\', '$', '\"', '\n'};
  std::unordered_set<std::string> outRedirect = {">", "1>"};
  std::unordered_set<std::string> outAppend = {">>", "1>>"};
  std::vector<std::string> pathDirs;
  std::string currentDir;
  static Trie complete;
  int stdOut, stdError;
  bool out = false, error = false, append = false, errorAppend = false;

  std::vector<std::string> tokenizeInput(const std::string &input);
  void handleOutputRedirection(std::string file);
  void handleOutputAppend(std::string file);
  void handleErrorRedirection(std::string file);
  void handleErrorAppend(std::string file);
  void closeErrorRedirect();
  void closeOutputRedirect();
  void handleEcho(const std::vector<std::string> &tokens);
  void handleType(const std::vector<std::string> &tokens);
  void handleCd(const std::vector<std::string> &tokens);
  void executeExternalCommand(const std::vector<std::string> &tokens);
  void initializePath();

  static char **shell_completion(const char *text, int start, int end);

public:
  Shell();
  ~Shell();

  void run();
};