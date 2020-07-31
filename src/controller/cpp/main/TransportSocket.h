/*
 * @file   TransportSocket.h
 * @author original C++ code from https://github.com/Pseudomanifold/SimpleServer
 *         modified and further developed by Armin Zare Zadeh, ali.a.zarezadeh@gmail.com
 * @date   25 July 2020
 * @version 0.1
 * @brief   A TCP/IP Transport Socket implementation.
 */

// Note: This transport class has been built with mingw-w64 compiler on Windows 10.
// The Linux implementation is incomplete and has not been tested yet.

#ifndef D_TRANSPORT_SOCKET_H
#define D_TRANSPORT_SOCKET_H


#include <errno.h>
#include <string.h>
#include <iostream>

#include <sys/types.h>

#ifdef __WIN32__
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
# include <sys/socket.h>
#endif


#include <unistd.h>

#include <algorithm>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>

#include <functional>
#include <memory>
#include <mutex>
#include <vector>


namespace Net {

// ToDo: The TCP port is fixed! It should be configurable!
#define DEFAULT_PORT "8080"


class TransportSocket
{
public:
  class ClientSocket
  {
  public:
    ClientSocket(int fileDescriptor, TransportSocket& server) :
        _fileDescriptor(fileDescriptor)
	  , _server(server) {}


    ~ClientSocket() {}


    int fileDescriptor() const {
      return _fileDescriptor;
    }


    void close() {
      _server.close(_fileDescriptor);
    }


    void write(const std::vector<uint8_t>& data) {
    #ifndef __WIN32__
      auto result = send( _fileDescriptor,
                          reinterpret_cast<const void*>( &data[0] ),
                          data.size(),
                          0 );
    #else
      auto result = send( _fileDescriptor,
                          reinterpret_cast<const char*>( &data[0] ),
                          data.size(),
                          0 );
    #endif

      if(result == -1)
        throw std::runtime_error(std::string(strerror(errno)));
    }


    std::vector<uint8_t> read() {
      std::vector<uint8_t> message;

      char buffer[256] = { 0 };
      ssize_t numBytes = 0;
      std::cout << "read" << std::endl;

#ifdef __WIN32__
      // Set the socket I/O mode: In this case FIONBIO
      // enables or disables the blocking mode for the
      // socket based on the numerical value of iMode.
      // If iMode = 0, blocking is enabled;
      // If iMode != 0, non-blocking mode is enabled.
      u_long iMode = 1;
      ioctlsocket(_fileDescriptor, FIONBIO, &iMode);
#endif

      do {
#ifdef __WIN32__
        numBytes = recv( _fileDescriptor, buffer, sizeof(buffer), 0 );
#else
        numBytes = recv( _fileDescriptor, buffer, sizeof(buffer), MSG_DONTWAIT );
#endif
//        buffer[numBytes]  = 0;
        std::cout << "numBytes:" << numBytes << std::endl;
        if (numBytes > 0) {
          for (int i=0; i<numBytes; i++) { message.push_back(buffer[i]); }
        }
        else if (numBytes == 0)
          std::cout << "Connection closing...\n";
      } while (numBytes > 0);

#ifdef __WIN32__
      iMode = 0;
      ioctlsocket(_fileDescriptor, FIONBIO, &iMode);
#endif

      return message;
    }


    ClientSocket(const ClientSocket&)            = delete;
    ClientSocket& operator=(const ClientSocket&) = delete;

  private:
    int _fileDescriptor = -1;
    TransportSocket& _server;
  };

public:
  TransportSocket(int port) : _port(port) {
  }


  ~TransportSocket() {
    this->close();
  }


  TransportSocket( const TransportSocket& )            = delete;
  TransportSocket& operator=( const TransportSocket& ) = delete;


  void setBacklog( int backlog ) {
    _backlog = backlog;
  }


  void close() {
#ifndef __WIN32__
    if( _socket )
      ::close( _socket );
#else
    closesocket(_socket);
    WSACleanup();
#endif

    for( auto&& clientSocket : _clientSockets )
      clientSocket->close();

    _clientSockets.clear();
  }



#ifdef __WIN32__
  void listen(std::function<bool ()> stopRequested) {
    std::cout << "Transport Socket Listening starts..." << std::endl;
    WSADATA wsaData;
    int iResult;

    SOCKET _socket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
      throw std::runtime_error("WSAStartup failed with error: " + std::to_string(iResult));
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) {
      WSACleanup();
      throw std::runtime_error("getaddrinfo failed with error: " + std::to_string(iResult));
    }

    // Create a SOCKET for connecting to server
    _socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (_socket == INVALID_SOCKET) {
      freeaddrinfo(result);
      WSACleanup();
      throw std::runtime_error("Socket failed with error: " + std::to_string(WSAGetLastError()));
    }

    // Setup the TCP listening socket
    iResult = bind( _socket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
      freeaddrinfo(result);
      closesocket(_socket);
      WSACleanup();
      throw std::runtime_error("Bind failed with error: " + std::to_string(WSAGetLastError()));
    }

    freeaddrinfo(result);

    iResult = ::listen(_socket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
      closesocket(_socket);
      WSACleanup();
      throw std::runtime_error("Listen failed with error: " + std::to_string(WSAGetLastError()));
    }

    fd_set masterSocketSet;
    fd_set clientSocketSet;

    FD_ZERO( &masterSocketSet );
    FD_SET( _socket, &masterSocketSet );

    struct timeval tv;

    int highestFileDescriptor = _socket;

    while(stopRequested() == false) {
      clientSocketSet = masterSocketSet;
      tv.tv_sec = 1;
      tv.tv_usec = 0;

//      std::cout << "select" << std::endl;

      int numFileDescriptors = select( highestFileDescriptor + 1,
                                       &clientSocketSet,
                                       nullptr,   // no descriptors to write into
                                       nullptr,   // no descriptors with exceptions
                                       &tv );     // timeout


      if( numFileDescriptors == -1 )
        break;

      // Will be updated in the loop as soon as a new client has been
      // accepted. This saves us from modifying the variable *during*
      // the loop execution.
      int newHighestFileDescriptor = highestFileDescriptor;

      for(int i = 0; i <= highestFileDescriptor; i++) {
        if(!FD_ISSET( i, &clientSocketSet ))
          continue;

        // Handle new client
        if(i == _socket) {
          sockaddr_in clientAddress;
          auto clientAddressLength = sizeof(clientAddress);

          int clientFileDescriptor = accept( _socket,
                                             reinterpret_cast<sockaddr*>( &clientAddress ),
                                             reinterpret_cast<socklen_t*>( &clientAddressLength ) );
          std::cout << "accept Desc:" << clientFileDescriptor << std::endl;

          if( clientFileDescriptor == -1 )
            break;

          FD_SET( clientFileDescriptor, &masterSocketSet );
          newHighestFileDescriptor = std::max( highestFileDescriptor, clientFileDescriptor );

          auto clientSocket = std::make_shared<ClientSocket>( clientFileDescriptor, *this );

          if( _handleAccept ) {
            auto result = std::async( std::launch::async, _handleAccept, clientSocket );
          } else {
            std::cout << "_handleAccept NULL" << std::endl;
          }

          _clientSockets.push_back( clientSocket );
        }

        // Known client socket
        else {
          char buffer[2] = {0,0};

          // Let's attempt to read at least one byte from the connection, but
          // without removing it from the queue. That way, the server can see
          // whether a client has closed the connection.
          int result = recv( i, buffer, 1, MSG_PEEK );

          if(result <= 0) {
            // It would be easier to use erase-remove here, but this leads
            // to a deadlock. Instead, the current socket will be added to
            // the list of stale sockets and be closed later on.
            this->close( i );
          }
          else {
            auto itSocket = std::find_if( _clientSockets.begin(), _clientSockets.end(),
                                          [&] ( std::shared_ptr<ClientSocket> socket )
                                          {
                                            return socket->fileDescriptor() == i;
                                          } );

            if( itSocket != _clientSockets.end() && _handleRead )
              auto result = std::async( std::launch::async, _handleRead, *itSocket );
          }
        }
      }

      // Update the file descriptor if a new client has been accepted in
      // the loop above.
      highestFileDescriptor = std::max( newHighestFileDescriptor, highestFileDescriptor );

      // Handle stale connections. This is in an extra scope so that the
      // lock guard unlocks the mutex automatically.
      {
        std::lock_guard<std::mutex> lock( _staleFileDescriptorsMutex );

        for(auto&& fileDescriptor : _staleFileDescriptors) {
          FD_CLR( fileDescriptor, &masterSocketSet );
          ::close( fileDescriptor );
        }

        _staleFileDescriptors.clear();
      }
    }
    std::cout << "Transport Socket Listening exits." << std::endl;
  }

#else

  void listen() {
	  _socket = socket( AF_INET, SOCK_STREAM, 0 );

	  if( _socket == -1 )
	    throw std::runtime_error( std::string( strerror( errno ) ) );

	  {
	    int option = 1;

	    setsockopt( _socket,
	                SOL_SOCKET,
	                SO_REUSEADDR,
	                reinterpret_cast<const void*>( &option ),
	                sizeof( option ) );
	  }

	  sockaddr_in socketAddress;

	  std::fill( reinterpret_cast<char*>( &socketAddress ),
	             reinterpret_cast<char*>( &socketAddress ) + sizeof( socketAddress ),
	             0 );

	  socketAddress.sin_family      = AF_INET;
	  socketAddress.sin_addr.s_addr = htonl( INADDR_ANY );
	  socketAddress.sin_port        = htons( _port );

	  {
	    int result = bind( _socket,
	                       reinterpret_cast<const sockaddr*>( &socketAddress ),
	                       sizeof( socketAddress ) );

	    if( result == -1 )
	      throw std::runtime_error( std::string( strerror( errno ) ) );
	  }

	  {
	    int result = ::listen( _socket, _backlog );

	    if( result == -1 )
	      throw std::runtime_error( std::string( strerror( errno ) ) );
	  }

	  fd_set masterSocketSet;
	  fd_set clientSocketSet;

	  FD_ZERO( &masterSocketSet );
	  FD_SET( _socket, &masterSocketSet );

	  int highestFileDescriptor = _socket;

	  while( 1 )
	  {
	    clientSocketSet = masterSocketSet;

	    int numFileDescriptors = select( highestFileDescriptor + 1,
	                                     &clientSocketSet,
	                                     nullptr,   // no descriptors to write into
	                                     nullptr,   // no descriptors with exceptions
	                                     nullptr ); // no timeout

	    if( numFileDescriptors == -1 )
	      break;

	    // Will be updated in the loop as soon as a new client has been
	    // accepted. This saves us from modifying the variable *during*
	    // the loop execution.
	    int newHighestFileDescriptor = highestFileDescriptor;

	    for( int i = 0; i <= highestFileDescriptor; i++ )
	    {
	      if( !FD_ISSET( i, &clientSocketSet ) )
	        continue;

	      // Handle new client
	      if( i == _socket )
	      {
	        sockaddr_in clientAddress;
	        auto clientAddressLength = sizeof(clientAddress);

	        int clientFileDescriptor = accept( _socket,
	                                           reinterpret_cast<sockaddr*>( &clientAddress ),
	                                           reinterpret_cast<socklen_t*>( &clientAddressLength ) );

	        if( clientFileDescriptor == -1 )
	          break;

	        FD_SET( clientFileDescriptor, &masterSocketSet );
	        newHighestFileDescriptor = std::max( highestFileDescriptor, clientFileDescriptor );

	        auto clientSocket = std::make_shared<ClientSocket>( clientFileDescriptor, *this );

	        if( _handleAccept )
	          auto result = std::async( std::launch::async, _handleAccept, clientSocket );

	        _clientSockets.push_back( clientSocket );
	      }

	      // Known client socket
	      else
	      {
	        char buffer[2] = {0,0};

	        // Let's attempt to read at least one byte from the connection, but
	        // without removing it from the queue. That way, the server can see
	        // whether a client has closed the connection.
	        int result = recv( i, buffer, 1, MSG_PEEK );

	        if( result <= 0 )
	        {
	          // It would be easier to use erase-remove here, but this leads
	          // to a deadlock. Instead, the current socket will be added to
	          // the list of stale sockets and be closed later on.
	          this->close( i );
	        }
	        else
	        {
	          auto itSocket = std::find_if( _clientSockets.begin(), _clientSockets.end(),
	                                        [&] ( std::shared_ptr<ClientSocket> socket )
	                                        {
	                                          return socket->fileDescriptor() == i;
	                                        } );

	          if( itSocket != _clientSockets.end() && _handleRead )
	            auto result = std::async( std::launch::async, _handleRead, *itSocket );
	        }
	      }
	    }

	    // Update the file descriptor if a new client has been accepted in
	    // the loop above.
	    highestFileDescriptor = std::max( newHighestFileDescriptor, highestFileDescriptor );

	    // Handle stale connections. This is in an extra scope so that the
	    // lock guard unlocks the mutex automatically.
	    {
	      std::lock_guard<std::mutex> lock( _staleFileDescriptorsMutex );

	      for( auto&& fileDescriptor : _staleFileDescriptors )
	      {
	        FD_CLR( fileDescriptor, &masterSocketSet );
	        ::close( fileDescriptor );
	      }

	      _staleFileDescriptors.clear();
	    }
	  }
  }
#endif


  template <class F> void onAccept( F&& f ) {
    _handleAccept = f;
  }


  template <class F> void onRead( F&& f ) {
    _handleRead = f;
  }


  void close( int fileDescriptor ) {
    std::lock_guard<std::mutex> lock(_staleFileDescriptorsMutex);

    _clientSockets.erase(std::remove_if(_clientSockets.begin(), _clientSockets.end(),
                                         [&] (std::shared_ptr<ClientSocket> socket)
                                         {
                                           return socket->fileDescriptor() == fileDescriptor;
                                         }),
                                         _clientSockets.end());

    _staleFileDescriptors.push_back(fileDescriptor);
  }

private:
  int _backlog =  1;
  int _port    = -1;
  int _socket  = -1;

  std::function<void(std::weak_ptr<ClientSocket> socket)> _handleAccept;
  std::function<void(std::weak_ptr<ClientSocket> socket)> _handleRead;

  std::vector<std::shared_ptr<ClientSocket>> _clientSockets;

  std::vector<int> _staleFileDescriptors;
  std::mutex _staleFileDescriptorsMutex;
};

}

#endif /* D_TRANSPORT_SOCKET_H */
