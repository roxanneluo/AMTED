#ifndef __THREAD_POOL_LIB__
#define __THREAD_POOL_LIB__

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <queue>

// pipe file descriptor
using pfd_t = int;


class ThreadPool {
 public:
  ThreadPool(int n);

  const std::vector<pfd_t>& inputPipes() const {
    return m_inputPipes;
  }

  template <typename Func>
  void addTaks(const Func& task);


 protected:
  // P is output pipes
  using TP = std::pair<pthread_t, pdf_t>;

 protected:
  std::vecor<pfd_t> m_inputPipes;
  std::queue<TP>    m_inactiveThreads;
  std::queue<Func>  m_taskQueue;

  /**
   * thread condition variable.
   */
  pthread_cond_t cond;

  /**
   * thread mutex lock.
   */
  pthread_mutex_t mutex;
};



#endif
