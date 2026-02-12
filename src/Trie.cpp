#include "Trie.hpp"
#include <algorithm>
#include <memory>
#include <vector>

void Trie::insert(const std::string &cmd) {
  TrieNode *curr = &root;

  for (const char &c : cmd) {
    if (curr->children.find(c) == curr->children.end()) {
      curr->children[c] = std::make_unique<TrieNode>();
    }
    curr = curr->children[c].get();
  }

  curr->isEndOfWord = true;
}

std::vector<std::string> Trie::getMatches(const std::string &prefix) {
  std::vector<std::string> results;
  TrieNode *curr = &root;

  for (const char &c : prefix) {
    if (curr->children.find(c) != curr->children.end())
      curr = curr->children[c].get();
    else
      return results;
  }

  findWords(curr, prefix, results);

  std::sort(results.begin(), results.end());
  return results;
}

void Trie::findWords(TrieNode *node, const std::string &cPrefix,
                     std::vector<std::string> &results) {
  if (node->isEndOfWord)
    results.push_back(cPrefix);

  if (!node->children.empty()) {
    for (const auto &c : node->children)
      findWords(c.second.get(), cPrefix + c.first, results);
  }
}

void Trie::clear() { root.children.clear(); }