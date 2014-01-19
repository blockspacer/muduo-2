// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#include <muduo/base/CountDownLatch.h>

using namespace muduo;

CountDownLatch::CountDownLatch(int count)
  : mutex_(),
    condition_(mutex_),
    count_(count)
{
}

void CountDownLatch::wait() // ��count_>0��ʾ�����߳�û���������wait�������� �����ȴ���ȫ�����������
{
  MutexLockGuard lock(mutex_);
  while (count_ > 0) {
    condition_.wait();
  }
}

void CountDownLatch::countDown() // ÿ��һ���߳�������� ����һ�� ������һ ֪�����еĶ�������
{
  MutexLockGuard lock(mutex_);
  --count_;
  if (count_ == 0) {
    condition_.notifyAll(); // �߳�ȫ�������� ֪ͨwait�������õȴ��� ������������
  }
}

int CountDownLatch::getCount() const // ���صȴ�������
{
  MutexLockGuard lock(mutex_); // ��������unlock�ı�mutex_��״̬ ����Ƕ�mutex_��RAII��װ
  return count_;
}

