// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include <kcpolydb.h>

#include <cstdio>
#include <cstdlib>

#include "port/port.h"
#include "util/histogram.h"
#include "util/mutexlock.h"
#include "util/random.h"
#include "util/testutil.h"

// Comma-separated list of operations to run in the specified order
//   Actual benchmarks:
//
//   fillseq       -- write N values in sequential key order in async mode
//   fillrandom    -- write N values in random key order in async mode
//   overwrite     -- overwrite N values in random key order in async mode
//   fillseqsync   -- write N/100 values in sequential key order in sync mode
//   fillrandsync  -- write N/100 values in random key order in sync mode
//   fillrand100K  -- write N/1000 100K values in random order in async mode
//   fillseq100K   -- write N/1000 100K values in seq order in async mode
//   readseq       -- read N times sequentially (readseqsmall read N/1000)
//   readrandom    -- read N times in random order (readrandsmall read N/1000)
//   open          -- cost of opening a DB
static const char* FLAGS_benchmarks =
    "fillseq,"
    "fillseqsync,"
    "fillrandsync,"
    "fillrandom,"
    "overwrite,"
    "readrandom,"
    "readseq,"
    "fillrand100K,"
    "fillseq100K,"
    "readseqsmall,"
    "readrandsmall,";

// Number of key/values to place in database
static int FLAGS_num = 1000000;

// Number of read operations to do.  If negative, do FLAGS_num reads.
static int FLAGS_reads = -1;

// Number of concurrent threads to run.
static int FLAGS_threads = 1;

// Size of each value
static int FLAGS_value_size = 100;

// Arrange to generate values that shrink to this fraction of
// their original size after compression
static double FLAGS_compression_ratio = 0.5;

// Print histogram of operation timings
static bool FLAGS_histogram = false;

// Cache size. Default 4 MB
static int FLAGS_cache_size = 4194304;

// Page size. Default 1 KB
static int FLAGS_page_size = 1024;

// If true, do not destroy the existing database.  If you set this
// flag and also specify a benchmark that wants a fresh database, that
// benchmark will fail.
static bool FLAGS_use_existing_db = false;

// Compression flag. If true, compression is on. If false, compression
// is off.
static bool FLAGS_compression = true;

// Use the db with the following name.
static const char* FLAGS_db = nullptr;

inline static void DBSynchronize(kyotocabinet::TreeDB* db_) {
  // Synchronize will flush writes to disk
  if (!db_->synchronize()) {
    std::fprintf(stderr, "synchronize error: %s\n", db_->error().name());
  }
}

namespace leveldb {

// Helper for quickly generating random data.
namespace {
class RandomGenerator {
 private:
  std::string data_;
  int pos_;

 public:
  RandomGenerator() {
    // We use a limited amount of data over and over again and ensure
    // that it is larger than the compression window (32KB), and also
    // large enough to serve all typical value sizes we want to write.
    Random rnd(301);
    std::string piece;
    while (data_.size() < 1048576) {
      // Add a short fragment that is as compressible as specified
      // by FLAGS_compression_ratio.
      test::CompressibleString(&rnd, FLAGS_compression_ratio, 100, &piece);
      data_.append(piece);
    }
    pos_ = 0;
  }

  Slice Generate(size_t len) {
    if (pos_ + len > data_.size()) {
      pos_ = 0;
      assert(len < data_.size());
    }
    pos_ += len;
    return Slice(data_.data() + pos_ - len, len);
  }
};

static Slice TrimSpace(Slice s) {
  size_t start = 0;
  while (start < s.size() && isspace(s[start])) {
    start++;
  }
  size_t limit = s.size();
  while (limit > start && isspace(s[limit - 1])) {
    limit--;
  }
  return Slice(s.data() + start, limit - start);
}

static void AppendWithSpace(std::string* str, Slice msg) {
  if (msg.empty()) return;
  if (!str->empty()) {
    str->push_back(' ');
  }
  str->append(msg.data(), msg.size());
}

class Stats {
 private:
  double start_;
  double finish_;
  double seconds_;
  int done_;
  int next_report_;
  int64_t bytes_;
  double last_op_finish_;
  Histogram hist_;
  std::string message_;

 public:
  Stats() { Start(); }

  void Start() {
    next_report_ = 100;
    hist_.Clear();
    done_ = 0;
    bytes_ = 0;
    seconds_ = 0;
    message_.clear();
    start_ = finish_ = last_op_finish_ = Env::Default()->NowMicros();
  }

  void Merge(const Stats& other) {
    hist_.Merge(other.hist_);
    done_ += other.done_;
    bytes_ += other.bytes_;
    seconds_ += other.seconds_;
    if (other.start_ < start_) start_ = other.start_;
    if (other.finish_ > finish_) finish_ = other.finish_;

    // Just keep the messages from one thread
    if (message_.empty()) message_ = other.message_;
  }

  void Stop() {
    finish_ = Env::Default()->NowMicros();
    seconds_ = (finish_ - start_) * 1e-6;
  }

  void AddMessage(Slice msg) { AppendWithSpace(&message_, msg); }

  void FinishedSingleOp() {
    if (FLAGS_histogram) {
      double now = Env::Default()->NowMicros();
      double micros = now - last_op_finish_;
      hist_.Add(micros);
      if (micros > 20000) {
        std::fprintf(stderr, "long op: %.1f micros%30s\r", micros, "");
        std::fflush(stderr);
      }
      last_op_finish_ = now;
    }

    done_++;
    if (done_ >= next_report_) {
      if (next_report_ < 1000)
        next_report_ += 100;
      else if (next_report_ < 5000)
        next_report_ += 500;
      else if (next_report_ < 10000)
        next_report_ += 1000;
      else if (next_report_ < 50000)
        next_report_ += 5000;
      else if (next_report_ < 100000)
        next_report_ += 10000;
      else if (next_report_ < 500000)
        next_report_ += 50000;
      else
        next_report_ += 100000;
      std::fprintf(stderr, "... finished %d ops%30s\r", done_, "");
      std::fflush(stderr);
    }
  }

  void AddBytes(int64_t n) { bytes_ += n; }

  void Report(const Slice& name) {
    // Pretend at least one op was done in case we are running a benchmark
    // that does not call FinishedSingleOp().
    if (done_ < 1) done_ = 1;

    std::string extra;
    if (bytes_ > 0) {
      // Rate is computed on actual elapsed time, not the sum of per-thread
      // elapsed times.
      double elapsed = (finish_ - start_) * 1e-6;
      char rate[100];
      std::snprintf(rate, sizeof(rate), "%6.1f MB/s",
                    (bytes_ / 1048576.0) / elapsed);
      extra = rate;
    }
    AppendWithSpace(&extra, message_);

    std::fprintf(stdout, "%-12s : %11.3f micros/op;%s%s\n",
                 name.ToString().c_str(), seconds_ * 1e6 / done_,
                 (extra.empty() ? "" : " "), extra.c_str());
    if (FLAGS_histogram) {
      std::fprintf(stdout, "Microseconds per op:\n%s\n",
                   hist_.ToString().c_str());
    }
    std::fflush(stdout);
  }
};

// State shared by all concurrent executions of the same benchmark.
struct SharedState {
  port::Mutex mu;
  port::CondVar cv GUARDED_BY(mu);
  int total GUARDED_BY(mu);

  // Each thread goes through the following states:
  //    (1) initializing
  //    (2) waiting for others to be initialized
  //    (3) running
  //    (4) done

  int num_initialized GUARDED_BY(mu);
  int num_done GUARDED_BY(mu);
  bool start GUARDED_BY(mu);

  SharedState(int total)
      : cv(&mu), total(total), num_initialized(0), num_done(0), start(false) {}
};

// Per-thread state for concurrent executions of the same benchmark.
struct ThreadState {
  int tid;      // 0..n-1 when running in n threads
  Random rand;  // Has different seeds for different threads
  Stats stats;
  SharedState* shared;

  ThreadState(int index, int seed) : tid(index), rand(seed), shared(nullptr) {}
};
}  // namespace

class Benchmark {
 private:
  kyotocabinet::TreeDB* db_;
  int num_;
  int value_size_;
  int reads_;
  int total_thread_count_;
  kyotocabinet::LZOCompressor<kyotocabinet::LZO::RAW> comp_;

  void PrintHeader() {
    const int kKeySize = 16;
    PrintEnvironment();
    std::fprintf(stdout, "Keys:       %d bytes each\n", kKeySize);
    std::fprintf(
        stdout, "Values:     %d bytes each (%d bytes after compression)\n",
        FLAGS_value_size,
        static_cast<int>(FLAGS_value_size * FLAGS_compression_ratio + 0.5));
    std::fprintf(stdout, "Entries:    %d\n", num_);
    std::fprintf(stdout, "RawSize:    %.1f MB (estimated)\n",
                 ((static_cast<int64_t>(kKeySize + FLAGS_value_size) * num_) /
                  1048576.0));
    std::fprintf(
        stdout, "FileSize:   %.1f MB (estimated)\n",
        (((kKeySize + FLAGS_value_size * FLAGS_compression_ratio) * num_) /
         1048576.0));
    PrintWarnings();
    std::fprintf(stdout, "------------------------------------------------\n");
  }

  void PrintWarnings() {
#if defined(__GNUC__) && !defined(__OPTIMIZE__)
    std::fprintf(
        stdout,
        "WARNING: Optimization is disabled: benchmarks unnecessarily slow\n");
#endif
#ifndef NDEBUG
    std::fprintf(
        stdout,
        "WARNING: Assertions are enabled; benchmarks unnecessarily slow\n");
#endif
  }

  void PrintEnvironment() {
    std::fprintf(
        stderr, "Kyoto Cabinet:    version %s, lib ver %d, lib rev %d\n",
        kyotocabinet::VERSION, kyotocabinet::LIBVER, kyotocabinet::LIBREV);

#if defined(__linux)
    time_t now = time(nullptr);
    std::fprintf(stderr, "Date:           %s",
                 ctime(&now));  // ctime() adds newline

    FILE* cpuinfo = std::fopen("/proc/cpuinfo", "r");
    if (cpuinfo != nullptr) {
      char line[1000];
      int num_cpus = 0;
      std::string cpu_type;
      std::string cache_size;
      while (fgets(line, sizeof(line), cpuinfo) != nullptr) {
        const char* sep = strchr(line, ':');
        if (sep == nullptr) {
          continue;
        }
        Slice key = TrimSpace(Slice(line, sep - 1 - line));
        Slice val = TrimSpace(Slice(sep + 1));
        if (key == "model name") {
          ++num_cpus;
          cpu_type = val.ToString();
        } else if (key == "cache size") {
          cache_size = val.ToString();
        }
      }
      std::fclose(cpuinfo);
      std::fprintf(stderr, "CPU:            %d * %s\n", num_cpus,
                   cpu_type.c_str());
      std::fprintf(stderr, "CPUCache:       %s\n", cache_size.c_str());
    }
#endif
  }

 public:
  Benchmark()
      : db_(nullptr),
        num_(FLAGS_num),
        value_size_(FLAGS_value_size),
        reads_(FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads),
        total_thread_count_(0) {
    if (!FLAGS_use_existing_db) {
      Env::Default()->RemoveFile(std::string(FLAGS_db));
    }
  }

  ~Benchmark() {
    if (!db_->close()) {
      std::fprintf(stderr, "close error: %s\n", db_->error().name());
    }
  }

  void Run() {
    PrintHeader();
    Open(false);

    const char* benchmarks = FLAGS_benchmarks;
    while (benchmarks != nullptr) {
      const char* sep = strchr(benchmarks, ',');
      Slice name;
      if (sep == nullptr) {
        name = benchmarks;
        benchmarks = nullptr;
      } else {
        name = Slice(benchmarks, sep - benchmarks);
        benchmarks = sep + 1;
      }

      // Reset parameters that may be overridden below
      num_ = FLAGS_num;
      reads_ = (FLAGS_reads < 0 ? FLAGS_num : FLAGS_reads);
      value_size_ = FLAGS_value_size;

      void (Benchmark::*method)(ThreadState*) = nullptr;
      bool fresh_db = false;
      bool sync = false;
      bool should_db_synchronize = false;
      int num_threads = FLAGS_threads;

      if (name == Slice("open")) {
        method = &Benchmark::OpenBench;
        num_ /= 10000;
        if (num_ < 1) num_ = 1;
      } else if (name == Slice("fillseq")) {
        fresh_db = true;
        should_db_synchronize = true;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillrandom")) {
        fresh_db = true;
        should_db_synchronize = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("overwrite")) {
        fresh_db = false;
        should_db_synchronize = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fillrandsync")) {
        fresh_db = true;
        num_ /= 100;
        sync = true;
        should_db_synchronize = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fillseqsync")) {
        fresh_db = true;
        num_ /= 100;
        sync = true;
        should_db_synchronize = true;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("fillrand100K")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        should_db_synchronize = true;
        method = &Benchmark::WriteRandom;
      } else if (name == Slice("fillseq100K")) {
        fresh_db = true;
        num_ /= 1000;
        value_size_ = 100 * 1000;
        should_db_synchronize = true;
        method = &Benchmark::WriteSeq;
      } else if (name == Slice("readseq")) {
        method = &Benchmark::ReadSequential;
      } else if (name == Slice("readseqsmall")) {
        reads_ /= 1000;
        method = &Benchmark::ReadSequential;
      } else if (name == Slice("readrandom")) {
        method = &Benchmark::ReadRandom;
      } else if (name == Slice("readrandsmall")) {
        reads_ /= 1000;
        method = &Benchmark::ReadRandom;
      } else {
        if (!name.empty()) {  // No error message for empty name
          std::fprintf(stderr, "unknown benchmark '%s'\n",
                       name.ToString().c_str());
        }
      }

      if (fresh_db) {
        if (FLAGS_use_existing_db) {
          std::fprintf(stdout, "%-12s : skipped (--use_existing_db is true)\n",
                       name.ToString().c_str());
          method = nullptr;
        } else {
          delete db_;
          db_ = nullptr;
          Open(sync);
        }
      }

      if (method != nullptr) {
        RunBenchmark(num_threads, name, method);
      }

      if (should_db_synchronize) {
        DBSynchronize(db_);
      }
    }
  }

 private:
  struct ThreadArg {
    Benchmark* bm;
    SharedState* shared;
    ThreadState* thread;
    void (Benchmark::*method)(ThreadState*);
  };

  static void ThreadBody(void* v) {
    ThreadArg* arg = reinterpret_cast<ThreadArg*>(v);
    SharedState* shared = arg->shared;
    ThreadState* thread = arg->thread;
    {
      MutexLock l(&shared->mu);
      // printf("### 2 (%p)\n", (void *)&shared->mu);
      shared->num_initialized++;
      if (shared->num_initialized >= shared->total) {
        shared->cv.SignalAll();
      }
      while (!shared->start) {
        shared->cv.Wait();
      }
    }

    thread->stats.Start();
    (arg->bm->*(arg->method))(thread);
    thread->stats.Stop();

    {
      MutexLock l(&shared->mu);
      // printf("### 3 (%p)\n", (void *)&shared->mu);
      shared->num_done++;
      if (shared->num_done >= shared->total) {
        shared->cv.SignalAll();
      }
    }
  }

  void RunBenchmark(int n, Slice name,
                    void (Benchmark::*method)(ThreadState*)) {
    SharedState shared(n);

    ThreadArg* arg = new ThreadArg[n];
    for (int i = 0; i < n; i++) {
      arg[i].bm = this;
      arg[i].method = method;
      arg[i].shared = &shared;
      ++total_thread_count_;
      // Seed the thread's random state deterministically based upon thread
      // creation across all benchmarks. This ensures that the seeds are unique
      // but reproducible when rerunning the same set of benchmarks.
      arg[i].thread = new ThreadState(i, /*seed=*/1000 + total_thread_count_);
      arg[i].thread->shared = &shared;
      Env::Default()->StartThread(ThreadBody, &arg[i]);
    }

    shared.mu.Lock();
    // printf("### 4 (%p)\n", (void *)&shared.mu);
    while (shared.num_initialized < n) {
      shared.cv.Wait();
    }

    shared.start = true;
    shared.cv.SignalAll();
    while (shared.num_done < n) {
      shared.cv.Wait();
    }
    shared.mu.Unlock();

    for (int i = 1; i < n; i++) {
      arg[0].thread->stats.Merge(arg[i].thread->stats);
    }
    arg[0].thread->stats.Report(name);

    for (int i = 0; i < n; i++) {
      delete arg[i].thread;
    }
    delete[] arg;
  }

  void Open(bool sync) {
    assert(db_ == nullptr);

    // Initialize db_
    db_ = new kyotocabinet::TreeDB();
    char file_name[100];
    std::snprintf(file_name, sizeof(file_name), FLAGS_db);

    // Create tuning options and open the database
    int open_options =
        kyotocabinet::PolyDB::OWRITER | kyotocabinet::PolyDB::OCREATE;
    int tune_options =
        kyotocabinet::TreeDB::TSMALL | kyotocabinet::TreeDB::TLINEAR;
    if (FLAGS_compression) {
      tune_options |= kyotocabinet::TreeDB::TCOMPRESS;
      db_->tune_compressor(&comp_);
    }
    db_->tune_options(tune_options);
    db_->tune_page_cache(FLAGS_cache_size);
    db_->tune_page(FLAGS_page_size);
    db_->tune_map(256LL << 20);
    if (sync) {
      open_options |= kyotocabinet::PolyDB::OAUTOSYNC;
    }
    if (!db_->open(file_name, open_options)) {
      std::fprintf(stderr, "open error: %s\n", db_->error().name());
    }
  }

  void OpenBench(ThreadState* thread) {
    for (int i = 0; i < num_; i++) {
      delete db_;
      db_ = nullptr;
      Open(false);
      thread->stats.FinishedSingleOp();
    }
  }

  void WriteSeq(ThreadState* thread) { DoWrite(thread, true); }

  void WriteRandom(ThreadState* thread) { DoWrite(thread, false); }

  void DoWrite(ThreadState* thread, bool seq) {
    if (num_ != FLAGS_num) {
      char msg[100];
      std::snprintf(msg, sizeof(msg), "(%d ops)", num_);
      thread->stats.AddMessage(msg);
    }

    RandomGenerator gen;
    int64_t bytes = 0;

    // Write to database
    for (int i = 0; i < num_; i++) {
      const int k = seq ? i : thread->rand.Uniform(num_);
      char key[100];
      std::snprintf(key, sizeof(key), "%016d", k);
      bytes += value_size_ + strlen(key);
      std::string cpp_key = key;
      if (!db_->set(cpp_key, gen.Generate(value_size_).ToString())) {
        std::fprintf(stderr, "set error: %s\n", db_->error().name());
      }
      thread->stats.FinishedSingleOp();
    }
    thread->stats.AddBytes(bytes);
  }

  void ReadSequential(ThreadState* thread) {
    int64_t bytes = 0;
    kyotocabinet::DB::Cursor* cur = db_->cursor();
    cur->jump();
    std::string ckey, cvalue;
    while (cur->get(&ckey, &cvalue, true)) {
      bytes += ckey.size() + cvalue.size();
      thread->stats.FinishedSingleOp();
    }
    delete cur;
    thread->stats.AddBytes(bytes);
  }

  void ReadRandom(ThreadState* thread) {
    std::string value;
    for (int i = 0; i < reads_; i++) {
      char key[100];
      const int k = thread->rand.Uniform(reads_);
      std::snprintf(key, sizeof(key), "%016d", k);
      db_->get(key, &value);
      thread->stats.FinishedSingleOp();
    }
  }
};

}  // namespace leveldb

int main(int argc, char** argv) {
  std::string default_db_path;
  for (int i = 1; i < argc; i++) {
    double d;
    int n;
    char junk;
    if (leveldb::Slice(argv[i]).starts_with("--benchmarks=")) {
      FLAGS_benchmarks = argv[i] + strlen("--benchmarks=");
    } else if (sscanf(argv[i], "--compression_ratio=%lf%c", &d, &junk) == 1) {
      FLAGS_compression_ratio = d;
    } else if (sscanf(argv[i], "--histogram=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_histogram = n;
    } else if (sscanf(argv[i], "--use_existing_db=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_use_existing_db = n;
    } else if (sscanf(argv[i], "--num=%d%c", &n, &junk) == 1) {
      FLAGS_num = n;
    } else if (sscanf(argv[i], "--reads=%d%c", &n, &junk) == 1) {
      FLAGS_reads = n;
    } else if (sscanf(argv[i], "--threads=%d%c", &n, &junk) == 1) {
      FLAGS_threads = n;
    } else if (sscanf(argv[i], "--value_size=%d%c", &n, &junk) == 1) {
      FLAGS_value_size = n;
    } else if (sscanf(argv[i], "--cache_size=%d%c", &n, &junk) == 1) {
      FLAGS_cache_size = n;
    } else if (sscanf(argv[i], "--page_size=%d%c", &n, &junk) == 1) {
      FLAGS_page_size = n;
    } else if (sscanf(argv[i], "--compression=%d%c", &n, &junk) == 1 &&
               (n == 0 || n == 1)) {
      FLAGS_compression = (n == 1) ? true : false;
    } else if (strncmp(argv[i], "--db=", 5) == 0) {
      FLAGS_db = argv[i] + 5;
    } else {
      std::fprintf(stderr, "Invalid flag '%s'\n", argv[i]);
      std::exit(1);
    }
  }

  // Choose a location for the test database if none given with --db=<path>
  if (FLAGS_db == nullptr) {
    leveldb::Env::Default()->GetTestDirectory(&default_db_path);
    default_db_path += "/dbbench_tree.kct";
    FLAGS_db = default_db_path.c_str();
  }

  leveldb::Benchmark benchmark;
  benchmark.Run();
  return 0;
}
