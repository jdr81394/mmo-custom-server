#pragma once
#include "net_common.h"
#include <thread>
#include <mutex>

/*
	It needs to be threadsafe because at any given time
	it is accessed by either the client, or the connection.
*/

namespace olc {

	namespace net {

		/*
			server's queue is going to care about 
			which client it is coming from, server 
			knows which client based no the connection
			they received the message from
		*/

		template <typename T>
		class tsqueue {

		protected:
			std::mutex muxQueue;
			std::deque<T> deqQueue;


		public:
			tsqueue() = default;
			tsqueue(const tsqueue<T>&) = delete;
			virtual ~tsqueue() {// clear(); 
			};

			const T& front() {
				std::scoped_lock lock(muxQueue);
				return deqQueue.front();
			}

			/*const T& back() {
				std::scoped_lock lock(muxQueue);
				return deqQueue.emplace_back();
			}*/
			// Returns and maintains item at back of Queue
			const T& back()
			{
				std::scoped_lock lock(muxQueue);
				return deqQueue.back();
			}


			const T& push_back(const T& item) {
				std::scoped_lock lock(muxQueue);
				return deqQueue.emplace_back(std::move(item));
			}


			const T& push_front(const T& item) {
				std::scoped_lock lock(muxQueue);
				return deqQueue.emplace_front(std::move(item));
			}

			bool empty() {
				std::scoped_lock lock(muxQueue);
				return deqQueue.empty();
			}

			size_t count() {
				std::scoped_lock lock(muxQueue);
				return deqQueue.size();
			}

			T pop_front() {
				std::scoped_lock lock(muxQueue);
				auto t = std::move(deqQueue.front()); // binds to r value reference ( temporary value ) 
				deqQueue.pop_front(); // doesn't actually return it
				return t;
			}




		};

	

	}
}