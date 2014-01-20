// Use of this source code is governed by a BSD-style license
// that can be found in the License file.
//
// Author: Shuo Chen (chenshuo at chenshuo dot com)

#ifndef MUDUO_BASE_SINGLETON_H
#define MUDUO_BASE_SINGLETON_H

#include <boost/noncopyable.hpp>
#include <pthread.h>
#include <stdlib.h> // atexit

namespace muduo
{

template<typename T>
class Singleton : boost::noncopyable
{
 public:
  static T& instance()
  {
    pthread_once(&ponce_, &Singleton::init); // ��֤��Singleton::init()����ֻ�����һ�� ��֤���̰߳�ȫ
    return *value_;
  }

 private:
  Singleton();
  ~Singleton();

  static void init()
  {
    value_ = new T();
    ::atexit(destroy); // ע��һ�����ٺ���
  }

  static void destroy()
  {
	// ������ɱ�֤��������ȫ C���Բ�������-1 ��С������ ����: class name;����һ��������������
    typedef char T_must_be_complete_type[sizeof(T) == 0 ? -1 : 1];
    delete value_;
  }

 private:
  static pthread_once_t ponce_;
  static T*             value_; // ��̬�ı�֤���ڴ���ֻ��һ�� ��������
};

template<typename T>
pthread_once_t Singleton<T>::ponce_ = PTHREAD_ONCE_INIT; // must init static
// �����Ǿ�̬��ȫ�������� ponce_ ��ҪPTHREAD_ONCE_INIT����ʼ�� ��Ա����Ҫ��init()��������ʼ��

template<typename T>
T* Singleton<T>::value_ = NULL;			// must init static

}
#endif

