#include <cstring>
#include "sys_tools.h"
#include "pt_action.h"

namespace pt {
using namespace std;

static string trace_error_str = " instruction trace error";
bool ActionSet::out_of_order = false;
using defer = std::shared_ptr<void>;

void Action::init_for_sched() {
  if ((to->name == sys_sched_funcname ||
				to->name == sys_sched_funcname_419) && to->offset == 0 &&
			(type == PT_ACTION_CALL || type == PT_ACTION_TR_START)) {
    sched_begin = true;
  } else if ((from->name == sys_sched_funcname ||
				from->name == sys_sched_funcname_419) &&
      (type == PT_ACTION_RETURN || type == PT_ACTION_TR_END_RETURN)) {
    sched_end = true;
  }
}
void Action::init_for_ancestor(const string &ancestor) {
  if (to->name == ancestor && to->offset == 0 &&
      (type == PT_ACTION_CALL || type == PT_ACTION_TR_START)) {
    ancestor_begin = true;
  } else if (from->name == ancestor && (type == PT_ACTION_RETURN ||
              type == PT_ACTION_TR_END_RETURN)) {
    ancestor_end = true;
  }
}

void report_error_action(const string &type, Action *action, bool verbose) {
  if (verbose)
#ifdef DEBUG
    printf("impletement %s error is detected in action of "
           "file %d, line %d\n", type.c_str(), action->id, action->lnum);
#else
    printf("impletement %s error is detected in action "
           "with timestamp: %llu\n", type.c_str(), action->ts);
#endif
}

static Symbol* get_symbol_from_string(const string &str,
		Symbol *caller, SymbolMgr &sym_mgr) {
  if (str.find("[unknown]") != string::npos) {
		return sym_mgr.create(0, 0, "[unknown]");
  }
  
  std::string name = "";
  uint64_t addr;
  uint32_t offset;
  size_t start, end;

  // address
  end = str.find_first_of(' ');
  addr = stoull(str.substr(0, end), nullptr, 16);

  // name
  start = end + 1;
  end = str.find_last_of('+');
  name = str.substr(start, end - start);

  // offset
  start = str.find_last_of('+');
  end = str.find_first_of(' ', start);
  if (end == string::npos) end = str.size() - 1;
  offset = stol(str.substr(start + 1, end-start+1), nullptr, 16);
  
	return sym_mgr.create(addr, offset, name);
}

static std::vector<std::string> action_type_strs = {
	"call", "return", "jcc", "jmp", "tr strt", "tr end  call", "tr end  return",
	"tr end  hw int", "tr end  syscall", "tr end", "int", "iret", "syscall",
	"sysret", "async", "hw int", "tx abrt", "vmentry", "vmexit"
};

bool create_action_from_string(Action &action,
    SymbolMgr &sym_mgr, const string &line) {
  action.is_error = false;
  action.from_target = action.to_target = false;
  if (!line.compare(0, trace_error_str.size(),
          trace_error_str, 0, trace_error_str.size())) {
    if (line.find("Lost trace data") == string::npos) {
      return true;
    }
    // action implies the trace data lost
    action.pt_type = PT_ACTION_TYPE_ERROR;
    action.is_error = true;
    size_t start = line.find("tid") + 3;
    size_t end = line.find("ip");
    action.tid = stol(line.substr(start, end - start));

    start = line.find("time") + 4;
    end = line.find_first_of('.', start);
    long sec = stol(line.substr(start, end - start));
    start = end + 1;
    end = line.find("cpu");
    long nsec = stol(line.substr(start, end - start));
    action.ts = sec * NSECS_PER_SECS + nsec;

    return false;
  }

  /* action thread */
  size_t start = 0;
  size_t end = line.find_first_of('[', start);
  long tid = stol(line.substr(start, end - start));
  action.tid = tid;

  /* action cpu */
  start = end + 1;
  end = line.find_first_of(']', start);

  /* action timestamp */
  start = end + 1;
  end = line.find_first_of('.', start);
  long sec = stol(line.substr(start, end - start));
  
  start = end + 1;
  end = line.find_first_of(':', start);
  long nsec = stol(line.substr(start, end - start));
  action.ts = sec*NSECS_PER_SECS + nsec;

  /* action type */
  start = line.find_first_not_of(' ', end + 1);
  end = string::npos;
  for (size_t i=0; i<action_type_strs.size(); ++i) {
    string &type_str = action_type_strs[i];
    if (line.substr(start, type_str.size()) == type_str) {
      end = start + type_str.size();
      action.type = PT_ACTION_CALL + i;
      break;
    }
  }

  if (end == string::npos) {
    printf("error action type for line: %s\n", line.c_str());
    abort();
  }

  /* action symbol */
  start = line.find_first_not_of(' ', end);
  end = line.find("=>", start);
  action.from = get_symbol_from_string(line.substr(start, end - start),
      nullptr, sym_mgr);
  
  start = line.find_first_not_of(' ', end + 2);
  end = line.size();
  action.to = get_symbol_from_string(line.substr(start, end - start),
      action.from, sym_mgr);

  action.pt_type = PT_ACTION_TYPE_BRANCH;
  return false;
}

unsigned char *create_action_from_binary(Action &action,
    SymbolMgr &sym_mgr, unsigned char *ptr) {
  action.is_error = false;
  action.pt_type = read1bytes(ptr);
  ptr += 1;

  if (action.pt_type == PT_ACTION_TYPE_UNDEFINE) {
    return nullptr;
  } else if (action.pt_type == PT_ACTION_TYPE_BRANCH) {
    uint32_t tid;
    uint32_t from_id;
    uint32_t to_id;
    ptr = pt_read_branch_action(ptr, &tid, &action.ts,
        &action.type, &from_id, &to_id);
    action.tid = (int) tid;
    action.from = sym_mgr.get_by_id(from_id);
    action.to = sym_mgr.get_by_id(to_id);
    //printf("%d, %llu, %s, %d\n", tid, action.ts, action.from->name.c_str(), to_id);
  } else if (action.pt_type == PT_ACTION_TYPE_SYMBOL) {
    uint32_t sym_id;
    uint64_t addr;
    uint32_t offset;
    char *name;
    uint32_t name_len;
    ptr = pt_read_symbol_action(ptr, &sym_id,
        &addr, &offset, &name, &name_len);
    assert(sym_mgr.create(sym_id, addr, offset, std::string(name, name_len)));
  } else if (action.pt_type == PT_ACTION_TYPE_ERROR) {
    uint32_t tid;
    uint8_t code;
    ptr = pt_read_error_action(ptr, &tid, &action.ts, &code);
    action.tid = (int) tid;
    if (code != 8) {
      /* not data lost error */
      action.pt_type = PT_ACTION_TYPE_UNDEFINE;
    } else {
      action.is_error = true;
    }
  }
  return ptr;
}

bool SrclineMap::get(const std::string &function, std::string &srcline) {
  srcline = "-";
  m_lock.s_lock();
  defer _(nullptr, [&](...) {m_lock.s_unlock();});
  auto it = srcline_map.find(function);
  if (it != srcline_map.end()) {
    srcline = it->second;
    return true;
  }
  return false;
}

void SrclineMap::process_address() {
  m_lock.x_lock();
  defer _(nullptr, [&](...) {m_lock.x_unlock();});
  vector<string> func_vec;
  vector<uint64_t> addr_vec;
  vector<string> filename_vec;
  vector<uint> line_nr_vec;
  for (auto it = addr_map.begin(); it != addr_map.end(); ++it) {
    func_vec.push_back(it->first);
    addr_vec.push_back(it->second);
  }
  addr2line(binary, addr_vec, filename_vec, line_nr_vec);
  if (filename_vec.size() != addr_vec.size()) {
    printf("ERROR: addr2line failed !!!");
    return;
  }
  for (size_t i = 0; i < func_vec.size(); ++i) {
    string &name = func_vec[i];
    srcline_map[name] = filename_vec[i] + ":" + to_string(line_nr_vec[i]);
  }
}

uint64_t SrclineMap::get(const std::string &function) {
  m_lock.s_lock();
  defer _(nullptr, [&](...) {m_lock.s_unlock();});
  auto it = addr_map.find(function);
  if (it != addr_map.end()) {
    return it->second;
  }
  return 0;
}

void SrclineMap::put(const std::string &function, uint64_t addr) {
  m_lock.x_lock();
  defer _(nullptr, [&](...) {m_lock.x_unlock();});
  auto it = addr_map.find(function);
  if (it == addr_map.end()) {
    addr_map[function] = addr;
  }
}

};
