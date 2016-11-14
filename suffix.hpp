#ifndef __SUFFIX_HPP__
#define __SUFFIX_HPP__

#include <string>
#include <vector>
#include <algorithm>

using std::wstring;
using std::vector;
using std::sort;

template<typename Char>
class SuffixArray {
public:
  typedef typename vector<Char>::size_type index_type;

  vector<Char> word;
  vector<index_type> suffix_indicies;

  template<typename Iterator>
  SuffixArray(Iterator begin, Iterator end) : word(begin, end) {
    word.push_back((Char)0);
    for(index_type k = 0; k < word.size(); k++) {
      suffix_indicies.push_back(k);
    }
    sort(suffix_indicies.begin(), suffix_indicies.end(), [&](index_type left, index_type right) {
      for(; left < word.size() && right < word.size() && word[left] == word[right]; left++, right++) {}

      if (left >= word.size()) return true;
      if (right >= word.size()) return false;
      return word[left] < word[right];
    });
  }
};

#endif
