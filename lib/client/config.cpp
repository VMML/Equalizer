
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

#include "config.h"

#include "canvas.h"
#include "client.h"
#include "configDeserializer.h"
#include "configEvent.h"
#include "configStatistics.h"
#include "frame.h"
#include "frameData.h"
#include "global.h"
#include "layout.h"
#include "log.h"
#include "node.h"
#include "nodeFactory.h"
#include "packets.h"
#include "server.h"
#include "task.h"
#include "view.h"

#include <eq/net/command.h>
#include <eq/net/global.h>

#include "configCommitVisitor.h"

namespace eq
{
/** @cond IGNORE */
typedef net::CommandFunc<Config> ConfigFunc;
/** @endcond */

Config::Config( ServerPtr server )
        : net::Session()
        , _eyeBase( 0.f )
        , _lastEvent( 0 )
        , _latency( 0 )
        , _currentFrame( 0 )
        , _unlockedFrame( 0 )
        , _finishedFrame( 0 )
        , _running( false )
{
    base::Log::setClock( &_clock );
    EQINFO << "New config @" << (void*)this << std::endl;
}

Config::~Config()
{
    EQINFO << "Delete config @" << (void*)this << std::endl;
    EQASSERT( _observers.empty( ));
    EQASSERT( _layouts.empty( ));
    EQASSERT( _canvases.empty( ));
    
    while( tryNextEvent( )) /* flush all pending events */ ;
    if( _lastEvent )
        _lastEvent->release();
    _eventQueue.flush();
    _lastEvent = 0;

    _appNodeID = net::NodeID::ZERO;
    _appNode   = 0;
    base::Log::setClock( 0 );
}

void Config::notifyMapped( net::NodePtr node )
{
    net::Session::notifyMapped( node );

    ServerPtr          server = getServer();
    net::CommandQueue* queue  = server->getNodeThreadQueue();

    registerCommand( CMD_CONFIG_CREATE_NODE,
                     ConfigFunc( this, &Config::_cmdCreateNode ), queue );
    registerCommand( CMD_CONFIG_DESTROY_NODE,
                     ConfigFunc( this, &Config::_cmdDestroyNode ), queue );
    registerCommand( CMD_CONFIG_INIT_REPLY, 
                     ConfigFunc( this, &Config::_cmdInitReply ), queue );
    registerCommand( CMD_CONFIG_EXIT_REPLY, 
                     ConfigFunc( this, &Config::_cmdExitReply ), queue );
    registerCommand( CMD_CONFIG_RELEASE_FRAME_LOCAL, 
                     ConfigFunc( this, &Config::_cmdReleaseFrameLocal ), queue);
    registerCommand( CMD_CONFIG_FRAME_FINISH, 
                     ConfigFunc( this, &Config::_cmdFrameFinish ), 0 );
    registerCommand( CMD_CONFIG_EVENT, 
                     ConfigFunc( this, &Config::_cmdUnknown ), &_eventQueue );
    registerCommand( CMD_CONFIG_SYNC_CLOCK, 
                     ConfigFunc( this, &Config::_cmdSyncClock ), 0 );
    registerCommand( CMD_CONFIG_UNMAP, ConfigFunc( this, &Config::_cmdUnmap ),
                     queue );
}

CommandQueue* Config::getNodeThreadQueue()
{
    return getClient()->getNodeThreadQueue();
}

namespace
{
template< typename P, typename T > class IDFinder : public P
{
public:
    IDFinder( const uint32_t id ) : _id( id ), _result( 0 ) {}
    virtual ~IDFinder(){}

    virtual VisitorResult visitPre( T* node ) { return visit( node ); }
    virtual VisitorResult visit( T* node )
        {
            if( node->getID() == _id )
            {
                _result = node;
                return TRAVERSE_TERMINATE;
            }
            return TRAVERSE_CONTINUE;
        }

    T* getResult() { return _result; }

private:
    const uint32_t _id;
    T*             _result;
};

typedef IDFinder< ConfigVisitor, Observer > ObserverIDFinder;
typedef IDFinder< ConfigVisitor, Layout > LayoutIDFinder;
typedef IDFinder< ConfigVisitor, View > ViewIDFinder;
}

Observer* Config::findObserver( const uint32_t id )
{
    ObserverIDFinder finder( id );
    accept( finder );
    return finder.getResult();
}

Layout* Config::findLayout( const uint32_t id )
{
    LayoutIDFinder finder( id );
    accept( finder );
    return finder.getResult();
}

View* Config::findView( const uint32_t id )
{
    ViewIDFinder finder( id );
    accept( finder );
    return finder.getResult();
}

namespace
{
template< class C, class V >
VisitorResult _accept( C* config, V& visitor )
{ 
    VisitorResult result = visitor.visitPre( config );
    if( result != TRAVERSE_CONTINUE )
        return result;

    const NodeVector& nodes = config->getNodes();
    for( NodeVector::const_iterator i = nodes.begin(); i != nodes.end(); ++i )
    {
        switch( (*i)->accept( visitor ))
        {
            case TRAVERSE_TERMINATE:
                return TRAVERSE_TERMINATE;

            case TRAVERSE_PRUNE:
                result = TRAVERSE_PRUNE;
                break;
                
            case TRAVERSE_CONTINUE:
            default:
                break;
        }
    }

    const ObserverVector& observers = config->getObservers();
    for( ObserverVector::const_iterator i = observers.begin(); 
         i != observers.end(); ++i )
    {
        switch( (*i)->accept( visitor ))
        {
            case TRAVERSE_TERMINATE:
                return TRAVERSE_TERMINATE;

            case TRAVERSE_PRUNE:
                result = TRAVERSE_PRUNE;
                break;
                
            case TRAVERSE_CONTINUE:
            default:
                break;
        }
    }

    const LayoutVector& layouts = config->getLayouts();
    for( LayoutVector::const_iterator i = layouts.begin(); 
         i != layouts.end(); ++i )
    {
        switch( (*i)->accept( visitor ))
        {
            case TRAVERSE_TERMINATE:
                return TRAVERSE_TERMINATE;

            case TRAVERSE_PRUNE:
                result = TRAVERSE_PRUNE;
                break;
                
            case TRAVERSE_CONTINUE:
            default:
                break;
        }
    }

    const CanvasVector& canvases = config->getCanvases();
    for( CanvasVector::const_iterator i = canvases.begin();
         i != canvases.end(); ++i )
    {
        switch( (*i)->accept( visitor ))
        {
            case TRAVERSE_TERMINATE:
                return TRAVERSE_TERMINATE;

            case TRAVERSE_PRUNE:
                result = TRAVERSE_PRUNE;
                break;
                
            case TRAVERSE_CONTINUE:
            default:
                break;
        }
    }

    switch( visitor.visitPost( config ))
    {
        case TRAVERSE_TERMINATE:
            return TRAVERSE_TERMINATE;

        case TRAVERSE_PRUNE:
            result = TRAVERSE_PRUNE;
            break;
                
        case TRAVERSE_CONTINUE:
        default:
            break;
    }

    return result;
}
}

VisitorResult Config::accept( ConfigVisitor& visitor )
{
    return _accept( this, visitor );
}

ServerPtr Config::getServer()
{ 
    net::NodePtr node = net::Session::getServer();
    EQASSERT( dynamic_cast< Server* >( node.get( )));
    ServerPtr server = static_cast< Server* >( node.get( ));
    return server;
}

ClientPtr Config::getClient()
{ 
    return getServer()->getClient(); 
}

void Config::_addNode( Node* node )
{
    EQASSERT( node->getConfig() == this );
    _nodes.push_back( node );
}

void Config::_removeNode( Node* node )
{
    NodeVector::iterator i = find( _nodes.begin(), _nodes.end(), node );
    EQASSERT( i != _nodes.end( ));
    _nodes.erase( i );
}

Node* Config::_findNode( const uint32_t id )
{
    for( NodeVector::const_iterator i = _nodes.begin(); i != _nodes.end(); 
         ++i )
    {
        Node* node = *i;
        if( node->getID() == id )
            return node;
    }
    return 0;
}

void Config::_addObserver( Observer* observer )
{
    observer->_config = this;
    _observers.push_back( observer );
}

void Config::_removeObserver( Observer* observer )
{
    ObserverVector::iterator i = find( _observers.begin(), _observers.end(), 
                                       observer );
    EQASSERT( i != _observers.end( ));
    _observers.erase( i );
}

void Config::_addLayout( Layout* layout )
{
    layout->_config = this;
    _layouts.push_back( layout );
}

void Config::_removeLayout( Layout* layout )
{
    LayoutVector::iterator i = find( _layouts.begin(), _layouts.end(), layout );
    EQASSERT( i != _layouts.end( ));
    _layouts.erase( i );
}

void Config::_addCanvas( Canvas* canvas )
{
    canvas->_config = this;
    _canvases.push_back( canvas );
}

void Config::_removeCanvas( Canvas* canvas )
{
    CanvasVector::iterator i = find( _canvases.begin(), _canvases.end(),
                                     canvas );
    EQASSERT( i != _canvases.end( ));
    _canvases.erase( i );
}

bool Config::init( const uint32_t initID )
{
    EQASSERT( !_running );
    _currentFrame = 0;
    _unlockedFrame = 0;
    _finishedFrame = 0;

    ConfigInitPacket packet;
    packet.requestID  = _requestHandler.registerRequest();
    packet.initID     = initID;

    send( packet );
    
    ClientPtr client = getClient();
    while( !_requestHandler.isServed( packet.requestID ))
        client->processCommand();

    _requestHandler.waitRequest( packet.requestID, _running );

    handleEvents();
    return _running;
}

bool Config::exit()
{
    finishAllFrames();

    ConfigExitPacket packet;
    packet.requestID = _requestHandler.registerRequest();
    send( packet );

    ClientPtr client = getClient();
    while( !_requestHandler.isServed( packet.requestID ))
        client->processCommand();

    bool ret = false;
    _requestHandler.waitRequest( packet.requestID, ret );

    while( tryNextEvent( )) /* flush all pending events */ ;
    if( _lastEvent )
        _lastEvent->release();
    _eventQueue.flush();
    _lastEvent = 0;
    _running = false;

    _appNode   = 0;
    _appNodeID = net::NodeID::ZERO;
    return ret;
}

uint32_t Config::startFrame( const uint32_t frameID )
{
    ConfigStatistics stat( Statistic::CONFIG_START_FRAME, this );

    ConfigCommitVisitor committer;
    accept( committer );
    const std::vector< net::ObjectVersion >& changes = committer.getChanges();
    
    if( committer.needsFinish( ))
        finishAllFrames();
    
    // Request new frame
    ConfigStartFramePacket packet;
    packet.frameID   = frameID;
    packet.nChanges  = changes.size();

    send( packet, changes );
    ++_currentFrame;

    EQLOG( base::LOG_ANY ) << "---- Started Frame ---- " << _currentFrame
                           << std::endl;
    stat.event.data.statistic.frameNumber = _currentFrame;
    return _currentFrame;
}

uint32_t Config::finishFrame()
{
    ConfigStatistics        stat( Statistic::CONFIG_FINISH_FRAME, this );

    ClientPtr client        = getClient();
    const uint32_t   frameToFinish = (_currentFrame >= _latency) ? 
                                      _currentFrame - _latency : 0;
    const bool needsLocalSync = _needsLocalSync();
    {
        ConfigStatistics waitStat( Statistic::CONFIG_WAIT_FINISH_FRAME, this );
        
        // local draw sync
        if( needsLocalSync )
            while( _unlockedFrame < _currentFrame )
                client->processCommand();

        // local node  finish (frame-latency) sync
        if( !_nodes.empty( ))
        {
            EQASSERT( _nodes.size() == 1 );
            const Node* node = _nodes.front();

            while( node->getFinishedFrame() < frameToFinish )
                client->processCommand();
        }

        // global sync
        _finishedFrame.waitGE( frameToFinish );

        // handle directly, it would not be processed in time using the normal
        // event flow
        waitStat.event.data.statistic.frameNumber = frameToFinish;
        waitStat.event.data.statistic.endTime     = getTime();
        handleEvent( &waitStat.event );
        waitStat.ignore = true; // don't send again
    }

    handleEvents();
    
    // handle directly - see above
    stat.event.data.statistic.frameNumber = frameToFinish;
    stat.event.data.statistic.endTime     = getTime();
    handleEvent( &stat.event );
    stat.ignore = true; // don't send again

    _updateStatistics( frameToFinish );

    EQLOG( base::LOG_ANY ) << "---- Finished Frame --- " << frameToFinish
                           << " (" << _currentFrame << ')' << std::endl;
    return frameToFinish;
}

uint32_t Config::finishAllFrames()
{
    if( _finishedFrame == _currentFrame )
        return _currentFrame;

    EQLOG( base::LOG_ANY ) << "-- Finish All Frames --" << std::endl;
    ConfigFinishAllFramesPacket packet;
    send( packet );

    ClientPtr client = getClient();
    while( _finishedFrame < _currentFrame )
        client->processCommand();

    handleEvents();
    EQLOG( base::LOG_ANY ) << "-- Finished All Frames --" << std::endl;
    return _currentFrame;
}

void Config::sendEvent( ConfigEvent& event )
{
    EQASSERT( event.data.type != Event::STATISTIC ||
              event.data.statistic.type != Statistic::NONE );
    EQASSERT( _appNodeID );

    if( !_appNode )
    {
        net::NodePtr localNode = getLocalNode();
        _appNode = localNode->connect( _appNodeID );
    }
    EQASSERT( _appNode );

    event.sessionID = getID();
    EQLOG( LOG_EVENTS ) << "send event " << &event << std::endl;
    _appNode->send( event );
}

const ConfigEvent* Config::nextEvent()
{
    if( _lastEvent )
        _lastEvent->release();
    _lastEvent = _eventQueue.pop();
    return _lastEvent->getPacket<ConfigEvent>();
}

const ConfigEvent* Config::tryNextEvent()
{
    net::Command* command = _eventQueue.tryPop();
    if( !command )
        return 0;

    if( _lastEvent )
        _lastEvent->release();
    _lastEvent = command;
    return command->getPacket<ConfigEvent>();
}

void Config::handleEvents()
{
    for( const ConfigEvent* event = tryNextEvent(); event; 
         event = tryNextEvent( ))
    {
        if( !handleEvent( event ))
            EQVERB << "Unhandled " << event << std::endl;
    }
}

bool Config::handleEvent( const ConfigEvent* event )
{
    switch( event->data.type )
    {
        case Event::EXIT:
        case Event::WINDOW_CLOSE:
            _running = false;
            return true;

        case Event::KEY_PRESS:
            if( event->data.keyPress.key == KC_ESCAPE )
            {
                _running = false;
                return true;
            }    
            break;

        case Event::POINTER_BUTTON_PRESS:
            if( event->data.pointerButtonPress.buttons == 
                ( PTR_BUTTON1 | PTR_BUTTON2 | PTR_BUTTON3 ))
            {
                _running = false;
                return true;
            }
            break;

        case Event::STATISTIC:
        {
            EQLOG( LOG_STATS ) << event->data << std::endl;

            const uint32_t originator = event->data.originator;
            EQASSERT( originator != EQ_ID_INVALID );
            if( originator == EQ_ID_INVALID )
                return false;

            const Statistic& statistic = event->data.statistic;
            const uint32_t   frame     = statistic.frameNumber;
            EQASSERT( statistic.type != Statistic::NONE )

            if( frame == 0 ||      // Not a frame-related stat event or
                statistic.type == Statistic::NONE ) // No event-type set
            {
                return false;
            }

            base::ScopedMutex mutex( _statisticsMutex );

            for( std::deque< FrameStatistics >::iterator i =_statistics.begin();
                 i != _statistics.end(); ++i )
            {
                FrameStatistics& frameStats = *i;
                if( frameStats.first == frame )
                {
                    SortedStatistics& sortedStats = frameStats.second;
                    Statistics&       statistics  = sortedStats[ originator ];
                    statistics.push_back( statistic );
                    return false;
                }
            }
            
            _statistics.push_back( FrameStatistics( ));
            FrameStatistics& frameStats = _statistics.back();
            frameStats.first = frame;

            SortedStatistics& sortedStats = frameStats.second;
            Statistics&       statistics  = sortedStats[ originator ];
            statistics.push_back( statistic );
            
            return false;
        }

        case Event::VIEW_RESIZE:
        {
            View* view = findView( event->data.originator );
            EQASSERT( view );
            if( view )
                return view->handleEvent( event->data );
            break;
        }
    }

    return false;
}

bool Config::_needsLocalSync() const
{
    if( _nodes.empty( ))
        return false;

    EQASSERT( _nodes.size() == 1 );
    const Node* node = _nodes.front();
    switch( node->getIAttribute( Node::IATTR_THREAD_MODEL ))
    {
        case ASYNC:
            return false;
            break;

        case DRAW_SYNC:
            if( !(node->getTasks() & TASK_DRAW) )
                return false;
            break;

        case LOCAL_SYNC:
            if( node->getTasks() == TASK_NONE )
                return false;
            break;
                
        default:
            EQUNIMPLEMENTED;
    }

    return true;
}

void Config::_updateStatistics( const uint32_t finishedFrame )
{
    // keep statistics for five frames
    _statisticsMutex.set();
    while( !_statistics.empty() &&
           finishedFrame - _statistics.front().first > 5 )
    {
        _statistics.pop_front();
    }
    _statisticsMutex.unset();
}

void Config::getStatistics( std::vector< FrameStatistics >& statistics )
{
    _statisticsMutex.set();

    for( std::deque< FrameStatistics >::const_iterator i = _statistics.begin();
         i != _statistics.end(); ++i )
    {
        if( (*i).first <= _finishedFrame.get( ))
            statistics.push_back( *i );
    }

    _statisticsMutex.unset();
}

void Config::setWindowSystem( const WindowSystem windowSystem )
{
    // called from pipe threads - but only during init
    static base::Lock _lock;
    base::ScopedMutex mutex( _lock );

    if( _eventQueue.getWindowSystem() == WINDOW_SYSTEM_NONE )
    {
        _eventQueue.setWindowSystem( windowSystem );
        EQVERB << "Client event message pump set up for " << windowSystem
               << std::endl;
    }
    else if( _eventQueue.getWindowSystem() != windowSystem )
        EQWARN << "Can't switch to window system " << windowSystem 
               << ", already using " <<  _eventQueue.getWindowSystem()
               << std::endl;

    ClientPtr client = getClient();
    client->setWindowSystem( windowSystem );    
}

#ifdef EQ_USE_DEPRECATED
void Config::setHeadMatrix( const eq::Matrix4f& matrix )
{
    for( ObserverVector::const_iterator i = _observers.begin();
         i != _observers.end(); ++i )
    {
        (*i)->setHeadMatrix( matrix );
    }
}

const eq::Matrix4f& Config::getHeadMatrix() const
{
    if( _observers.empty( ))
        return eq::Matrix4f::IDENTITY;

    return _observers[0]->getHeadMatrix();
}

void Config::setEyeBase( const float eyeBase )
{
    for( ObserverVector::const_iterator i = _observers.begin();
         i != _observers.end(); ++i )
    {
        (*i)->setEyeBase( eyeBase );
    }
}

float Config::getEyeBase() const
{
    if( _observers.empty( ))
        return _eyeBase;

    return _observers[0]->getEyeBase();
}
#endif

void Config::freezeLoadBalancing( const bool onOff )
{
    ConfigFreezeLoadBalancingPacket packet;
    packet.freeze = onOff;
    send( packet );
}

void Config::_initAppNode( const uint32_t distributorID )
{
    ConfigDeserializer distributor( this );
    EQCHECK( mapObject( &distributor, distributorID ));
    unmapObject( &distributor ); // data was retrieved, unmap
}

//---------------------------------------------------------------------------
// command handlers
//---------------------------------------------------------------------------
net::CommandResult Config::_cmdCreateNode( net::Command& command )
{
    const ConfigCreateNodePacket* packet = 
        command.getPacket<ConfigCreateNodePacket>();
    EQVERB << "Handle create node " << packet << std::endl;
    EQASSERT( packet->nodeID != EQ_ID_INVALID );

    Node* node = Global::getNodeFactory()->createNode( this );
    attachObject( node, packet->nodeID, EQ_ID_INVALID );

    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdDestroyNode( net::Command& command ) 
{
    const ConfigDestroyNodePacket* packet =
        command.getPacket<ConfigDestroyNodePacket>();
    EQVERB << "Handle destroy node " << packet << std::endl;

    Node* node = _findNode( packet->nodeID );
    if( !node )
        return net::COMMAND_HANDLED;

    detachObject( node );
    Global::getNodeFactory()->releaseNode( node );

    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdInitReply( net::Command& command )
{
    const ConfigInitReplyPacket* packet = 
        command.getPacket<ConfigInitReplyPacket>();
    EQVERB << "handle init reply " << packet << std::endl;

    if( !packet->result )
        _error = packet->error;

    _requestHandler.serveRequest( packet->requestID, (void*)(packet->result) );
    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdExitReply( net::Command& command )
{
    const ConfigExitReplyPacket* packet = 
        command.getPacket<ConfigExitReplyPacket>();
    EQVERB << "handle exit reply " << packet << std::endl;

    _requestHandler.serveRequest( packet->requestID, (void*)(packet->result) );
    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdReleaseFrameLocal( net::Command& command )
{
    const ConfigReleaseFrameLocalPacket* packet =
        command.getPacket< ConfigReleaseFrameLocalPacket >();

    releaseFrameLocal( packet->frameNumber );
    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdFrameFinish( net::Command& command )
{
    const ConfigFrameFinishPacket* packet = 
        command.getPacket<ConfigFrameFinishPacket>();
    EQLOG( LOG_TASKS ) << "frame finish " << packet << std::endl;

    _finishedFrame = packet->frameNumber;

    if( _unlockedFrame < _finishedFrame.get( ))
    {
        EQWARN << "Finished frame " << _unlockedFrame 
               << " was not locally unlocked, enforcing unlock" << std::endl;
        _unlockedFrame = _finishedFrame.get();
    }

    getNodeThreadQueue()->wakeup();
    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdSyncClock( net::Command& command )
{
    const ConfigSyncClockPacket* packet = 
        command.getPacket< ConfigSyncClockPacket >();

    EQVERB << "sync global clock to " << packet->time << ", drift " 
           << packet->time - _clock.getTime64() << std::endl;

    _clock.set( packet->time );
    return net::COMMAND_HANDLED;
}

net::CommandResult Config::_cmdUnmap( net::Command& command )
{
    const ConfigUnmapPacket* packet = command.getPacket< ConfigUnmapPacket >();
    EQVERB << "Handle unmap " << packet << std::endl;

    NodeFactory* nodeFactory = Global::getNodeFactory();

    for( CanvasVector::const_iterator i = _canvases.begin();
         i != _canvases.end(); ++i )
    {
        Canvas* canvas = *i;
        canvas->_deregister();
        canvas->_config = 0;
        nodeFactory->releaseCanvas( canvas );
    }
    _canvases.clear();

    for( LayoutVector::const_iterator i = _layouts.begin();
         i != _layouts.end(); ++i )
    {
        Layout* layout = *i;
        layout->_deregister();
        layout->_config = 0;
        nodeFactory->releaseLayout( layout );
    }
    _layouts.clear();

    for( ObserverVector::const_iterator i = _observers.begin();
         i != _observers.end(); ++i )
    {
        Observer* observer = *i;
        observer->deregister();
        observer->_config = 0;
        nodeFactory->releaseObserver( observer );
    }
    _observers.clear();

    ConfigUnmapReplyPacket reply( packet );
    send( command.getNode(), reply );

    return net::COMMAND_HANDLED;
}

}
