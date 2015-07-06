
/* Copyright (c) 2007-2014, Stefan Eilemann <eile@equalizergraphics.com>
 *               2011-2012, Daniel Nachbaur <danielnachbaur@gmail.com>
 *               2013-2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "compoundUpdateOutputVisitor.h"

#include "config.h"
#include "frame.h"
#include "frameData.h"
#include "log.h"
#include "server.h"
#include "tileQueue.h"
#include "chunkQueue.h"
#include "window.h"
#include "node.h"

#include "tiles/hilbertStrategy.h"
// #include "tiles/mortonStrategy.h"
// #include "tiles/zigzagStrategy.h"

#include <eq/fabric/iAttribute.h>
#include <eq/fabric/tile.h>
#include <eq/fabric/chunk.h>

namespace eq
{
namespace server
{
CompoundUpdateOutputVisitor::CompoundUpdateOutputVisitor(
    const uint32_t frameNumber )
        : _frameNumber( frameNumber )
{}

VisitorResult CompoundUpdateOutputVisitor::visit( Compound* compound )
{
    if( !compound->isActive( ))
        return TRAVERSE_PRUNE;

    _updateQueues( compound );
    _updateFrames( compound );
    _updateSwapBarriers( compound );

    return TRAVERSE_CONTINUE;
}

void CompoundUpdateOutputVisitor::_updateQueues( Compound* compound )
{
    const TileQueues* outputTileQueues;
    compound->getOutputPackageQueues( &outputTileQueues );
    for( TileQueuesCIter i = outputTileQueues->begin(); i != outputTileQueues->end(); ++i )
    {
        //----- Check uniqueness of output queue name
        TileQueue* queue  = *i;
        const std::string& name   = queue->getName();

        if( _outputTileQueues.find( name ) != _outputTileQueues.end( ))
        {
            LBWARN << "Multiple output queues of the same name are unsupported"
                << ", ignoring output queue " << name << std::endl;
            queue->unsetData();
            continue;
        }

        queue->cycleData( _frameNumber, compound );

        //----- Generate tile task commands
        _generateTiles( queue, compound );
        _outputTileQueues[name] = queue;
    }

    const ChunkQueues* outputChunkQueues;
    compound->getOutputPackageQueues( &outputChunkQueues );
    for( ChunkQueuesCIter i = outputChunkQueues->begin(); i != outputChunkQueues->end(); ++i )
    {
        //----- Check uniqueness of output queue name
        ChunkQueue* queue  = *i;
        const std::string& name   = queue->getName();

        if( _outputTileQueues.find( name ) != _outputTileQueues.end( ))
        {
            LBWARN << "Multiple output queues of the same name are unsupported"
                   << ", ignoring output queue " << name << std::endl;
            queue->unsetData();
            continue;
        }

        queue->cycleData( _frameNumber, compound );

        //----- Generate tile task commands
        _generateChunks( queue, compound );
        _outputChunkQueues[name] = queue;
    }
}

void CompoundUpdateOutputVisitor::_updateFrames( Compound* compound )
{
    const Frames& outputFrames = compound->getOutputFrames();
    if( outputFrames.empty( ))
        compound->unsetInheritTask( fabric::TASK_READBACK );

    const Channel* channel = compound->getChannel();
    if( !compound->testInheritTask( fabric::TASK_READBACK ) || !channel )
        return;

    for( FramesCIter i = outputFrames.begin(); i != outputFrames.end(); ++i )
    {
        //----- Check uniqueness of output frame name
        Frame*             frame  = *i;
        const std::string& name   = frame->getName();

        if( _outputFrames.find( name ) != _outputFrames.end())
        {
            LBWARN << "Multiple output frames of the same name are unsupported"
                   << ", ignoring output frame " << name << std::endl;
            frame->unsetData();
            continue;
        }

        //----- compute readback area
        const Viewport& frameVP = frame->getViewport();
        const PixelViewport& inheritPVP = compound->getInheritPixelViewport();
        PixelViewport framePVP( inheritPVP );
        framePVP.apply( frameVP );

        if( !framePVP.hasArea( )) // output frame has no pixels
        {
            LBINFO << "Skipping output frame " << name << ", no pixels"
                   << std::endl;
            frame->unsetData();
            continue;
        }

        //----- Create new frame datas
        // * one frame data used for each eye pass
        // * data is set only on master frame data (will copy to all others)
        frame->cycleData( _frameNumber, compound );
        FrameData* frameData = frame->getMasterData();
        LBASSERT( frameData );

        LBLOG( LOG_ASSEMBLY )
            << lunchbox::disableFlush << "Output frame \"" << name << "\" id "
            << frame->getID() << " v" << frame->getVersion()+1
            << " data id " << frameData->getID() << " v"
            << frameData->getVersion() + 1 << " on channel \""
            << channel->getName() << "\" tile pos " << framePVP.x << ", "
            << framePVP.y;

        //----- Set frame data parameters:
        // 1) offset is position wrt destination view, used only by input frames
        const TileQueues* inputTileQueues;
        compound->getInputPackageQueues( &inputTileQueues );
        const bool tiled = !inputTileQueues->empty();
        frameData->setOffset( tiled ? Vector2i( 0 , 0 ) :
                                      Vector2i( framePVP.x, framePVP.y ));

        // 2) pvp is area within channel
        framePVP.x = static_cast< int32_t >( frameVP.x * inheritPVP.w );
        framePVP.y = static_cast< int32_t >( frameVP.y * inheritPVP.h );
        frameData->setPixelViewport( framePVP );

        // 3) image buffers and storage type
        uint32_t buffers = frame->getBuffers();

        frameData->setType( frame->getType() );
        frameData->setBuffers( buffers == Frame::BUFFER_UNDEFINED ?
                                   compound->getInheritBuffers() : buffers );

        // 4) (source) render context
        frameData->setRange( compound->getInheritRange( ));
        frameData->setPixel( compound->getInheritPixel( ));
        frameData->setSubPixel( compound->getInheritSubPixel( ));
        frameData->setPeriod( compound->getInheritPeriod( ));
        frameData->setPhase( compound->getInheritPhase( ));

        //----- Set frame parameters:
        // 1) offset is position wrt window, i.e., the channel position
        if( compound->getInheritChannel() == channel )
            frame->setOffset( Vector2i( inheritPVP.x, inheritPVP.y ));
        else
        {
            const PixelViewport& nativePVP = channel->getPixelViewport();
            frame->setOffset( Vector2i( nativePVP.x, nativePVP.y ));
        }

        // 2) zoom
        _updateZoom( compound, frame );

        //----- Commit
        frame->commitData();

        _outputFrames[name] = frame;
        LBLOG( LOG_ASSEMBLY )
            << " buffers " << frameData->getBuffers() << " read area "
            << framePVP << " readback " << frame->getZoom() << " assemble "
            << frameData->getZoom()<< lunchbox::enableFlush << std::endl ;
    }
}

void CompoundUpdateOutputVisitor::_generateTiles( TileQueue* queue,
                                                  Compound* compound )
{
    const Vector2i& dim = queue->getPackageSize();
    const PixelViewport pvp = compound->getInheritPixelViewport();
    const Vector2i tileSize(pvp.w / dim.x(), pvp.h / dim.y( ));

    if( !pvp.hasArea( ))
        return;

    std::vector< Vector2i > tiles;
    tiles.reserve( dim.x() * dim.y() );

    tiles::generateHilbert( tiles, dim );
    _addTilesToQueue( queue, compound, tiles );
}

void CompoundUpdateOutputVisitor::_addTilesToQueue( TileQueue* queue,
                                                    Compound* compound,
                                         const std::vector< Vector2i >& tiles )
{
    const Vector2i& dim = queue->getPackageSize();
    const PixelViewport pvp = compound->getInheritPixelViewport();
    const Vector2i tileSize(pvp.w / dim.x(), pvp.h / dim.y( ));

    const double xFraction = 1.0 / pvp.w;
    const double yFraction = 1.0 / pvp.h;

    Config* config = compound->getConfig();
    Node* appNode = config->findAppNode();
    eq::server::Nodes nodes = config->getNodes();
    co::Nodes slaveNodes;
    for( eq::server::NodesCIter i = nodes.begin(); i != nodes.end(); ++i )
    {
        if(( *i)->getID() != appNode->getID( ))
        {
            slaveNodes.push_back(( *i )->getNode( ));
        }
    }

    for( fabric::Eye eye = fabric::EYE_CYCLOP; eye < fabric::EYES_ALL;
            eye = fabric::Eye(eye<<1) )
    {
        if ( !(compound->getInheritEyes() & eye) ||
                !compound->isInheritActive( eye ))
        {
            continue;
        }

        int nTiles = tiles.size();
        queue->selectPackageDistributor( eye );
        queue->setSlaveNodes( slaveNodes, eye );
        for( int i = 0; i < nTiles; ++i )
        {
            const Vector2i& tile = tiles[i];
            PixelViewport tilePVP( tile.x() * tileSize.x(), tile.y() * tileSize.y(),
                                   tileSize.x(), tileSize.y( ));

            if ( tilePVP.x + tileSize.x() > pvp.w ) // no full tile
                tilePVP.w = pvp.w - tilePVP.x;

            if ( tilePVP.y + tileSize.y() > pvp.h ) // no full tile
                tilePVP.h = pvp.h - tilePVP.y;

            const Viewport tileVP( tilePVP.x * xFraction, tilePVP.y * yFraction,
                                   tilePVP.w * xFraction, tilePVP.h * yFraction );

            Tile tileItem( tilePVP, tileVP );
            compound->computeTileFrustum( tileItem.frustum, eye, tileItem.vp,
                                          false );
            compound->computeTileFrustum( tileItem.ortho, eye, tileItem.vp,
                                          true );
            float position = (i + .5f) / nTiles;
            queue->addPackage( tileItem, position, eye );
        }
        queue->addEnd( eye );
    }
}

void CompoundUpdateOutputVisitor::_generateChunks( ChunkQueue* queue,
        Compound* compound )
{
    const float& chunkSize = queue->getPackageSize();
    if ( chunkSize == 0.0f )
        return;

    Config* config = compound->getConfig();
    Node* appNode = config->findAppNode();
    eq::server::Nodes nodes = config->getNodes();
    co::Nodes slaveNodes;
    for( eq::server::NodesCIter i = nodes.begin(); i != nodes.end(); ++i )
    {
        if(( *i)->getID() != appNode->getID( ))
        {
            slaveNodes.push_back(( *i )->getNode( ));
        }
    }

    for( fabric::Eye eye = fabric::EYE_CYCLOP; eye < fabric::EYES_ALL;
            eye = fabric::Eye( eye<<1 ) )
    {
        if ( !( compound->getInheritEyes() & eye ) ||
                !compound->isInheritActive( eye ) )
        {
            continue;
        }

        queue->selectPackageDistributor( eye );
        queue->setSlaveNodes( slaveNodes, eye );
        for ( float rangeBegin = 0.0f, rangeEnd = chunkSize;
                rangeBegin < 1.0f;
                rangeBegin = rangeEnd, rangeEnd += chunkSize )
        {
            Chunk chunk( Range( rangeBegin, rangeEnd > 1.0f ? 1.0f : rangeEnd ) );
            float position = (rangeBegin + rangeEnd) * .5f;
            queue->addPackage( chunk, position, eye );
        }
        queue->addEnd( eye );
    }
}

void CompoundUpdateOutputVisitor::_updateZoom( const Compound* compound,
                                               Frame* frame )
{
    Zoom zoom = frame->getNativeZoom();
    Zoom zoom_1;

    if( !zoom.isValid( )) // if zoom is not set, auto-calculate from parent
    {
        zoom_1 = compound->getInheritZoom();
        LBASSERT( zoom_1.isValid( ));
        zoom.x() = 1.0f / zoom_1.x();
        zoom.y() = 1.0f / zoom_1.y();
    }
    else
    {
        zoom_1.x() = 1.0f / zoom.x();
        zoom_1.y() = 1.0f / zoom.y();
    }

    if( frame->getType( ) == Frame::TYPE_TEXTURE )
    {
        FrameData* frameData = frame->getMasterData();
        frameData->setZoom( zoom_1 ); // textures are zoomed by input frame
        frame->setZoom( Zoom::NONE );
    }
    else
    {
        Zoom inputZoom;
        /* Output frames downscale pixel data during readback, and upscale it on
         * the input frame by setting the input frame's inherit zoom. */
        if( zoom.x() > 1.0f )
        {
            inputZoom.x() = zoom_1.x();
            zoom.x()      = 1.f;
        }
        if( zoom.y() > 1.0f )
        {
            inputZoom.y() = zoom_1.y();
            zoom.y()      = 1.f;
        }

        FrameData* frameData = frame->getMasterData();
        frameData->setZoom( inputZoom );
        frame->setZoom( zoom );
    }
}

void CompoundUpdateOutputVisitor::_updateSwapBarriers( Compound* compound )
{
    SwapBarrierConstPtr swapBarrier = compound->getSwapBarrier();
    if( !swapBarrier )
        return;

    Window* window = compound->getWindow();
    LBASSERT( window );
    if( !window )
        return;

    if( swapBarrier->isNvSwapBarrier( ))
    {
        if( !window->hasNVSwapBarrier( ))
        {
            const std::string name( "__NV_swap_group_protection_barrier__" );
            _swapBarriers[name] =
                window->joinNVSwapBarrier( swapBarrier, _swapBarriers[name] );
        }
    }
    else
    {
        const std::string& name = swapBarrier->getName();
        _swapBarriers[name] = window->joinSwapBarrier( _swapBarriers[name] );
    }
}

}
}
