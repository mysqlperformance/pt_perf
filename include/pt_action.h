#ifndef _h_pt_action_
#define _h_pt_action_

#include <shared_mutex>
#include <assert.h>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include "tools/perf/include/perf/pt_compact_format.h"

namespace pt {
struct Symbol {
  std::string name;
  uint64_t addr;
  uint32_t offset;
  bool equal(struct Symbol *sym) {
    uint64_t func_addr1 = addr - offset;
    uint64_t func_addr2 = sym->addr - sym->offset;
    return (func_addr1 == func_addr2);
  }
};

class SymbolMgr {
public:
	SymbolMgr() {}
	~SymbolMgr() {
		for (auto it = m_map.begin(); it != m_map.end(); ++it) {
			delete it->second;
			it->second = nullptr;
		}
	}
  Symbol *create(uint64_t addr, uint32_t offset,
			const std::string &name) {
		auto it = m_map.find(addr);
		if (it != m_map.end()) {
			return it->second;
		} 	
		Symbol *sym = new Symbol();
		sym->addr = addr;
		sym->offset = offset;
		sym->name = name;
		m_map[addr] = sym;
		return sym;
	}

  Symbol *create(uint64_t sym_id, uint64_t addr,
      uint32_t offset, const std::string &name) {
    assert(sym_id == m_vec.size());
		Symbol *sym = new Symbol();
		sym->addr = addr;
		sym->offset = offset;
		sym->name = name;
    m_vec.push_back(sym);
    return sym;
  }

  Symbol *get_by_id(uint64_t sym_id) { return m_vec[sym_id]; }
private:
  std::unordered_map<uint64_t, Symbol*> m_map;
  std::vector<Symbol *> m_vec;
};

class Action {
public:
  uint8_t pt_type;
  uint8_t type;      // type for branch
  bool from_target;
  bool to_target;
  bool sched_begin;  // begin of schedule
  bool sched_end;    // end of schedule
  bool ancestor_begin; // begin of ancestor func
  bool ancestor_end;   // end of ancestor func
  bool is_error;
  int tid;
  uint64_t ts; // timestamp for nanosecond
	Symbol *from;
	Symbol *to;
#ifdef DEBUG
  long id;
  long lnum;
#endif
  void init_for_sched();

  void init_for_ancestor(const std::string &ancestor);
};

struct ActionSet {
  static bool out_of_order;
  int tid;
  uint32_t target;
  std::vector<Action> actions;
  void add_action(Action &action) {
    assert(ActionSet::out_of_order || actions.empty() ||
        action.ts >= actions.back().ts || action.is_error);
    actions.emplace_back(action);
  }
  Action &operator[] (uint32_t i) { return actions[i]; }
  size_t size() { return actions.size(); }
  static inline bool cmp_action(const Action &a1, const Action &a2) {
    return a1.ts < a2.ts;
  }
  void sort() { std::stable_sort(actions.begin(), actions.end(), cmp_action); }
  uint64_t min_timestamp() { return actions.empty() ? UINT64_MAX : actions[0].ts; }
  uint64_t max_timestamp() { return actions.empty() ? 0 : actions.back().ts; }
  ActionSet() : target(0) {}
};

class SrclineMap {
public:
	void init(const std::string &b) {
		binary = b;
	}
  bool get(const std::string &function, std::string &srcline);
  uint64_t get(const std::string &function);
  void put(const std::string &function, uint64_t addr);
  void process_address();
private:
  std::unordered_map<std::string, uint64_t> addr_map;
  std::unordered_map<std::string, std::string> srcline_map;
  RwSpinLock m_lock;
	std::string binary;
};

inline void funcname_add_addr(std::string &name, uint64_t addr) {
  name += ("!" + std::to_string(addr));
}
inline uint64_t funcname_get_addr(const std::string &name) {
  size_t sep = name.find_last_of('!');
  if (sep == std::string::npos || sep == name.size() - 1)
    return 0;
  return stoull(name.substr(sep + 1, name.size() - sep + 1));
}
inline void funcname_add_string_mark(std::string &name, const std::string &str) {
  name += ("!" + str);
}
inline std::string funcname_get_name(const std::string &name) {
  size_t sep = name.find_last_of('!');
  if (sep == std::string::npos)
    return name;
  return name.substr(0, sep);
}

bool create_action_from_string(Action &action,
    SymbolMgr &sym_mgr, const std::string &line);
unsigned char *create_action_from_binary(Action &action,
    SymbolMgr &sym_mgr, unsigned char *ptr);

void report_error_action(const std::string &type, Action *action, bool verbose);

template <typename InitActionFunc>
void read_actions_from_text_file(const std::string &filename,
    uint32_t id, uint32_t from, uint32_t to, SymbolMgr &sym_mgr,
    InitActionFunc init_func) {
  std::fstream ifs(filename, std::ios::in | std::ios::out);
  if (ifs.is_open()) {
    std::string line;
    size_t lnum = 0;
    while (getline(ifs, line)) {
      Action action;
      if (lnum < from || lnum >= to) {
        ++lnum;
        continue;
      }
      ++lnum;
      if (create_action_from_string(action,
            sym_mgr, line)) {
        /* invalid action */
        continue;
      }
#ifdef DEBUG
      action.id = id;
      action.lnum = lnum;
#endif
      init_func(action);
    }
    ifs.close();
  }
}

template <typename InitActionFunc>
void read_actions_from_compact_file(const std::string &filename,
    uint32_t id, SymbolMgr &sym_mgr, InitActionFunc init_func) {
  std::ifstream ifs(filename, std::ios::binary);
  unsigned char buffer[PT_FILE_BLOCK_SIZE];
  if (ifs.is_open()) {
    size_t lnum = 0;
    while (!ifs.eof()) {
      ifs.read((char *)buffer, PT_FILE_BLOCK_SIZE);
      auto len = ifs.gcount();
      unsigned char *ptr = buffer;
      unsigned char *end_ptr = buffer + len;
      Action action;
      while (ptr && ptr < end_ptr) {
        ptr = create_action_from_binary(action,
            sym_mgr, ptr);
#ifdef DEBUG
        action.id = id;
        action.lnum = ++lnum;
#endif
        init_func(action);
      }
    }
    ifs.close(); 
  }
}
};

#endif
