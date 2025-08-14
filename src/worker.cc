
#include <assert.h>
#include "worker.h"

void ParallelWorker::run() {
  while(!should_stop.load()) {
    job_doing = nullptr;
    {
      std::unique_lock<std::mutex> ul(m_mutex);
      if (jobs.empty()) {
        idle_cv.notify_one();
        jobs_cv.wait(ul, [&]() -> bool { return !jobs.empty() || should_stop.load(); });
      }
      if (jobs.empty()) {
        assert(should_stop.load());
        break;
      }
      job_doing = jobs.front();
      jobs.pop();
    }
    if (job_doing) {
      job_doing->exec();
    }
  }
  alive.store(false);
  close_cv.notify_one();
}
