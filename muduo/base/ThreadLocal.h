// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_THREADLOCAL_H
#define MUDUO_BASE_THREADLOCAL_H

#include <boost/noncopyable.hpp>
#include <pthread.h>

namespace muduo
{

template<typename T>
class ThreadLocal : boost::noncopyable
{
 public:
  ThreadLocal()
  {
    pthread_key_create(&pkey_, &ThreadLocal::destructor); // ע��һ�����ٺ��� �����߳��ض�����
  }

  ~ThreadLocal()
  {
    pthread_key_delete(pkey_); // �������߳�key ����ɾ���߳��ض�����
  }
  T& value()
  {
    T* perThreadValue = static_cast<T*>(pthread_getspecific(pkey_));
    if (!perThreadValue) {
      T* newObj = new T();
      pthread_setspecific(pkey_, newObj);  // �������newobj �������Ǿֲ��洢��
      perThreadValue = newObj;
    }
    return *perThreadValue; // ȡֵ����
  }

 private:

  static void destructor(void *x)// �����߳��ض�����
  {
    T* obj = static_cast<T*>(x);
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1]; // ������ȫ�ļ��
    delete obj;
  }

 private:
  pthread_key_t pkey_; // �ֲ��洢��key
};

}
#endif
