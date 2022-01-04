vdsotest
========

vdsotest is a utility for testing and benchmarking a Linux VDSO.  C
library support for the VDSO is not necessary to use vdsotest.

Building
--------
$ ./autogen.sh && ./configure && make

Usage
-------
Usage: vdsotest [OPTION...] API TEST-TYPE
  or:  vdsotest [OPTION...] list-apis
  or:  vdsotest [OPTION...] list-test-types
where API must be one of:
        clock-gettime-monotonic
        clock-getres-monotonic
        clock-gettime-monotonic-coarse
        clock-getres-monotonic-coarse
        clock-gettime-realtime
        clock-getres-realtime
        clock-gettime-realtime-coarse
        clock-getres-realtime-coarse
        getcpu
        gettimeofday
and TEST-TYPE must be one of:
        verify
        bench
        abi

  -d, --duration=SEC         Duration of test run in seconds (default 1)
  -f, --maxfails=NUM         Maximum number of failures before terminating test
                             run (default 10)
  -g, --debug                Enable debug output which may perturb bench
                             results; implies --verbose
  -v, --verbose              Enable verbose output
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version

Examples
--------
Run all tests.  vdsotest-all is a simple script included in the
distribution that runs all API and test combinations:
$ vdsotest-all

Verify that the VDSO gettimeofday returns results consistent with the
system call gettimeofday over a period of 3 seconds:
$ vdsotest -d 3 gettimeofday verify

Check that the VDSO gettimeofday does not unnecessarily deviate from
the system call's ABI (an exception would be a case where the system call
would return EFAULT instead of dereferencing a bad pointer, the VDSO
is allowed to crash the process):
$ vdsotest gettimeofday abi

Note that the ABI tests will produce several dozen processes that are
expected to crash.  You may want to disable kernel reporting of fatal
signals, exception traces, etc.

Benchmark the VDSO gettimeofday against the system call over a period
of 10 seconds:
$ vdsotest -d 10 gettimeofday bench
gettimeofday system calls per second: 12071347
gettimeofday vdso calls per second:   46331157 (3.84x speedup)
