#include <iostream>
#include <atomic>
#include <list>
#include <thread>
#include <iterator>
#include <algorithm>
#include <tuple>
#include <limits.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <pthread.h>

using namespace std;

template <typename T>
struct threadParams;

const int EMPTY_ITEM = INT_MIN;
const int INVALID_ITEM = EMPTY_ITEM + 1;

const long NEW_ITEM_TIME = LONG_MAX;

template<typename T>
struct TimestampedItem {
	long timestamp;
	TimestampedItem *next;
	atomic<bool> taken;
	int abaCounter;
	T item;
};

template<typename T>
class SPBuffer {
public:
	atomic<TimestampedItem<T>*> atomic_top;
	atomic<int> size;

	TimestampedItem<T> *sentinel;

	SPBuffer<T>(T sentinel_item) {
		sentinel = (TimestampedItem<T>*)malloc(sizeof(TimestampedItem<T>));
		sentinel->next = NULL;
		sentinel->timestamp = LONG_MIN;
		sentinel->taken = false;
		sentinel->item = sentinel_item;
		atomic_top.store(sentinel, std::memory_order_relaxed);
		size.store(0, std::memory_order_relaxed);	
	}

	TimestampedItem<T>* createNode(T _item, bool _taken, long _timestamp) {
		TimestampedItem<T>* newNode;
		newNode = (TimestampedItem<T>*)malloc(sizeof(TimestampedItem<T>));
		newNode->item = _item;
		newNode->taken.store(_taken, std::memory_order_relaxed);
		newNode->timestamp = _timestamp;

		return newNode;
	}
	
	TimestampedItem<T>* ins(T item) {
		TimestampedItem<T>* newNode = createNode(item, false, NEW_ITEM_TIME);
		TimestampedItem<T>* topMost = atomic_top.load(std::memory_order_relaxed);

		while (topMost->next != NULL && topMost->taken.load(std::memory_order_relaxed)) {
			topMost = topMost->next;
		}

		newNode->next = topMost;

		if (atomic_top.compare_exchange_weak(topMost, newNode)) {
			size.fetch_add(1, std::memory_order_relaxed);
		}

		return newNode;
	}

	bool tryRemSP(TimestampedItem<T>* oldTop) {
		return tryRemSP(oldTop, oldTop->next);
	}

	bool tryRemSP(TimestampedItem<T>* oldTop, TimestampedItem<T>* node)	{
		if (atomic_top.compare_exchange_weak(oldTop, node)) {
			size.fetch_sub(1, std::memory_order_relaxed);
			return true;
		} else {
			return false;
		}
	}

	void printTop() {
		cout << atomic_top.load(std::memory_order_relaxed)->item << endl;
	}
	
	void printRemove() {
		TimestampedItem<T>* oldTop = atomic_top.load(std::memory_order_relaxed);
		tryRemSP(oldTop, oldTop->next);
		printTop();
	}

	void printStack() {
		TimestampedItem<T> *n = atomic_top.load(std::memory_order_relaxed);

		while(n != NULL) {
			if (n->next != NULL) {
				cout << "Item: " << n->item << ", ts: " << n->timestamp << ", next: " << n->next->item << endl;
			} else {
				cout << "Item: " << n->item << ", ts: " << n->timestamp << ", next: NULL" << endl;
			}
			n = n->next;
		}
	}
};

template<typename T>
class TS_Buffer {
private:
	// TS-atomic counter.
	atomic<long> timestampCounter;

	// Number of threads TS_Stack will support.
	int NUM_THREADS;

	// List of SPBuffers.
	vector <SPBuffer<T> *> spBuffers;

	// Sentinel item, determines bottom (unattainable item) of stack.
	T SENTINEL_ITEM;
		
public:
	// Constants. Don't change.
	TimestampedItem<T> emptyItem;
	TimestampedItem<T> invalidItem;

	// TS_Buffer constructor.
	// Must provide number of threads (value is 1:1 ratio of threads and SPBuffers)
	// and sentinel_item -- varies based on typename datatype.
	TS_Buffer(int num_threads, T sentinel_item) {
		// See the random generator.
		srand(time(NULL));
		// Set number of threads.
		NUM_THREADS = num_threads;
		// Set sentinel item.
		SENTINEL_ITEM = sentinel_item;
		// Initialize TS-atomic counter.
		timestampCounter.store(1, memory_order_relaxed);
		// Initialize TimestmapedItems.
		emptyItem.item = EMPTY_ITEM;
		invalidItem.item = INVALID_ITEM;

		for (int i = 0; i < NUM_THREADS; i++) {
			spBuffers.push_back(new SPBuffer<T>(SENTINEL_ITEM));
		}
	}

	// Prints the each SPBuffer stack.
	void printBuffers() {
		for (int i = 0; i < NUM_THREADS; i++) {
			std::cout << "Printing " << i << "'s stack:" << endl;
			std::cout << "============\n";
			spBuffers[i]->printStack();
			std::cout << "============\n" << endl;
		}
	}

	// Inserts an element into a SPBuffer and returns a TimestampedItem object.
	TimestampedItem<T>* ins(T element, int threadId) {
		TimestampedItem<T> *item = threadSPBuffer(threadId)->ins(element);
		return item;
	}

	// TODO change so thread owns its SPBuffer rather than generate a random value.
	// Return a reference to a SPBuffer.
	SPBuffer<T>* threadSPBuffer(int threadId) {
		return spBuffers[threadId];
	}

	// Returns the latest start time.
	long getStart() {
		if (timestampCounter.load(std::memory_order_relaxed) >= 1) {
			return timestampCounter.fetch_add(1, std::memory_order_relaxed);
		} else {
			return timestampCounter.fetch_sub(1, std::memory_order_relaxed);
		}
	}

	// Generates the newest time.
	long newTimestamp() {
		return timestampCounter.fetch_add(1, std::memory_order_relaxed);
	}

	// Sets a TimestampedItem's timestamp value -- item should not have a timestamp value prior to invocation.
	void setTimestamp(TimestampedItem<T> *item , long t) {
		item->timestamp = t;
	}

	// Try to remove an item with a timestamp greater than the startTime param.
	// If successful, the TimestampedItem will be removed from the stack, otherwise
	TimestampedItem<T>* tryRem(long startTime) {
		TimestampedItem<T>* youngest = NULL;
		SPBuffer<T>* buf = NULL;
		TimestampedItem<T>* item = NULL;
		atomic<int> i;
		i.store(0, std::memory_order_relaxed);

		for (auto &spBuffer : spBuffers) {
			item = spBuffer->atomic_top.load(std::memory_order_relaxed);

			if (item->item == SENTINEL_ITEM) {
				i.fetch_add(1, std::memory_order_relaxed);
				continue;
			}

			// Eliminate item if possible.
			if (item->timestamp >= startTime) {
				if (spBuffer->tryRemSP(item)) {
					return item;
				}
			}

			if (youngest == NULL || item->timestamp > youngest->timestamp) {
				youngest = item;
				buf = spBuffer;
			}
		}

		if (i == spBuffers.size()) {
			return &emptyItem;
		}

		if (youngest != NULL && buf->tryRemSP(youngest)) {
			return youngest;
		}

		return &invalidItem;
	}

};

template<typename T>
class TS_Stack {
public:

	TS_Buffer<T> *buffer;
	int _num_threads;

	TS_Stack<T>(int num_threads, T sentinel_flag) {
		buffer = new TS_Buffer<T>(num_threads, sentinel_flag);
		_num_threads = num_threads;
	}

	static void *thread_push(void* args) {
		// std::cout << "Pushing\n" << std::flush;
		threadParams<T> *a = (threadParams<T> *) args;
		int e = rand() % 50;
		((TS_Stack *)a->context)->push(e, a->threadId);
		return NULL;
	}

	void push(T element, int threadId) {
		TimestampedItem<T>* item = buffer->ins(element, threadId);
		long ts = buffer->newTimestamp();
		buffer->setTimestamp(item, ts);
	}

	static void *thread_pop(void* args) {
		// std::cout << "Popping\n" << std::flush;
		threadParams<T> *a = (threadParams<T> *) args;
		T e = ((TS_Stack *)a->context)->pop();
		// std::cout << "thread_pop: " << e << "\n" << std::flush;
		return NULL;
	}

	T pop() {
		long ts = buffer->getStart();
		TimestampedItem<T>* item;
		do {
			item = buffer->tryRem(ts);
		} while (item->item == INVALID_ITEM);

		if (item->item == EMPTY_ITEM) {
			return EMPTY_ITEM;
		} else {
			return item->item;
		}
	}

	static void *doWork(void *args) {

		threadParams<T> *a = (threadParams<T> *) args;
		TS_Stack<T>* the_stack = (TS_Stack<T> *) a->context;
		int threadId = a->threadId;
		int op;
		int totalPop = 0, totalPush = 0;
		double maxPop = a->maxPop, maxPush = a->maxPush;
		int num;

		while (true) {
			op = rand() % 2;

			if (op == 0 && totalPush < maxPush) {
				num = rand() % 100;
				the_stack->push(num, threadId);
				// std::cout << "Thread: " << threadId << " pushing...\n" << std::flush;
				totalPush++;
			} else if (op == 1 && totalPop < maxPop) {
				// std::cout << "Thread: " << threadId << " popping...\n" << std::flush;
				T e = the_stack->pop();
				totalPop++;
			}

			if (totalPop >= maxPop && totalPush >= maxPush) {
				break;
			}
		}

		return NULL;
	}

	void printBuffers() {
		buffer->printBuffers();
	}
};

template <typename T>
struct threadParams {
	T item;
	TS_Stack<T> *context;
	int threadId;
	double maxPop;
	double maxPush;
};

template <typename T>
static void *thread_push(void* args) {
	// std::cout << "Pushing\n" << std::flush;
	threadParams<T> *a = (threadParams<T> *) args;
	int e = rand() % 50;
	((TS_Stack<T> *)a->context)->push(e, a->threadId);
	return NULL;
}

template <typename T>
static void *thread_pop(void* args) {
	// std::cout << "Popping\n" << std::flush;
	threadParams<T> *a = (threadParams<T> *) args;
	T e = ((TS_Stack<T> *)a->context)->pop();
	// std::cout << "thread_pop: " << e << "\n" << std::flush;
	return NULL;
}

void start(int operations, double pushRatio, double popRatio, int _num_threads) {
	TS_Stack<int> the_Stack(_num_threads, INT_MIN);
	pthread_t threads[_num_threads];
	pthread_attr_t attr;
	double totalPush = 0.0, totalPop = 0.0;
	double maxPush = (double) operations * pushRatio;
	double maxPop = (double) operations * popRatio;

	/* Initialize and set thread detached attribute */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

	for (int i = 0; i < _num_threads; i++) {
		threads[i] = (pthread_t) malloc(sizeof(pthread_t));
	}

	vector <threadParams<int> > args;

	for (int i = 0; i < _num_threads; i++) {
		threadParams<int> a;
		args.push_back(a);
	}

	int c = 0;
	int rc = -1;

	std::clock_t start;
	double duration;

	start = std::clock();
	for (int i = 0; i < _num_threads; i++) {
		args[i].context = &the_Stack;
		args[i].threadId = i;
		args[i].maxPop = maxPop / _num_threads;
		args[i].maxPush = maxPush / _num_threads;
		rc = pthread_create(&threads[i], NULL, &TS_Stack<int>::doWork, &args[i]);
		if (rc != 0) {
			std::cout << "Thread was not created " << i << std::flush;
			exit(-1);
		}
	}

	// std::cout << "Waiting for threads...\n" << std::flush;

	/* Free attribute and wait for the other threads */
    pthread_attr_destroy(&attr);

	// Wait for threads to finish.
	for (int i = 0; i < _num_threads; i++) {
		pthread_join(threads[i], NULL);
	}

	duration = (std::clock() - start) / (double) CLOCKS_PER_SEC;
	std::cout << duration << "\n" << std::flush;

	// Clean up memory.
	for (int i = 0; i < _num_threads; i++) {
		pthread_detach(threads[i]);
	}

	// the_Stack.printBuffers();
}


int main(int argc, char *argv[]) {
    double popRatio = 0.25, pushRatio = 0.75;
    int operations = 1000000, threads = 8;

    if (argc != 5) {
        // threads = atoi(argv[0]);
        // operations = atoi(argv[1]);
        // pushRatio = atof(argv[2]);
        // popRatio = atof(argv[3]);
    } else {
        std::cout << "Incorrect argument count. Default values choosen:\n" << std::endl;
        std::cout << "Number of threads 4\n";
        std::cout << "Number of operations 1000\n";
        std::cout << "Number of push ratio 0.5\n";
        std::cout << "Number of pop ratio 0.5\n";
    }
	for (int i = 0; i < 10; i++) {
		start(operations, pushRatio, popRatio, threads);
	}
	
	return 0;
}
