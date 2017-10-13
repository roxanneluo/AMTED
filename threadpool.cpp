#include "threadpool.hpp"

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

  for(int i = 0; i < N; i++) {
    printf("creating thread %d\n", i);
    pfd_t pipes[2];
    pipe(pipes);
    m_inputPipes.push_back(pipes[kRead]);

    struct arg_struct args;
    args.pipe_out = pipes[kWrite];
    args.pool = this;
    if (pthread_create(&(m_threads[i]), NULL, worker, (void*)(&args)) != 0) {
      perror("pthread_create");
      exit(EXIT_FAILURE);
    }
    printf("created thread %d\n", i);
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
  pthread_mutex_unlock(&mutex);
  pthread_cond_signal(&cond);
}

