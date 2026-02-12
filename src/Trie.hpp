#pragma once
#include <map>
#include <memory>
#include <string>
#include <vector>

class Trie {
private:
  struct TrieNode {
    std::map<char, std::unique_ptr<TrieNode>> children;
    bool isEndOfWord;

    TrieNode() : isEndOfWord(false) {}
  } root;

  void findWords(TrieNode *node, const std::string &cPrefix,
                 std::vector<std::string> &results);

public:
  void insert(const std::string &cmd);
  void clear();
  std::vector<std::string> getMatches(const std::string &prefix);
};