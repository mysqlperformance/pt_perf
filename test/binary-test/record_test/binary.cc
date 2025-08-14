#include <thread>
#include <fstream>
#include <cstdlib>
#include <chrono>
#include <string>
#include <vector>
#include <atomic>
#include <signal.h>
#include <unistd.h>
#include <iostream>
#include <pthread.h>

using namespace std;

int thread_num = 3;
atomic_bool should_stop{false};

static void sig_handler(int sig) {
  should_stop = true;
}

void do_io(const string &file, string &str, int rand_num) {
  ofstream of(file);
  if (of.is_open()) {
    of << str << std::endl;
    this_thread::sleep_for(std::chrono::milliseconds(1 + rand_num % 10));
    of.close();
  }
}

void do_cpu_work(string &str, int rand_num) {
  str = "";
  for (size_t i = 0; i < 512; ++i) {
    str += to_string(rand_num);
  }
  this_thread::sleep_for(std::chrono::milliseconds(1 + rand_num % 10));
}

void thread_func(int id) {
  string file = "binary_output_" + to_string(id);
  string str = "";
  while (!should_stop.load()) {
    int rand_num = std::rand() % 100;
    if (rand_num < 50) {
      do_io(file, str, rand_num);
    } else {
      do_cpu_work(str, rand_num);
    }
  }
  std::remove(file.c_str());
}

int main() {
  should_stop = false;
  signal(SIGINT, sig_handler);
  
  int pid = getpid();
  ofstream of("pid.file");
  if (of.is_open()) {
    of << pid;
    of.close();
  }
  cout << "pid: " << pid << endl;

  std::vector<thread> thrs;
  for (int i = 0; i < thread_num; ++i) {
    thrs.push_back(thread(thread_func, i));
    string name = "thread_" + to_string(i);
    pthread_setname_np(thrs[i].native_handle(), name.c_str());
  }
  
  for (int i = 0; i < thread_num; ++i) {
    thrs[i].join();
  }
  std::remove("pid.file");
  return 0;
}
