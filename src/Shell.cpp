#include "Shell.hpp"
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <ostream>
#include <string_view>
#include <system_error>
#include <unordered_set>

namespace fs = std::filesystem;

Shell::Shell() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  initializePath();
  using_history();
  rl_attempted_completion_function = shell_completion;
  currentDir = fs::current_path();

  stdError = dup(STDERR_FILENO);
  stdOut = dup(STDOUT_FILENO);
  if (stdOut == -1 || stdError == -1)
    perror("dup");
}

Shell::~Shell() {
  close(stdOut);
  close(stdError);
}

void Shell::run() {
  while (true) {
    char *input = readline("$ ");

    if (!input || !*input)
      continue;
    else
      add_history(input);

    std::vector<std::string> tokens = tokenizeInput(std::string(input));

    free(input);
    input = NULL;

    if (tokens.empty())
      continue;

    for (size_t i = 0; i < tokens.size(); ++i) {
      if (outRedirect.count(tokens[i])) {
        handleOutputRedirection(tokens[i + 1]);
        out = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        i--; // Step back to check the new token at this position
      } else if (outAppend.count(tokens[i])) {
        handleOutputAppend(tokens[i + 1]);
        append = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        i--;
      } else if (tokens[i] == "2>") {
        handleErrorRedirection(tokens[i + 1]);
        error = true;
        tokens.erase(tokens.begin() + i, tokens.begin() + i + 2);
        i--;
      } else if (tokens[i] == "2>>") {
        handleErrorAppend(tokens[i + 1]);
        errorAppend = true;
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

    if (out || append)
      closeOutputRedirect();
    if (error || errorAppend)
      closeErrorRedirect();
  }
}

// -------------------- Private Classes ------------------------------

char **Shell::shell_completion(const char *text, int start, int end) {
  if (start == 0) {
    return rl_completion_matches(
        text, [](const char *text, int state) -> char * {
          static std::vector<std::string> commands;
          static size_t index = 0;

          if (state == 0) {
            index = 0;
            commands = complete.getMatches(std::string(text));
          }

          if (index < commands.size()) {
            return strdup(commands[index++].c_str());
          }
          return nullptr;
        });
  } else
    return nullptr;
}

void Shell::initializePath() {
  pathDirs.clear();
  complete.clear();

  char *env = std::getenv("PATH");
  if (!env) {
    std::cerr << "ERROR: PATH environnment variable not set" << std::endl;
    return;
  }

  for (const std::string &s : builtIn) {
    complete.insert(s);
  }

  std::unordered_set<std::string> seenPaths;

  std::string_view envView(env);
  size_t start = 0;
  size_t end = envView.find(':');

  while (start < envView.size() + 1) {
    if (end == std::string_view::npos) {
      end = envView.size();
    }

    std::string pathStr(envView.substr(start, end - start));
    if (pathStr.empty())
      pathStr = ".";

    start = end + 1;
    end = envView.find(':', start);

    if (seenPaths.find(pathStr) != seenPaths.end()) {
      continue;
    }
    seenPaths.insert(pathStr);

    std::error_code ec;
    fs::path p(pathStr);

    if (!fs::exists(p, ec) || !fs::is_directory(p, ec)) {
      continue;
    }

    pathDirs.push_back(pathStr);

    for (const auto &entry : fs::directory_iterator(
             p, fs::directory_options::skip_permission_denied, ec)) {
      if (ec)
        break;

      if (entry.is_regular_file(ec)) {
        fs::perms perms = entry.status(ec).permissions();

        if ((perms & fs::perms::owner_exec) != fs::perms::none) {
          complete.insert(entry.path().filename().string());
        }
      }
    }
  }
}

void Shell::executeExternalCommand(const std::vector<std::string> &tokens) {
  std::vector<char *> args(tokens.size() + 1);
  for (size_t i = 0; i < tokens.size(); i++) {
    args[i] = const_cast<char *>(strdup(tokens[i].c_str()));
  }
  args[tokens.size()] = nullptr;

  pid_t pid = fork();

  if (pid < 0) {
    // perror("fork");
    return;
  } else if (pid > 0) { // Parent
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      // perror("waitpid");
      return;
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 127) {
      std::cerr << tokens[0] << ": command not found" << std::endl;
    } else if (WIFSIGNALED(status)) {
      // std::cout << tokens[0] << ": terminated by signal " <<
      // WTERMSIG(status) << std::endl;
    } else if (WIFEXITED(status) != 0 && WEXITSTATUS(status) != 0) {
      // std::cout << tokens[0] << ": exit status " << WEXITSTATUS(status) <<
      // std::endl;
    }
  } else if (pid == 0) { // Child
    execvp(tokens[0].c_str(), args.data());
    // std::cerr << "Execution failed for command: " << tokens[0] <<
    // std::endl; perror("execvp");
    exit(127);
  }

  for (size_t i = 0; i < tokens.size(); i++) {
    free(args[i]);
  }
}

void Shell::handleCd(const std::vector<std::string> &tokens) {
  int status = 0;

  if (tokens.size() == 1) {
    status = chdir(getenv("HOME"));
  } else if (tokens[1][0] == '~') {
    std::string dir = tokens[1].size() > 1
                          ? getenv("HOME") + tokens[1].substr(1)
                          : getenv("HOME");
    status = chdir(dir.c_str());
  } else {
    status = chdir(tokens[1].c_str());
  }

  if (status == -1) {
    std::cerr << tokens[0] << ": " << tokens[1] << ": No such file or directory"
              << std::endl;
  } else {
    currentDir = fs::current_path();
  }
}

void Shell::handleType(const std::vector<std::string> &tokens) {
  if (tokens.size() < 2) {
    std::cout << tokens[0] << ": missing argument\n"
              << "usage: " << tokens[0] << " <command>" << std::endl;
    return;
  }

  if (builtIn.find(tokens[1]) != builtIn.end()) {
    std::cout << tokens[1] << " is a shell builtin" << std::endl;
    return;
  }

  // Check in current directory
  std::string localPath = tokens[1];
  if (access(localPath.c_str(), X_OK) == 0) {
    std::cout << tokens[1] << " is ./" << localPath << std::endl;
    return;
  }

  // Search in PATH directories
  for (const std::string &dir : pathDirs) {
    std::string fullPath = fs::path(dir) / tokens[1];
    if (access(fullPath.c_str(), X_OK) == 0) {
      std::cout << tokens[1] << " is " << fullPath << std::endl;
      return;
    }
  }
  std::cerr << tokens[1] << ": not found" << std::endl;
}

void Shell::handleEcho(const std::vector<std::string> &tokens) {
  if (tokens.size() <= 1) {
    std::cout << std::endl;
    return;
  }

  std::string output = tokens[1];

  if (output[0] == '$') {
    output = std::string(getenv(output.substr(1).c_str()));
  } else {
    for (size_t i = 2; i < tokens.size(); i++) {
      output += " " + tokens[i];
    }
  }
  std::cout << output << std::endl;
}

void Shell::closeErrorRedirect() {
  dup2(stdError, STDERR_FILENO);
  error = false;
}

void Shell::closeOutputRedirect() {
  dup2(stdOut, STDOUT_FILENO);
  out = false;
}

std::vector<std::string> Shell::tokenizeInput(const std::string &input) {
  std::vector<std::string> tokens;
  std::string token;
  bool singleQuote = false;
  bool doubleQuote = false;

  for (size_t i = 0; i < input.size(); ++i) {
    char c = input[i];

    if (c == '\'' && !doubleQuote) {
      singleQuote = !singleQuote;
    } else if (c == '\"' && !singleQuote) {
      doubleQuote = !doubleQuote;
    } else if (c == '\\' && !singleQuote) {
      if (doubleQuote) {
        // In double quotes, backslash is only special if followed by $, ", \,
        // or newline
        if (i + 1 < input.size() &&
            escapedChars.find(input[i + 1]) != escapedChars.end()) {
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
    } else if (c == ' ' && !singleQuote && !doubleQuote) {
      // Split token on space only if not inside quotes
      if (!token.empty()) {
        tokens.push_back(token);
        token.clear();
      }
    } else {
      token += c;
    }
  }

  // Capture the final token after the loop ends
  if (!token.empty()) {
    tokens.push_back(token);
  }

  return tokens;
}

void Shell::handleOutputRedirection(std::string file) {
  int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    // perror("open");
    return;
  }

  dup2(fd, STDOUT_FILENO);
  close(fd);
}

void Shell::handleOutputAppend(std::string file) {
  int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    // perror("open");
    return;
  }

  dup2(fd, STDOUT_FILENO);
  close(fd);
}

void Shell::handleErrorRedirection(std::string file) {
  int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    // perror("open");
    return;
  }

  dup2(fd, STDERR_FILENO);
  close(fd);
}

void Shell::handleErrorAppend(std::string file) {
  int fd = open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (fd == -1) {
    // perror("open");
    return;
  }

  dup2(fd, STDERR_FILENO);
  close(fd);
}