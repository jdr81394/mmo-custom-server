#pragma once
#include "net_common.h"

namespace olc {


	namespace net {

		/*
			Mesage Header is sent at start of all messages. The template allows us
			to use enum class to ensure that all messages are valid at compile time.
		*/
		template <typename T>
		struct message_header {
			T id{};
			// Not choosing size_t bc that's platform specific
			// Uint32 is always 32 bits 
			// not in series, but things to think about is dif between x86 and Arm architecture because the byte ordering is different
			uint32_t size = 0;
		};

		template <typename T>
		struct message {

			message_header<T> header{};
			std::vector<uint64_t> body;


			// Returns size of entire packet in bytes
			size_t size() const {
				return sizeof(message_header<T>) + body.size();
			}

			// Override for std::cout compatibility - produces friendly description of image 
			friend std::ostream& operator << (std::ostream& os, const message<T>& msg) {

				os << "ID: " << int(msg.header.id) << " Size: " << msg.header.size;
				return os;
			}

			// Pushes any POD like data into the message buffer
			template <typename DataType>
			friend message<T>& operator << (message<T>& msg, const DataType& data) {

				// Before we serialize this, we want to make sure the data type can be serialized
				// This checks that the type of data being push is trivally copyable.
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex");

				size_t i = msg.body.size(); // cache current size of vector as this will be the point we insert the data

				msg.body.resize(msg.body.size() + sizeof(DataType));

				//			dest				source	size
				// data() returns a pointer to 
				// where the vector STARTS
				// That's why you must add i to it
				// to get to the end of the vector
				std::memcpy(msg.body.data() + i, &data, sizeof(DataType));

				// recalculate the message size
				msg.header.size = msg.size();

				return msg;

				/*
					Advantages:
					1. handle most data types
					2. handles most of the implementation
					Disadvantages:
					1. constant vector resizing hits performance
				*/
			}

			// Puts information from message buffer into variable
			template <typename DataType>
			friend message<T>& operator >> (message<T>& msg, DataType& data) {

				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be copied");

				size_t i = msg.body.size();

				// Take from end, treat it like a stack
				std::memcpy(&data, msg.body.data() + i, sizeof(DataType));

				// Shrink the vector remove read bytes 
				msg.body.resize(i);

				msg.header.size = msg.size();

				return msg;
			}
		};



		/* forward declaration*/
		template <typename T>
		class connection;

		template <typename T>
		struct owned_message {

			//owned_message() = default;
			
			/* */

			std::shared_ptr<connection<T>> remote = nullptr;
			message<T> msg;


			friend std::ostream& operator <<(std::ostream& os, const owned_message<T>& msg) {

				os << msg.msg;
				return os;

			}

		};
	}

}