#ifndef MUDUO_BASE_LOGFILE_H
#define MUDUO_BASE_LOGFILE_H

#include <muduo/base/Mutex.h>
#include <muduo/base/Types.h>

#include <boost/noncopyable.hpp>
#include <boost/scoped_ptr.hpp>

namespace muduo
{

class LogFile : boost::noncopyable
{
 public:
  LogFile(const string& basename,
          size_t rollSize,
          bool threadSafe = true,
          int flushInterval = 3);
  ~LogFile();

  void append(const char* logline, int len); // �����־
  void flush(); // д���ļ�

 private:
  void append_unlocked(const char* logline, int len);

  static string getLogFileName(const string& basename, time_t* now);
  void rollFile(); // �����ļ�

  const string basename_; 						// basename(filename)
  const size_t rollSize_; 						// �ﵽ�����Сʱ�����µ��ļ�
  const int flushInterval_;						// ��־д����ʱ��

  int count_;

  boost::scoped_ptr<MutexLock> mutex_;  
  
  time_t startOfPeriod_;  // ��ʼ��¼��־ʱ��(����������ʱ��) ��һ�����1971.9.1��ʱ����һ����
  time_t lastRoll_;      						// ��һ�ι�����־�ļ�ʱ��
  time_t lastFlush_;	 						// ��һ��д����־�ļ�ʱ��
  class File;
  boost::scoped_ptr<File> file_; // ����File

  const static int kCheckTimeRoll_ = 1024;      // ���count_��������С�ﵽ���ֵ�͹���
  const static int kRollPerSeconds_ = 60*60*24; // ÿһ����� �µ�һ�� ÿ���������
};

}
#endif  // MUDUO_BASE_LOGFILE_H
