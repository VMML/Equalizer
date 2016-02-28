
/* Copyright (c)      2015, Enrique G. Paredes <egparedes@ifi.uzh.ch>
 *               2008-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2007, Tobias Wolf <twolf@access.unizh.ch>
 *
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

#include "mkdTreeGenerator.h"
#include "modelTreeData.h"
#include "modelTreeRoot.h"
#include "modelTreeNode.h"
#include "modelTreeLeaf.h"
#include "meshData.h"
#include <cstdlib>
#include <algorithm>
#include <utility>

#if (( __GNUC__ > 4 ) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 4)) )
#  include <parallel/algorithm>
using __gnu_parallel::sort;
#else
using std::sort;
#endif

namespace triply
{

/*  Helper structure to sort Triangles with standard library sort function.  */
MKDGenerator::TriangleLessCmpFunctor::TriangleLessCmpFunctor(
            const MeshData& dataArg, const MKDGenerator::Axis axisArg )
    : data( dataArg ), axis( axisArg )
{ }

bool MKDGenerator::TriangleLessCmpFunctor::operator()( const Triangle& t1,
                                                       const Triangle& t2 ) const
{
    // references to both triangles' three vertices
    const Vertex& v11 = data.vertices[ t1[0] ];
    const Vertex& v12 = data.vertices[ t1[1] ];
    const Vertex& v13 = data.vertices[ t1[2] ];
    const Vertex& v21 = data.vertices[ t2[0] ];
    const Vertex& v22 = data.vertices[ t2[1] ];
    const Vertex& v23 = data.vertices[ t2[2] ];

    // compare first by given axis
    int currentAxis = axis;
    do
    {
        // test median of 'axis' component of all three vertices
        const float median1 = (v11[currentAxis] + v12[currentAxis] + v13[currentAxis] ) / 3.0f;
        const float median2 = (v21[currentAxis] + v22[currentAxis] + v23[currentAxis] ) / 3.0f;
        if( median1 != median2 )
            return ( median1 < median2 );

        // if still equal, move on to the next axis
        currentAxis = ( currentAxis + 1 ) % 3;
    }
    while( currentAxis != axis );

    return false;
}

// Register class in the TreeGenerator factory
const std::string MKDGenerator::Partition =
    TreeGenerator::addGenerator< MKDGenerator >( "kd" );

MKDGenerator::MKDGenerator()
    : _meshData( 0 ), _treeRoot( 0 ), _treeData( 0 ), _progress( 0 ),
      _initialCount( 0 )
{ }

const std::string MKDGenerator::getPartition() const
{
    return Partition;
}

bool MKDGenerator::generate( MeshData& meshData,
                             ModelTreeRoot& treeRoot, ModelTreeData& treeData,
                             boost::progress_display& progress )
{
    using std::swap;

    _meshData = &meshData;
    _treeRoot = &treeRoot;
    _treeData = &treeData;
    _progress = &progress;
    _initialCount = progress.count();

    //  Begin kd-tree setup, go through full range starting with largest axis.
    State state( getLongestAxis( 0, _meshData->triangles.size() ),
                 0, _meshData->triangles.size(), 0);
    ModelTreeNode* generatedNode = generateNode( static_cast< void* >( &state) );

    // Free the (not longer needed) generated root node without calling the
    // destructor, since it would delete the whole tree
    static_cast< ModelTreeNode& >( treeRoot ) = *generatedNode;
    operator delete( generatedNode );

    return true;
}

/*  Continue mkd-tree setup, create intermediary or leaf nodes as required.  */
ModelTreeNode* MKDGenerator::generateNode( void* state )
{
    Axis& axis = static_cast< MKDGenerator::State* >( state )->axis;
    Index& start = static_cast< MKDGenerator::State* >( state )->start;
    Index& length = static_cast< MKDGenerator::State* >( state )->length;
    size_t& depth = static_cast< MKDGenerator::State* >( state )->depth;
    const unsigned& arity = _treeRoot->getArity();

#ifndef NDEBUG
    TRIPLYINFO << "MKDGenerator::generateNode - " << arity << " "
               << "( " << start << ", " << length << ", "
               << axis << ", " << depth << " )." << std::endl;
#endif

    ModelTreeBasePtr* children = new ModelTreeBasePtr[arity];
    for( unsigned i=0; i < arity; ++i )
    {
        children[i] = 0;
    }

    sortVertices( start, length, axis );

    const Index childStep = length / arity;
    for( unsigned i=0; i < arity; ++i )
    {
        const Index childStart = start + ( i * childStep );
        const Index childLength = (i + 1 < arity) ? childStep : length - ( i * childStep );
        State childState( AXIS_X, childStart, childLength, depth + 1 );

        if( subdivide( childLength, depth ))
        {
            // move to next axis and continue contruction in the child nodes
            childState.axis = getLongestAxis( childStart, childLength );
            children[ i ] =  generateNode( static_cast< void* >( &childState ));
        }
        else
        {
            children[ i ] = generateLeaf( static_cast< void* >( &childState ) );
        }
    }

    if ( depth <= 4 )
    {
        size_t count = ( MaxProgressCount * (start + length)) / _meshData->triangles.size();
        while( _initialCount + count > _progress->count() )
            ++(*_progress);
    }

    return new ModelTreeNode( arity, children );
}


/*  Finish partial multiway kd-tree setup - sort, reindex and merge into global data.  */
ModelTreeLeaf* MKDGenerator::generateLeaf( void* state )
{
    Axis& axis = static_cast< MKDGenerator::State* >( state )->axis;
    Index& start = static_cast< MKDGenerator::State* >( state )->start;
    Index& length = static_cast< MKDGenerator::State* >( state )->length;
    MeshData& modelData = *_meshData;
    ModelTreeData& treeData = *_treeData;

    sortVertices( start, length, axis );
    Index indexStart = treeData.indices.size();
    Index indexLength = 0;
    Index vertexStart = treeData.vertices.size();
    ShortIndex vertexLength = 0;

    // stores the new indices (relative to _start)
    std::map< Index, ShortIndex > newIndex;

    for( Index t = 0; t < length; ++t )
    {
        for( Index v = 0; v < 3; ++v )
        {
            Index i = modelData.triangles[start + t][v];
            if( newIndex.find( i ) == newIndex.end() )
            {
                newIndex[i] = vertexLength++;
                // assert number of vertices does not exceed SmallIndex range
                TRIPLYASSERT( vertexLength );
                treeData.vertices.push_back( modelData.vertices[i] );
                if( treeData.hasColors() )
                    treeData.colors.push_back( modelData.colors[i] );
                treeData.normals.push_back( modelData.normals[i] );
            }
            treeData.indices.push_back( newIndex[i] );
            ++indexLength;
        }
    }

#ifndef NDEBUG
    TRIPLYINFO << "MKDGenerator::generateLeaf" << "( " << indexStart << ", " << indexLength
             << "/ start " << vertexStart << ", " << vertexLength
             << " vertices)." << std::endl;
#endif

    return new ModelTreeLeaf( treeData, indexStart, indexLength,
                              vertexStart, vertexLength );
}

/* Calculates longest axis for a set of triangles */
MKDGenerator::Axis MKDGenerator::getLongestAxis( const size_t start,
                                                 const size_t elements )
{
    std::vector< Triangle >& triangles = _meshData->triangles;
    std::vector< Vertex >& vertices = _meshData->vertices;

    if( start + elements > triangles.size() )
    {
        TRIPLYERROR << "incorrect request to getLongestAxis" << std::endl
                    << "start:     " << start                << std::endl
                    << "elements:  " << elements             << std::endl
                    << "sum:       " << start+elements       << std::endl
                    << "data size: " << triangles.size()     << std::endl;
        return MKDGenerator::AXIS_X;
    }

    BoundingBox bb;
    bb[0] = vertices[ triangles[start][0] ];
    bb[1] = vertices[ triangles[start][0] ];

    for( size_t t = start; t < start+elements; ++t )
        for( size_t v = 0; v < 3; ++v )
            for( size_t i = 0; i < 3; ++i )
            {
                bb[0][i] = std::min( bb[0][i], vertices[ triangles[t][v] ][i] );
                bb[1][i] = std::max( bb[1][i], vertices[ triangles[t][v] ][i] );
            }

    const GLfloat bbX = bb[1][0] - bb[0][0];
    const GLfloat bbY = bb[1][1] - bb[0][1];
    const GLfloat bbZ = bb[1][2] - bb[0][2];

    if( bbX >= bbY && bbX >= bbZ )
        return MKDGenerator::AXIS_X;

    if( bbY >= bbX && bbY >= bbZ )
        return MKDGenerator::AXIS_Y;

    return MKDGenerator::AXIS_Z;
}

inline bool MKDGenerator::subdivide( const Index length, const size_t depth )
{
    return ( length / 2 > LEAF_SIZE ) || ( depth < 3 && length > 1 );
}

/*  Sort the index data from start to start + length along the given axis.  */
inline void MKDGenerator::sortVertices( const Index start, const Index length,
                                        const MKDGenerator::Axis axis )
{
    TRIPLYASSERT( length > 0 );
    TRIPLYASSERT( start + length <= _meshData->triangles.size() );

    ::sort( _meshData->triangles.begin() + start,
            _meshData->triangles.begin() + start + length,
            TriangleLessCmpFunctor( *_meshData, axis ) );
}

std::ostream& operator<<( std::ostream& os, const MKDGenerator::Axis& axis )
{
    switch( axis )
    {
    case MKDGenerator::AXIS_X :
        os << "x axis";
        break;
    case MKDGenerator::AXIS_Y :
        os << "y axis";
        break;
    case MKDGenerator::AXIS_Z :
        os << "z axis";
        break;
    default:
        os << "ERROR";
        break;
    }
    return os;
}

} // namespace triply
