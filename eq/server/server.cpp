
/* Copyright (c) 2005-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
 *               2010-2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *               2014-2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "server.h"

#include "channel.h"
#include "compound.h"
#include "config.h"
#include "global.h"
#include "loader.h"
#include "node.h"
#include "nodeFactory.h"
#include "pipe.h"
#include "window.h"
#ifdef EQUALIZER_USE_HWSD
#  include "config/server.h"
#endif

#include <eq/fabric/commands.h>
#include <eq/fabric/statistic.h>
#include <eq/fabric/configParams.h>

#include <co/iCommand.h>
#include <co/connectionDescription.h>
#include <co/global.h>
#include <co/init.h>
#include <co/localNode.h>
#include <lunchbox/refPtr.h>
#include <lunchbox/sleep.h>
#include <lunchbox/perThread.h>

#include <sstream>

#include "configBackupVisitor.h"
#include "configRestoreVisitor.h"

namespace eq
{
namespace server
{
namespace
{
static NodeFactory _nf;
}

typedef co::CommandFunc<Server> ServerFunc;
typedef fabric::Server< co::Node, Server, Config, NodeFactory, co::LocalNode,
                        ServerVisitor > Super;

struct Logger;
void deleteLogger( Logger* logger );

static lunchbox::PerThread< Logger, deleteLogger > _logInstance;
struct Logger
{
    static Logger& instance( Server *parent )
    {
        Logger* logger = _logInstance.get();
        if( !logger )
        {
            logger = new Logger( parent );
            _logInstance = logger;
        }
        return *logger;
    }

    static void log( Server *parent, const std::string& message )
    {
        Logger& logger = instance( parent );
        logger._perflog << message << "\n";
        logger.flush();
    }

    static void log( Server *parent, int64_t time, const co::NodeID &nodeID, const std::string& name, const std::string& message, const std::string& src )
    {
        Logger& logger = instance( parent );
        logger._perflog << time << "\t" << nodeID << "\t" << name << ":\t" << message << "\tframe\t" << logger._frame << "\t(" << src << ")\n";
        logger.flush();
    }

    static void log( Server *parent, const co::NodeID &nodeID, const std::string& name, const std::string& message, const std::string& src )
    {
        Logger& logger = instance( parent );
        logger._perflog << logger.getTime() << "\t" << nodeID << "\t" << name << ":\t" << message << "\tframe\t" << logger._frame << "\t(" << src << ")\n";
        logger.flush();
    }

    static void log( Server *parent, int64_t time, co::NodePtr node, const Statistic& statistic )
    {
//         if (!_coAppNode)
//         {
//             Config *config = _parent->getConfig();
//             eq::server::Node *appNode = config->findAppNode();
//             LBASSERT( appNode );
//             _coAppNode = appNode->getNode();
//         }
//         co::NodeID appNodeID = _coAppNode->getNodeID();

        int64_t startTime = std::numeric_limits< int64_t >::max();
        int64_t endTime   = 0;
//         int64_t transmitTime = 0;
        int64_t timing = 0;
        int64_t frame = std::numeric_limits< int64_t >::max();
        std::string name = Statistic::getName( statistic.type );
        std::string src;

        std::stringstream ss;
        switch( statistic.type )
        {
            case Statistic::CHANNEL_CLEAR:
            case Statistic::CHANNEL_DRAW:
            case Statistic::CHANNEL_ASSEMBLE:
            case Statistic::CHANNEL_READBACK:
            case Statistic::CHANNEL_TILES:
            case Statistic::CHANNEL_CHUNKS:
                src = "CHANNEL";
                startTime = LB_MIN( startTime, statistic.startTime );
                endTime   = LB_MAX( endTime, statistic.endTime );
                timing = endTime - startTime;
                ss << "\ttiming\t" << timing;
                break;
//             case Statistic::CHANNEL_ASYNC_READBACK:
//             case Statistic::CHANNEL_FRAME_TRANSMIT:
//                 src = "CHANNEL";
//                 transmitTime += statistic.startTime - statistic.endTime;
//                 ss << "\tttime\t" << transmitTime;
//                 break;
//             case Statistic::CHANNEL_FRAME_WAIT_SENDTOKEN:
//                 src = "CHANNEL";
//                 transmitTime -= statistic.endTime - statistic.startTime;
//                 ss << "\tttime\t" << transmitTime;
//                 break;
            case Statistic::WINDOW_FPS:
                src = "WINDOW";
                ss << statistic.currentFPS << "\t" << statistic.averageFPS << "\t" ;
                break;

            default:
                ss << "\t\t";
                break;
        }
        ss << "\t\t\t";
        if( src.size() > 0 )
        {
            frame = LB_MIN( frame, statistic.frameNumber );
            {
                lunchbox::ScopedMutex<> mutex();
                if( frame > _frame ) _frame = frame;
            }
//             std::string appNodeString = (node->getNodeID() == appNodeID) ? "appnode\t" : "client\t";
            log( parent, time, node->getNodeID(), name, ss.str(), src);
        }
    }

    void flush()
    {
        if( _frame < 2000 ) return;
        lunchbox::ScopedMutex<> mutex();

        if( _flushed ) return;
        std::ofstream logfile;
    
        LBINFO << "Creating performance log..." << std::endl;
        logfile.open("performance.csv", std::ofstream::out | std::ofstream::app);     // TEST
        logfile << getLog();
        logfile.close();

        _flushed = true;
    }

    int64_t getTime()
    {
        return _parent->getTime();
    }

    std::string getLog()
    {
        return _perflog.str();
    }

private:
    Logger(Server *parent)
        : _parent( parent )
        , _flushed( false )
    {}

    Server *_parent;
    co::NodePtr _coAppNode;
    std::stringstream _perflog;
    static int64_t _frame;
    bool _flushed;
};

int64_t Logger::_frame = 0;

void deleteLogger( Logger* logger )
{
//  logger->flush();
    delete logger;
}

Server::Server()
        : Super( &_nf )
        , _mainThreadQueue( co::Global::getCommandQueueLimit( ))
        , _running( false )
{
    lunchbox::Log::setClock( &_clock );
    disableInstanceCache();

    registerCommand( fabric::CMD_SERVER_CHOOSE_CONFIG,
                     ServerFunc( this, &Server::_cmdChooseConfig ),
                     &_mainThreadQueue );
    registerCommand( fabric::CMD_SERVER_RELEASE_CONFIG,
                     ServerFunc( this, &Server::_cmdReleaseConfig ),
                     &_mainThreadQueue );
    registerCommand( fabric::CMD_SERVER_DESTROY_CONFIG_REPLY,
                     ServerFunc( this, &Server::_cmdDestroyConfigReply ), 0 );
    registerCommand( fabric::CMD_SERVER_SHUTDOWN,
                     ServerFunc( this, &Server::_cmdShutdown ),
                     &_mainThreadQueue );
    registerCommand( fabric::CMD_SERVER_MAP,
                     ServerFunc( this, &Server::_cmdMap ), &_mainThreadQueue );
    registerCommand( fabric::CMD_SERVER_UNMAP,
                     ServerFunc( this, &Server::_cmdUnmap ),
                     &_mainThreadQueue );
}

Server::~Server()
{
    LBASSERT( getConfigs().empty( )); // not possible - config RefPtr's myself
    deleteConfigs();
    lunchbox::Log::setClock( 0 );
}

void Server::init()
{
    lunchbox::Thread::setName( "Server" );
    LBASSERT( isListening( ));

    const Configs& configs = getConfigs();
#ifndef EQUALIZER_USE_HWSD
    if( configs.empty( ))
        LBWARN << "No configurations loaded" << std::endl;
#endif

    LBDEBUG << lunchbox::disableFlush << lunchbox::disableHeader
           << "Running server: " << std::endl
           << lunchbox::indent << Global::instance() << *this
           << lunchbox::exdent << lunchbox::enableHeader
           << lunchbox::enableFlush << std::endl;

    for( Configs::const_iterator i = configs.begin(); i != configs.end(); ++i )
        (*i)->register_();
}

void Server::exit()
{
    const Configs& configs = getConfigs();
    for( Configs::const_iterator i = configs.begin(); i != configs.end(); ++i )
        (*i)->deregister();
}

void Server::run()
{
    init();
    handleCommands();
    exit();
}

void Server::deleteConfigs()
{
    const Configs& configs = getConfigs();
    while( !configs.empty( ))
    {
        Config* config = configs.back();
        _removeConfig( config );
        delete config;
    }
}

//===========================================================================
// ICommand handling methods
//===========================================================================
void Server::handleCommands()
{
    _running = true;
    while( _running ) // set to false in _cmdShutdown()
    {
        const co::ICommands& commands = _mainThreadQueue.popAll();
        LBASSERT( !commands.empty( ));

        for( co::ICommandsCIter i = commands.begin(); i != commands.end(); ++i )
        {
            // We want to avoid a non-const copy of commands, hence the cast...
            co::ICommand& command = const_cast< co::ICommand& >( *i );

            if( !command( ))
            {
                LBABORT( "Error handling " << command );
            }
            if( !_running )
                break;
        }
    }
    _mainThreadQueue.flush();
}

bool Server::_cmdChooseConfig( co::ICommand& command )
{
    const uint32_t requestID = command.read< uint32_t >();
    const fabric::ConfigParams& params = command.read< fabric::ConfigParams >();

    LBVERB << "Handle choose config " << command << " req " << requestID
           << " renderer " << params.getWorkDir() << '/'
           << params.getRenderClient() << std::endl;

    Config* config = 0;
    const Configs& configs = getConfigs();
    for( ConfigsCIter i = configs.begin(); i != configs.end() && !config; ++i )
    {
        Config* candidate = *i;
        const float version = candidate->getFAttribute( Config::FATTR_VERSION );
        LBASSERT( version == 1.2f );
        if( !candidate->isUsed() && version == 1.2f )
            config = candidate;
    }

#ifdef EQUALIZER_USE_HWSD
    if( !config )
    {
        const std::string& configFile = command.read< std::string >();
        config = config::Server::configure( this, configFile, params );
        if( config )
        {
            config->register_();
            LBDEBUG << lunchbox::disableFlush << lunchbox::disableHeader
                   << "Configured:" << std::endl
                   << lunchbox::indent << Global::instance() << *this
                   << lunchbox::exdent << lunchbox::enableHeader
                   << lunchbox::enableFlush << std::endl;
        }
    }
#endif

    co::NodePtr node = command.getRemoteNode();

    if( !config )
    {
        node->send( fabric::CMD_SERVER_CHOOSE_CONFIG_REPLY )
            << uint128_t() << requestID;
        return true;
    }

    ConfigBackupVisitor backup;
    config->accept( backup );
    config->setApplicationNetNode( node );
    config->setWorkDir( params.getWorkDir( ));
    config->setRenderClient( params.getRenderClient( ));
    config->commit();

    node->send( fabric::CMD_SERVER_CREATE_CONFIG )
            << co::ObjectVersion( config ) << LB_UNDEFINED_UINT32;

    server::Node* appNode = config->findApplicationNode();
    const co::ConnectionDescriptions& descs =
        appNode->getConnectionDescriptions();

    if( config->getNodes().size() > 1 )
    {
        if( descs.empty() && node->getConnectionDescriptions().empty( ))
        {
            LBWARN << "Likely misconfiguration: Neither the application nor the"
                   << " config file has a connection for this multi-node "
                   << "config. Render clients will be unable to communicate "
                   << "with the application process." << std::endl;
        }
        if( getConnectionDescriptions().empty( ))
        {
            LBWARN << "Likely misconfiguration: The server has no listening "
                   << "connection for this multi-node config. Render clients "
                   << "will be unable to communicate with the server."
                   << std::endl;
        }
    }

    node->send( fabric::CMD_SERVER_CHOOSE_CONFIG_REPLY )
            << config->getID() << requestID << co::serialize( descs );
    return true;
}

bool Server::_cmdReleaseConfig( co::ICommand& command )
{
    const uint128_t& configID = command.read< uint128_t >();
    const uint32_t requestID = command.read< uint32_t >();

    LBVERB << "Handle release config " << command << " config " << configID
           << std::endl;

    co::NodePtr node = command.getRemoteNode();

    Config* config = 0;
    const Configs& configs = getConfigs();
    for( Configs::const_iterator i = configs.begin();
         i != configs.end() && !config; ++i )
    {
        Config* candidate = *i;
        if( candidate->getID() == configID )
            config = candidate;
    }

    if( !config )
    {
        LBWARN << "Release request for unknown config" << std::endl;
        node->send( fabric::CMD_SERVER_RELEASE_CONFIG_REPLY ) << requestID;
        return true;
    }

    if( config->isRunning( ))
    {
        LBWARN << "Release of running configuration" << std::endl;
        config->exit(); // Make sure config is exited
    }

    lunchbox::Request< void > request = registerRequest< void >();
    node->send( fabric::CMD_SERVER_DESTROY_CONFIG )
        << config->getID() << request;
    request.wait();

#ifdef EQUALIZER_USE_HWSD
    if( config->isAutoConfig( ))
    {
        LBASSERT( _admins.empty( ));
        config->deregister();
        config::Server::release( config );
    }
    else
#endif
    {
        ConfigRestoreVisitor restore;
        config->accept( restore );
        config->commit();
    }

    node->send( fabric::CMD_SERVER_RELEASE_CONFIG_REPLY ) << requestID;
    LBVERB << "Released config " << configID << std::endl;
    return true;
}

bool Server::_cmdDestroyConfigReply( co::ICommand& command )
{
    serveRequest( command.read< uint32_t >( ));
    return true;
}

bool Server::_cmdShutdown( co::ICommand& command )
{
    const uint32_t requestID = command.read< uint32_t >();

    co::NodePtr node = command.getRemoteNode();

    if( !_admins.empty( ))
    {
        LBWARN << "Ignoring shutdown request, " << _admins.size()
               << " admin clients connected" << std::endl;

        node->send( fabric::CMD_SERVER_SHUTDOWN_REPLY ) << requestID << false;
        return true;
    }

    const Configs& configs = getConfigs();
    for( Configs::const_iterator i = configs.begin(); i != configs.end(); ++i )
    {
        Config* candidate = *i;
        if( candidate->isUsed( ))
        {
            LBWARN << "Ignoring shutdown request due to used config"
                   << std::endl;

            node->send( fabric::CMD_SERVER_SHUTDOWN_REPLY )
                    << requestID << false;
            return true;
        }
    }

    LBDEBUG << "Shutting down server" << std::endl;

    _running = false;
    node->send( fabric::CMD_SERVER_SHUTDOWN_REPLY ) << requestID << true;

#ifndef WIN32
    // WAR for 2874188: Lockup at shutdown
    lunchbox::sleep( 100 );
#endif

    return true;
}

bool Server::_cmdMap( co::ICommand& command )
{
    co::NodePtr node = command.getRemoteNode();
    _admins.push_back( node );

    const Configs& configs = getConfigs();
    for( Configs::const_iterator i = configs.begin(); i != configs.end(); ++i )
    {
        Config* config = *i;
        node->send( fabric::CMD_SERVER_CREATE_CONFIG )
                << co::ObjectVersion( config ) << LB_UNDEFINED_UINT32;
    }

    node->send( fabric::CMD_SERVER_MAP_REPLY ) << command.read< uint32_t >();
    return true;
}

bool Server::_cmdUnmap( co::ICommand& command )
{
    co::NodePtr node = command.getRemoteNode();
    co::Nodes::iterator i = lunchbox::find( _admins, node );

    LBASSERT( i != _admins.end( ));
    if( i != _admins.end( ))
    {
        _admins.erase( i );
        const Configs& configs = getConfigs();
        for( Configs::const_iterator j = configs.begin();
             j != configs.end(); ++j )
        {
            Config* config = *j;
            node->send( fabric::CMD_SERVER_DESTROY_CONFIG )
                    << config->getID() << LB_UNDEFINED_UINT32;
        }
    }

    node->send( fabric::CMD_SERVER_UNMAP_REPLY ) << command.read< uint32_t >();
    return true;
}

void Server::log( const std::string& message )
{
    Logger::log( this, message );
}

void Server::log( const co::NodeID& nodeID, const std::string& name, const std::string& message, const std::string& src )
{
    Logger::log( this, nodeID, name, message, src );
}

void Server::log(int64_t time, const co::NodeID& nodeID, const std::string& name, const std::string& message, const std::string& src )
{
    Logger::log( this, time, nodeID, name, message, src );
}

void Server::log(co::NodePtr node, int64_t time, const Statistic& statistic )
{
    Logger::log( this, time, node, statistic );
}

}
}
#include "../fabric/server.ipp"
template class eq::fabric::Server< co::Node, eq::server::Server,
                                   eq::server::Config,
                                   eq::server::NodeFactory,
                                   co::LocalNode, eq::server::ServerVisitor >;

/** @cond IGNORE */
template std::ostream&
eq::fabric::operator << ( std::ostream&, const eq::server::Super& );
/** @endcond */
