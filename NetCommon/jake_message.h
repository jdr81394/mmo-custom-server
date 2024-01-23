#pragma once
#include "net_common.h"

template <typename T>
struct jake_header {
	T id{};
	size_t size = 0;
};


template <typename T>
struct jake_message {
	jake_header<T> header;
	std::vector<uint64_t> body;


	size_t size() {
		return (jake_header<T>) + body.size();
	}

	// Pushes any POD like data into the message buffer
	template <typename DataType>
	jake_message<T>& << operator (jake_message<T>&msg,const DataType& data ) {
		// Let's first assert that the data is serializable
		static_assert(std::is_standard_layout<DataType>::value, "This is serializable");

		// Get the size of the body
		int i = msg.body.size();

		// Expand it by the data size
		int j = sizeof(DataType);
		msg.body.resize(i + j);

		// Memory copy from the end of the vector
		std::memcpy(msg.body.data() + i, data, sizeof(DataType));






	}
};