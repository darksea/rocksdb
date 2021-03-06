//  Copyright (c) 2013, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//

#pragma once
#include <algorithm>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include "rocksdb/env.h"
#include "rocksdb/status.h"

#ifdef USE_HDFS
#include "hdfs/hdfs.h"

namespace rocksdb {

static const std::string kProto = "hdfs://";
static const std::string pathsep = "/";

// Thrown during execution when there is an issue with the supplied
// arguments.
class HdfsUsageException : public std::exception { };

// A simple exception that indicates something went wrong that is not
// recoverable.  The intention is for the message to be printed (with
// nothing else) and the process terminate.
class HdfsFatalException : public std::exception {
public:
  explicit HdfsFatalException(const std::string& s) : what_(s) { }
  virtual ~HdfsFatalException() throw() { }
  virtual const char* what() const throw() {
    return what_.c_str();
  }
private:
  const std::string what_;
};

//
// The HDFS environment for rocksdb. This class overrides all the
// file/dir access methods and delegates the thread-mgmt methods to the
// default posix environment.
//
class HdfsEnv : public Env {

 public:
  HdfsEnv(const std::string& fsname) : fsname_(fsname) {
    posixEnv = Env::Default();
    fileSys_ = connectToPath(fsname_);
  }

  virtual ~HdfsEnv() {
    fprintf(stderr, "Destroying HdfsEnv::Default()\n");
    hdfsDisconnect(fileSys_);
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result);

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result);

  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result);

  virtual Status NewRandomRWFile(const std::string& fname,
                                 unique_ptr<RandomRWFile>* result,
                                 const EnvOptions& options);

  virtual bool FileExists(const std::string& fname);

  virtual Status GetChildren(const std::string& path,
                             std::vector<std::string>* result);

  virtual Status DeleteFile(const std::string& fname);

  virtual Status CreateDir(const std::string& name);

  virtual Status CreateDirIfMissing(const std::string& name);

  virtual Status DeleteDir(const std::string& name);

  virtual Status GetFileSize(const std::string& fname, uint64_t* size);

  virtual Status GetFileModificationTime(const std::string& fname,
                                         uint64_t* file_mtime);

  virtual Status RenameFile(const std::string& src, const std::string& target);

  virtual Status LockFile(const std::string& fname, FileLock** lock);

  virtual Status UnlockFile(FileLock* lock);

  virtual Status NewLogger(const std::string& fname, Logger** result);

  virtual void Schedule(void (*function)(void* arg), void* arg,
                        Priority pri = LOW) {
    posixEnv->Schedule(function, arg, pri);
  }

  virtual void StartThread(void (*function)(void* arg), void* arg) {
    posixEnv->StartThread(function, arg);
  }

  virtual Status GetTestDirectory(std::string* path) {
    return posixEnv->GetTestDirectory(path);
  }

  virtual uint64_t NowMicros() {
    return posixEnv->NowMicros();
  }

  virtual void SleepForMicroseconds(int micros) {
    posixEnv->SleepForMicroseconds(micros);
  }

  virtual Status GetHostName(char* name, uint64_t len) {
    return posixEnv->GetHostName(name, len);
  }

  virtual Status GetCurrentTime(int64_t* unix_time) {
    return posixEnv->GetCurrentTime(unix_time);
  }

  virtual Status GetAbsolutePath(const std::string& db_path,
      std::string* output_path) {
    return posixEnv->GetAbsolutePath(db_path, output_path);
  }

  virtual void SetBackgroundThreads(int number, Priority pri = LOW) {
    posixEnv->SetBackgroundThreads(number, pri);
  }

  virtual std::string TimeToString(uint64_t number) {
    return posixEnv->TimeToString(number);
  }

  static uint64_t gettid() {
    assert(sizeof(pthread_t) <= sizeof(uint64_t));
    return (uint64_t)pthread_self();
  }

 private:
  std::string fsname_;  // string of the form "hdfs://hostname:port/"
  hdfsFS fileSys_;      //  a single FileSystem object for all files
  Env*  posixEnv;       // This object is derived from Env, but not from
                        // posixEnv. We have posixnv as an encapsulated
                        // object here so that we can use posix timers,
                        // posix threads, etc.

  /**
   * If the URI is specified of the form hdfs://server:port/path,
   * then connect to the specified cluster
   * else connect to default.
   */
  hdfsFS connectToPath(const std::string& uri) {
    if (uri.empty()) {
      return NULL;
    }
    if (uri.find(kProto) != 0) {
      // uri doesn't start with hdfs:// -> use default:0, which is special
      // to libhdfs.
      return hdfsConnectNewInstance("default", 0);
    }
    const std::string hostport = uri.substr(kProto.length());

    std::vector <std::string> parts;
    split(hostport, ':', parts);
    if (parts.size() != 2) {
      throw HdfsFatalException("Bad uri for hdfs " + uri);
    }
    // parts[0] = hosts, parts[1] = port/xxx/yyy
    std::string host(parts[0]);
    std::string remaining(parts[1]);

    int rem = remaining.find(pathsep);
    std::string portStr = (rem == 0 ? remaining :
                           remaining.substr(0, rem));

    tPort port;
    port = atoi(portStr.c_str());
    if (port == 0) {
      throw HdfsFatalException("Bad host-port for hdfs " + uri);
    }
    hdfsFS fs = hdfsConnectNewInstance(host.c_str(), port);
    return fs;
  }

  void split(const std::string &s, char delim,
             std::vector<std::string> &elems) {
    elems.clear();
    size_t prev = 0;
    size_t pos = s.find(delim);
    while (pos != std::string::npos) {
      elems.push_back(s.substr(prev, pos));
      prev = pos + 1;
      pos = s.find(delim, prev);
    }
    elems.push_back(s.substr(prev, s.size()));
  }
};

}  // namespace rocksdb

#else // USE_HDFS


namespace rocksdb {

static const Status notsup;

class HdfsEnv : public Env {

 public:
  HdfsEnv(const std::string& fsname) {
    fprintf(stderr, "You have not build rocksdb with HDFS support\n");
    fprintf(stderr, "Please see hdfs/README for details\n");
    throw new std::exception();
  }

  virtual ~HdfsEnv() {
  }

  virtual Status NewSequentialFile(const std::string& fname,
                                   unique_ptr<SequentialFile>* result,
                                   const EnvOptions& options);

  virtual Status NewRandomAccessFile(const std::string& fname,
                                     unique_ptr<RandomAccessFile>* result,
                                     const EnvOptions& options) {
    return notsup;
  }

  virtual Status NewWritableFile(const std::string& fname,
                                 unique_ptr<WritableFile>* result,
                                 const EnvOptions& options) {
    return notsup;
  }

  virtual Status NewRandomRWFile(const std::string& fname,
                                 unique_ptr<RandomRWFile>* result,
                                 const EnvOptions& options) {
    return notsup;
  }

  virtual bool FileExists(const std::string& fname){return false;}

  virtual Status GetChildren(const std::string& path,
                             std::vector<std::string>* result){return notsup;}

  virtual Status DeleteFile(const std::string& fname){return notsup;}

  virtual Status CreateDir(const std::string& name){return notsup;}

  virtual Status CreateDirIfMissing(const std::string& name){return notsup;}

  virtual Status DeleteDir(const std::string& name){return notsup;}

  virtual Status GetFileSize(const std::string& fname, uint64_t* size){return notsup;}

  virtual Status GetFileModificationTime(const std::string& fname,
                                         uint64_t* time) {
    return notsup;
  }

  virtual Status RenameFile(const std::string& src, const std::string& target){return notsup;}

  virtual Status LockFile(const std::string& fname, FileLock** lock){return notsup;}

  virtual Status UnlockFile(FileLock* lock){return notsup;}

  virtual Status NewLogger(const std::string& fname,
                           shared_ptr<Logger>* result){return notsup;}

  virtual void Schedule(void (*function)(void* arg), void* arg,
                        Priority pri = LOW) {}

  virtual void StartThread(void (*function)(void* arg), void* arg) {}

  virtual Status GetTestDirectory(std::string* path) {return notsup;}

  virtual uint64_t NowMicros() {return 0;}

  virtual void SleepForMicroseconds(int micros) {}

  virtual Status GetHostName(char* name, uint64_t len) {return notsup;}

  virtual Status GetCurrentTime(int64_t* unix_time) {return notsup;}

  virtual Status GetAbsolutePath(const std::string& db_path,
      std::string* outputpath) {return notsup;}

  virtual void SetBackgroundThreads(int number, Priority pri = LOW) {}

  virtual std::string TimeToString(uint64_t number) { return "";}
};
}

#endif // USE_HDFS
