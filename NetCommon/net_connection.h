#pragma once
#include "net_common.h"
#include "net_threadsafe_queue.h"
#include "net_message.h"

namespace olc {

	namespace net {


		template<typename T>
		// enabled_shared_... allows us to create a shared pointer from within this object
		class connection : public std::enable_shared_from_this<connection<T>> {

		public:

			enum class owner {
				server,
				client
			};

			connection(owner parent, asio::io_context& asioContext, asio::ip::tcp::socket socket, tsqueue<owned_message<T>>& qIn):
				m_asioContext(asioContext), m_socket(std::move(socket)), m_qMessagesIn(qIn)
			{
			
				m_nOwnerType = parent;	// he initializes this here, just to mentally remind hismelf this this may not be 100% necessary 
			}
			virtual ~connection(){}

			void ConnectToClient(uint32_t uid = 0) {
				// Only relevant if owner is the server
				if (m_nOwnerType == owner::server) {
					if (m_socket.is_open()) {
						id = uid;
						ReadHeader();
					}
				}
			}

			uint32_t GetID() const {
				return id;
			}

			void ReadHeader()
			{
				// If this function is called, we are expecting asio to wait until it receives
				// enough bytes to form a header of a message. We know the headers are a fixed
				// size, so allocate a transmission buffer large enough to store it. In fact, 
				// we will construct the message in a "temporary" message object as it's 
				// convenient to work with.
				asio::async_read(m_socket, asio::buffer(&m_msgTemporaryIn.header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// A complete message header has been read, check if this message
							// has a body to follow...
							if (m_msgTemporaryIn.header.size > 0)
							{
								// ...it does, so allocate enough space in the messages' body
								// vector, and issue asio with the task to read the body.
								m_msgTemporaryIn.body.resize(m_msgTemporaryIn.header.size);
								ReadBody();
							}
							else
							{
								// it doesn't, so add this bodyless message to the connections
								// incoming message queue
								AddToIncomingMessageQueue();
							}
						}
						else
						{
							// Reading form the client went wrong, most likely a disconnect
							// has occurred. Close the socket and let the system tidy it up later.
							std::cout << "[" << id << "] Read Header Fail.\n";
							m_socket.close();
						}
					});
			}

			// ASYNC - Prime context ready to read a message body
			void ReadBody()
			{
				// If this function is called, a header has already been read, and that header
				// request we read a body, The space for that body has already been allocated
				// in the temporary message object, so just wait for the bytes to arrive...
				asio::async_read(m_socket, asio::buffer(m_msgTemporaryIn.body.data(), m_msgTemporaryIn.body.size()),
					[this](std::error_code ec, std::size_t length)
					{
						if (!ec)
						{
							// ...and they have! The message is now complete, so add
							// the whole message to incoming queue
							AddToIncomingMessageQueue();
						}
						else
						{
							// As above!
							std::cout << "[" << id << "] Read Body Fail.\n";
							m_socket.close();
						}
					});
			}


			// Async - Prime context to write a message header
			void WriteHeader() {
				asio::async_write(m_socket, asio::buffer(&m_qMessagesOut.front().header, sizeof(message_header<T>)),
					[this](std::error_code ec, std::size_t length) {
						if (!ec) {
							if (m_qMessagesOut.front().body.size() > 0) {
								WriteBody();
							}
							else {
								m_qMessagesOut.pop_front();

								if (!m_qMessagesOut.empty()) {	// If another header
									// do a recursive call
									WriteHeader();
								}
							}
						}
						else {
							// ...asio failed to write the message, we could analyse why but 
							// for now simply assume the connection has died by closing the
							// socket. When a future attempt to write to this client fails due
							// to the closed socket, it will be tidied up.
							std::cout << "[" << id << "] Write Header Fail.\n";
							m_socket.close();
						}

					}

				);
			}

			// Async - Prime context to read a write body
			void WriteBody() {
				asio::async_write(m_socket,
					asio::buffer(
						m_qMessagesOut.front().body.data(),
						m_qMessagesOut.front().body.size()
					),
					[this](std::error_code ec, std::size_t length) {
						if (!ec) {
							m_qMessagesOut.pop_front();

							if (!m_qMessagesOut.empty()) {
								WriteHeader();
							}
						}
						else {
							std::cout << "[" << id << "] Write Body Fail.\n";
							m_socket.close();
						}
					}
				);
			}


			void AddToIncomingMessageQueue() {
				if (m_nOwnerType == owner::server) {
					// servers connections can have multiple connections
					// Can extract a shared pointer from the shared_from_this func pointer
					m_qMessagesIn.push_back({ this->shared_from_this(), m_msgTemporaryIn });
				}
				else {
					// clients can only have one connections
					m_qMessagesIn.push_back({ nullptr, m_msgTemporaryIn });	// comes from client
				}

				ReadHeader();
			}

			bool ConnectToServer(const asio::ip::tcp::resolver::results_type& endpoints) {
			
				// Only client(s) can connect to server
				if (m_nOwnerType == owner::client) {
					asio::async_connect(m_socket, endpoints,
						[this](std::error_code ec, asio::ip::tcp::endpoint endpoint) {
							if (!ec) {
								ReadHeader();
							}
							else {
							}

						});

				}
				return true;
			}
			bool Disconnect() {
			
				if (IsConnected()) {
					asio::post(m_asioContext, [this]() {m_socket.close(); });
				}
				return true;
			}
			bool IsConnected() const {
				return m_socket.is_open();
			}

			bool Send(const message<T>& msg) {
	
				// asio post inject work into asio context
				asio::post(m_asioContext, [this, msg]() {
					bool bWritingMessage = !m_qMessagesOut.empty();

					m_qMessagesOut.push_back(msg);

					if (!bWritingMessage) {	// This is done to prevent asio from firing while it already is reading a header, thereby creating an desychronization
						WriteHeader();

					}
				});
				return true;
			}

		protected:
			// Each connection has a unique socket to a remote
			asio::ip::tcp::socket m_socket;

			// This context will be shared by the entire asio instance
			// context handles the underlying implementation of sockets on the host machine
			asio::io_context& m_asioContext;

			// Sent to the remote side
			tsqueue<message<T>> m_qMessagesOut;
			message<T> m_msgTemporaryIn;

			// Received from the remote side
			// It is a reference as the "owner" of this connection is to provide a queue?
			tsqueue<owned_message<T>>& m_qMessagesIn;

			// The "owner" decides how some of the connectios behav
			owner m_nOwnerType = owner::server;
			uint32_t id = 0;

		};


	}

}