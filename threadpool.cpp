#include "threadpool.hpp"

#include <assert>

ThreadPool::ThreadPool(int n) {
  // TODO: create n threads and n pair of input and output pipes
  // put input pipes into m_inputPipes and output pipes and corresponding
  // threads into m_inactiveThreads;
  pthread_cond_init(&cond, NULL);
  pthread_mutex_init(&mutex, NULL);

  int pipes[n][2];
  int thread_tid[n];
  for(i = 0; i < n; i++) {
      pipe(pipes[i])
      int tid;
      if (pthread_create(&tid, NULL, worker, (void*)(pipes[i])) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
      }
      m_inputPipes.push_back(pipes[i][0]);
      m_inactiveThreads.push_back({tid, pipes[i][1]});
  }
}


void * worker(void* arg) {
  int* pipe = (int*) arg;

  while (1) {
    pthread_mutex_lock(&mutex);        

    while (/*task queue non empty*/) {
        pthread_cond_wait(&cond, &mutex);
    }

    task = /* task queue.pop */
    
    pthread_mutex_unlock(&mutex);

    /* do work */
  }

          
}


template <typename Func>
void addTaks(const Func& task) {
  // FIXME easy version here
  // TODO
  // lock
  assert(!m_inactiveThreads.empty());
  TP& tp = m_inactiveThreads.front();
  m_inactiveThreads.pop();
  // unlock
  // TODO: create a function that 
  // [] () {
  //  task(tp.second);
  //  lock
  //  m_inactiveThreads.push(tp);
  //  unlock
  // }
  // create a thread calling the above function
}
