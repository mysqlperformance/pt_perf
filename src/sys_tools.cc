#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <libgen.h>
#include <vector>
#include <sys/wait.h>
#include <stdexcept>

#include "sys_tools.h"

// current executing pid which do bash command, and we wait it
static pid_t current_cmd_pid = 0;

long str2long(const std::string &str) {
  long res;
  try {
    res = std::stol(str);
  } catch (const std::exception& e) {
    printf("ERROR: wrong integer format, please check your argument!\n");
    exit(0);
  }
  return res;
}

int str2int(const std::string &str) {
  int res;
  try {
    res = std::stoi(str);
  } catch (const std::exception& e) {
    printf("ERROR: wrong integer format, please check your argument!\n");
    exit(0);
  }
  return res;
}

std::pair<uint64_t, uint64_t> get_interval_from_string(const std::string &str) {
  std::pair<uint64_t, uint64_t> res;
  int sep = str.find_first_of(',');
  if (sep == std::string::npos) {
    printf("ERROR: wrong latency interval format!\n");
    exit(0);
    return {0, UINT64_MAX};
  }
  res.first = str2long(str.substr(0, sep));
  res.second = str2long(str.substr(sep + 1, str.size()));
  return res;
}
size_t get_file_linecount(const std::string &path) {
  size_t total = 0;
  std::ifstream ifs(path);
  if (ifs.is_open()) {
    std::string line;
    while (getline(ifs, line)) ++total;
  }
  return total;
}

std::string parse_number_range_to_sequence(const std::string &str) {
  std::stringstream ss(str);
  std::string range;
  std::string res = "";
  while (getline(ss, range, ',')) {
    int sep = range.find_first_of('-');
    if (sep == std::string::npos) {
      res += (std::string(range) + ",");
    } else {
      int b = str2int(range.substr(0, sep));
      int e = str2int(range.substr(sep + 1, range.size()));
      for (int i = b; i <= e; i++) {
        res += (std::to_string(i) + ",");
      }
    }
  }
  if (res != "") {
    res.pop_back();
  }
  return res;
}

bool check_system() {
  // first check if intel pt is supported
  FILE *file = fopen(PT_HOME_PATH, "r");
  if (file == nullptr) {
    printf("ERROR: intel pt is not supported by your cpu architecture, check your cpu model and linux version (>= 4.x).\n");
    return -1;
  }
  fclose(file);

  file = fopen(PT_IP_FILTER_PATH, "r");
  if (file == nullptr) {
    printf("ERROR: ip_filtering is not supported by your cpu architecture, check your cpu model and linux version (>= 4.x).\n");
    return -1;
  }
  fclose(file);
  return 0;
}

bool check_pt_flame(const std::string &pt_flame_home) {
  FILE *file = fopen(pt_flame_home.c_str(), "r");
  if (file == nullptr) {
    printf("ERROR: Pt_frame is not intalled at '%s', try \"yum install t-pt-flame -b test\"\n"
           "       or clone the source code to install, use '--pt_flame' to set the intalled path\n"
           "       refer to https://github.com/mysqlperformance/pt-flame.\n", pt_flame_home.c_str());
    return -1;
  }
  fclose(file);
  return 0;
}

void exec_cmd_killable(const std::string &cmd) {
  current_cmd_pid = fork();
  if (current_cmd_pid == 0) {
    execl("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
    exit(0);
  } else {
    waitpid(current_cmd_pid, NULL, 0);
  }
  current_cmd_pid = 0;
}

void abort_cmd_killable(int sig) {
  if (current_cmd_pid != 0) {
    kill(current_cmd_pid, sig);
    current_cmd_pid = 0;
  }
}
/*
 *  get current directory
 * */
std::string get_current_dir() {
  char path[1024];
  int len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  path[len] = '\0';
  return std::string(dirname(path));
}

static int filename_split(char *line, std::string &filename, uint &line_nr)
{
  std::string line_str(line);

  if (line_str == "??:?\n" || line_str == "??:0\n")
    return 1;

  size_t start = line_str.find_last_of('/');
  if (start == std::string::npos) {
    start = -1;
  }
  start += 1;
  size_t end = line_str.find_first_of(':', start);
  if (end == std::string::npos) {
    return 1;
  }
  filename = line_str.substr(start, end - start);
  start = end + 1;
  end = line_str.size() - 1; // in last char of "\n"
  std::string line_nr_str = line_str.substr(start, end - start);
  if (line_nr_str != "?")
    line_nr = std::stoul(line_nr_str);
  return 0;
}

/*
 *  parse source file and line by address
 * */
void addr2line(const std::string &binary,
               uint64_t address,
               std::string &filename,
               uint &line_nr)
{
  std::stringstream cmd;
  char *line = NULL;
  size_t line_len = 0;

  filename = "";
  line_nr = 0;
  cmd << "addr2line -e " << binary << " "
      << std::hex << address << std::dec;
  FILE* a2l = popen(cmd.str().c_str(), "r");
  if (!a2l) return;

  if (getline(&line, &line_len, a2l) != -1 && line_len) {
    filename_split(line, filename, line_nr);
  }

  free(line);
  pclose(a2l);
}

void addr2line(const std::string &binary,
               std::vector<uint64_t> &address_vec,
               std::vector<std::string> &filename_vec,
               std::vector<uint> &line_nr_vec)
{
  std::stringstream cmd;
  char *line = NULL;
  size_t line_len = 0;

  if (!address_vec.size()) return;

  cmd << "addr2line -e " << binary;
  for (size_t i = 0; i < address_vec.size(); ++i) {
    cmd << " " << std::hex << address_vec[i] << std::dec;
  }
  cmd << " 2> /dev/null";
  FILE* a2l = popen(cmd.str().c_str(), "r");
  if (!a2l) return;

  while (getline(&line, &line_len, a2l) != -1 && line_len) {
    std::string filename;
    uint line_nr;
    filename_split(line, filename, line_nr);
    filename_vec.push_back(filename);
    line_nr_vec.push_back(line_nr);
  }

  if (filename_vec.size() != address_vec.size()) {
    printf("ERROR: the line number is not equal to the address number, "
           " the command is '%s'\n",cmd.str().c_str());
  }

  free(line);
  pclose(a2l);
}
