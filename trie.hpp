#ifndef __TRIE_HPP__
#define __TRIE_HPP__

#include <map>
#include <deque>
#include <string>

using std::map;
using std::deque;
using std::wstring;

class Trie {
public:
  struct Node {
    size_t words;
    size_t prefixes;
    map<wchar_t, Node*> edges;

    Node() : words(0), prefixes(0), edges() { }
  };

  Node * root;

  // TODO: convert to stack destruction
  void destruct(Node * node) {
    for(auto i = node->edges.begin(); i != node->edges.end(); i++) {
      destruct(i->second);
    }
    delete node;
  }

  Trie() : root(new Node()) {}

  void add_word(wstring const & word) {
    add_word_helper(root, word.begin(), word.end());
  }

  template<typename Iter>
  void add_word_helper(Node * node, Iter begin, Iter end) {
    while(begin != end) {
      node->prefixes++;
      wchar_t c = *begin++;
      auto k = node->edges.find(c);
      if(k == node->edges.end()) {
        Node * temp = new Node();
        node->edges[c] = temp;
        node = temp;
      } else {
        node = k->second;
      }
    }
    node->words++;
  }

  size_t count_words(wstring const & word) {
    return count_words_helper(root, word.begin(), word.end());
  }

  template<typename Iter>
  size_t count_words_helper(Node * node, Iter begin, Iter end) {
    while(begin != end) {
      wchar_t c = *begin++;
      auto k = node->edges.find(c);
      if (k == node->edges.end()) {
        return 0;
      }
      node = k->second;
    }
    return node->words;
  }

  size_t count_prefixes(wstring const & prefix) {
    return count_prefixes_helper(root, prefix.begin(), prefix.end());
  }

  template<typename Iter>
  size_t count_prefixes_helper(Node * node, Iter begin, Iter end) {
    while(begin != end) {
      wchar_t c = *begin++;
      auto k = node->edges.find(c);
      if (k == node->edges.end()) {
        return 0;
      }
      node = k->second;
    }
    return node->prefixes;
  }


  ~Trie() {
    destruct(root);
  }
};

#endif
