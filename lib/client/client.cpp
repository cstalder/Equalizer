
/* Copyright (c) 2005-2007, Stefan Eilemann <eile@equalizergraphics.com> 
   All rights reserved. */

#include "client.h"

#include "commands.h"
#include "global.h"
#include "nodeFactory.h"
#include "packets.h"
#include "server.h"

#include <eq/net/command.h>
#include <eq/net/connection.h>

using namespace eq;
using namespace eqBase;
using namespace std;

Client::Client()
{
}

Client::~Client()
{
}

eqBase::RefPtr<eqNet::Node> Client::createNode( const CreateReason reason )
{ 
    switch( reason )
    {
        case REASON_CLIENT_LAUNCH:
            return new Server;

        default:
            return new eqNet::Node;
    }
}

eqNet::Session* Client::createSession()
{
    return Global::getNodeFactory()->createConfig(); 
}

void Client::clientLoop()
{
    _used.waitGE( 1 );  // Wait to be used once (see Server::_cmdCreateConfig)
    _used.waitEQ( 0 );  // Wait to become unused (see Server::_cmdDestroyConfig)
}

bool Client::runClient( const std::string& clientArgs )
{
    const bool ret = eqNet::Node::runClient( clientArgs );
    eq::exit();
    ::exit( ret ? EXIT_SUCCESS : EXIT_FAILURE ); // never return from eq::init
    return ret;
}

eqNet::CommandResult Client::handleCommand( eqNet::Command& command )
{
    EQVERB << "handleCommand " << command << endl;

    switch( command->datatype )
    {
        case DATATYPE_EQ_SERVER:
        {
            RefPtr<eqNet::Node> node = command.getNode();

            EQASSERT( dynamic_cast<Server*>( node.get( )) );
            RefPtr<Server> server = 
                RefPtr_static_cast<eqNet::Node, Server>( node );

            return server->invokeCommand( command );
        }
        default:
            return eqNet::COMMAND_ERROR;
    }
}
