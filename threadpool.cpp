#include "threadpool.hpp"

#include <assert>

ThreadPool::ThreadPool(int n) {
  // TODO: create n threads and n pair of input and output pipes
  // put input pipes into m_inputPipes and output pipes and corresponding
  // threads into m_inactiveThreads;
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
