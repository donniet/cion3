
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <algorithm>
#include <random>
#include <cwchar>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

#include "trie.hpp"
#include "suffix.hpp"

using boost::graph_traits;
using boost::tie;
using boost::adjacency_list;
using boost::vecS;
using boost::directedS;
using boost::edge;
using boost::add_edge;
using boost::add_vertex;
using boost::vertices;
using boost::target;

using std::wcslen;
using std::wstring;
using std::uint64_t;
using std::map;
using std::set;
using std::pair;
using std::wcin;
using std::wcout;
using std::wcerr;
using std::wostream;
using std::numeric_limits;
using std::vector;
using std::move;
using std::poisson_distribution;

wostream & print_string(wostream & os, wstring const & s) {
  for(auto i = s.begin(); i != s.end(); i++) {
    if(*i == '\"') {
      os << "\\\"";
    } else if(*i >= 32 && *i < 127) {
      os << *i;
    } else {
      os << "\\u" << std::hex << std::setw(4) << std::setfill(L'0') << (unsigned int)*i;
    }
  }
  return os;
}

struct Symbol {
  uint64_t id;
  wstring name;
  uint64_t count;

  Symbol() : id(0), name(), count(0)  {}

  template<typename Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & id;
    ar & name;
    ar & count;
  }

  wostream & print_id(wostream & os) const {
    return os << "id: " << std::dec << id;
  }
  wostream & print(wostream & os) const {
    print_id(os) << ", name: \"";
    print_string(os, name) << "\", count: " << std::dec << count;
    return os;
  }
};

struct Edge {
  uint64_t count;
  uint64_t rep_symbol_id;

  Edge() : count(0), rep_symbol_id(0) {}

  template<typename Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & count;
  }

  wostream & print(wostream & os) const {
    return os << "count: " << std::dec << count << ", rep_symbol_id: " << std::dec << rep_symbol_id;
  }
};

wostream & operator<<(wostream & os, Symbol const & symbol) {
  return os << "{ id: " << symbol.id << ", name: \"" << symbol.name << "\", count: " << std::dec << symbol.count << " }";
}
wostream & operator<<(wostream & os, Edge const & edge) {
  return os << "{ count: " << std::dec << edge.count << " }";
}

template<typename T>
struct FalsePredicate {
  bool operator()(T const &) const {
    return false;
  }
};

class Runes {
public:
  typedef adjacency_list<vecS, vecS, directedS, Symbol, Edge> rune_graph;
  typedef graph_traits<rune_graph>::vertex_descriptor vertex_descriptor;
  typedef graph_traits<rune_graph>::edge_descriptor edge_descriptor;

  rune_graph graph;
  map<wstring, vertex_descriptor> vertex_map;
  typedef map<wstring, vertex_descriptor>::iterator vertex_map_iterator;
  set<pair<uint64_t, vertex_descriptor>> vertex_counts;
  set<pair<uint64_t, edge_descriptor>> edge_counts;
  uint64_t id;
  uint64_t char_count;

  Runes() : id(0) {
    wchar_t c = 0;
    vertex_descriptor n = get_vertex(c);
  }


  pair<vertex_map_iterator, vertex_map_iterator> starts_with(wstring prefix) {
    const wstring::value_type max_char = numeric_limits<wstring::value_type>::max();

    pair<vertex_map_iterator, vertex_map_iterator> empty(vertex_map.end(), vertex_map.end());
    if (prefix.size() == 0) {
      return empty;
    }
    vertex_map_iterator b = vertex_map.lower_bound(prefix);
    vertex_map_iterator e = vertex_map.end();

    while(prefix.size() > 0) {
      if (prefix.back() < max_char) {
        prefix.back() += 1;
        break;
      } else {
        prefix.pop_back();
      }
    }
    if (prefix.size() > 0) {
      e = vertex_map.lower_bound(prefix);
    }

    return pair<vertex_map_iterator, vertex_map_iterator>(b, e);
  }

  vertex_descriptor guess_one(vertex_descriptor from) {
    graph_traits<rune_graph>::out_edge_iterator ebegin, eend;
    tie(ebegin, eend) = out_edges(from, graph);

    if (ebegin == eend) {
      return vertex_map[wstring()];
    }

    edge_descriptor max = *ebegin++;
    vertex_descriptor t = boost::target(max, graph);
    uint64_t count = graph[max].count * graph[t].name.size();

    for(; ebegin != eend; ebegin++) {
      t = boost::target(*ebegin, graph);
      if (count < graph[*ebegin].count * graph[t].name.size()) {
        max = *ebegin;
        count = graph[max].count * graph[t].name.size();
      }
    }

    return target(max, graph);
  }

  wstring const & guess(wstring const & prefix) {
    vertex_descriptor v = get_vertex(prefix);
    vertex_descriptor n = guess_one(v);

    return graph[n].name;
  }

  void tally_vertex(vertex_descriptor vertex) {
    uint64_t count = graph[vertex].count;
    vertex_counts.erase(pair<uint64_t, vertex_descriptor>(count, vertex));
    graph[vertex].count = count + 1;
    vertex_counts.insert(pair<uint64_t, vertex_descriptor>(count + 1, vertex));
    char_count += graph[vertex].name.size();
  }

  edge_descriptor tally_edge(vertex_descriptor from, vertex_descriptor to) {
    edge_descriptor e;
    bool exists;
    tie(e, exists) = edge(from, to, graph);

    if (!exists) {
      tie(e, exists) = add_edge(from, to, graph);
      graph[e].count = 1;
      edge_counts.insert(pair<uint64_t, edge_descriptor>(1, e));
    } else {
      uint64_t count = graph[e].count;
      edge_counts.erase(pair<uint64_t, edge_descriptor>(count, e));
      graph[e].count = count + 1;
      edge_counts.insert(pair<uint64_t, edge_descriptor>(count + 1, e));
    }

    return e;
  }

  vertex_descriptor get_vertex(wchar_t c) {
    if (c == 0) return get_vertex(wstring());

    return get_vertex(wstring(&c, 1));
  }

  vertex_descriptor get_vertex(wstring const & name) {
    auto i = vertex_map.find(name);
    if (i != vertex_map.end()) {
      return i->second;
    }

    vertex_descriptor vertex = add_vertex(graph);
    vertex_map[name] = vertex;
    graph[vertex].id = ++id;
    graph[vertex].count = 0;
    graph[vertex].name = name;
    vertex_counts.insert(pair<uint64_t, vertex_descriptor>(0, vertex));

    return vertex;
  }

  struct Watch {
    Runes & runes;
    vertex_descriptor source_;
    vertex_descriptor target_;
    wstring::iterator target_position;
    wstring::iterator target_end;

    Watch(Runes & runes, edge_descriptor edge)
      : source_(boost::source(edge, runes.graph)), runes(runes), target_(boost::target(edge, runes.graph)),
        target_position(runes.graph[target_].name.begin()),
        target_end(runes.graph[target_].name.end())
    { }

    Watch(Runes & runes, vertex_descriptor source_, vertex_descriptor target_)
      : runes(runes), source_(source_), target_(target_),
        target_position(runes.graph[target_].name.begin()),
        target_end(runes.graph[target_].name.end())
    { }

    inline bool operator<(Watch const & right) const {
      if (&runes < &(right.runes)) return true;
      if (&(right.runes) < &runes) return false;
      if (source_ < right.source_) return true;
      if (right.source_ < source_) return false;
      if (target_position < right.target_position) return true;
      return false;
    }

    vertex_descriptor source() const {
      return source_;
    }
    vertex_descriptor target() const {
      return target_;
    }

    wchar_t expects() const {
      return *target_position;
    }

    inline bool at_end() const {
      return target_position == target_end;
    }

    bool advance() {
      if (at_end()) return false;
      target_position++;
      return true;
    }
  };

  struct Reader {
    set<vertex_descriptor> froms;
    set<Watch> watches;
    Runes & runes;

    Reader(Runes & runes, wstring const & start = wstring())
      : runes(runes)
    {
      froms.insert(runes.get_vertex(start));
      rune_graph::out_edge_iterator ebegin, eend;

      for(auto m = froms.begin(); m != froms.end(); m++) {
        tie(ebegin, eend) = out_edges(*m, runes.graph);
        for(; ebegin != eend; ebegin++) {
          watches.insert(Watch(runes, *ebegin));
        }
      }
    }

    void read(wchar_t c) {
      read(c, FalsePredicate<Runes::edge_descriptor>());
    }

    template<typename EdgeLearningPredicate>
    void read(wchar_t c, EdgeLearningPredicate predicate) {
      set<vertex_descriptor> completed;
      set<edge_descriptor> traversed;
      set<Watch> moved;

      vertex_descriptor vc = runes.get_vertex(c);
      completed.insert(vc);

      Runes::vertex_map_iterator b, e;
      tie(b, e) = runes.starts_with(wstring(&c, 1));

      for(auto m = froms.begin(); m != froms.end(); m++) {
        for(auto i = b; i != e; i++) {
          if (i->second == vc) {
            edge_descriptor e = runes.tally_edge(*m, vc);
            traversed.insert(e);
          } else {
            moved.insert(Watch(runes, *m, i->second));
          }
        }
      }


      set<Watch>::iterator i = watches.begin();

      while(i != watches.end()) {
        Watch w = *i;
        i = watches.erase(i);
        // wcout << "\nprocessing watch: " << runes.graph[w.source()].name << " --> " << runes.graph[w.target()].name << "\n";
        edge_descriptor e;
        bool found;
        tie(e, found) = boost::edge(w.source(), w.target(), runes.graph);

        if (found && traversed.find(e) != traversed.end()) {
          // already took care of it
        } else if (w.expects() == c) {
          w.advance();
          if (w.at_end()) {
            edge_descriptor e2 = runes.tally_edge(w.source(), w.target());
            completed.insert(w.target());
            if (runes.graph[e2].rep_symbol_id == 0) {
              traversed.insert(e2);
            }
          } else {
            moved.insert(w);
          }
        }
      }
      for(auto l = traversed.begin(); l != traversed.end(); l++) {
        if (predicate(*l)) {
          // learn a new symbol
          vertex_descriptor s = source(*l, runes.graph);
          vertex_descriptor t = target(*l, runes.graph);

          wstring name = runes.graph[s].name + runes.graph[t].name;

          // does this exist already?
          auto f = runes.vertex_map.find(name);
          if (f == runes.vertex_map.end()) {
            vertex_descriptor n = runes.get_vertex(name);
            // runes.graph[n].count = runes.graph[*l].count;
            runes.graph[*l].rep_symbol_id = runes.graph[n].id;
            completed.insert(n);
          } else {
            runes.graph[*l].rep_symbol_id = runes.graph[f->second].id;
            completed.insert(f->second);
          }
        }
      }

      for(auto k = completed.begin(); k != completed.end(); k++) {
        runes.tally_vertex(*k);
        rune_graph::out_edge_iterator ebegin, eend;
        tie(ebegin, eend) = out_edges(*k, runes.graph);
        for(; ebegin != eend; ebegin++) {
          moved.insert(Watch(runes, *ebegin));
        }
      }

      froms = move(completed);
      watches = move(moved);
    }

    wstring guess() {
      set<vertex_descriptor> guesses;
      for(auto m = froms.begin(); m != froms.end(); m++) {
        guesses.insert(runes.guess_one(*m));
      }

      if (guesses.size() == 0) {
        return wstring();
      }

      auto i = guesses.begin();
      vertex_descriptor v = *i++;
      uint64_t max = runes.graph[v].count * runes.graph[v].name.size();

      for(; i != guesses.end(); i++) {
        if (runes.graph[*i].count * runes.graph[*i].name.size() > max) {
          v = *i;
          max = runes.graph[*i].count * runes.graph[*i].name.size();
        }
      }
      return runes.graph[v].name;
    }
  };

  Reader get_reader(wstring const & start = wstring()) {
    return Reader(*this, start);
  }

  wostream & print_graph(wostream & os) {
    graph_traits<rune_graph>::vertex_iterator vbegin, vend;
    for(tie(vbegin, vend) = vertices(graph); vbegin != vend; vbegin++) {
      print_vertex(os, *vbegin);
    }
    return os;
  }

  wostream & print_vertex(wostream & os, vertex_descriptor vertex) {
    os << "{ ";
    graph[vertex].print(os);
    os << ", out_edges: [";

    graph_traits<rune_graph>::out_edge_iterator ebegin, eend;
    bool first = true;
    for(tie(ebegin, eend) = out_edges(vertex, graph); ebegin != eend; ebegin++) {
      if (!first) os << ",";
      else first = false;

      vertex_descriptor t = target(*ebegin, graph);
      os << "\n\t{ ";
      graph[t].print_id(os);
      os << ", ";
      graph[*ebegin].print(os);
      os << ", rep_symbol_name: \"";
      print_string(os, graph[boost::target(*ebegin, graph)].name);
      os << "\", probability: ";
      os << std::dec << double(graph[*ebegin].count) / double(graph[vertex].count);
      os << " }";
    }

    return os << "\n]}\n";
  }
};

struct EdgeLearner {
  Runes & runes;
  EdgeLearner(Runes & runes) : runes(runes) {}

  bool operator()(Runes::edge_descriptor const & ed) const {
    Edge const & edge = runes.graph[ed];
    return edge.count >= 10;
  }
};

struct PoissonLearner {
  Runes & runes;
  double sigma;
  PoissonLearner(Runes & runes, double sigma)
    : runes(runes), sigma(sigma)
  {}

  bool operator()(Runes::edge_descriptor const & ed) const {
    Edge const & edge = runes.graph[ed];
    Runes::vertex_descriptor s = boost::source(ed, runes.graph);
    Runes::vertex_descriptor t = boost::target(ed, runes.graph);

    if (edge.count <= 20) return false;

    Symbol const & head = runes.graph[s];
    Symbol const & tail = runes.graph[t];

    //TODO: only works for single char tails
    double prob = double(tail.count) / double(runes.char_count);
    double var = double(head.count) * prob * (1. - prob);

    double expect = double(head.count) * prob;

    // five sigma
    bool ret = edge.count > expect && (double(edge.count) - expect) * (double(edge.count) - expect) > sigma * sigma * var;

    if (ret) {
      if (tail.name.size() > 1) {
        wcerr << "head: '" << head.name << "' tail: '" << tail.name << "'" << std::endl;
        // wcerr << std::endl << "NOT: expect: " << expect << " count: " << edge.count << " diff: " << (double(edge.count) - expect) << " stdev: " << sqrt(var) << std::endl;
      }
    }
    // if (!ret) {
    //   wcerr << std::endl << "NOT: expect: " << expect << " count: " << edge.count << " diff: " << (double(edge.count) - expect) << " stdev: " << sqrt(var) << std::endl;
    // }
    // five sigma
    return ret;
  }
};

int main(int argc, char * argv[]) {
  Runes runes;
  // EdgeLearner learner(runes);
  PoissonLearner learner(runes, 5.);
  Runes::Reader reader = runes.get_reader();

  wchar_t c = 0;

  while(wcin.get(c) && c > 0) {
    // wstring g = reader.guess();
    // if (g.size() < 1) wcout << "_";
    // else wcout << g;
    reader.read(c, learner);

    // if (c == '\n') wcout << '\n';
  }

  runes.print_graph(wcout);
  runes.print_vertex(wcout, runes.get_vertex(L"a"));
  runes.print_vertex(wcout, runes.get_vertex(L"e"));

  wcout << std::endl << "guessing R -> " << runes.guess(L"R") << std::endl; 
  wcout << std::endl << "guessing ra -> " << runes.guess(L"ra") << std::endl;
  wcout << std::endl << "guessing t -> " << runes.guess(L"t") << std::endl;
  wcout << std::endl << "guessing th -> " << runes.guess(L"th") << std::endl;
  wcout << std::endl << "guessing a -> " << runes.guess(L"a") << std::endl;


  // wcout << runes.get_symbol(L"E") << std::endl;
  // wcout << runes.get_symbol(L"e") << std::endl;

  // Runes::vertex_map_iterator b, e;
  // wcout << "starts with b:\n";
  // for(tie(b, e) = runes.starts_with(L"b"); b != e; b++) {
  //   wcout << "'" << b->first << "'\n";
  // }

  // Trie trie;
  // trie.add_word(L"word");
  // trie.add_word(L"worry");
  // trie.add_word(L"try");
  //
  // wcout << "prefixes of 'worr': " << trie.count_prefixes(L"worr") << "\n";
  // wcout << "prefixes of 'wor': " << trie.count_prefixes(L"wor") << "\n";
  // wcout << "prefixes of 't': " << trie.count_prefixes(L"t") << "\n";
  //
  // return 0;

  // wchar_t const * str = L"banana";
  // SuffixArray<wchar_t> suffix(str, str + wcslen(str));
  //
  // std::for_each(suffix.word.begin(), suffix.word.end(), [](wchar_t c) {
  //   if(c >= 32 && c < 127) {
  //     wcout << std::setw(4) << c;
  //   } else {
  //     wcout << std::setw(4) << std::hex << (unsigned int)c;
  //   }
  // });
  // wcout << "\n";
  //
  // std::for_each(suffix.suffix_indicies.begin(), suffix.suffix_indicies.end(), [](auto index) {
  //   wcout << std::setw(4) << index;
  // });
  // wcout << "\n";
}
