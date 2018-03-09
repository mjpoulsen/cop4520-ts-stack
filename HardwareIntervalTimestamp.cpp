// Reference:
//		https://github.com/cksystemsgroup/scal/blob/master/src/datastructures/ts_timestamp.h
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <atomic>
#include <sys/time.h>
#include <cstdlib>
#include <iostream>

using namespace std;

class HardwareIntervalTimestamp {
private:

    // Length of the interval.
    uint64_t delay_;

    inline uint64_t get_hwtime(void) {
  		unsigned int hi, lo;
  		__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  		return ((uint64_t) lo) | (((uint64_t) hi) << 32);
	}

	inline uint64_t get_hwptime(void) {
    	uint64_t aux;
    	uint64_t rax,rdx;
    	asm volatile ( "rdtscp\n" : "=a" (rax), "=d" (rdx), "=c" (aux) : : );
    	return (rdx << 32) + rax;
    }

public:
  
    inline void initialize(uint64_t delay) {
      delay_ = delay;
    }

    inline void init_sentinel_atomic(std::atomic<uint64_t> *result) {
      result[0].store(0);
      result[1].store(0);
    }

    inline void init_top_atomic(std::atomic<uint64_t> *result) {
      result[0].store(UINT64_MAX);
      result[1].store(UINT64_MAX);
    }

    inline void load_timestamp(uint64_t *result, std::atomic<uint64_t> *source) {
      result[0] = source[0].load();
      result[1] = source[1].load();
    }

    // Acquires a new timestamp and stores it in result.
    inline void set_timestamp(std::atomic<uint64_t> *result) {
    	// Set the first timestamp.
    	result[0].store(get_hwptime());
    	// Wait for delay_ time.
    	uint64_t wait = get_hwtime() + delay_;

    	while (get_hwtime() < wait) {}

    	// Set the second timestamp.
    	result[1].store(get_hwptime());
    }

    inline void read_time(uint64_t *result) {
      result[0] = get_hwptime();
      result[1] = result[0];
    }

    // Compares two timestamps, returns true if timestamp1 is later than
    // timestamp2.
    inline bool is_later(uint64_t *timestamp1, uint64_t *timestamp2) {
      return timestamp2[1] < timestamp1[0];
    }

};

int main() {
	std::cout << "Hello, World." << endl;


	uint64_t result[2];

	HardwareIntervalTimestamp h;
	h.initialize(100);
	h.set_timestamp(&result);
	



	

	return 0;
}