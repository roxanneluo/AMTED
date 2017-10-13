#ifndef __THREAD_POOL_LIB__
#define __THREAD_POOL_LIB__

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <queue>
#include <functional>

// pipe file descriptor
using pfd_t = int;
using job_t = std::function<void(pfd_t)>; 

template <unsigned int ThreadCount>
class ThreadPool {
 public:
  ThreadPool();
  ~ThreadPool();

  const std::array<pfd_t, ThreadCount>& inputPipes() const {
    return m_inputPipes;
  }

  void addTask(const job_t& task);

 protected:
  void * worker(pfd_t pipe_out);

 protected:
  std::array<pfd_t, ThreadCount> m_inputPipes;
  std::array<pthread_t, ThreadCount> m_threads;
  std::queue<job_t>  m_taskQueue;

  /**
   * thread condition variable.
   */
  pthread_cond_t cond;

  /**
   * thread mutex lock.
   */
  pthread_mutex_t mutex;
};


#include <cassert>
#include <fcntl.h>
#include <unistd.h>

template <unsigned int N>
void * ThreadPool<N>::worker(pfd_t pipe_out) {
  while (1) {
    pthread_mutex_lock(&mutex);        

    while (m_taskQueue.empty()) {
        pthread_cond_wait(&cond, &mutex);
    }

    job_t& task = m_taskQueue.front();
    m_taskQueue.pop();
    
    pthread_mutex_unlock(&mutex);

    /* do work */
    task(pipe_out);
  }
}

template <unsigned int N>
ThreadPool<N>::ThreadPool() {
  constexpr int kRead = 0, kWrite = 1;

  pthread_cond_init(&cond, NULL);
  pthread_mutex_init(&mutex, NULL);

  auto func = [this](pfd_t pipe_out) { worker(pipe_out); };
  for(int i = 0; i < N; i++) {
    pfd_t pipes[2];
    pipe(pipes);
    m_inputPipes[i] = pipes[kRead];

    if (pthread_create(&(m_threads[i]), NULL, (void* (*)(void*))&func, (void*)(pipes[kWrite])) != 0) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
  }
}

template <unsigned int N>
ThreadPool<N>::~ThreadPool() {
  for (int i = 0; i < N; ++i) {
    pthread_join(m_threads[i], NULL);
  }
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
}

template <unsigned int N>
void ThreadPool<N>::addTask(const job_t& job) {
  pthread_mutex_lock(&mutex);
  m_taskQueue.push(job);
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&cond);
}


#endif
