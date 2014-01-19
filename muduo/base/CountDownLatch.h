// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_COUNTDOWNLATCH_H
#define MUDUO_BASE_COUNTDOWNLATCH_H

#include <muduo/base/Condition.h>
#include <muduo/base/Mutex.h>

#include <boost/noncopyable.hpp>

namespace muduo
{
// ���������������̵߳ȴ����̷߳������� 
// Ҳ�����������̵߳ȴ��������̳߳�ʼ����ϲſ��ǹ���
class CountDownLatch : boost::noncopyable // ��MutexLock Condition�ķ�װ ������Ҫ���ʹ��
{
 public:

  explicit CountDownLatch(int count);

  void wait();

  void countDown();

  int getCount() const;

 private:
  mutable MutexLock mutex_;
  // �����mutable��ԭ����   int getCount() const; �����ܸı������Ҫ��mutable���ܸı�mutex_���ı�
  Condition condition_;
  int count_;
};

}
#endif  // MUDUO_BASE_COUNTDOWNLATCH_H
