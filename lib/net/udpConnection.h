
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

#ifndef EQNET_UDPCONNECTION_H
#define EQNET_UDPCONNECTION_H

#include <eq/net/connection.h> // base class

#include <eq/base/base.h>
#include <eq/base/buffer.h> // member

#ifndef WIN32
#  include <netinet/in.h>
#endif

namespace eq
{
namespace net
{
    /** A udp connection (Attn: only multicast usage implemented). */
    class UDPConnection : public Connection
    {
    public:
        /** Create a new udp-based connection. */
        UDPConnection();

        virtual bool connect();
        /**
         * Listening on a UDP connection is the same as connecting.
         *
         * Consequently, listen will put this connection into the connected
         * state.
         * @sa Connection::listen
         */
        virtual bool listen();             
        virtual void acceptNB();           
        virtual ConnectionPtr acceptSync();
        virtual void close();              


#ifdef WIN32
        /** @sa Connection::getNotifier */
        virtual Notifier getNotifier() const { return _overlapped.hEvent; }
#endif

    protected:
        virtual ~UDPConnection();

        virtual void readNB( void* buffer, const uint64_t bytes );
        virtual int64_t readSync( void* buffer, const uint64_t bytes );
        virtual int64_t write( const void* buffer, const uint64_t bytes );

#ifdef WIN32
        typedef SOCKET Socket;
#else
        //! @cond IGNORE
        typedef int    Socket;
        enum
        {
            INVALID_SOCKET = -1
        };
        //! @endcond
#endif

    private:
        void _initAIOAccept();
        void _exitAIOAccept();
        void _initAIORead();
        void _exitAIORead();

        Socket _createSocket();
        void _tuneSocket( const Socket fd );
        bool _parseAddress( sockaddr_in& address );
        uint16_t _getPort() const;

        sockaddr_in _writeAddress;
        sockaddr_in _readAddress;

        Socket _readFD;
        Socket _writeFD;

#ifdef WIN32
        // overlapped data structures
        OVERLAPPED _overlapped;
#endif
    };
}
}

#endif //EQNET_UDPCONNECTION_H