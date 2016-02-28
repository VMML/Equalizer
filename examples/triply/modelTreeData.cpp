
/* Copyright (c) 2007, Tobias Wolf <twolf@access.unizh.ch>
 *               2016, Enrique G. Paredes <egparedes@ifi.uzh.ch>
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

#include "modelTreeData.h"

namespace triply 
{    

ModelTreeData::ModelTreeData()
{
    clear();
}

void ModelTreeData::clear()
{
    _numVertices = 0;
    _numIndices = 0;
    _dataArrays[VERTEX_BUFFER_TYPE] = 0;
    _dataArrays[NORMAL_BUFFER_TYPE] = 0;
    _dataArrays[COLOR_BUFFER_TYPE] = 0;
    _dataArrays[INDEX_BUFFER_TYPE] = 0;
    vertices.clear();
    colors.clear();
    normals.clear();
    indices.clear();
    _boundingBox = BoundingBox();
    _hasColors = false;
    _outOfCore = false;
}
        
void ModelTreeData::update()
{
    calculateBoundingBox();
    _dataArrays[VERTEX_BUFFER_TYPE] = reinterpret_cast< char* >( vertices.data( ));
    _dataArrays[NORMAL_BUFFER_TYPE] = reinterpret_cast< char* >( normals.data( ));
    _dataArrays[COLOR_BUFFER_TYPE] = reinterpret_cast< char* >( colors.data( ));
    _dataArrays[INDEX_BUFFER_TYPE] = reinterpret_cast< char* >( indices.data( ));
    _numVertices = vertices.size();
    _numIndices = indices.size();
    _hasColors = !colors.empty();
    _outOfCore = false;
}


void ModelTreeData::toStream( std::ostream& os )
{
    calculateBoundingBox();
    writeBoundingBox( os );
    writeVector( os, vertices );
    writeVector( os, colors );
    writeVector( os, normals );
    writeVector( os, indices );
}

void ModelTreeData::fromMemory( char** addr )
{
    clear();
    readBoundingBox( addr );
    readVector( addr, vertices );
    readVector( addr, colors );
    readVector( addr, normals );
    readVector( addr, indices );
    _dataArrays[VERTEX_BUFFER_TYPE] = reinterpret_cast< char* >( vertices.data( ));
    _dataArrays[NORMAL_BUFFER_TYPE] = reinterpret_cast< char* >( normals.data( ));
    _dataArrays[COLOR_BUFFER_TYPE] = reinterpret_cast< char* >( colors.data( ));
    _dataArrays[INDEX_BUFFER_TYPE] = reinterpret_cast< char* >( indices.data( ));
    _numVertices = vertices.size();
    _numIndices = indices.size();
    _hasColors = !colors.empty();
    _outOfCore = false;
}

bool ModelTreeData::fromFileOutOfCore( const std::string& filename,
                                       size_t startOffset,
                                       size_t* readBytes )
{
    if( readBytes != 0)
        *readBytes = 0;
    clear();
    if( _mmap.open( filename ))
    {
        try
        {
            char* dataAddr = _mmap.getPtr() + startOffset;
            readBoundingBox( &dataAddr );

            memRead( reinterpret_cast< char* >( &_numVertices ),
                     &dataAddr, sizeof( size_t ) );
            _dataArrays[VERTEX_BUFFER_TYPE] = dataAddr;
            dataAddr += _numVertices * sizeof( Vertex );

            size_t readSize;
            memRead( reinterpret_cast< char* >( &readSize ),
                     &dataAddr, sizeof( size_t ) );
            if( readSize > 0)
            {
                _dataArrays[COLOR_BUFFER_TYPE] = dataAddr;
                dataAddr += readSize * sizeof( Color );
                _hasColors = true;
            }

            memRead( reinterpret_cast< char* >( &readSize ),
                     &dataAddr, sizeof( size_t ) );
            _dataArrays[NORMAL_BUFFER_TYPE] = dataAddr;
            TRIPLYASSERT( readSize == _numVertices );
            dataAddr += readSize * sizeof( Normal );

            memRead( reinterpret_cast< char* >( &_numIndices ),
                     &dataAddr, sizeof( size_t ) );
            _dataArrays[INDEX_BUFFER_TYPE] = dataAddr;
            dataAddr += _numIndices * sizeof( ShortIndex );

            if( readBytes != 0)
                *readBytes = dataAddr - ( _mmap.getPtr() + startOffset );
            _outOfCore = true;
        }
        catch( const std::exception& e )
        {
            TRIPLYERROR << "Unable to read model tree binary file, an exception occured:  "
                        << e.what() << std::endl;
        }
    }
    else
    {
        TRIPLYERROR << "Unable to open model tree binary file."
                  << std::endl;
    }

    return _outOfCore;
}

bool ModelTreeData::getVertexData( Index start, char** verticesPtr,
                                   char** normalsPtr, char** colorsPtr )
{
    if( start >= _numVertices )
        return false;

    if( verticesPtr != 0 )
        *verticesPtr = _dataArrays[VERTEX_BUFFER_TYPE] + ( start * sizeof( Vertex ));
    if( normalsPtr != 0 )
        *normalsPtr = _dataArrays[NORMAL_BUFFER_TYPE] + ( start * sizeof( Normal ));
    if( colorsPtr != 0 )
        *colorsPtr = _dataArrays[COLOR_BUFFER_TYPE] + ( start * sizeof( Color ));

    return true;
}

bool ModelTreeData::getIndexData(Index start, char** indicesPtr )
{
    if( start >= _numIndices )
        return false;

    *indicesPtr = _dataArrays[INDEX_BUFFER_TYPE] + ( start * sizeof( ShortIndex ));

    return true;
}

Vertex& ModelTreeData::vertex( Index i )
{
    TRIPLYASSERT( i < _numVertices );
    return *( reinterpret_cast< Vertex* >( _dataArrays[VERTEX_BUFFER_TYPE] + ( i * sizeof( Vertex ))));
}

Normal& ModelTreeData::normal( Index i )
{
    TRIPLYASSERT( i < _numVertices );
    return *( reinterpret_cast< Normal* >( _dataArrays[NORMAL_BUFFER_TYPE] + ( i * sizeof( Normal ))));
}

Color& ModelTreeData::color( Index i )
{
    TRIPLYASSERT( i < _numVertices && _hasColors );
    return *( reinterpret_cast< Color* >( _dataArrays[COLOR_BUFFER_TYPE] + ( i * sizeof( Color ))));
}

ShortIndex& ModelTreeData::index( Index i )
{
    TRIPLYASSERT( i < _numIndices );
    return *( reinterpret_cast< ShortIndex* >( _dataArrays[INDEX_BUFFER_TYPE] + ( i * sizeof( ShortIndex ))));
}

void ModelTreeData::calculateBoundingBox()
{
    _boundingBox[0] = vertices[0];
    _boundingBox[1] = vertices[0];
    for( size_t v = 1; v < vertices.size(); ++v )
    {
        for( size_t i = 0; i < 3; ++i )
        {
            _boundingBox[0][i] = std::min( _boundingBox[0][i], vertices[v][i] );
            _boundingBox[1][i] = std::max( _boundingBox[1][i], vertices[v][i] );
        }
    }
}

void ModelTreeData::readBoundingBox( char** addr )
{
    memRead( reinterpret_cast< char* >( &_boundingBox[0] ), addr, 2*sizeof( Vertex ) );
}

void ModelTreeData::writeBoundingBox( std::ostream& os )
{
    os.write( reinterpret_cast< char* >( &_boundingBox[0] ), 2 * sizeof( Vertex ) );
}

} //namespace triply
