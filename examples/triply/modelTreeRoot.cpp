
/* Copyright (c) 2007, Tobias Wolf <twolf@access.unizh.ch>
 *          2009-2012, Stefan Eilemann <eile@equalizergraphics.com>
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


#include "modelTreeRoot.h"
#include "renderState.h"
#include "meshData.h"
#include "treeGenerator.h"
#include "mmap.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>
#include <cstring>


namespace triply
{

namespace detail
{
    static const unsigned StepsInSetup = TreeGenerator::MaxProgressCount + 1;
    static const unsigned StepsInConstruct = StepsInSetup + 3;
} // namespace detail

typedef vmml::frustum_culler< float >  FrustumCuller;

void ModelTreeRoot::clear()
{
    ModelTreeNode::clear();
    _treeData.clear();
}

//#define LOGCULL
void ModelTreeRoot::cullDraw( RenderState& state ) const
{
    beginRendering( state );

#ifdef LOGCULL
    size_t verticesRendered = 0;
    size_t verticesOverlap  = 0;
#endif

    const Range& range = state.getRange();
    FrustumCuller culler;
    culler.setup( state.getProjectionModelViewMatrix( ));

    // start with root node
    std::vector< const triply::ModelTreeBase* > candidates;
    candidates.push_back( this );

    while( !candidates.empty() )
    {
        if( state.stopRendering( ))
            return;

        const triply::ModelTreeBase* treeNode = candidates.back();
        candidates.pop_back();

        // completely out of range check
        if( treeNode->getRange()[0] >= range[1] ||
            treeNode->getRange()[1] < range[0] )
        {
            continue;
        }

        // bounding sphere view frustum culling
        const vmml::Visibility visibility = state.useFrustumCulling() ?
                            culler.test_sphere( treeNode->getBoundingSphere( )) :
                            vmml::VISIBILITY_FULL;
        switch( visibility )
        {
            case vmml::VISIBILITY_FULL:
                // if fully visible and fully in range, render it
                if( treeNode->getRange()[0] >= range[0] &&
                    treeNode->getRange()[1] <  range[1] )
                {
                    treeNode->draw( state );
                    if( state.showBoundingSpheres() )
                        treeNode->drawBoundingSphere( state );
#ifdef LOGCULL
                    verticesRendered += treeNode->getNumberOfVertices();
#endif
                    break;
                }
                // partial range, fall through to partial visibility

            case vmml::VISIBILITY_PARTIAL:
            {

                if( treeNode->getNumberOfChildren() == 0 )
                {
                    if( treeNode->getRange()[0] >= range[0] )
                    {
                        treeNode->draw( state );
                        if( state.showBoundingSpheres() )
                            treeNode->drawBoundingSphere( state );
#ifdef LOGCULL
                        verticesRendered += treeNode->getNumberOfVertices();
                        if( visibility == vmml::VISIBILITY_PARTIAL )
                            verticesOverlap  += treeNode->getNumberOfVertices();
#endif
                    }
                    // else drop, to be drawn by 'previous' channel
                }
                else
                {
                    for( unsigned i=0; i < ModelTreeNode::_children.size(); ++i)
                    {
                        const triply::ModelTreeBase* child = treeNode->getChild( i );
                        if( child != 0 )
                            candidates.push_back( child );
                    }
                }
                break;
            }
            case vmml::VISIBILITY_NONE:
                // do nothing
                break;
        }
    }

    endRendering( state );

#ifdef LOGCULL
    const size_t verticesTotal = _globalData.vertices.size(); //model->getNumberOfVertices();
    TRIPLYINFO
        << getName() << " rendered " << verticesRendered * 100 / verticesTotal
        << "% of model, overlap <= " << verticesOverlap * 100 / verticesTotal
        << "%" << std::endl;
#endif
}

/*  Delegate rendering to node routine.  */
void ModelTreeRoot::draw( RenderState& state ) const
{
    ModelTreeNode::draw( state );
}

bool ModelTreeRoot::setupTree( MeshData& modelData,
                               const TreeInfo& info,
                               boost::progress_display* progressPtr )
{
    bool useTempProgress = false;
    if (progressPtr == 0)
    {
        progressPtr = new boost::progress_display( detail::StepsInSetup );
        useTempProgress = true;
    }

    if( modelData.getBoundingBox()[0].length() == 0.0f
        && modelData.getBoundingBox()[1].length() == 0.0f )
    {
        modelData.calculateBoundingBox();
    }

    clear();
    ModelTreeNode::_children.clear( true );

    ModelTreeNode::_children.resize( info.arity );
    _treeData._hasColors = modelData.colors.size() > 0;
    _treeData._boundingBox = modelData.getBoundingBox(); // _treeData.calculateBoundingBox();

    TreeGenerator* treeGenerator = TreeGenerator::instantiate( info.partition );
    if( treeGenerator == 0 )
    {
        TRIPLYASSERT( treeGenerator );
        return false;
    }

    unsigned expectedCount = progressPtr->count() + TreeGenerator::MaxProgressCount;

    treeGenerator->generate( modelData, *this, _treeData, *progressPtr );
    _treeData.refreshState();

    while( progressPtr->count() < expectedCount )
        ++(*progressPtr);

    ModelTreeNode::updateBoundingSphere();
    ++(*progressPtr);
    ModelTreeNode::updateRange();
    ++(*progressPtr);

#if 0
    TRIPLYINFO << _treeData.boundingBox[0] << std::endl;
    TRIPLYINFO << _treeData.boundingBox[1] << std::endl;
    TRIPLYINFO << ModelTreeBase::_boundingSphere << std::endl;
    
    // re-test all points to be in the bounding sphere
    Vertex center( _boundingSphere.array );
    float  radius        = _boundingSphere.w();
    float  radiusSquared =  radius * radius;
    for( size_t offset = 0; offset < _globalData.vertices.size(); ++offset )
    {
        const Vertex& vertex = _globalData.vertices[ offset ];

        const Vertex centerToPoint   = vertex - center;
        const float  distanceSquared = centerToPoint.squared_length();
        LBASSERTINFO( distanceSquared <= radiusSquared,
                      distanceSquared << " > " << radiusSquared );
    }
#endif

    if ( useTempProgress )
    {
        while( progressPtr->count() < progressPtr->expected_count() )
            ++(*progressPtr);
        delete progressPtr;
    }

    return true;
}

/*  Write binary representation of the tree to file.  */
bool ModelTreeRoot::writeToFile( const std::string& filename )
{
    bool result = false;
    _name = filename;

    std::ofstream output( getBinaryName().c_str(),
                          std::ios::out | std::ios::binary );
    if( output )
    {
        // enable exceptions on stream errors
        output.exceptions( std::ofstream::failbit | std::ofstream::badbit );
        try
        {
            toStream( output );
            result = true;
        }
        catch( const std::exception& e )
        {
            TRIPLYERROR << "Unable to write binary file, an exception "
                      << "occured:  " << e.what() << std::endl;
        }
        output.close();
    }
    else
    {
        TRIPLYERROR << "Unable to create binary file." << std::endl;
    }

    return result;
}

/*  Read binary octree representation, construct from ply if unavailable.  */
bool ModelTreeRoot::readFromFile( const std::string& filename,
                                  const TreeInfo &info, bool inCoreData )
{
    if( !(info.isValid() && TreeGenerator::isValidName( info.partition )))
    {
        TRIPLYERROR << "Invalid tree description." << std::endl;
        return false;
    }

    ModelTreeNode::_children.resize( info.arity, 0 );
    _inCoreData = inCoreData;
    _partition = info.partition;
    _name = filename;

    if( !readBinary( getBinaryName( ) ))
    {
        if( !constructFromPly( filename, info ))
        {
            _name = "";
            return false;
        }
    }

    return true;
}

bool ModelTreeRoot::hasColors() const
{
    return _treeData._hasColors;
}

BoundingBox ModelTreeRoot::getBoundingBox() const
{
    return _treeData.getBoundingBox();
}

size_t ModelTreeRoot::getTotalVertices() const
{
    return _treeData.vertices.size();
}

size_t ModelTreeRoot::getTotalIndices() const
{
    return _treeData.indices.size();
}

size_t ModelTreeRoot::getTotalMemory() const
{
    return _treeData.getTotalSize();
}

std::string ModelTreeRoot::getBinaryName( ) const
{
    std::ostringstream oss;
    oss << _partition;
    oss << ModelTreeNode::getArity();
    return getArchitectureFilename( _name, oss.str() );
}

/*  Write root node to output stream and continue with other nodes.  */
void ModelTreeRoot::toStream( std:: ostream& os )
{
    size_t version = FILE_VERSION;
    os.write( reinterpret_cast< char* >( &version ), sizeof( size_t ) );
    size_t partitionLength = _partition.length();
    os.write( reinterpret_cast< char* >( &partitionLength ), sizeof( size_t ) );
    os.write( _partition.c_str(), partitionLength  * sizeof( char ) );
    size_t treeArity = ModelTreeNode::getArity();
    os.write( reinterpret_cast< char* >( &treeArity ), sizeof( size_t ) );
    size_t totalTreeNodes = getNumberOfDescendants();
    os.write( reinterpret_cast< char* >( &totalTreeNodes ), sizeof( size_t ) );
    size_t nodeType = ROOT_TYPE;
    os.write( reinterpret_cast< char* >( &nodeType ), sizeof( size_t ) );
    _treeData.toStream( os );
    ModelTreeNode::toStream( os );
}


/*  Read root node from memory and continue with other nodes.  */
void ModelTreeRoot::fromMemory( char* start )
{
    char* addr = start;
    char** addrPtr = &addr;
    size_t version;
    memRead( reinterpret_cast< char* >( &version ), addrPtr, sizeof( size_t ) );
    if( version != FILE_VERSION )
        throw MeshException( "Error reading binary file. Version in file "
                             "does not match the expected version." );

    size_t partitionLength;
    char partitionChars[256];
    memRead( reinterpret_cast< char* >( &partitionLength ), addrPtr, sizeof( size_t ) );
    memRead( &(partitionChars[0]), addrPtr, partitionLength * sizeof( char ) );
    std::string partition( partitionChars, partitionLength );
    size_t treeArity;
    memRead( reinterpret_cast< char* >( &treeArity ), addrPtr, sizeof( size_t ) );
    if( _partition != partition || ModelTreeNode::getArity() != treeArity)
        throw MeshException( "Error reading binary file. Invalid tree specification." );

    size_t totalTreeNodes;
    memRead( reinterpret_cast< char* >( &totalTreeNodes ), addrPtr, sizeof( size_t ) );
    size_t nodeType;
    memRead( reinterpret_cast< char* >( &nodeType ), addrPtr, sizeof( size_t ) );
    if( nodeType != ROOT_TYPE )
        throw MeshException( "Error reading binary file. Expected the root "
                             "node, but found something else instead." );

    if( _inCoreData )
    {
        _treeData.fromMemory( addrPtr );
    }
    else
    {
        std::string binName = getBinaryName();
        size_t readBytes = 0;
        _treeData.fromFile( binName, addr - start, &readBytes );
        TRIPLYASSERT( readBytes > 0 );
        addr += readBytes;
    }

    ModelTreeNode::fromMemory( addrPtr, _treeData );
}

/*  Functions extracted out of readFromFile to enhance readability.  */
bool ModelTreeRoot::constructFromPly( const std::string& filename,
                                      const TreeInfo& info )
{
    TRIPLYINFO << "Reading PLY file." << std::endl;
    boost::progress_display progress( detail::StepsInConstruct );

    MeshData modelData;
    if( _invertFaces )
        modelData.useInvertedFaces();
    if( !modelData.readPlyFile( filename ) )
    {
        TRIPLYERROR << "Unable to load PLY file." << std::endl;
        return false;
    }
    ++progress;

    modelData.calculateNormals();
    modelData.scale( 2.0f );
    ++progress;

    setupTree( modelData, info, &progress );

    if( !writeToFile( filename ))
    {
        TRIPLYWARN << "Unable to write binary representation." << std::endl;
        TRIPLYWARN << "Out-of-core will not work." << std::endl;
        _inCoreData = false;
    }
    ++progress;

    while( progress.count() < progress.expected_count() )
        ++progress;
    std::cout << std::endl;
    showBuildStats( modelData );

    return true;
}

bool ModelTreeRoot::readBinary( std::string filename )
{
    bool result = false;

    // create memory mapped file
    MMap mmapedFile;
    if( mmapedFile.open( filename ))
    {
        try
        {
            fromMemory( mmapedFile.getPtr() );
            result = true;
        }
        catch( const std::exception& e )
        {
            TRIPLYERROR << "Unable to read binary file, an exception occured:  "
                      << e.what() << std::endl;
        }
    }
    else
    {
        TRIPLYWARN << "Unable to read binary file, memory mapping failed."
                  << std::endl;
    }
    mmapedFile.close( );

    return result;
}

void ModelTreeRoot::showBuildStats( const MeshData& modelData )
{
    int modelVertices = modelData.vertices.size();
    int modelTriangles = modelData.triangles.size();
    unsigned treeVertices = _treeData.vertices.size();
    unsigned treeTriangles = _treeData.indices.size() / 3;
    std::vector< std::pair< unsigned, unsigned > > nodesPerLevel =
            ModelTreeNode::getDescendantsPerLevel( );

    unsigned totalNodes = 1;
    unsigned totalLeaves = 0;
    for ( auto l : nodesPerLevel )
    {
        totalNodes += l.first;
        totalLeaves += l.second;
    }

    TRIPLYINFO << std::endl
               << "--[Tree stats]-- " << std::endl
               << "    + arity (root) => " << ModelTreeNode::getArity() << std::endl
               << "    + vertices [ model | tree | %up ] => "
               << modelVertices << " | " << treeVertices << " | "
               << 100*( ((1.0*treeVertices)/(1.0*modelVertices)) - 1.0 ) << " %" << std::endl
               << "    + triangles [ model == tree ] => "
               << modelTriangles << " == " << treeTriangles << std::endl
               << "    + nodes => " << totalNodes << std::endl
               << "    + leaf nodes => " << totalLeaves << std::endl
               << "    + nodes per level [ total | leaves] => "  << std::endl
               << "        0 : 1 | 0" << std::endl;
    for( unsigned l=0; l < nodesPerLevel.size(); ++l )
    {
        TRIPLYINFO << "        " << l + 1<< " : "
                   << nodesPerLevel[l].first << " | "
                   << nodesPerLevel[l].second << std::endl;
    }
    TRIPLYINFO << std::endl;
}

/*  Set up the common OpenGL state for rendering of all nodes.  */
void ModelTreeRoot::beginRendering( RenderState& state ) const
{
    state.resetRegion();
    switch( state.getRenderMode() )
    {
#ifdef GL_ARB_vertex_buffer_object
    case RENDER_MODE_BUFFER_OBJECT:
        glPushClientAttrib( GL_CLIENT_VERTEX_ARRAY_BIT );
        glEnableClientState( GL_VERTEX_ARRAY );
        glEnableClientState( GL_NORMAL_ARRAY );
        if( state.useColors() )
            glEnableClientState( GL_COLOR_ARRAY );
#endif
    case RENDER_MODE_DISPLAY_LIST:
    case RENDER_MODE_IMMEDIATE:
    default:
        ;
    }
}

/*  Tear down the common OpenGL state for rendering of all nodes.  */
void ModelTreeRoot::endRendering( RenderState& state ) const
{
    switch( state.getRenderMode() )
    {
#ifdef GL_ARB_vertex_buffer_object
    case RENDER_MODE_BUFFER_OBJECT:
    {
        // deactivate VBO and EBO use
#define glewGetContext state.glewGetContext
        glBindBuffer( GL_ARRAY_BUFFER_ARB, 0);
        glBindBuffer( GL_ELEMENT_ARRAY_BUFFER_ARB, 0);
        glPopClientAttrib();
    }
#endif
    case RENDER_MODE_DISPLAY_LIST:
    case RENDER_MODE_IMMEDIATE:
    default:
        ;
    }
}

}
