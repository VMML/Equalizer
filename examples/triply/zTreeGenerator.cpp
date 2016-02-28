
/* Copyright (c)      2015, Enrique G. Paredes <egparedes@ifi.uzh.ch>
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

#include "zTreeGenerator.h"
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

#define ZKEY_BIT_SIZE ( 3 * ( 8*sizeof( ZKey ) / 3 ) )

namespace triply
{

// Register class in the TreeGenerator factory
const std::string ZTreeGenerator::Partition =
    TreeGenerator::addGenerator< ZTreeGenerator >( "z" );

const unsigned ZTreeGenerator::MaxZLevel = ( ZKEY_BIT_SIZE / 3 ) - 1;

const ZTreeGenerator::ZKey ZTreeGenerator::MinZKey = 0;

const ZTreeGenerator::ZKey ZTreeGenerator::MaxZKey =
        ( ~0ull ) >> ( 8 * sizeof( ZKey ) - ZKEY_BIT_SIZE );

ZTreeGenerator::ZTreeGenerator()
    : _zKeys( 0 ), _meshData( 0 ), _treeRoot( 0 ), _treeData( 0 ), _progress( 0 ),
      _initialCount( 0 )
{ }

const std::string ZTreeGenerator::getPartition() const
{
    return Partition;
}

bool ZTreeGenerator::generate( MeshData& meshData,
                               ModelTreeRoot& treeRoot, ModelTreeData& treeData,
                               boost::progress_display& progress )
{
    using std::swap;

    _bbox = meshData.getBoundingBox();
    _meshData = &meshData;
    _treeRoot = &treeRoot;
    _treeData = &treeData;
    _progress = &progress;
    _initialCount = progress.count();

    TRIPLYASSERT( _bbox[0] != _bbox[1] );

    initZKeys( MaxZLevel );

    State state( MinZKey, MaxZKey + 1, 0);
    ModelTreeNode* generatedNode = generateNode( static_cast< void* >( &state) );

    // Free the (not longer needed) generated root node without calling the
    // destructor, since it would delete the whole tree
    static_cast< ModelTreeNode& >( treeRoot ) = *generatedNode;
    operator delete( generatedNode );

    while( progress.count() < TreeGenerator::MaxProgressCount )
        ++progress;

    return true;
}

/*  Continue z-octree setup, create intermediary or leaf nodes as required.  */
ModelTreeNode* ZTreeGenerator::generateNode( void* state )
{
    ZKey& beginKey = static_cast< ZTreeGenerator::State* >( state )->beginKey;
    ZKey& endKey = static_cast< ZTreeGenerator::State* >( state )->endKey;
    size_t& depth = static_cast< ZTreeGenerator::State* >( state )->depth;
    const unsigned& arity = _treeRoot->getArity();

#ifndef NDEBUG
    TRIPLYINFO << "ZTreeGenerator::generateNode - " << arity << " "
             << "( " << beginKey << ", " << endKey << ", " << depth
             << " )." << std::endl;
#endif

    ModelTreeBasePtr* children = new ModelTreeBasePtr[arity];
    for( unsigned i=0; i < arity; ++i )
    {
        children[i] = 0;
    }

    ZKey currentKey = beginKey;
    ZKey keyStep = (endKey - beginKey) / arity;
    //ZKey keyStep = 1ull << 3 * ( MaxZLevel - depth ); // Only for octrees

    std::vector< ZKeyIndexPair >::iterator childBeginIt =
            std::lower_bound( _zKeys.begin(), _zKeys.end(), currentKey,
                              ZKeyIndexPairLessCmpFunctor() );
    std::vector< ZKeyIndexPair >::iterator childEndIt;

    for( unsigned i=0; i < arity; ++i )
    {
        childEndIt = std::lower_bound( childBeginIt, _zKeys.end(),
                                       currentKey + keyStep,
                                       ZKeyIndexPairLessCmpFunctor() );
        Index length = std::distance( childBeginIt, childEndIt );

        if( length > 0 )
        {
            State childState( currentKey, currentKey + keyStep, depth + 1 );
            if( depth < MaxZLevel - 1 && subdivide( length, depth ) )
            {
                children[i] = generateNode( static_cast< void* >( &childState ));
            }
            else
            {
                children[i] = generateLeaf( static_cast< void* >( &childState ));
            }
        }

        currentKey += keyStep;
        childBeginIt = childEndIt;
    }

    TRIPLYASSERT( currentKey == endKey);
    UNUSED( endKey ); // Needed in the release version

    if ( depth <= 4 )
    {
        size_t count =
                ( MaxProgressCount * std::distance( _zKeys.begin(), childEndIt ))
                / _meshData->triangles.size();
        while( _initialCount + count > _progress->count() )
            ++(*_progress);
    }

    return new ModelTreeNode( arity, children );
}

/*  Finish partial octree setup - sort, reindex and merge into global data.  */
ModelTreeLeaf* ZTreeGenerator::generateLeaf( void* state )
{
    ZKey& beginKey = static_cast< ZTreeGenerator::State* >( state )->beginKey;
    ZKey& endKey = static_cast< ZTreeGenerator::State* >( state )->endKey;
    MeshData& meshData = *_meshData;
    ModelTreeData& treeData = *_treeData;

    Index indexStart = treeData.indices.size();
    Index vertexStart = treeData.vertices.size();
    Index indexLength = 0;
    ShortIndex vertexLength = 0;

    std::vector< ZKeyIndexPair >::iterator beginIt =
            std::lower_bound( _zKeys.begin(), _zKeys.end(), beginKey,
                              ZKeyIndexPairLessCmpFunctor());
    std::vector< ZKeyIndexPair >::iterator endIt =
            std::lower_bound( beginIt, _zKeys.end(), endKey,
                              ZKeyIndexPairLessCmpFunctor());
    Index triStart = std::distance( _zKeys.begin(), beginIt );
    Index triEnd = std::distance( _zKeys.begin(), endIt );

    // stores the new indices (relative to start)
    std::map< Index, ShortIndex > newIndex;

    for( Index t=triStart; t < triEnd; ++t )
    {
        for( Index v = 0; v < 3; ++v )
        {
            Index i = meshData.triangles[ _zKeys[t].second ][v];
            if( newIndex.find( i ) == newIndex.end() )
            {
                newIndex[i] = vertexLength++;
                // assert number of vertices does not exceed SmallIndex range
                TRIPLYASSERT( vertexLength );
                treeData.vertices.push_back( meshData.vertices[i] );
                if( treeData.hasColors( ))
                    treeData.colors.push_back( meshData.colors[i] );
                treeData.normals.push_back( meshData.normals[i] );
            }
            treeData.indices.push_back( newIndex[i] );
            ++indexLength;
        }
    }

#ifndef NDEBUG
    TRIPLYINFO << "ZTreeGenerator::generateLeaf" << "( " << indexStart << ", " << indexLength
             << "/ start " << vertexStart << ", " << vertexLength
             << " vertices)." << std::endl;
#endif

    return new ModelTreeLeaf( treeData, indexStart, indexLength,
                              vertexStart, vertexLength );
}

inline ZTreeGenerator::ZKey ZTreeGenerator::generateZCode( const Vertex& point,
                                                           unsigned maxLevel )
{
    Vertex minPoint = _bbox[0];
    Vertex maxPoint = _bbox[1];
    ZKey key = 0;

    for( int i=maxLevel; i >= 0; --i )
    {
        key <<= 3;
        if( point[0] + point[0] > minPoint[0] + maxPoint[0] )
        {
            key |= 0x1;
            minPoint[0] = ( minPoint[0] + maxPoint[0] ) * 0.5;
        }
        else
        {
            maxPoint[0] = ( minPoint[0] + maxPoint[0] ) * 0.5;
        }
        if( point[1] + point[1] > minPoint[1] + maxPoint[1] )
        {
            key |= 0x2;
            minPoint[1] = ( minPoint[1] + maxPoint[1] ) * 0.5;
        }
        else
        {
            maxPoint[1] = ( minPoint[1] + maxPoint[1] ) * 0.5;
        }
        if( point[2] + point[2] > minPoint[2] + maxPoint[2] )
        {
            key |= 0x4;
            minPoint[2] = ( minPoint[2] + maxPoint[2] ) * 0.5;
        }
        else
        {
            maxPoint[2] = ( minPoint[2] + maxPoint[2] ) * 0.5;
        }
    }

    return key;
}

// Compute Z-codes for triangles and sort them
void ZTreeGenerator::initZKeys( unsigned maxLevel )
{
    const std::vector< Vertex >& vertices = _meshData->vertices;
    const std::vector< Triangle >& triangles = _meshData->triangles;

    _zKeys.resize( triangles.size() );

#pragma omp parallel for
    for( Index i=0; i < _zKeys.size(); ++i )
    {
        // Compute Z-key of triangle centroid
        _zKeys[ i ].first = generateZCode( ( vertices[ triangles[i][0] ] +
                                             vertices[ triangles[i][1] ] +
                                             vertices[ triangles[i][2] ] ) / 3.0f,
                                             maxLevel );
        _zKeys[ i ].second = i;
    }

    ::sort( _zKeys.begin(), _zKeys.end(), ZKeyIndexPairLessCmpFunctor() );
}

inline bool ZTreeGenerator::subdivide( const Index length, const size_t depth )
{
    return ( length / 2 > LEAF_SIZE ) || ( depth < 3 && length > 1 );
}

} // namespace triply




