
/* Copyright (c) 2007-2009, Stefan Eilemann <eile@equalizergraphics.com> 
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

#include <test.h>

#include <eq/net/dataIStream.h>
#include <eq/net/dataOStream.h>

#include <eq/base/thread.h>
#include <eq/net/command.h>
#include <eq/net/commandQueue.h>
#include <eq/net/connection.h>
#include <eq/net/init.h>
#include <eq/net/packets.h>

// Tests the functionality of the DataOStream and DataIStream

#define CONTAINER_SIZE 4096
static std::string _message( "So long, and thanks for all the fish" );

struct HeaderPacket : public eq::net::Packet
{
    HeaderPacket()
        {
            command        = 0;
            size           = sizeof( HeaderPacket ); 
        }
};

struct DataPacket : public eq::net::Packet
{
    DataPacket()
        {
            command        = 2;
            size           = sizeof( DataPacket ); 
            data[0]        = '\0';
        }
        
    uint64_t dataSize;
    EQ_ALIGN8( uint8_t data[8] );
};

struct FooterPacket : public eq::net::Packet
{
    FooterPacket()
        {
            command        = 4;
            size           = sizeof( FooterPacket ); 
        }
};

class DataOStream : public eq::net::DataOStream
{
protected:
    virtual void sendHeader( const void* buffer, const uint64_t size )
        {
            HeaderPacket packet;
            eq::net::Connection::send( _connections, packet, true/*isLocked*/ );
            EQINFO << "Sent header" << std::endl;

            sendBuffer( buffer, size );
        }

    virtual void sendBuffer( const void* buffer, const uint64_t size )
        {
            DataPacket packet;
            packet.dataSize = size;
            eq::net::Connection::send( _connections, packet, buffer, size, 
                                     true /*isLocked*/ );
            EQINFO << "Sent buffer of " << size << " bytes" << std::endl;
        }

    virtual void sendFooter( const void* buffer, const uint64_t size )
        {
            if( size > 0 )
                sendBuffer( buffer, size );

            FooterPacket packet;
            eq::net::Connection::send( _connections, packet, true/*isLocked*/ );
            EQINFO << "Sent footer" << std::endl;
        }
};

class DataIStream : public eq::net::DataIStream
{
public:
    void addDataCommand( eq::net::Command& command )
        {
            TESTINFO( command->command == 2, command );
            _commands.push( command );
        }

    virtual size_t nRemainingBuffers() const { return _commands.getSize(); }
    virtual uint32_t getVersion() const { return eq::net::VERSION_NONE;}

protected:
    virtual bool getNextBuffer( const uint8_t** buffer, uint64_t* size )
        {
            eq::net::Command* command = _commands.tryPop();
            if( !command )
                return false;

            TESTINFO( (*command)->command == 2, *command );

            DataPacket* packet = command->getPacket<DataPacket>();
            
            *buffer = packet->data;
            *size   = packet->dataSize;
            EQINFO << "Got buffer of " << *size << " bytes" << std::endl;

            command->release();
            return true;
        }

private:
    eq::net::CommandQueue _commands;
};

namespace eq
{
namespace net
{
namespace DataStreamTest
{
class Sender : public eq::base::Thread
{
public:
    Sender( eq::base::RefPtr< eq::net::Connection > connection )
            : Thread(),
              _connection( connection )
        {}
    virtual ~Sender(){}

protected:
    virtual void* run()
        {
            ::DataOStream stream;

            _connection->lockSend();
            stream._connections.push_back( _connection );
            stream.enable();

            int foo = 42;
            stream << foo;
            stream << 43.0f;
            stream << 44.0;

            std::vector< double > doubles;
            for( size_t i=0; i<CONTAINER_SIZE; ++i )
                doubles.push_back( static_cast< double >( i ));
            stream << doubles;

            stream << _message;

            stream.disable();
            return EXIT_SUCCESS;
        }

private:
    eq::base::RefPtr< eq::net::Connection > _connection;
};
}
}
}

int main( int argc, char **argv )
{
    eq::net::init( argc, argv );
    eq::net::ConnectionDescriptionPtr desc = new eq::net::ConnectionDescription;
    desc->type = eq::net::CONNECTIONTYPE_PIPE;
    eq::net::ConnectionPtr connection = eq::net::Connection::create( desc );

    TEST( connection->connect( ));
    eq::net::DataStreamTest::Sender sender( connection );
    TEST( sender.start( ));

    DataIStream           stream;
    eq::net::CommandCache commandCache;
    bool                  receiving = true;

    while( receiving )
    {
        uint64_t size;
        connection->recvNB( &size, sizeof( size ));
        TEST( connection->recvSync( 0, 0 ));
        TEST( size );

        eq::net::Command& command = commandCache.alloc( 0, 0, size );
        size -= sizeof( size );

        char*      ptr     = reinterpret_cast< char* >( command.getPacket( )) +
                                                        sizeof( size );
        connection->recvNB( ptr, size );
        TEST( connection->recvSync( 0, 0 ) );
        TEST( command.isValid( ));
        
        switch( command->command )
        {
            case 0: // header, nop
                TEST( command.isFree( ));
                break;
            case 2:
                stream.addDataCommand( command );
                TEST( !command.isFree( ));
                break;
            case 4:
                receiving = false;
                TEST( command.isFree( ));
                break;
            default:
                TEST( false );
        }
    }

    int foo;
    stream >> foo;
    TESTINFO( foo == 42, foo );

    float fFoo;
    stream >> fFoo;
    TEST( fFoo == 43.f );

    double dFoo;
    stream >> dFoo;
    TEST( dFoo == 44.0 );

    std::vector< double > doubles;
    stream >> doubles;
    for( size_t i=0; i<CONTAINER_SIZE; ++i )
        TEST( doubles[i] == static_cast< double >( i ));

    std::string message;
    stream >> message;
    TEST( message.length() == _message.length() );
    TESTINFO( message == _message, 
              '\'' <<  message << "' != '" << _message << '\'' );

    TEST( sender.join( ));
    connection->close();
    return EXIT_SUCCESS;
}