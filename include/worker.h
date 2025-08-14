#ifndef _h_worker_
#define _h_worker_

#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <stdint.h>

class ParallelJob {
public:
  virtual ~ParallelJob() {}
  virtual void exec() = 0;
};

class MemoryFreeJob : public ParallelJob {
public:
  MemoryFreeJob() : to_free(nullptr) {}
  MemoryFreeJob(ParallelJob *job) : to_free(job) {}
  void set_to_free(ParallelJob *job) { to_free = job; };
  void exec() override {
    if (to_free) delete to_free;
  }
private:
  ParallelJob *to_free;
};

class ParallelWorker {
public:
  ParallelWorker(uint32_t i) : idx(i), thr(nullptr), should_stop(false), alive(false) {}
  ~ParallelWorker() {
    if (thr) {
      delete thr;
      thr = nullptr;
    }
  }
  void start() {
    should_stop.store(false);
    alive.store(true);
    if (!thr) {
      thr = new std::thread(&ParallelWorker::run, this);
      thr->detach();
    }
  }
  void run();
  void stop() {
    should_stop.store(true);
    std::unique_lock<std::mutex> ul(m_mutex);
    jobs_cv.notify_one();
    close_cv.wait(ul, [&]() -> bool { return !alive.load();});
  }

  void add_job(ParallelJob *job) {
    std::unique_lock<std::mutex> ul(m_mutex);
    jobs.push(job);
    jobs_cv.notify_one();
  }

  void wait_idle() {
    std::unique_lock<std::mutex> ul(m_mutex);
    idle_cv.wait(ul, [&]() -> bool { return job_doing == nullptr && jobs.empty();});
  }

private:
  uint32_t idx;
  std::thread *thr;
  std::atomic_bool should_stop;
  std::atomic_bool alive;

  ParallelJob *job_doing;
  std::queue<ParallelJob *> jobs;
  std::mutex m_mutex;
  std::condition_variable jobs_cv;
  std::condition_variable idle_cv;
  std::condition_variable close_cv;
};

class ParallelWorkerPool {
public:
  ParallelWorkerPool() : alive(false), pool_size(0) {}
  ~ParallelWorkerPool() {
    for (size_t i=0; i<pool_size; ++i) {
      workers[i]->stop();
      delete workers[i];
      workers[i] = nullptr;
    }
  }
  void start(uint32_t size) {
    printf("[ start %d parallel workers ]\n", size);
    if (!alive) {
      pool_size = size;
      for (size_t i=0; i<pool_size; ++i) {
        workers.push_back(new ParallelWorker(i));
        workers[i]->start();
      }
      alive = true;
    }
  }
  void wait_all_idle() {
    if (!alive) return;
    for (ParallelWorker *worker : workers) {
      worker->wait_idle();
    }
  }
  void add_job(ParallelJob *job, uint32_t idx) {
    if (!alive) return;
    workers[idx % pool_size]->add_job(job);
  }
private:
  bool alive;
  uint32_t pool_size;
  std::vector<ParallelWorker *> workers;
};

#endif
