// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/ThreadPool.h>

#include <muduo/base/Exception.h>

#include <boost/bind.hpp>
#include <assert.h>
#include <stdio.h>

using namespace muduo;

ThreadPool::ThreadPool(const string& name)
  : mutex_(),
    cond_(mutex_),
    name_(name),
    running_(false)
{
}

ThreadPool::~ThreadPool()
{
  if (running_)
  {
    stop();
  }
}

void ThreadPool::start(int numThreads)
{
  assert(threads_.empty());
  running_ = true;
  threads_.reserve(numThreads); // must
  for (int i = 0; i < numThreads; ++i)
  {
    char id[32];
    snprintf(id, sizeof id, "%d", i);
    threads_.push_back(new muduo::Thread( // �����̶߳����Ҽ���threads_(ptr_vector������ָ���vector)
          boost::bind(&ThreadPool::runInThread, this), name_+id));// ͬʱ�������߳����к���
    threads_[i].start(); // start runInThread��������
  }
}

void ThreadPool::stop()
{
  {
  MutexLockGuard lock(mutex_);
  running_ = false;
  cond_.notifyAll();
  }
  for_each(threads_.begin(),
           threads_.end(),
           boost::bind(&muduo::Thread::join, _1)); // wait for every pthread stop
}

void ThreadPool::run(const Task& task) // �������� Task(function)
{
  if (threads_.empty())     // �̶߳���Ϊ�� û���߳����� ��ֱ�������߳�(����)����
  {
    task();
  }
  else                      // ����������ӵ��������֪ͨ���еĵȴ��߳�
  {
    MutexLockGuard lock(mutex_);
    queue_.push_back(task); 
    cond_.notify(); // ֪ͨ��task�� ����ȡ��ִ�� ʵ���̼߳��ͬ��
  }
}

ThreadPool::Task ThreadPool::take()
{
  MutexLockGuard lock(mutex_);
  // always use a while-loop, due to spurious wakeup
  while (queue_.empty() && running_) // ������ �����̳߳ؽ��� ���ٵȴ�����ѭ��
  {
    cond_.wait(); // ��task�� ��������
  }
  Task task;
  if(!queue_.empty())
  {
    task = queue_.front(); 	   // ȡ������
    queue_.pop_front();
  }
  return task;
}

void ThreadPool::runInThread() // �߳����к���
{
  try
  {
    while (running_) 		// �̳߳�������
    {
      Task task(take());    // ���������ȡ������ û�������������take����
      if (task)             // ����ǿ� ִ������
      {
        task();
      }
    }
  }
  catch (const Exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    fprintf(stderr, "stack trace: %s\n", ex.stackTrace());
    abort();
  }
  catch (const std::exception& ex)
  {
    fprintf(stderr, "exception caught in ThreadPool %s\n", name_.c_str());
    fprintf(stderr, "reason: %s\n", ex.what());
    abort();
  }
  catch (...)
  {
    fprintf(stderr, "unknown exception caught in ThreadPool %s\n", name_.c_str());
    throw; // rethrow
  }
}

