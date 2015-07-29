
/* Copyright (c) 2007, Tobias Wolf <twolf@access.unizh.ch>
 *               2009, Cedric Stalder <cedric.stalder@gmail.com>
 *               2011-2014, Stefan Eilemann <eile@eyescale.ch>
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


 Type definitions for the mesh classes.
*/

#ifndef PLYLIB_TYPEDEFS_H
#define PLYLIB_TYPEDEFS_H

#if defined(EQ_DEFINES_H) || defined(EQUALIZER_VERSION)
#  define EQUALIZER
#endif

#ifdef EQUALIZER
#  include <eq/eq.h>
#  define PLYLIBASSERT  LBASSERT
#  define PLYLIBERROR   LBERROR
#  define PLYLIBWARN    LBWARN
#  define PLYLIBINFO    LBINFO
#else
#  include <vmmlib/vmmlib.hpp>
#  ifdef _WIN32
#    include <Winsock2.h>
#    include <Windows.h>
#  endif
#  ifdef __APPLE__
#    include <OpenGL/gl.h>
#  else
#    include <GL/gl.h>
#  endif
#  include <cassert>
#  define PLYLIBASSERT  assert
#  define PLYLIBERROR   std::cerr
#  define PLYLIBWARN    std::cout
#  define PLYLIBINFO    std::cout
#endif

#define NUM_ELEMS( a ) (sizeof( a ) / sizeof( a[ 0 ] ))

#include <cstdint>
#include <exception>
#include <iostream>
#include <string>

namespace triply
{
// class forward declarations
class ModelTreeBase;
class ModelTreeData;
class ModelTreeNode;
class ModelTreeRoot;
class TreeRenderState;
class VertexData;
class PagedTreeData;

// basic type definitions
typedef vmml::vector< 3, float >      Vertex;
typedef vmml::vector< 3, uint8_t >    Color;
typedef vmml::vector< 3, float >      Normal;
typedef vmml::matrix< 4, 4, float >   Matrix4f;
typedef vmml::vector< 4, float >      Vector4f;
typedef size_t                        Index;
typedef GLushort                      ShortIndex;
typedef std::size_t                   PageKey;
typedef std::uint64_t                 ZKey;

// mesh exception
struct MeshException : public std::exception
{
    explicit MeshException( const std::string& msg ) : _message( msg ) {}
    virtual ~MeshException() throw() {}
    virtual const char* what() const throw() { return _message.c_str(); }
private:
    std::string _message;
};

// wrapper to enable array use where arrays would not be allowed otherwise
template< class T, size_t d > struct ArrayWrapper
{
    ArrayWrapper() {}
    ArrayWrapper( const T* from ) { memcpy( data, from, sizeof( data )); }
    T& operator[]( const size_t i )
        {
            PLYLIBASSERT( i < d );
            return data[i];
        }

    const T& operator[]( const size_t i ) const
        {
            PLYLIBASSERT( i < d );
            return data[i];
        }

private:
    T data[d];
};


// compound type definitions
typedef vmml::vector< 3, Index >    Triangle;
typedef ArrayWrapper< Vertex, 2 >   BoundingBox;
typedef vmml::vector< 4, float >    BoundingSphere;
typedef ArrayWrapper< float, 2 >    Range;
typedef std::pair< ZKey, Index >    ZKeyIndexPair;

struct ZKeyIndexPairLessCmpFunctor
{
   inline bool operator()( const ZKeyIndexPair& lhs, const ZKey& rhs ) const
   {
       return lhs.first < rhs;
   }
};

// Tree description
enum TreePartitionRule
{
    KDTREE_PARTITION = 0,
    OCTREE_PARTITION = 1,
};


const unsigned short    ZKEY_BIT_SIZE( 3 * ( 8*sizeof( ZKey ) / 3 ) );

// maximum triangle count per leaf node (keep in mind that the number of
// different vertices per leaf must stay below ShortIndex range; usually
// #vertices ~ #triangles/2, but max #vertices = #triangles * 3)
const Index             LEAF_SIZE( 21845 );

// binary mesh file version, increment if changing the file format
const unsigned short    FILE_VERSION( 0x011A );

// enumeration for the sort axis
enum Axis
{
    AXIS_X,
    AXIS_Y,
    AXIS_Z
};
inline std::ostream& operator << ( std::ostream& os, const Axis axis )
{
    os << ( axis == AXIS_X ? "x axis" : axis == AXIS_Y ? "y axis" :
            axis == AXIS_Z ? "z axis" : "ERROR" );
    return os;
}

// enumeration for the buffer objects
enum BufferObject
{
    VERTEX_OBJECT,
    NORMAL_OBJECT,
    COLOR_OBJECT,
    INDEX_OBJECT
};

// enumeration for the render modes
enum RenderMode
{
    RENDER_MODE_IMMEDIATE = 0,
    RENDER_MODE_DISPLAY_LIST,
    RENDER_MODE_BUFFER_OBJECT,
    RENDER_MODE_ALL // must be last
};
inline std::ostream& operator << ( std::ostream& os, const RenderMode mode )
{
    os << ( mode == RENDER_MODE_IMMEDIATE     ? "immediate mode" :
            mode == RENDER_MODE_DISPLAY_LIST  ? "display list mode" :
            mode == RENDER_MODE_BUFFER_OBJECT ? "VBO mode" : "ERROR" );
    return os;
}

// enumeration for kd-tree node types
enum NodeType
{
    ROOT_TYPE = 0x07,
    NODE_TYPE = 0xde,
    LEAF_TYPE = 0xef
};

enum PageType
{
    INVALID_PAGE_TYPE = -1,
    POSITION_PAGE_TYPE = 0,
    COLOR_PAGE_TYPE = 1,
    NORMAL_PAGE_TYPE = 2,
    SHORTINDEX_PAGE_TYPE = 3
};

const int TOTAL_PAGE_TYPES( SHORTINDEX_PAGE_TYPE + 1 );


// helper function for MMF (memory mapped file) reading
inline void memRead( char* destination, char** source, size_t length )
{
    memcpy( destination, *source, length );
    *source += length;
}

template <typename IntT>
inline unsigned int getByte(IntT word, unsigned char byte)
{
    return ((word >> (byte * 8)) & 0xFF);
}

/*  Determine whether the current architecture is little endian or not.  */
inline bool isArchitectureLittleEndian()
{
    unsigned char test[2] = { 1, 0 };
    short* x = reinterpret_cast< short* >( test );
    return ( *x == 1 );
}

/*  Determine number of bits used by the current architecture.  */
inline size_t getArchitectureBits()
{
    return ( sizeof( void* ) * 8 );
}

/*  Construct architecture dependent file name.  */
inline std::string getArchitectureFilename( const std::string& filename,
                                            const std::string& tag )
{
    std::ostringstream oss;
    oss << filename;
    if( tag.size() > 0 )
        oss << "." << tag;
    oss << ( isArchitectureLittleEndian() ? ".le" : ".be" ) << getArchitectureBits();
    oss << ".bin";
    return oss.str();
}

}


#ifdef EQUALIZER
namespace lunchbox
{
template<> inline void byteswap( triply::RenderMode& value )
{ byteswap( reinterpret_cast< uint32_t& >( value )); }

template<> inline void byteswap( triply::Range& value )
{
    byteswap( value[ 0 ]);
    byteswap( value[ 1 ]);
}
}
#endif
#endif // PLYLIB_TYPEDEFS_H
