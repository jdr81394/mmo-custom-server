
// Defined in the C/C++ > Preprocessor section
//#ifndef _WIN32
//#define _WIN32_WINNT = 0x0A00		// Each windows operating system handles networking differnetly, so this tells us which way to handle it
////#define _WIN32_WINNT 0x601		// Each windows operating system handles networking differnetly, so this tells us which way to handle it
//
//#endif

//fine _WIN32_WINNT or _WIN32_WINDOWS appropriately.For example :
//1 > -add - D_WIN32_WINNT = 0x0601 to the compiler command line; or
//1 > -add _WIN32_WINNT = 0x0601 to you

#define ASIO_STANDALONE

#include <iostream>
#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>


std::vector<char> vBuffer(20 * 1024);

void GrabSomeData(asio::ip::tcp::socket& socket) {

	// Need to give this a lambda function since its async
	// It "primes" the context with some work to do WHEN there is data to read
	socket.async_read_some(asio::buffer(vBuffer.data(), vBuffer.size()),
		// Call back function
		[&](std::error_code ec, std::size_t length) 
		{
			if (!ec) {
				std::cout << "\n\nRead " << length << " bytes\n\n";
				for(int i = 0; i < length; i++) {
					std::cout << vBuffer[i];
				}
				GrabSomeData(socket);
			}
		}
	);

}


int main() {

	std::cout << "Hello World" << std::endl;

	asio::error_code ec;

	asio::io_context context;	// Unique instance of asio, this instance handles all platform specific requirements

	asio::io_context::work idleWork(context); // fake tasks to asio so context doesnt finish

	std::thread thrContext = std::thread([&]() { context.run(); }); // Runs in its own thread so if it needs to stop and wait it doesn't block the main thread

	//asio::ip::tcp::endpoint endpoint(asio::ip::make_address("93.184.216.34", ec), 80); // Get the address of where we want to connect
	asio::ip::tcp::endpoint endpoint(asio::ip::make_address("51.38.81.49", ec), 80); // Get the address of where we want to connect

	asio::ip::tcp::socket socket(context);	// Context delivers the implementation of the socket.

	socket.connect(endpoint, ec);

	if (!ec) {
		std::cout << "CONNECT MF" << std::endl;
	} 
	else {
		std::cout << "Did not connect: " << ec.message() << std::endl;
	}


	if (socket.is_open()) {

		// Primes the context with work to do.
		GrabSomeData(socket); // I think the keyword is "PRIME", it gets it ready to do soemthing. 

		std::string sRequest =
			"GET /index.html HTTP/1.1\r\n"
			"Host: example.com\r\n"
			"Connection: close\r\n\r\n";

		socket.write_some(asio::buffer(sRequest.data(), sRequest.size()), ec); // R/W ops use buffer in asio, buffer just a container that holds bytes

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(20000ms);

		context.stop();
		if (thrContext.joinable()) thrContext.join();

	}

	
	system("pause");
	return 0;
}