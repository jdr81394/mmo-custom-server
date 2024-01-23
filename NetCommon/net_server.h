#pragma once
#include "./net_common.h"
#include "./net_threadsafe_queue.h"
#include "./net_message.h"
#include "./net_connection.h"

#include <algorithm>

namespace olc {


	namespace net {


		/* 
		T will equal a class of possible enums.
		These enums will be related to the types of messages it can receive
		
		*/
		template<typename T>
		class server_interface {

		public:
			server_interface(uint16_t port) 
				// Context to do the work,   endpoint - type of connection and the port number
				: m_asioAcceptor(m_asioContext, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))  {
			};

			virtual ~server_interface() {
				Stop();
			};

			bool Start() {
			
				try {

					// Get Connection first
					WaitForClientConnection();

					// Create thread
					// If did the other way around, this thread could close. We wait for client connection first so we can issue some work first
					m_threadContext = std::thread([this]() {
						m_asioContext.run();
					});
				
				}
				catch (std::exception& e) {
					std::cerr << "[SERVER] Exception: " << e.what() << "\n";
					return false;
				}

				std::cout << "[SERVER] Started!\n";
				return true;
			
			};
			bool Stop() {
			
				// Context will attempt to stop, so it will take time to finish up all its tasks
				m_asioContext.stop();

				if (m_threadContext.joinable()) {
					m_threadContext.join();
				}

				std::cout << "[SERVER] Stopped!\n";
				return true;
			};

			// ASYNC - Instruct asio to wait for connection
			void WaitForClientConnection() {

				// accepts lambda function that does the work
				// This function produces a socket that the connection will be able to use. 
				// I assume it will tie the asioContext to the server and use the Context to handle the socket implementation?
				m_asioAcceptor.async_accept(
					[this](std::error_code ec, asio::ip::tcp::socket socket) {
						if (!ec) {
							std::cout << "[SERVER] New Connection: " << socket.remote_endpoint() << "\n";

							std::shared_ptr<connection<T>> newconn = std::make_shared<connection<T>>(
								connection<T>::owner::server,		// idk?
								m_asioContext,						// will use this context to do work?
								std::move(socket),					// std move binds an r value, from this async func it allows the socket variable to persist in memory i think
								m_qMessagesIn						// The message queue will be shared across instances of connections. Just for incoming mesages. Didnt realize that was the set up. this wil lbe thread safe though
							);

							 // Give the user server a chance to deny connection ... i dont know why the user would deny it
							if (OnClientConnect(newconn)) {

								this->m_deqConnections.push_back(std::move(newconn));	// again, usage of move to bind the R value and allow it to exist once scope ends.

								
								this->m_deqConnections.back()->ConnectToClient(nIDCounter++); // connects to client and passes in it's id

								std::cout << "[" << this->m_deqConnections.back()->GetID() << "] Connection Approved";
							}
							else {
								std::cout << "[-----] Connection Denied\n";
							}

						}
						else {
							std::cout << "[SERVER] New Connection Error: " << ec.message() << "\n";
						}

						// Prime the asio context with more work - again let's just wait for another connection
						WaitForClientConnection();
					}
				);
			};


			// How do we send messages to clients? 
			void MessageClient(std::shared_ptr<connection<T>> client, const message<T>& msg) {
				
				
				/* 
					TCP issue - using TCP, we don't know if the client is still connected.
					It's only after attempting to communicate with the client, that we know if they
					disconnected DUE to the lack of response.
				*/
				if (client && client->IsConnected()) {
					client->Send(msg);
				}
				else {
					OnClientDisconnect(client);
					client.reset();	// delets client
					this->m_deqConnections.erase(
						std::remove(
							this->m_deqConnections.begin(),
							this->m_deqConnections.end(),
							client
						),
						this->m_deqConnections.erase()
					);
				}


			};

			// send message to all clients
			void MessageAllClients(const message<T>& msg, std::shared_ptr<connection<T>> pIgnoreClient = nullptr) {

				bool bInvalidClientExists = false;

				for (auto& client : this->m_deqConnections) {
					// Check if client is connected...

					if (client && client->IsConnected()) {
						// ..it is!
						if (client != pIgnoreClient) {
							client->Send(msg);
						}
					}
					else {
						OnClientDisconnect(client);
						client.reset();
						bInvalidClientExists = true;
					}
				}

				if (bInvalidClientExists == false) {

					// std::deque::erase removes the "removed" elements
					// first parameter == begin, last parameter == last. Only 2 parameters

					this->m_deqConnections.erase(
						std::remove(this->m_deqConnections.begin(), this->m_deqConnections.end(), nullptr)	// first pos, last pos, value to be removed
					);


				}
			};


			// This will process the messages in the queue through the OnMessage function
			void Update(size_t nMaxMessages = -1) {	// size_t is unsigned, so setting it to -1 sets it to MAXIMUM VALUE lol 
				size_t nMessageCount = 0;
				while (nMessageCount < nMaxMessages && !m_qMessagesIn.empty()) {
					auto msg = m_qMessagesIn.pop_front();

					OnMessage(msg.remote, msg.msg);	// msg.remote is the shared ptr to the specific client

					nMessageCount++;

				}
			}

		protected:
			// virtual allows for it to be overriden in a child class
			// Called when a client appears to have disconnected
			virtual bool OnClientConnect(std::shared_ptr<connection<T>> client) {
				return false;
			};

			// Called when a client appears to have disconnected 
			virtual void OnClientDisconnect(std::shared_ptr<connection<T>> client) {

			};

			// Called when a message arrives
			// Can be async or sync. 
			// connect clients connection to this function in order to keep it async, 
			// so that way when the connectino connects to it, it calls it.
			// but we will SYNCHRONIZE THIS INTO A SINGLE QUEUE
			virtual void OnMessage(std::shared_ptr<connection<T>> client, message<T>& msg) {


			};

			protected: 

			tsqueue<owned_message<T>> m_qMessagesIn;

			std::deque<std::shared_ptr<connection<T>>> m_deqConnections;

			// Order of declaration is important - it is also the order of initialization
			asio::io_context m_asioContext;
			std::thread m_threadContext; // asio context needs it's own thread

			// We need sockets of the connected client, they need a context
			asio::ip::tcp::acceptor m_asioAcceptor;

			// Clients will be identified in the wider system via an ID. Needs to be unique
			// Purpose: 1. consistent ID to be used to inform client of their own id, as well as other client's ids in network
			// Purpose: 2. We COULD use IP and port address, but we should hide this from other clients. Also, it's much simpler.
			uint32_t nIDCounter = 10000;


		


		};
	}

}