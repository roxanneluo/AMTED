#ifndef __THREAD_POOL_LIB__
#define __THREAD_POOL_LIB__

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <array>
#include <queue>
#include <functional>
#include <memory>

// pipe file descriptor
using pfd_t = int;
using job_t = std::function<void(pfd_t)>; 


void* worker(void * argument);

class ThreadPool {
 public:
  ThreadPool(int N);
  ~ThreadPool();

  const std::vector<pfd_t>& inputPipes() const {
    return m_inputPipes;
  }

  void addTask(const job_t& task);

  friend void* worker(void * argument);

 protected:
  std::vector<pfd_t> m_inputPipes;
  std::vector<pthread_t> m_threads;
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

struct arg_struct {
  pfd_t pipe_out;
  ThreadPool* pool;
};

void* worker(void * argument) {
  struct arg_struct *args = (struct arg_struct *)argument;
  pfd_t pipe_out = args->pipe_out;
  ThreadPool* thread_pool = args->pool;
  while (1) {
    pthread_mutex_lock(&(thread_pool->mutex));        

    while (thread_pool->m_taskQueue.empty()) {
        pthread_cond_wait(&(thread_pool->cond),
                          &(thread_pool->mutex));
    }


    job_t& task = thread_pool->m_taskQueue.front();
    thread_pool->m_taskQueue.pop();
    
    pthread_mutex_unlock(&(thread_pool->mutex));

    /* do work */
    task(pipe_out);
  }
}

ThreadPool::ThreadPool(int N)
  : m_threads(N) {
  constexpr int kRead = 0, kWrite = 1;
  
  pthread_cond_init(&cond, NULL);
  pthread_mutex_init(&mutex, NULL);

  static auto args_arr = std::make_unique<arg_struct[]>(N); 

  for (int i = 0; i < N; i++) {
    pfd_t pipes[2];
    if (pipe(pipes)) {
      perror("error creating pipes");
      exit(-1);
    }

    m_inputPipes.push_back(pipes[kRead]);

    auto& args = args_arr[i];
    args.pipe_out = pipes[kWrite];
    args.pool = this;
    if (pthread_create(&(m_threads[i]), NULL, worker, (void*)(&args)) != 0) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
  }
}

ThreadPool::~ThreadPool() {
  for (int i = 0; i < m_threads.size(); ++i) {
    pthread_join(m_threads[i], NULL);
  }
  pthread_mutex_destroy(&mutex);
  pthread_cond_destroy(&cond);
}

void ThreadPool::addTask(const job_t& job) {
  pthread_mutex_lock(&mutex);
  m_taskQueue.push(job);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
}


#endif
