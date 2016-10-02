
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>
#include <map>
#include <set>
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
  set<pair<uint64_t, vertex_descriptor>> vertex_counts;
  set<pair<uint64_t, edge_descriptor>> edge_counts;
  uint64_t id;

  Runes() : id(0) {
    wchar_t c = 0;
    vertex_descriptor n = get_vertex(c);
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

  void tally_edge(vertex_descriptor from, vertex_descriptor to) {
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

  vertex_descriptor add_transition(vertex_descriptor from, wchar_t c) {
    if (c == 0) return add_transition(from, wstring());

    return add_transition(from, wstring(&c, 1));
  }

  vertex_descriptor add_transition(vertex_descriptor from, wstring const & name) {
    vertex_descriptor to = get_vertex(name);
    tally_vertex(to);
    tally_edge(from, to);

    return to;
  }

  struct Reader {
    vertex_descriptor from;
    Runes & runes;

    Reader(Runes & runes, wstring const & start = wstring()) : runes(runes) {
      from = runes.get_vertex(start);
    }

    void read(wchar_t c) {
      from = runes.add_transition(from, c);
    }
    wstring guess() {
      return runes.graph[runes.guess_one(from)].name;
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
}
