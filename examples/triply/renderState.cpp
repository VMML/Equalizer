
/* Copyright (c) 2011-2012, Stefan Eilemann <eile@eyescale.ch>
 *                    2015, Enrique G. Paredes <egparedes@ifi.uzh.ch>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of Eyescale Software GmbH nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include "renderState.h"
#include <lunchbox/scopedMutex.h>

namespace triply 
{
RenderState::RenderState( const GLEWContext* glewContext )
        : _pmvMatrix( Matrix4f::IDENTITY )
        , _glewContext( glewContext )
        , _renderMode( RENDER_MODE_BUFFER_OBJECT ) /* RENDER_MODE_DISPLAY_LIST  */
        , _useColors( false )
        , _useFrustumCulling( true )
        , _useBoundingSpheres( false )
        , _outOfCore( false )
        , _allocatedBufferMemory( 0 )
        , _maxBufferMemory( 2ull*1024*1024*1024 ) // 2 Gib
{
    _range[0] = 0.f;
    _range[1] = 1.f;
    resetRegion();
    TRIPLYASSERT( glewContext );
} 

void RenderState::setRenderMode( const RenderMode mode )
{ 
    if( _renderMode == mode )
        return;

    _renderMode = mode;

    // Check if VBO funcs available, else fall back to display lists
    if( _renderMode == RENDER_MODE_BUFFER_OBJECT && !GLEW_VERSION_1_5 )
    {
        TRIPLYINFO << "VBO not available, using display lists" << std::endl;
        _renderMode = RENDER_MODE_DISPLAY_LIST;
    }
}

void RenderState::resetRegion()
{
    _region[0] = std::numeric_limits< float >::max();
    _region[1] = std::numeric_limits< float >::max();
    _region[2] = -std::numeric_limits< float >::max();
    _region[3] = -std::numeric_limits< float >::max();
}

void RenderState::updateRegion( const BoundingBox& box )
{
    const Vertex corners[8] = { Vertex( box[0][0], box[0][1], box[0][2] ),
                                Vertex( box[1][0], box[0][1], box[0][2] ),
                                Vertex( box[0][0], box[1][1], box[0][2] ),
                                Vertex( box[1][0], box[1][1], box[0][2] ),
                                Vertex( box[0][0], box[0][1], box[1][2] ),
                                Vertex( box[1][0], box[0][1], box[1][2] ),
                                Vertex( box[0][0], box[1][1], box[1][2] ),
                                Vertex( box[1][0], box[1][1], box[1][2] ) };

    Vector4f region(  std::numeric_limits< float >::max(),
                      std::numeric_limits< float >::max(),
                     -std::numeric_limits< float >::max(),
                     -std::numeric_limits< float >::max( ));

    for( size_t i = 0; i < 8; ++i )
    {
        const Vertex corner = _pmvMatrix * corners[i];
        region[0] = std::min( corner[0], region[0] );
        region[1] = std::min( corner[1], region[1] );
        region[2] = std::max( corner[0], region[2] );
        region[3] = std::max( corner[1], region[3] );
    }

    // transform region of interest from [ -1 -1 1 1 ] to normalized viewport
    const Vector4f normalized( region[0] * .5f + .5f,
                               region[1] * .5f + .5f,
                               ( region[2] - region[0] ) * .5f,
                               ( region[3] - region[1] ) * .5f );

    declareRegion( normalized );
    _region[0] = std::min( _region[0], normalized[0] );
    _region[1] = std::min( _region[1], normalized[1] );
    _region[2] = std::max( _region[2], normalized[2] );
    _region[3] = std::max( _region[3], normalized[3] );
}


Vector4f RenderState::getRegion() const
{
    if( _region[0] > _region[2] || _region[1] > _region[3] )
        return Vector4f::ZERO;

    return _region;
}

GLuint RenderState::reserveBufferObject( ResourceKey key, size_t size,
                                         GLenum glTarget, GLenum glUsage )
{
    lunchbox::ScopedFastWrite mutex( &_lock );

    TRIPLYASSERT( size - 1 < BufferSizeUnit * _availableBuffers.size( ));

    const size_t bufferCacheId = ( size - 1 ) / BufferSizeUnit;
    const size_t bufferSize = ( bufferCacheId + 1 ) * BufferSizeUnit;

    GLuint bufferId = INVALID;
    auto cacheIt = _cacheMap.find( key );
    if( cacheIt != _cacheMap.end( ))
    {
        TRIPLYASSERT( cacheIt->second.cacheId == bufferCacheId );
        if( cacheIt->second.cacheId == bufferCacheId )
        {
            bufferId = getBufferObject( key );
            if( bufferId != INVALID )
            {
                if( cacheIt->second.active == false )
                {
                    cacheIt->second.active = true;
                    _availableBuffers[cacheIt->second.cacheId].remove( key );
                }
                TRIPLY_GL_CALL( glBindBuffer( glTarget, bufferId ));
            }
        }

        return bufferId;
    }

    const size_t MinCacheItems = 0; //( 0.01 * _maxBufferMemory / BufferSizesCount ) / BufferSizeUnit;
    ResourceKey deletedKey = 0;
    if( _allocatedBufferMemory + bufferSize > _maxBufferMemory )
    {
        if( _availableBuffers[bufferCacheId].size() > MinCacheItems )
        {
            _availableBuffers[bufferCacheId].pop( &deletedKey );
            TRIPLYASSERT( deletedKey != 0 && _cacheMap.count( deletedKey ) > 0 );
            _cacheMap.erase( deletedKey );
            bufferId = getBufferObject( deletedKey );
            TRIPLYASSERT( bufferId != INVALID );
            if( bufferId != INVALID )
            {
                _cacheMap[key] = KeyInfo( bufferCacheId, true );
                remapBufferObject( deletedKey, key );
                TRIPLY_GL_CALL( glInvalidateBufferData( bufferId ));
            }
        }
        else
        {
            size_t tmpCacheId = ( bufferCacheId + 1 ) % BufferSizesCount;
            size_t tmpBufferSize = ( tmpCacheId + 1 ) * BufferSizeUnit;
            size_t disposableMemory = 0;
            while( _allocatedBufferMemory + bufferSize > _maxBufferMemory )
            {
                if( _availableBuffers[tmpCacheId].size() > MinCacheItems )
                {
                    deletedKey = 0;
                    _availableBuffers[tmpCacheId].pop( &deletedKey );
                    TRIPLYASSERT( deletedKey != 0 && _cacheMap.count( deletedKey ) > 0 );
                    _cacheMap.erase( deletedKey );
                    bufferId = getBufferObject( deletedKey );
                    TRIPLYASSERT( bufferId != INVALID );
                    deleteBufferObject( deletedKey );
                    bufferId = INVALID ;
                    _allocatedBufferMemory -= tmpBufferSize;
                }

                disposableMemory +=
                        _availableBuffers[tmpCacheId].size() * ( tmpCacheId + 1 ) * BufferSizeUnit;
                tmpCacheId = ( tmpCacheId + 1 ) % BufferSizesCount;

                if( tmpCacheId == bufferCacheId && disposableMemory < bufferSize )
                {
                    throw RenderException( "GPU manager run out of memory!!" );
//                    TRIPLYERROR << "GPU manager run out of memory!!";
//                    return INVALID;
                }
            }
        }
    }

    if( bufferId == INVALID )
    {
        bufferId = newBufferObject( key );
        if( bufferId != INVALID )
        {
            _allocatedBufferMemory += bufferSize;
            _cacheMap[key] = KeyInfo( bufferCacheId, true );
        }
    }

    TRIPLYASSERT( bufferId != INVALID );

    if( bufferId != INVALID )
    {
        TRIPLY_GL_CALL( glBindBuffer( glTarget, bufferId ));
        TRIPLY_GL_CALL( glBufferData( glTarget, bufferSize, 0, glUsage ));
    }

    return bufferId;
}

GLuint RenderState::bindBufferObject( ResourceKey key, GLenum glTarget )
{
    GLuint bufferId = useBufferObject( key );
    if( bufferId == INVALID )
        throw RenderException( "Missing allocated VBO!!" );

    TRIPLY_GL_CALL( glBindBuffer( glTarget, bufferId ));

    return bufferId;
}

GLuint RenderState::useBufferObject( ResourceKey key )
{
    lunchbox::ScopedFastWrite mutex( &_lock );

    GLuint bufferId = getBufferObject( key );
    if( bufferId != INVALID )
    {
        auto it = _cacheMap.find( key );
        if( it != _cacheMap.end( ) && it->second.active == false )
        {
            it->second.active = true;
            _availableBuffers[it->second.cacheId].remove( key );
        }
    }

    return bufferId;
}

void RenderState::discardBufferObject( ResourceKey key )
{
    lunchbox::ScopedFastWrite mutex( &_lock );

    auto it = _cacheMap.find( key );
    if( it != _cacheMap.end( ) && it->second.active == true )
    {
        it->second.active = false;
        _availableBuffers[it->second.cacheId].push( key );
    }
}

void RenderState::removeBufferObject( ResourceKey key )
{
    lunchbox::ScopedFastWrite mutex( &_lock );

    auto it = _cacheMap.find( key );
    if( it != _cacheMap.end( ))
    {
        const size_t bufferCacheId = it->second.cacheId;
        const size_t bufferSize = ( bufferCacheId + 1 ) * BufferSizeUnit;
        _availableBuffers[bufferCacheId].remove( key );
        _cacheMap.erase( it );
        deleteBufferObject( key );
        _allocatedBufferMemory -= bufferSize;
    }
}

// ---- SimpleRenderState
GLuint SimpleRenderState::getDisplayList( const void* key )
{
    if( _displayLists.find( key ) == _displayLists.end() )
        return INVALID;
    return _displayLists[key];
}
        
GLuint SimpleRenderState::newDisplayList( const void* key )
{
    _displayLists[key] = glGenLists( 1 );
    return _displayLists[key];
}

void SimpleRenderState::deleteDisplayList( const void* key )
{
    if( _displayLists.find( key ) != _displayLists.end() )
    {
        TRIPLY_GL_CALL( glDeleteLists( _displayLists[key], 1 ));
        _displayLists.erase( key );
    }
}
        
GLuint SimpleRenderState::getBufferObject( const void* key )
{
    if( _bufferObjects.find( key ) == _bufferObjects.end() )
        return INVALID;
    return _bufferObjects[key];
}
        
GLuint SimpleRenderState::newBufferObject( const void* key )
{
    if( !GLEW_VERSION_1_5 )
        return INVALID;
    TRIPLY_GL_CALL( glGenBuffers( 1, &_bufferObjects[key] ));
    return _bufferObjects[key];
}
        
void SimpleRenderState::deleteBufferObject( const void* key )
{
    if( _bufferObjects.find( key ) != _bufferObjects.end() )
    {
        TRIPLY_GL_CALL( glDeleteBuffers( 1, &_bufferObjects[key] ));
        _bufferObjects.erase( key );
    }
}

bool SimpleRenderState::remapBufferObject( ResourceKey deletedKey,
                                           ResourceKey key )
{
    GLMap::iterator it = _bufferObjects.find( deletedKey );
    if( it != _bufferObjects.end() && deletedKey != key )
    {
        _bufferObjects[key] = it->second;
        _bufferObjects.erase( it );
        return true;
    }
    return false;
}

GLuint SimpleRenderState::getVertexArray( const void* key )
{
    if( _vertexArrays.find( key ) == _vertexArrays.end() )
        return INVALID;
    return _vertexArrays[key];
}

GLuint SimpleRenderState::newVertexArray( const void* key )
{
    TRIPLY_GL_CALL( glGenVertexArrays( 1, &_vertexArrays[key] ));
    return _vertexArrays[key];
}

void SimpleRenderState::deleteVertexArray( const void* key )
{
    if( _vertexArrays.find( key ) != _vertexArrays.end() )
    {
        TRIPLY_GL_CALL( glDeleteVertexArrays( 1, &_vertexArrays[key] ));
        _vertexArrays.erase( key );
    }
}

void SimpleRenderState::deleteGlObjects()
{
    for( GLMapCIter i = _displayLists.begin(); i != _displayLists.end(); ++i )
        TRIPLY_GL_CALL( glDeleteLists( i->second, 1 ));

    for( GLMapCIter i = _bufferObjects.begin(); i != _bufferObjects.end(); ++i )
        TRIPLY_GL_CALL( glDeleteBuffers( 1, &(i->second) ));

    for( GLMapCIter i = _vertexArrays.begin(); i != _vertexArrays.end(); ++i )
        TRIPLY_GL_CALL( glDeleteVertexArrays( 1, &(i->second) ));

    _displayLists.clear();
    _bufferObjects.clear();
    _vertexArrays.clear();
}

}
