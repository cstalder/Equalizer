
/* Copyright (c) 2005-2009, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *  
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "udpConnection.h"

#include "connectionDescription.h"

#include <eq/base/base.h>
#include <eq/base/log.h>

#include <limits>
#include <sstream>
#include <string.h>
#include <sys/types.h>

#ifdef WIN32
#  include <Mswsock.h>
#  include <Ws2tcpip.h>
#  include <Winsock2.h>
#else
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <netdb.h>
#  include <netinet/tcp.h>
#  include <sys/errno.h>
#  include <sys/socket.h>
#endif

namespace eq
{
namespace net
{
namespace
{
#ifdef WIN32
	static const size_t _mtu = 65000 ;
#else
	static const size_t _mtu = 1470 ;
#endif

}

UDPConnection::UDPConnection()
{
    _description->type = CONNECTIONTYPE_UDP;
    _description->bandwidth = 102400;

    EQVERB << "New UDPConnection @" << (void*)this << std::endl;
}

UDPConnection::~UDPConnection()
{
}

//----------------------------------------------------------------------
// connect
//----------------------------------------------------------------------
bool UDPConnection::connect()
{
    EQASSERT( _description->type == CONNECTIONTYPE_UDP );
    if( _state != STATE_CLOSED )
        return false;

    _state = STATE_CONNECTING;
    _fireStateChanged();

    if( !_parseAddress( _writeAddress ))
    {
        EQWARN << "Can't parse connection parameters" << std::endl;
        return false;
    }

    _writeFD = socket( AF_INET, SOCK_DGRAM, 0 );
    if( _writeFD == INVALID_SOCKET )
        return false;

    _readFD = _createSocket();
    if( _readFD == INVALID_SOCKET )
    {
        close();
        return false;
    }

    _readAddress = _writeAddress;
    _readAddress.sin_addr.s_addr = htonl( INADDR_ANY );

    if( ::bind( _readFD, (sockaddr*)&_readAddress, sizeof( _readAddress )) < 0 )
    {
        EQWARN << "Can't bind read socket: " << base::sysError << std::endl;
        close();
        return false;
    }

    ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = _writeAddress.sin_addr.s_addr;
    mreq.imr_interface.s_addr = htonl( INADDR_ANY );

    if( setsockopt( _readFD, IPPROTO_IP, IP_ADD_MEMBERSHIP, 
                    (char*)&mreq, sizeof( mreq )) < 0 )
    {
        EQWARN << "Can't join multicast group: " << base::sysError << std::endl;
        close();
        return false;
    }
    
    int rc = ::connect( _writeFD, (sockaddr*)&_writeAddress, 
                  sizeof( _writeAddress ));
    _setSendBufferSize( _writeFD,  8 * 1024 );
    _setRecvBufferSize( _writeFD,  8 * 1024 );

    _setSendBufferSize( _readFD,  8 * 1024 );
    _setRecvBufferSize( _readFD,  8 * 1024 );
    uint64_t ttl = 1;
    if( setsockopt(_writeFD, IPPROTO_IP, IP_MULTICAST_TTL, 
               reinterpret_cast<char *>(&ttl), sizeof(ttl)))
    {
        EQWARN << "Can't set TTL: " << base::sysError << std::endl;
        close();
        return false;
    }

    int nodelay = 1;
    int len = sizeof(nodelay);
    rc = setsockopt( _writeFD, IPPROTO_TCP, TCP_NODELAY,
                     (char*) &nodelay, len );
    _initAIORead();
    _state = STATE_CONNECTED;
    _fireStateChanged();
    return true;
}

void UDPConnection::close()
{
    if( _state == STATE_CLOSED )
        return;

    if( isConnected( ))
        _exitAIORead();

    _state = STATE_CLOSED;

    if( _writeFD > 0 )
    {
#ifdef WIN32
        const bool closed = ( ::closesocket( _writeFD ) == 0 );
#else
        const bool closed = ( ::close( _writeFD ) == 0 );
#endif
        if( !closed )
            EQWARN << "Could not close socket: " << base::sysError << std::endl;
    }

    if( _readFD > 0 )
    {
#ifdef WIN32
        const bool closed = ( ::closesocket( _readFD ) == 0 );
#else
        const bool closed = ( ::close( _readFD ) == 0 );
#endif
        if( !closed )
            EQWARN << "Could not close socket: " << base::sysError << std::endl;
    }

    _readFD  = INVALID_SOCKET;
    _writeFD = INVALID_SOCKET;
    _fireStateChanged();
}

uint32_t UDPConnection::getMTU(){ return _mtu; }
//----------------------------------------------------------------------
// Async IO handles
//----------------------------------------------------------------------
#ifdef WIN32
void UDPConnection::_initAIORead()
{
    _overlapped.hEvent = CreateEvent( 0, FALSE, FALSE, 0 );
    EQASSERT( _overlapped.hEvent );

    if( !_overlapped.hEvent )
        EQERROR << "Can't create event for AIO notification: " 
                << base::sysError << std::endl;
}

void UDPConnection::_exitAIORead()
{
    if( _overlapped.hEvent )
    {
        CloseHandle( _overlapped.hEvent );
        _overlapped.hEvent = 0;
    }
}
#else
void UDPConnection::_initAIORead(){ /* NOP */ }
void UDPConnection::_exitAIORead(){ /* NOP */ }
#endif

void UDPConnection::_initAIOAccept()
{
    EQUNIMPLEMENTED;
}

void UDPConnection::_exitAIOAccept()
{
    EQUNIMPLEMENTED;
}

//----------------------------------------------------------------------
// accept
//----------------------------------------------------------------------
void UDPConnection::acceptNB()
{
    EQASSERT( _state == STATE_LISTENING );
    EQDONTCALL;
}
    
ConnectionPtr UDPConnection::acceptSync()
{
    EQASSERT( _state == STATE_LISTENING );
    EQDONTCALL;

    return 0;
}

//----------------------------------------------------------------------
// read
//----------------------------------------------------------------------
void UDPConnection::readNB( void* buffer, const uint64_t bytes )
{
    if( _state == STATE_CLOSED )
        return;

#ifdef WIN32
    WSABUF wsaBuffer = { EQ_MIN( _mtu, bytes ),
                         reinterpret_cast< char* >( buffer ) };
    DWORD  got   = 0;
    DWORD  flags = 0;
    int size = sizeof( _readAddress );

    ResetEvent( _overlapped.hEvent );

    if( WSARecvFrom( _readFD, &wsaBuffer, 1, &got, &flags, 
                     (sockaddr*)&_readAddress, &size, &_overlapped, 0 ) != 0 &&
        GetLastError() != WSA_IO_PENDING )
    {
        EQWARN << "Could not start overlapped receive: " << base::sysError
               << ", closing connection" << std::endl;
        close();
    }
#else
    /* nop */
#endif
}

int64_t UDPConnection::readSync( void* buffer, const uint64_t bytes )
{
#ifdef WIN32
    CHECK_THREAD( _recvThread );

    if( _readFD == INVALID_SOCKET )
    {
        EQERROR << "Invalid read handle" << std::endl;
        return -1;
    }

    DWORD got   = 0;
    DWORD flags = 0;
    if( !WSAGetOverlappedResult( _readFD, &_overlapped, &got, TRUE, &flags ))
    {
        if( GetLastError() == WSASYSCALLFAILURE ) // happens sometimes!?
            return 0;

        EQWARN << "Read complete failed: " << base::sysError 
               << ", closing connection" << std::endl;
        close();
        return -1;
    }

    if( got == 0 )
    {
        EQWARN << "Read operation returned with nothing read"
               << ", closing connection." << std::endl;
        close();
        return -1;
    }

    return got;
#else // !WIN32
    if( _readFD < 1 )
        return -1;

    const size_t use = EQ_MIN( _mtu, bytes );
    socklen_t size = sizeof( _readAddress );
    const ssize_t bytesRead = ::recvfrom( _readFD, buffer, use, 0,
                                          (sockaddr*)&_readAddress, &size );
    if( bytesRead == 0 ) // EOF
    {
        EQINFO << "Got EOF, closing connection" << std::endl;
        close();
        return -1;
    }

    if( bytesRead == -1 ) // error
    {
        if( errno == EINTR ) // if interrupted, try again
            return 0;

        EQWARN << "Error during read: " << strerror( errno ) << ", " 
               << bytes << "b on fd " << _readFD << std::endl;
        return -1;
    }

    return bytesRead;
#endif
}

int64_t UDPConnection::write( const void* buffer, const uint64_t bytes)
{
    if( _state != STATE_CONNECTED || _writeFD == INVALID_SOCKET )
        return -1;

#ifdef WIN32
    DWORD  wrote;
    WSABUF wsaBuffer = { EQ_MIN( bytes, _mtu ),
                         const_cast<char*>( static_cast<const char*>(buffer)) };
    while( true )
    {
        if( WSASendTo( _writeFD, &wsaBuffer, 1, &wrote, 0,
                       (sockaddr*)&_writeAddress, sizeof( _writeAddress ),
                       0, 0 ) ==  0 )
        {   // ok
            return wrote;
        }
        // error
        if( GetLastError( ) != WSAEWOULDBLOCK )
        {
            EQWARN << "Error during write: " << base::sysError << std::endl;
            return -1;
        }
        // Buffer full - try again
#if 1
        // Wait for writable socket
        fd_set set;
        FD_ZERO( &set );
        FD_SET( _writeFD, &set );

        const int result = select( _writeFD+1, 0, &set, 0, 0 );
        if( result <= 0 )
        {
            EQWARN << "Error during select: " << base::sysError << std::endl;
            return -1;
        }
#endif
    }

    EQUNREACHABLE;
    return -1;

#else // !WIN32

    int flags = 0;

    int wrote = ::send( _writeFD, const_cast<char*>( static_cast<const char*>(buffer)), EQ_MIN( bytes, _mtu ), flags );

    // use for control congestion
    uint64_t delay = 10000 * 8 / 50000;
    for ( uint64_t i = 0 ; i < delay ; i++ ) ;

    if( wrote == -1 ) // error
    {
        if( errno == EINTR ) // if interrupted, try again
            return 0;

        EQWARN << "Error during write: " << strerror( errno ) << std::endl;
        return -1;
    }

    return wrote;
#endif
}

bool UDPConnection::_setSendBufferSize( const Socket fd, const int newSize )
{
    EQASSERT(newSize >= 0);
    
    if ( ::setsockopt( fd, SOL_SOCKET, SO_SNDBUF, 
         ( char* )&newSize, sizeof( int )) == -1 ) 
    {
        EQWARN << "can't SetSendBufferSize, error: " 
               <<  base::sysError << std::endl;
        return false;
    }

    return true;
}
bool UDPConnection::_setRecvBufferSize( const Socket fd, int newSize )
{
    if ( ::setsockopt( fd, SOL_SOCKET, SO_RCVBUF,
         (char*)&newSize, sizeof(int)) == -1 ) 
    {
        EQWARN << "can't GetSendBufferSize, error: " 
               <<  base::sysError << std::endl;
        return false;
    }
    return true;
}
UDPConnection::Socket UDPConnection::_createSocket()
{
#ifdef WIN32
    const DWORD flags = WSA_FLAG_OVERLAPPED;
    const SOCKET fd = WSASocket( AF_INET, SOCK_DGRAM, IPPROTO_UDP, 0,0,flags );
#else
    const Socket fd = ::socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
#endif

    if( fd == INVALID_SOCKET )
    {
        EQERROR << "Could not create socket: " << base::sysError << std::endl;
        return false;
    }

    _tuneSocket( fd );
    return fd;
}

void UDPConnection::_tuneSocket( const Socket fd )
{
    const int on = 1;
    setsockopt( fd, SOL_SOCKET, SO_REUSEADDR, 
                reinterpret_cast<const char*>( &on ), sizeof( on ));
}

bool UDPConnection::_parseAddress( sockaddr_in& address )
{
    if( _description->port == 0 )
        _description->port = EQ_DEFAULT_PORT;
    if( _description->getHostname().empty( ))
        _description->setHostname( "224.0.67.67" );

    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl( INADDR_ANY );
    address.sin_port        = htons( _description->port );
    memset( &(address.sin_zero), 0, 8 ); // zero the rest

    const std::string& hostname = _description->getHostname();
    if( !hostname.empty( ))
    {
        hostent *hptr = gethostbyname( hostname.c_str( ));
        if( hptr )
            memcpy( &address.sin_addr.s_addr, hptr->h_addr, hptr->h_length );
        else
        {
            EQWARN << "Can't resolve host " << hostname << std::endl;
            return false;
        }
    }

    EQINFO << "Address " << inet_ntoa( address.sin_addr )
           << ":" << ntohs( address.sin_port ) << std::endl;
    return true;
}
//----------------------------------------------------------------------
// listen
//----------------------------------------------------------------------
bool UDPConnection::listen()
{
    return connect();
}



uint16_t UDPConnection::_getPort() const
{
    sockaddr_in address;
    socklen_t used = sizeof( address );
    getsockname( _readFD, ( sockaddr * ) &address, &used ); 
    return ntohs( address.sin_port );
}

}
}