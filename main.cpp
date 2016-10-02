
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/adj_list_serialize.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>

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
    return os << "id: " << id;
  }
  wostream & print(wostream & os) const {
    print_id(os) << ", name: \"";
    print_string(os, name) << "\", count: " << count;
    return os;
  }
};

struct Edge {
  uint64_t count;

  Edge() : count(0) {}
  template<typename Archive>
  void serialize(Archive & ar, const unsigned int version) {
    ar & count;
  }

  wostream & print(wostream & os) const {
    return os << "count: " << count;
  }
};

wostream & operator<<(wostream & os, Symbol const & symbol) {
  return os << "{ id: " << symbol.id << ", name: \"" << symbol.name << "\", count: " << symbol.count << " }";
}
wostream & operator<<(wostream & os, Edge const & edge) {
  return os << "{ count: " << edge.count << " }";
}

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
    uint64_t count = graph[max].count;

    for(; ebegin != eend; ebegin++) {
      if (count < graph[*ebegin].count) {
        max = *ebegin;
        count = graph[max].count;
      }
    }

    return target(max, graph);
  }

  void tally_vertex(vertex_descriptor vertex) {
    uint64_t count = graph[vertex].count;
    vertex_counts.erase(pair<uint64_t, vertex_descriptor>(count, vertex));
    graph[vertex].count = count + 1;
    vertex_counts.insert(pair<uint64_t, vertex_descriptor>(count + 1, vertex));
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

  // edge_descriptor add_transition(vertex_descriptor from, wchar_t c) {
  //   if (c == 0) return add_transition(from, wstring());
  //
  //   return add_transition(from, wstring(&c, 1));
  // }
  //
  // edge_descriptor add_transition(vertex_descriptor from, wstring const & name) {
  //   vertex_descriptor to = get_vertex(name);
  //   tally_vertex(to);
  //   return tally_edge(from, to);
  // }

  struct Watch {
    Runes & runes;
    edge_descriptor edge;
    vertex_descriptor target_;
    wstring::iterator target_position;
    wstring::iterator target_end;

    Watch(Runes & runes, edge_descriptor edge)
      : edge(edge), runes(runes), target_(boost::target(edge, runes.graph)),
        target_position(runes.graph[target_].name.begin()),
        target_end(runes.graph[target_].name.end())
    { }

    inline bool operator<(Watch const & right) const {
      if (&runes < &(right.runes)) return true;
      if (&(right.runes) < &runes) return false;
      if (edge < right.edge) return true;
      if (right.edge < edge) return false;
      if (target_position < right.target_position) return true;
      return false;
    }

    vertex_descriptor source() const {
      return boost::source(edge, runes.graph);
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
    uint64_t learning_minimum;

    Reader(Runes & runes, wstring const & start = wstring())
      : runes(runes), learning_minimum(10)
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
      set<vertex_descriptor> completed;
      set<edge_descriptor> traversed;
      vertex_descriptor vc = runes.get_vertex(c);

      for(auto m = froms.begin(); m != froms.end(); m++) {
        edge_descriptor e = runes.tally_edge(*m, vc);
        completed.insert(vc);
        traversed.insert(e);
      }

      set<Watch>::iterator i = watches.begin();
      set<Watch> moved;

      while(i != watches.end()) {
        Watch w = *i;
        // wcout << "\nprocessing watch: " << runes.graph[w.source()].name << " --> " << runes.graph[w.target()].name << "\n";
        i = watches.erase(i);

        if (traversed.find(w.edge) != traversed.end()) {
          // already took care of it
        } else if (w.expects() == c) {
          w.advance();
          if (w.at_end()) {
            edge_descriptor e2 = runes.tally_edge(w.source(), w.target());
            completed.insert(w.target());
            traversed.insert(e2);
          } else {
            moved.insert(w);
          }
        }
      }
      for(auto l = traversed.begin(); l != traversed.end(); l++) {
        if (runes.graph[*l].count >= learning_minimum) {
          // learn a new symbol
          vertex_descriptor s = source(*l, runes.graph);
          vertex_descriptor t = target(*l, runes.graph);

          wstring name = runes.graph[s].name + runes.graph[t].name;

          // does this exist already?
          auto f = runes.vertex_map.find(name);
          if (f == runes.vertex_map.end()) {
            vertex_descriptor n = runes.get_vertex(name);
            completed.insert(n);
          } else {
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
      uint64_t max = runes.graph[v].count;

      for(; i != guesses.end(); i++) {
        if (runes.graph[*i].count > max) {
          v = *i;
          max = runes.graph[*i].count;
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
      os << "{ ";
      graph[*vbegin].print(os);
      os << ", out_edges: [";

      graph_traits<rune_graph>::out_edge_iterator ebegin, eend;
      bool first = true;
      for(tie(ebegin, eend) = out_edges(*vbegin, graph); ebegin != eend; ebegin++) {
        if (!first) os << ",";
        else first = false;

        vertex_descriptor t = target(*ebegin, graph);
        os << "\n\t{ ";
        graph[t].print_id(os);
        os << ", ";
        graph[*ebegin].print(os);
        os << " }";
      }

      os << "\n]}\n";
    }
    return os;
  }
};


int main(int argc, char * argv[]) {
  wcout << "Hello World!\n";

  Runes runes;
  Runes::Reader reader = runes.get_reader();

  wchar_t c = 0;

  while(wcin.get(c) && c > 0) {
    wstring g = reader.guess();
    if (g.size() < 1) wcout << "_";
    else wcout << g;
    reader.read(c);

    if (c == '\n') wcout << '\n';
  }

  runes.print_graph(wcout);

  Runes::vertex_map_iterator b, e;
  wcout << "starts with b:\n";
  for(tie(b, e) = runes.starts_with(L"b"); b != e; b++) {
    wcout << b->first << "\n";
  }
}
