#ifndef __THREADSAFEQUEUE_H_
#define __THREADSAFEQUEUE_H_

#include <queue>
#include <mutex>

template<typename T>
class ThreadSafeQueue
{
public:
	ThreadSafeQueue();
	ThreadSafeQueue(const ThreadSafeQueue& copy);

	void Push(T value);

	bool TryPop(T& value);
	bool Empty() const;
	size_t Size() const;

private:
	std::queue<T> mQueue;
	mutable std::mutex mMutex;
};

template<typename T>
ThreadSafeQueue<T>::ThreadSafeQueue() {}

template<typename T>
ThreadSafeQueue<T>::ThreadSafeQueue(const ThreadSafeQueue<T>& copy)
{
	std::lock_guard<std::mutex> lock(copy.mMutex);
	mQueue = copy.mQueue;
}

template<typename T>
void ThreadSafeQueue<T>::Push(T value)
{
	std::lock_guard<std::mutex> lock(mMutex);
	mQueue.push(std::move(value));
}

template<typename T>
bool ThreadSafeQueue<T>::TryPop(T& value)
{
	std::lock_guard<std::mutex> lock(mMutex);
	if (mQueue.empty())
		return false;

	value = mQueue.front();
	mQueue.pop();

	return true;
}

template<typename T>
bool ThreadSafeQueue<T>::Empty() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return mQueue.empty();
}

template<typename T>
size_t ThreadSafeQueue<T>::Size() const
{
	std::lock_guard<std::mutex> lock(mMutex);
	return mQueue.size();
}

#endif
