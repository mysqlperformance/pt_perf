#ifndef _h_sys_tools_
#define _h_sys_tools_
#include <string>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>

#define PT_HOME_PATH "/sys/devices/intel_pt"
#define PT_IP_FILTER_PATH "/sys/devices/intel_pt/caps/ip_filtering"

#define ut_time_now() std::chrono::steady_clock::now()
#define ut_time_diff(t2, t1) std::chrono::duration<double>(t2 - t1).count()

class RwSpinLock {
public:
  RwSpinLock() : m_word(WRITE) {};
  void s_lock() {
    uint32_t loop = 0;
    int lk = 0;
    do {
      lk = m_word.load(std::memory_order_relaxed);
      if (lk > 0 &&
          m_word.compare_exchange_weak(lk, lk - 1,
            std::memory_order_release,
            std::memory_order_relaxed)) {
        return;
      }
      if (++loop > 1024)
        std::this_thread::yield();
    } while (true);
  }
  void s_unlock() {
    m_word.fetch_add(1);
  }
  void x_lock() {
    m_writer.lock();
    uint32_t loop = 0;
    int lk = m_word.fetch_sub(WRITE);
    while (lk != 0) {
      if (++loop > 1024)
        std::this_thread::yield();
      lk = m_word.load();
    }
  }
  void x_unlock() {
    m_word.fetch_add(WRITE);
    m_writer.unlock();
  }
private:
  static constexpr int WRITE = 0x20000000;
  std::atomic<int> m_word;
  std::mutex m_writer; // mutex for synchronization; held by X lock holders
};

void exec_cmd_killable(const std::string &cmd);
void abort_cmd_killable(int sig);

long str2long(const std::string &str);
int str2int(const std::string &str);
std::pair<uint64_t, uint64_t> get_interval_from_string(const std::string &str);
std::string parse_number_range_to_sequence(const std::string &str);
size_t get_file_linecount(const std::string &path);
bool check_system();
bool check_pt_flame(const std::string &pt_flame_home);
std::string get_current_dir();
void addr2line(const std::string &binary,
               uint64_t address,
               std::string &filename,
               uint &line_nr);

void addr2line(const std::string &binary,
               std::vector<uint64_t> &address_vec,
               std::vector<std::string> &filename_vec,
               std::vector<uint> &line_nr_vec);
#endif
