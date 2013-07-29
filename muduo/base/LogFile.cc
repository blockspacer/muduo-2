#include <muduo/base/LogFile.h>
#include <muduo/base/Logging.h> // strerror_tl
#include <muduo/base/ProcessInfo.h>

#include <assert.h>
#include <stdio.h>
#include <time.h>

using namespace muduo;

// not thread safe
class LogFile::File : boost::noncopyable       		// �����class File��һ��Ƕ����
{
 public:
  explicit File(const string& filename)        		// �����ļ��� ���ļ�
    : fp_(::fopen(filename.data(), "ae")),
      writtenBytes_(0)
  {
    assert(fp_);
    ::setbuffer(fp_, buffer_, sizeof buffer_); 		// �����ļ������� ������Ҫд���ļ�������
    // posix_fadvise POSIX_FADV_DONTNEED ?
  }

  ~File()
  {
    ::fclose(fp_);                             		// �ر��ļ�
  }

  void append(const char* logline, const size_t len)// �����־ д���ļ�
  {
    size_t n = write(logline, len);
    size_t remain = len - n;
    while (remain > 0)
    {
      size_t x = write(logline + n, remain);
      if (x == 0)
      {
        int err = ferror(fp_);
        if (err)
        {
          fprintf(stderr, "LogFile::File::append() failed %s\n", strerror_tl(err));
        }
        break;
      }
      n += x; 			// �Ѿ�д��ĸ���
      remain = len - n; // remain -= x  ʣ��ĸ���
    }

    writtenBytes_ += len;
  }

  void flush()
  {
    ::fflush(fp_);
  }

  size_t writtenBytes() const { return writtenBytes_; }

 private:

  size_t write(const char* logline, size_t len)     // д����־���ݵ��ļ�
  {
#undef fwrite_unlocked
    return ::fwrite_unlocked(logline, 1, len, fp_);
  }

  FILE* fp_;
  char buffer_[64*1024]; // 64K �������������������Զ�flush���ļ���
  size_t writtenBytes_;
};

LogFile::LogFile(const string& basename,
                 size_t rollSize,
                 bool threadSafe,
                 int flushInterval)
  : basename_(basename),
    rollSize_(rollSize),
    flushInterval_(flushInterval),
    count_(0),
    mutex_(threadSafe ? new MutexLock : NULL),// ����̰߳�ȫ����Ҫ����һ��MUtex����
    startOfPeriod_(0),
    lastRoll_(0),
    lastFlush_(0)
{
  assert(basename.find('/') == string::npos);// basename������/
  rollFile();
}

LogFile::~LogFile()
{
}

void LogFile::append(const char* logline, int len)
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    append_unlocked(logline, len);
  }
  else
  {
    append_unlocked(logline, len);
  }
}

void LogFile::flush()
{
  if (mutex_)
  {
    MutexLockGuard lock(*mutex_);
    file_->flush();
  }
  else
  {
    file_->flush();
  }
}

void LogFile::append_unlocked(const char* logline, int len) // ���ĺ��� ���������־
{
  file_->append(logline, len);			 // ��������־д�뵱ǰ��־�ļ�

  if (file_->writtenBytes() > rollSize_) // ������С �����ļ�
  {
    rollFile();
  }
  else // û�г����ж��Ƿ�ʱ��������Ƿ��ǵڶ������������ �����ļ�
  {
    if (count_ > kCheckTimeRoll_) // ����ļ�¼�Ƿ񳬹�1024 ���������ǵڶ��������־�ļ�
    {
      count_ = 0;
      time_t now = ::time(NULL);
      time_t thisPeriod_ = now / kRollPerSeconds_ * kRollPerSeconds_;
      if (thisPeriod_ != startOfPeriod_) // �뿪ʼ��ʱ��������ͬ˵���ǵڶ��������־�ļ�
      {
        rollFile();
      }
      else if (now - lastFlush_ > flushInterval_) // �����ϴ�д���ʱ���ʱ�����Ƚ�
      {
        lastFlush_ = now;
        file_->flush(); // д��
      }
    }
    else
    {
      ++count_;
    }
  }
}

void LogFile::rollFile()
{
  time_t now = 0;
  string filename = getLogFileName(basename_, &now); // ��ȡ�ļ����ƺ�ʱ��
  time_t start = now / kRollPerSeconds_ * kRollPerSeconds_; 

  // ����kRollPerSeconds_��������������������õ�ʱ��

  if (now > lastRoll_)     // �����ϴι����ļ�¼
  {
    lastRoll_ = now;
    lastFlush_ = now;
    startOfPeriod_ = start;
    file_.reset(new File(filename));
  }
}

string LogFile::getLogFileName(const string& basename, time_t* now)
{
  string filename;
  filename.reserve(basename.size() + 64); // ��־�ļ����ĳ���
  filename = basename;

  char timebuf[32];
  char pidbuf[32];
  struct tm tm;
  *now = time(NULL);
  gmtime_r(now, &tm); // FIXME: localtime_r ? GMT(UTC)ʱ�����1970�������
  // gmtime�����̰߳�ȫ�� tm ���󱣴��˷��ص�ʱ��
  
  strftime(timebuf, sizeof timebuf, ".%Y%m%d-%H%M%S.", &tm); // ��ʱ���ʽ��
  filename += timebuf;
  filename += ProcessInfo::hostname();
  snprintf(pidbuf, sizeof pidbuf, ".%d", ProcessInfo::pid());
  filename += pidbuf; // ���̺�
  filename += ".log"; // �ټ���.log

  return filename;
}

