
/* Copyright (c) 2009, Stefan Eilemann <eile@equalizergraphics.com> 
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

#ifndef EQNET_PGMCONNECTION_H
#define EQNET_PGMCONNECTION_H

#include <eq/base/base.h>
#ifdef EQ_PGM

#include <eq/net/connection.h>
#include <eq/base/buffer.h> // member


namespace eq
{
namespace net
{
    /** A PGM connection (Pragmatic General Multicast). */
    class PGMConnection : public Connection
    {
    public:
        /** Create a new PGM connection */
        PGMConnection();

        virtual bool connect();            
        virtual bool listen();             
        virtual void acceptNB();           
        virtual ConnectionPtr acceptSync();
        virtual void close();              

        /** @sa Connection::getNotifier */
        virtual Notifier getNotifier() const { return _overlapped.hEvent; }

    protected:
        virtual ~PGMConnection();

        virtual void readNB( void* buffer, const uint64_t bytes );
        virtual int64_t readSync( void* buffer, const uint64_t bytes );
        virtual int64_t write( const void* buffer, const uint64_t bytes );

    private:
        void _initAIOAccept();
        void _exitAIOAccept();
        void _initAIORead();
        void _exitAIORead();

        SOCKET _initSocket( bool isRead, sockaddr_in address );
        void _tuneSocket( bool isRead, SOCKET socket );
        bool _parseAddress( sockaddr_in& address );
        uint16_t _getPort() const;

        bool _setSendRate( const SOCKET fd, const ULONG sendRate );

        bool  _getWindowSizeAndSendRate( const SOCKET fd);

        bool  _setSendBufferSize( const SOCKET fd,  const int newSize );
        bool  _setRecvBufferSize( const SOCKET fd,  const int newSize );
        bool  _setFecParameters( const SOCKET fd,
                                 const int blocksize, 
                                 const int groupsize, 
                                 const int ondemand, 
                                 const int proactive );

        bool  _enableHighSpeedLanOption( const SOCKET fd );

        SOCKET _readFD;
        SOCKET _writeFD;

        // overlapped data structures
        OVERLAPPED _overlapped;
        void*      _overlappedAcceptData;
        SOCKET     _overlappedSocket;

        CHECK_THREAD_DECLARE( _recvThread );
    };
}
}

#endif // EQ_PGM
#endif // EQNET_PGMCONNECTION_H