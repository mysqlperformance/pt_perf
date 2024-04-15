#ifndef _h_sys_tools_
#define _h_sys_tools_
#include <string>
#include <chrono>

#define PT_HOME_PATH "/sys/devices/intel_pt"
#define PT_IP_FILTER_PATH "/sys/devices/intel_pt/caps/ip_filtering"

#define ut_time_now() std::chrono::steady_clock::now()
#define ut_time_diff(t2, t1) std::chrono::duration<double>(t2 - t1).count()

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
