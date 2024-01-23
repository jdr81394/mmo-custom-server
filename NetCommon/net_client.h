#pragma once
#include "net_common.h"
#include "net_threadsafe_queue.h"
#include "net_message.h"
#include "net_threadsafe_queue.h"

namespace olc {


	namespace net {


		template <typename T> 
		class client_interface
		{

		public:

			client_interface() : m_socket(m_context) {
				// Initialize the socket with the io context, so it can do stuff
			}

			virtual ~client_interface() {
				Disconnect();
			}

			bool Connect(const std::string& host, const uint16_t port) {
				try {
					asio::ip::tcp::resolver resolver(m_context);		// resolver is used to take DNS names ( like www.example.com ) and convert it into actual ip addresses that can be connected to
					
					// resolver does some magic and gets the endpoints lol
					asio::ip::tcp::resolver::results_type m_endpoints = resolver.resolve(host, std::to_string(port));

					m_connection = std::make_unique<connection<T>>(
						connection<T>::owner::client,
						m_context,
						asio::ip::tcp::socket(m_context),
						m_qMessagesIn
					);	// The client creates that connection object


					m_connection->ConnectToServer(m_endpoints);	// connect object to server

					thrContext = std::thread([this]() {m_context.run();  });
				}
				catch (std::exception& e) {
					std::cerr << "Client Exception: " << e.what() << "\n";
					return false;
				}
			}

			bool IsConnected() {
				if (m_connection) {
					return m_connection->IsConnected();
				}
				else {
					return false;
				}
			}

			void Disconnect() {
				if (IsConnected()) {
					m_connection->Disconnect();
				}

				m_context.stop();

				if (thrContext.joinable()) {
					thrContext.join();
				}
				
			}

			// Send message to server
			void Send(const message<T>& msg)
			{
				if (IsConnected())
					m_connection->Send(msg);
			}

			// Retrieve queue of messages
			tsqueue<owned_message<T>>& Incoming() {
				return m_qMessagesIn;
			}



		protected:
			// asio context handles data transfer
			asio::io_context m_context;
			// we need a context because the client is going to be in charge of setting up the connection first
			std::thread thrContext;

			// socket that is connected to the server
			asio::ip::tcp::socket m_socket;

			// client has a single instnace of a connection object which handles data transfer
			std::unique_ptr<connection<T>> m_connection;


		private:
			// This is the thread safe queue for incoming messages from the server 
			tsqueue<owned_message<T>> m_qMessagesIn;



		};

	}

}