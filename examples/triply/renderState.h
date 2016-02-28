
/* Copyright (c) 2009-2012, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2007, Tobias Wolf <twolf@access.unizh.ch>
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


#ifndef TRIPLY_RENDERSTATE_H
#define TRIPLY_RENDERSTATE_H

#include "typedefs.h"
#include "lruCache.h"
#include <triply/api.h>
#include <map>

namespace triply
{
/*  The abstract base class for model-tree rendering state.  */
class RenderState
{
public:
    enum
    {
        INVALID = 0 //<! return value for failed operations.
    };

    typedef const void* ResourceKey;

    TRIPLY_API virtual bool useColors() const { return _useColors; }
    TRIPLY_API virtual void setColors( const bool colors ) { _useColors = colors; }
    TRIPLY_API virtual bool stopRendering() const { return false; }
    TRIPLY_API virtual RenderMode getRenderMode() const { return _renderMode; }
    TRIPLY_API virtual void setRenderMode( const RenderMode mode );
    TRIPLY_API virtual bool useFrustumCulling() const { return _useFrustumCulling; }
    TRIPLY_API virtual void setFrustumCulling( const bool frustumCullingState )
        { _useFrustumCulling = frustumCullingState; }
    TRIPLY_API virtual bool showBoundingSpheres() const { return _useBoundingSpheres; }
    TRIPLY_API virtual void setBoundingSpheres( const bool boundingSpheresState )
        { _useBoundingSpheres = boundingSpheresState; }
    TRIPLY_API virtual bool useOutOfCore() const { return _outOfCore; }
    TRIPLY_API virtual void setOutOfCore( const bool outOfCore )
        { _outOfCore = outOfCore; }

    TRIPLY_API void setProjectionModelViewMatrix( const Matrix4f& pmv )
        { _pmvMatrix = pmv; }
    TRIPLY_API const Matrix4f& getProjectionModelViewMatrix() const
        { return _pmvMatrix; }

    TRIPLY_API void setRange( const Range& range ) { _range = range; }
    TRIPLY_API const Range& getRange() const { return _range; }

    TRIPLY_API void resetRegion();
    TRIPLY_API void updateRegion( const BoundingBox& box );
    TRIPLY_API virtual void declareRegion( const Vector4f& ) {}
    TRIPLY_API Vector4f getRegion() const;

    TRIPLY_API virtual GLuint getDisplayList( ResourceKey key ) = 0;
    TRIPLY_API virtual GLuint newDisplayList( ResourceKey key ) = 0;
    TRIPLY_API virtual void deleteDisplayList( ResourceKey key ) = 0;

    TRIPLY_API virtual GLuint getBufferObject( ResourceKey key ) = 0;
    TRIPLY_API virtual GLuint newBufferObject( ResourceKey key ) = 0;
    TRIPLY_API virtual void deleteBufferObject( ResourceKey key ) = 0;
    TRIPLY_API virtual bool remapBufferObject( ResourceKey deletedKey,
                                               ResourceKey key ) = 0;

    TRIPLY_API virtual GLuint getVertexArray( ResourceKey key ) = 0;
    TRIPLY_API virtual GLuint newVertexArray( ResourceKey key ) = 0;
    TRIPLY_API virtual void deleteVertexArray( ResourceKey key ) = 0;

    TRIPLY_API virtual void deleteGlObjects() = 0;

    TRIPLY_API const GLEWContext* glewGetContext() const
        { return _glewContext; }

    TRIPLY_API GLuint reserveBufferObject( ResourceKey key, size_t size,
                                           GLenum glTarget, GLenum glUsage );
    TRIPLY_API GLuint bindBufferObject( ResourceKey key, GLenum glTarget );
    TRIPLY_API GLuint useBufferObject( ResourceKey key );
    TRIPLY_API void discardBufferObject( ResourceKey key );

    TRIPLY_API void setMaxBufferMemory( size_t maxMemSize )
        { _maxBufferMemory = maxMemSize; }
    TRIPLY_API size_t getMaxBufferMemory()
        { return _maxBufferMemory; }

protected:
    TRIPLY_API explicit RenderState( const GLEWContext* glewContext );
    TRIPLY_API virtual ~RenderState() {}

    Matrix4f            _pmvMatrix; //!< projection * modelView matrix
    Range               _range; //!< normalized [0,1] part of the model to draw
    const GLEWContext* const _glewContext;
    RenderMode          _renderMode;
    Vector4f            _region; //!< normalized x1 y1 x2 y2 region from cullDraw
    bool                _useColors;
    bool                _useFrustumCulling;
    bool                _useBoundingSpheres;
    bool                _outOfCore;

private:
    static const size_t BufferSizeUnit = 65536; // 64 Kib
    static const size_t BufferSizesCount = 20;

    size_t _currentBufferMemory;
    size_t _maxBufferMemory;
    stde::hash_map< ResourceKey, size_t > _keyCacheMap;
    std::array< LRUCache< ResourceKey >, BufferSizesCount > _lruBufferCaches;
};


/*  Simple state for stand-alone single-pipe usage.  */
class SimpleRenderState : public RenderState
{
private:
    typedef std::map< ResourceKey, GLuint > GLMap;
    typedef GLMap::const_iterator GLMapCIter;

public:
    TRIPLY_API explicit SimpleRenderState( const GLEWContext* glewContext )
        : RenderState( glewContext ) {}

    TRIPLY_API virtual GLuint getDisplayList( ResourceKey key );
    TRIPLY_API virtual GLuint newDisplayList( ResourceKey key );
    TRIPLY_API virtual void deleteDisplayList( ResourceKey key );

    TRIPLY_API virtual GLuint getBufferObject( ResourceKey key );
    TRIPLY_API virtual GLuint newBufferObject( ResourceKey key );
    TRIPLY_API virtual void deleteBufferObject( ResourceKey key );
    TRIPLY_API virtual bool remapBufferObject( ResourceKey deletedKey,
                                               ResourceKey key );

    TRIPLY_API virtual GLuint getVertexArray( ResourceKey key );
    TRIPLY_API virtual GLuint newVertexArray( ResourceKey key );
    TRIPLY_API virtual void deleteVertexArray( ResourceKey key );

    TRIPLY_API virtual void deleteGlObjects();

private:
    GLMap  _displayLists;
    GLMap  _bufferObjects;
    GLMap  _vertexArrays;
};
} // namespace triply


#endif // TRIPLY_RENDERSTATE_H
