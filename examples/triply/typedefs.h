
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

#ifndef TRIPLY_TYPEDEFS_H
#define TRIPLY_TYPEDEFS_H

#ifdef EQUALIZER_USE_OPENGL
#  define EQUALIZER
#endif

#ifdef EQUALIZER
#  include <eq/eq.h>
#  define TRIPLYASSERT  LBASSERT
#  define TRIPLYERROR   LBERROR
#  define TRIPLYWARN    LBWARN
#  define TRIPLYINFO    LBINFO
#else
#  include <GL/glew.h>
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
#  define TRIPLYASSERT  assert
#  define TRIPLYERROR   std::cerr
#  define TRIPLYWARN    std::cout
#  define TRIPLYINFO    std::cout
#endif

#define NUM_ELEMS( a ) (sizeof( a ) / sizeof( a[ 0 ] ))
#define UNUSED(VAR) (void)VAR

#include <triply/api.h>
#include <boost/progress.hpp>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <sstream>

#ifdef NDEBUG
#  define TRIPLY_GL_ERROR( when ) {}
#  define TRIPLY_GL_CALL( code ) { code; }
#else // NDEBUG
#  define TRIPLY_GL_ERROR( when )                                       \
    {                                                                   \
        const GLenum eqGlError = glGetError();                          \
        if( eqGlError )                                                 \
        {                                                               \
            TRIPLYWARN << "Got GlError = " << eqGlError << " "          \
                       << when << " in " << __FILE__ << ':' << __LINE__ \
                       << std::endl << std::endl;                       \
            throw RenderException( "GlError" );                         \
        }                                                               \
    }
#  define TRIPLY_GL_CALL( code )                                        \
    {                                                                   \
        TRIPLY_GL_ERROR( std::string( "before " ) + #code );            \
        code;                                                           \
        TRIPLY_GL_ERROR( std::string( "after " ) + #code );             \
    }
#endif

namespace triply
{
// class forward declarations
class MeshData;
class ModelTreeBase;
class ModelTreeData;
class ModelTreeLeaf;
class ModelTreeNode;
class ModelTreeRoot;
class TreeGenerator;
class MKDGenerator;
class ZTreeGenerator;
class TreeDataManager;
class RenderState;
class SegmentedBuffer;

typedef ModelTreeBase* ModelTreeBasePtr;
typedef const ModelTreeBase* ConstModelTreeBasePtr;
typedef std::vector< ModelTreeBasePtr > ModelTreeBasePtrs;

typedef lunchbox::RefPtr< TreeDataManager > SharedDataManagerPtr;

// basic type definitions
typedef vmml::vector< 3, float >      Vertex;
typedef vmml::vector< 3, uint8_t >    Color;
typedef vmml::vector< 3, float >      Normal;
typedef vmml::matrix< 4, 4, float >   Matrix4f;
typedef vmml::vector< 4, float >      Vector4f;
typedef size_t                        Index;
typedef unsigned short                ShortIndex;

// Mesh exception
struct MeshException : public std::exception
{
    explicit MeshException( const std::string& msg ) : _message( msg ) {}
    virtual ~MeshException() throw() {}
    virtual const char* what() const throw() { return _message.c_str(); }
private:
    std::string _message;
};

// Render exception
struct RenderException : public std::exception
{
    explicit RenderException( const std::string& msg ) : _message( msg ) {}
    virtual ~RenderException() throw() {}
    virtual const char* what() const throw() { return _message.c_str(); }
private:
    std::string _message;
};

// wrapper to enable array use where arrays would not be allowed otherwise
template< class T, size_t d >
struct ArrayWrapper
{
    typedef T DataType;

    ArrayWrapper() {}
    explicit ArrayWrapper( const T* from ) { memcpy( data, from, sizeof( data )); }
    T& operator[]( const size_t i )
    {
        TRIPLYASSERT( i < d );
        return data[i];
    }

    const T& operator[]( const size_t i ) const
    {
        TRIPLYASSERT( i < d );
        return data[i];
    }

private:
    T data[d];
};

// Forward declarations
template < typename T > class DynArrayWrapper;
template < typename T > class LargeDynArrayWrapper;

// compound type definitions
typedef vmml::vector< 3, Index >    Triangle;
typedef ArrayWrapper< Vertex, 2 >   BoundingBox;
typedef vmml::vector< 4, float >    BoundingSphere;
typedef ArrayWrapper< float, 2 >    Range;

// Tree description class
struct TreeInfo
{
    TRIPLY_API TreeInfo( const std::string& treePartition, unsigned treeArity )
        : partition( treePartition ), arity( treeArity )
    { }

    // 'treeInfoString' format -> partition:arity (e.g. "kd:2", "z:8")
    TRIPLY_API TreeInfo( const std::string& treeInfoString )
        : partition( "" ), arity( 0 )
    {
        size_t pos = treeInfoString.rfind(':');
        if( pos != std::string::npos && pos < treeInfoString.length() - 1 )
        {
            std::istringstream iss( treeInfoString.substr( pos + 1 ));
            iss >> arity;
            partition = treeInfoString;
            partition.erase( pos );
        }
    }

    TRIPLY_API bool isValid() const { return partition.length() > 0 && arity > 0; }

    std::string partition;
    unsigned arity;
};


// maximum triangle count per leaf node (keep in mind that the number of
// different vertices per leaf must stay below ShortIndex range; usually
// #vertices ~ #triangles/2, but max #vertices = #triangles * 3)
const Index             LEAF_SIZE( 21845 );

// binary mesh file version, increment if changing the file format
const unsigned short    FILE_VERSION( 0x011C );

// enumerations for the buffer objects
enum BufferType
{
    INVALID_BUFFER_TYPE = -1,
    VERTEX_BUFFER_TYPE = 0,
    NORMAL_BUFFER_TYPE,
    COLOR_BUFFER_TYPE,
    INDEX_BUFFER_TYPE,
    BUFFER_TYPE_ALL // must be last
};

enum BufferElementSizeConstant
{
    VERTEX_BUFFER_ELEMENT_SIZE = sizeof( Vertex ),
    NORMAL_BUFFER_ELEMENT_SIZE = sizeof( Normal ),
    COLOR_BUFFER_ELEMENT_SIZE = sizeof( Color ),
    INDEX_BUFFER_ELEMENT_SIZE = sizeof( ShortIndex )
};

static const size_t BufferElementSizes[] = {
    VERTEX_BUFFER_ELEMENT_SIZE,
    NORMAL_BUFFER_ELEMENT_SIZE,
    COLOR_BUFFER_ELEMENT_SIZE,
    INDEX_BUFFER_ELEMENT_SIZE
};

// enumeration for the render modes
enum RenderMode
{
    RENDER_MODE_IMMEDIATE = 0,
    RENDER_MODE_DISPLAY_LIST,
    RENDER_MODE_BUFFER_OBJECT,
    RENDER_MODE_VA_OBJECT,
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

// helper function for MMF (memory mapped file) reading
inline void memRead( char* destination, char** source, size_t length )
{
    memcpy( destination, *source, length );
    *source += length;
}

template < typename T>
inline T* getAs( char* data, size_t index )
{
    return reinterpret_cast< T* >( data + index * sizeof( T ));
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
#endif // TRIPLY_TYPEDEFS_H
