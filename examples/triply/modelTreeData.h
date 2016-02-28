
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


#ifndef TRIPLY_MODELTREEDATA_H
#define TRIPLY_MODELTREEDATA_H


#include "typedefs.h"
#include <vector>
#include <fstream>
#include "mmap.h"


namespace triply 
{    
    /** Holds the final kd-tree data, sorted and reindexed.  */
    class ModelTreeData
    {
    public:
        ModelTreeData();

        void clear();

        void update();

        /*  Write the vectors' sizes and contents to the given stream.  */
        void toStream( std::ostream& os );
        
        /*  Read the vectors' sizes and contents from the given MMF address.  */
        void fromMemory( char** addr );

        /*  Out-of-core load using a memory map. */
        bool fromFileOutOfCore( const std::string& filename, size_t startOffset,
                                size_t* readBytes );

        size_t getNumVertices() const
        {
            return _numVertices;
        }

        size_t getNumIndices() const
        {
            return _numIndices;
        }

        size_t getTotalSize() const
        {
            return getNumVertices() * sizeof( Vertex )
                   + getNumVertices() * sizeof( Color ) * size_t( _hasColors )
                   + getNumVertices() * sizeof( Normal )
                   + getNumIndices() * sizeof( ShortIndex );
        }

        BoundingBox getBoundingBox() const
        {
            return _boundingBox;
        }

        bool hasColors() const
        {
            return _hasColors;
        }

        bool outOfCore() const
        {
            return _outOfCore;
        }

        bool getVertexData( Index start, char** verticesPtr,
                            char** normalsPtr, char** colorsPtr );
        bool getIndexData( Index start, char** indicesPtr );

        Vertex& vertex( Index i );
        Normal& normal( Index i );
        Color& color( Index i );
        ShortIndex& index( Index i );

        std::vector< Vertex >       vertices;
        std::vector< Color >        colors;
        std::vector< Normal >       normals;
        std::vector< ShortIndex >   indices;

    protected:
        void calculateBoundingBox();
        void readBoundingBox( char** addr );
        void writeBoundingBox( std::ostream& os );

        /*  Helper function to read a vector from the MMF address.  */
        template< class T >
        void readVector( char** addr, std::vector< T >& v )
        {
            size_t length;
            memRead( reinterpret_cast< char* >( &length ), addr,
                     sizeof( size_t ) );
            if( length > 0 )
            {
                v.resize( length );
                memRead( reinterpret_cast< char* >( &v[0] ), addr,
                         length * sizeof( T ) );
            }
        }

        /*  Helper function to write a vector to output stream.  */
        template< class T >
        void writeVector( std::ostream& os, std::vector< T >& v )
        {
            size_t length = v.size();
            os.write( reinterpret_cast< char* >( &length ), 
                      sizeof( size_t ) );
            if( length > 0 )
                os.write( reinterpret_cast< char* >( &v[0] ), 
                          length * sizeof( T ) );
        }
        
        friend class ModelTreeRoot;
        friend class ModelTreeDist;

        size_t                      _numVertices;
        size_t                      _numIndices;
        char*                       _dataArrays[BUFFER_TYPE_ALL];
        BoundingBox                 _boundingBox;
        MMap                        _mmap;
        bool                        _hasColors;
        bool                        _outOfCore;
    };
}


#endif // TRIPLY_MODELTREEDATA_H
