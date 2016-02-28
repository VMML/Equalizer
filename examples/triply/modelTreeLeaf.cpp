
/* Copyright (c) 2007, Tobias Wolf <twolf@access.unizh.ch>
 *          2008-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *               2015, Enrique G. Paredes <egparedes@ifi.uzh.ch>
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


#include "modelTreeLeaf.h"
#include "modelTreeData.h"
#include "meshData.h"
#include "renderState.h"

#define glewGetContext state.glewGetContext

namespace triply
{

namespace detail
{
struct DrawElementsIndirectCommand
{
    DrawElementsIndirectCommand( uint countArg=0, uint primCountArg=0, uint firstIndexArg=0,
                                 uint baseVertexArg=0, uint baseInstanceArg=0)
        : count( countArg ), primCount( primCountArg ), firstIndex( firstIndexArg ),
          baseVertex( baseVertexArg ), baseInstance( baseInstanceArg )
    { }

    uint  count;
    uint  primCount;
    uint  firstIndex;
    uint  baseVertex;
    uint  baseInstance;
};
} // namespace detail


ModelTreeLeaf::ModelTreeLeaf( ModelTreeData &treeData )
    : _treeData( treeData ),
      _indexStart( 0 ), _indexLength( 0 ), _vertexStart( 0 ), _vertexLength( 0 ),
      _dataLoaded( false )
{ }

ModelTreeLeaf::ModelTreeLeaf( ModelTreeData& treeData,
                              Index indexStart, Index indexLength,
                              Index vertexStart, ShortIndex vertexLength )
    : _treeData( treeData ),
      _indexStart( indexStart ), _indexLength( indexLength ),
      _vertexStart( vertexStart ), _vertexLength( vertexLength ),
      _dataLoaded( false )
{ }

void ModelTreeLeaf::clear()
{
    _boundingBox[0] = Vertex::ZERO;
    _boundingBox[1] = Vertex::ZERO;
    _indexStart = 0;
    _indexLength = 0;
    _vertexStart = 0;
    _vertexLength = 0;
}

/*  Compute the bounding sphere of the leaf's indexed vertices.  */
const BoundingSphere& ModelTreeLeaf::updateBoundingSphere()
{
    // Initialize leaf bounding box using the vertices inside the leaf node
    _boundingBox[0] = _treeData.vertex( _vertexStart );
    _boundingBox[1] = _treeData.vertex( _vertexStart );

    for( Index i = 1; i < _vertexLength; ++i )
    {
        const Vertex& vertex = _treeData.vertex( _vertexStart + i );
        _boundingBox[0][0] = std::min( _boundingBox[0][0], vertex[0] );
        _boundingBox[1][0] = std::max( _boundingBox[1][0], vertex[0] );
        _boundingBox[0][1] = std::min( _boundingBox[0][1], vertex[1] );
        _boundingBox[1][1] = std::max( _boundingBox[1][1], vertex[1] );
        _boundingBox[0][2] = std::min( _boundingBox[0][2], vertex[2] );
        _boundingBox[1][2] = std::max( _boundingBox[1][2], vertex[2] );
    }

    // We determine a bounding sphere by:
    // 1) Using the inner sphere of the leaf cell bounding box as an estimate
    // 2) Test all points to be in that sphere
    // 3) Expand the sphere to contain all points outside.

    // 1) get inner sphere of bounding box as an initial estimate      
    _boundingSphere.x() = ( _boundingBox[0].x() + _boundingBox[1].x() ) * 0.5f;
    _boundingSphere.y() = ( _boundingBox[0].y() + _boundingBox[1].y() ) * 0.5f;
    _boundingSphere.z() = ( _boundingBox[0].z() + _boundingBox[1].z() ) * 0.5f;

    _boundingSphere.w()  = std::max( _boundingBox[1].x() - _boundingBox[0].x(),
                                   _boundingBox[1].y() - _boundingBox[0].y() );
    _boundingSphere.w()  = std::max( _boundingBox[1].z() - _boundingBox[0].z(),
                                   _boundingSphere.w() );
    _boundingSphere.w() *= 0.5f;
    
    float  radius        = _boundingSphere.w();
    float  radiusSquared =  radius * radius;
    Vertex center( _boundingSphere.array );

    // 2) test all points to be in the estimated bounding sphere
    for( Index offset = 0; offset < _vertexLength; ++offset )
    {
        const Vertex& vertex = _treeData.vertex( _vertexStart + offset );
        const Vertex centerToPoint   = vertex - center;
        const float  distanceSquared = centerToPoint.squared_length();

        if( distanceSquared > radiusSquared ) 
        {
            // 3) expand sphere to contain 'outside' points
            const float distance = sqrtf( distanceSquared );
            const float delta    = distance - radius;

            radius        = ( radius + distance ) * .5f;
            radiusSquared = radius * radius;
            const Vertex normDelta = normalize( centerToPoint ) * ( 0.5f * delta );
            center       += normDelta;

            LBASSERTINFO( Vertex( vertex-center ).squared_length() <=
                    ( radiusSquared + 2.f * std::numeric_limits<float>::epsilon( )),
                        vertex << " c " << center << " r " << radius << " ("
                                << Vertex( vertex-center ).length() << ")" );
        }
    }

#ifndef NDEBUG
    // 2a) re-test all points to be in the estimated bounding sphere
    for( Index offset = 0; offset < _vertexLength; ++offset )
    {
        const Vertex& vertex = _treeData.vertex( _vertexStart + offset );

        const Vertex centerToPoint   = vertex - center;
        const float  distanceSquared = centerToPoint.squared_length();
        LBASSERTINFO( distanceSquared <=
                ( radiusSquared + 2.f * std::numeric_limits<float>::epsilon( )),
                      vertex << " c " << center << " r " << radius << " ("
                             << Vertex( vertex-center ).length() << ")" );
    }
#endif

    // store optimal bounding sphere
    _boundingSphere.x() = center.x();
    _boundingSphere.y() = center.y();
    _boundingSphere.z() = center.z();
    _boundingSphere.w() = radius;

#ifndef NDEBUG
    TRIPLYINFO << "updateBoundingSphere" << "( " << _boundingSphere << " )."
             << std::endl;
#endif

    return _boundingSphere;
}

/*  Compute the range of this child.  */
void ModelTreeLeaf::updateRange()
{
    _range[0] = 1.0f * _indexStart / _treeData.getNumIndices();
    _range[1] = _range[0] + 1.0f * _indexLength / _treeData.getNumIndices();

#ifndef NDEBUG
    TRIPLYINFO << "updateRange" << "( " << _range[0] << ", " << _range[1]
             << " )." << std::endl;
#endif
}

/*  Set up rendering of the leaf nodes.  */
void ModelTreeLeaf::setupRendering( RenderState& state,
                                    GLuint* glBuffers ) const
{
    if( !_dataLoaded )
        loadLeafData( state.useColors( ));

    switch( state.getRenderMode() )
    {
    case RENDER_MODE_IMMEDIATE:
        break;

    case RENDER_MODE_DISPLAY_LIST:
    {
        if( glBuffers[0] == state.INVALID )
        {
            char* key = (char*)( this );
            if( state.useColors( ))
                ++key;
            glBuffers[0] = state.newDisplayList( key );
        }
        glNewList( glBuffers[0], GL_COMPILE );
        renderImmediate( state );
        glEndList();
        break;
    }

    case RENDER_MODE_BUFFER_OBJECT:
    case RENDER_MODE_VA_OBJECT:
    {
        // Allocate GL objects
        const char* charThis = reinterpret_cast< const char* >( this );
        if( state.getRenderMode() == RENDER_MODE_VA_OBJECT )
        {
            // Allocate empty VAOs
            static const int VAO_TYPE = BUFFER_TYPE_ALL;
            if( glBuffers[VAO_TYPE] == state.INVALID )
                glBuffers[VAO_TYPE] = state.newVertexArray( charThis );

            glBindVertexArray( glBuffers[VAO_TYPE] );  // VAO definition -- begin
        }

        if( glBuffers[VERTEX_BUFFER_TYPE] == state.INVALID )
        {

            glBuffers[VERTEX_BUFFER_TYPE] =
                    state.reserveBufferObject( charThis + VERTEX_BUFFER_TYPE,
                                               _vertexLength * sizeof( Vertex ),
                                               GL_ARRAY_BUFFER, GL_STATIC_DRAW );
        }
        if( glBuffers[NORMAL_BUFFER_TYPE] == state.INVALID )
        {
            glBuffers[NORMAL_BUFFER_TYPE] =
                    state.reserveBufferObject( charThis + NORMAL_BUFFER_TYPE,
                                               _vertexLength * sizeof( Normal ),
                                               GL_ARRAY_BUFFER, GL_STATIC_DRAW );
        }

        if( glBuffers[COLOR_BUFFER_TYPE] == state.INVALID )
        {
            if( state.useColors( ))
            {
                glBuffers[COLOR_BUFFER_TYPE] =
                        state.reserveBufferObject( charThis + COLOR_BUFFER_TYPE,
                                                   _vertexLength * sizeof( Color ),
                                                   GL_ARRAY_BUFFER, GL_STATIC_DRAW );
            }
            else
            {
                glBuffers[COLOR_BUFFER_TYPE] = state.newBufferObject( charThis + COLOR_BUFFER_TYPE );
            }
        }

        if( glBuffers[INDEX_BUFFER_TYPE] == state.INVALID )
        {
            glBuffers[INDEX_BUFFER_TYPE] =
                    state.reserveBufferObject( charThis + INDEX_BUFFER_TYPE,
                                               _indexLength * sizeof( ShortIndex ),
                                               GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW );
        }

        // Upload data to VBOs
        if( state.useColors() )
        {
            EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, glBuffers[COLOR_BUFFER_TYPE] ));
            EQ_GL_CALL( glBufferSubData( GL_ARRAY_BUFFER, 0, _vertexLength * sizeof( Color ),
                                         _dataBuffers[COLOR_BUFFER_TYPE] ));
        }

        EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, glBuffers[NORMAL_BUFFER_TYPE] ));
        EQ_GL_CALL( glBufferSubData( GL_ARRAY_BUFFER, 0, _vertexLength * sizeof( Normal ),
                                     _dataBuffers[NORMAL_BUFFER_TYPE] ));

        EQ_GL_CALL( glBindBuffer( GL_ARRAY_BUFFER, glBuffers[VERTEX_BUFFER_TYPE] ));
        EQ_GL_CALL( glBufferSubData( GL_ARRAY_BUFFER, 0, _vertexLength * sizeof( Vertex ),
                                     _dataBuffers[VERTEX_BUFFER_TYPE] ));

        EQ_GL_CALL( glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, glBuffers[INDEX_BUFFER_TYPE] ));
        EQ_GL_CALL( glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, _indexLength * sizeof( ShortIndex ),
                                     _dataBuffers[INDEX_BUFFER_TYPE] ));

        if( state.getRenderMode() == RENDER_MODE_VA_OBJECT )
        {
            // Define VAO elements
            if( state.useColors() )
            {
                glEnableClientState( GL_COLOR_ARRAY );
                glBindBuffer( GL_ARRAY_BUFFER, glBuffers[COLOR_BUFFER_TYPE] );
                glColorPointer( 3, GL_UNSIGNED_BYTE, 0, 0 );
            }

            glEnableClientState( GL_NORMAL_ARRAY );
            glBindBuffer( GL_ARRAY_BUFFER, glBuffers[NORMAL_BUFFER_TYPE] );
            glNormalPointer( GL_FLOAT, 0, 0 );

            glEnableClientState( GL_VERTEX_ARRAY );
            glBindBuffer( GL_ARRAY_BUFFER, glBuffers[VERTEX_BUFFER_TYPE] );
            glVertexPointer( 3, GL_FLOAT, 0, 0 );

            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, glBuffers[INDEX_BUFFER_TYPE] );

            glBindVertexArray( 0 ); // VAO definition -- end
        }
        break;
    }
    default:
        TRIPLYASSERT( false );
    }
}

void ModelTreeLeaf::loadLeafData( bool useColors ) const
{
    if( _dataLoaded )
        return;

    char** colorDataBufferPtr = ( useColors ) ? &(_dataBuffers[COLOR_BUFFER_TYPE]) : 0;

    _treeData.getVertexData( _vertexStart, &( _dataBuffers[VERTEX_BUFFER_TYPE]),
                             &(_dataBuffers[NORMAL_BUFFER_TYPE]), colorDataBufferPtr );
    _treeData.getIndexData( _indexStart, &(_dataBuffers[INDEX_BUFFER_TYPE]) );

    _dataLoaded =  true;
}


/*  Draw the leaf.  */
void ModelTreeLeaf::draw( RenderState& state ) const
{
    if( state.stopRendering() )
        return;

    state.updateRegion( _boundingBox );
    switch( state.getRenderMode() )
    {
      case RENDER_MODE_IMMEDIATE:
          renderImmediate( state );
          return;
      case RENDER_MODE_DISPLAY_LIST:
          renderDisplayList( state );
          return;
      case RENDER_MODE_VA_OBJECT:
          renderVAObject( state );
          return;
      case RENDER_MODE_BUFFER_OBJECT:
      default:
          renderBufferObject( state );
          return;
    }
}

/*  Render the leaf with immediate mode primitives or vertex arrays.  */
inline
void ModelTreeLeaf::renderImmediate( RenderState& state ) const
{
    setupRendering( state, NULL );
    glBegin( GL_TRIANGLES );

    for( Index i = 0; i < _indexLength; ++i )
    {
        const ShortIndex idx = *( getAs< ShortIndex >( _dataBuffers[INDEX_BUFFER_TYPE], i ));
        if( state.useColors() )
            glColor3ubv( &( *(getAs< Color >( _dataBuffers[COLOR_BUFFER_TYPE], idx ))[0] ));
        glNormal3fv( &( *(getAs< Normal >( _dataBuffers[NORMAL_BUFFER_TYPE], idx ))[0] ));
        glVertex3fv( &( *(getAs< Vertex >( _dataBuffers[VERTEX_BUFFER_TYPE], idx ))[0] ));
    }
    glEnd();
}

/*  Render the leaf with buffer objects.  */
void ModelTreeLeaf::renderBufferObject( RenderState& state ) const
{
    GLuint buffers[BUFFER_TYPE_ALL];
    for( int i = VERTEX_BUFFER_TYPE; i < BUFFER_TYPE_ALL; ++i )
    {
        buffers[i] =
            state.getBufferObject( reinterpret_cast< const char* >(this) + i );
    }

    if( buffers[VERTEX_BUFFER_TYPE] == state.INVALID ||
        buffers[NORMAL_BUFFER_TYPE] == state.INVALID ||
        buffers[COLOR_BUFFER_TYPE] == state.INVALID  ||
        buffers[INDEX_BUFFER_TYPE] == state.INVALID )
    {
        setupRendering( state, buffers );
    }

    const char* charThis = reinterpret_cast< const char* >( this );
    if( state.useColors() )
    {
        //glBindBuffer( GL_ARRAY_BUFFER, buffers[COLOR_BUFFER_TYPE] );
        state.bindBufferObject( charThis + COLOR_BUFFER_TYPE, GL_ARRAY_BUFFER );
        glColorPointer( 3, GL_UNSIGNED_BYTE, 0, 0 );
    }
    //glBindBuffer( GL_ARRAY_BUFFER, buffers[NORMAL_BUFFER_TYPE] );
    state.bindBufferObject( charThis + NORMAL_BUFFER_TYPE, GL_ARRAY_BUFFER );
    glNormalPointer( GL_FLOAT, 0, 0 );
    //glBindBuffer( GL_ARRAY_BUFFER, buffers[VERTEX_BUFFER_TYPE] );
    state.bindBufferObject( charThis + VERTEX_BUFFER_TYPE, GL_ARRAY_BUFFER );
    glVertexPointer( 3, GL_FLOAT, 0, 0 );
    //glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, buffers[INDEX_BUFFER_TYPE] );
    state.bindBufferObject( charThis + INDEX_BUFFER_TYPE, GL_ELEMENT_ARRAY_BUFFER );
    glDrawElements( GL_TRIANGLES, GLsizei(_indexLength), GL_UNSIGNED_SHORT, 0 );
}

/*  Render the leaf with a display list.  */
inline
void ModelTreeLeaf::renderDisplayList( RenderState& state ) const
{
    char* key = (char*)( this );
    if( state.useColors( ))
        ++key;

    GLuint displayList = state.getDisplayList( key );

    if( displayList == state.INVALID )
        setupRendering( state, &displayList );

    glCallList( displayList );
}

/*  Render the leaf with a VAO.  */
inline
void ModelTreeLeaf::renderVAObject( RenderState& state ) const
{
    static const int VAO_TYPE = BUFFER_TYPE_ALL;
    GLuint objects[BUFFER_TYPE_ALL + 1];
    for( int i = VERTEX_BUFFER_TYPE; i < BUFFER_TYPE_ALL; ++i )
    {
        objects[i] =
            state.getBufferObject( reinterpret_cast< const char* >(this) + i );
    }
    objects[VAO_TYPE] =
        state.getVertexArray( reinterpret_cast< const char* >(this));

    if( objects[VERTEX_BUFFER_TYPE] == state.INVALID ||
        objects[NORMAL_BUFFER_TYPE] == state.INVALID ||
        objects[COLOR_BUFFER_TYPE] == state.INVALID  ||
        objects[INDEX_BUFFER_TYPE] == state.INVALID  ||
        objects[VAO_TYPE] == state.INVALID )
    {
        setupRendering( state, objects );
    }

    const char* charThis = reinterpret_cast< const char* >( this );
    state.useBufferObject( charThis + VERTEX_BUFFER_TYPE );
    state.useBufferObject( charThis + NORMAL_BUFFER_TYPE );
    if( state.useColors( ))
        state.useBufferObject( charThis + COLOR_BUFFER_TYPE );
    state.useBufferObject( charThis + INDEX_BUFFER_TYPE );

    glBindVertexArray( objects[VAO_TYPE] );
    glDrawElements( GL_TRIANGLES, GLsizei(_indexLength), GL_UNSIGNED_SHORT, 0 );
    glBindVertexArray( 0 );
}

/*  Read leaf node from memory.  */
void ModelTreeLeaf::fromMemory( char** addr, ModelTreeData& treeData )
{
    size_t nodeType;
    memRead( reinterpret_cast< char* >( &nodeType ), addr, sizeof( size_t ) );
    if( nodeType != LEAF_TYPE )
        throw MeshException( "Error reading binary file. Expected a leaf "
                             "node, but found something else instead." );
    ModelTreeBase::fromMemory( addr, treeData );
    memRead( reinterpret_cast< char* >( &_boundingBox ), addr,
             sizeof( BoundingBox ) );
    memRead( reinterpret_cast< char* >( &_vertexStart ), addr,
             sizeof( Index ) );
    memRead( reinterpret_cast< char* >( &_vertexLength ), addr,
             sizeof( ShortIndex ) );
    memRead( reinterpret_cast< char* >( &_indexStart ), addr,
             sizeof( Index ) );
    memRead( reinterpret_cast< char* >( &_indexLength ), addr,
             sizeof( Index ) );
}


/*  Write leaf node to output stream.  */
void ModelTreeLeaf::toStream( std::ostream& os )
{
    size_t nodeType = LEAF_TYPE;
    os.write( reinterpret_cast< char* >( &nodeType ), sizeof( size_t ));
    ModelTreeBase::toStream( os );
    os.write( reinterpret_cast< char* >( &_boundingBox ), sizeof( BoundingBox));
    os.write( reinterpret_cast< char* >( &_vertexStart ), sizeof( Index ));
    os.write( reinterpret_cast< char* >( &_vertexLength ),sizeof( ShortIndex ));
    os.write( reinterpret_cast< char* >( &_indexStart ), sizeof( Index ));
    os.write( reinterpret_cast< char* >( &_indexLength ), sizeof( Index ));
}

} // namespace triply
