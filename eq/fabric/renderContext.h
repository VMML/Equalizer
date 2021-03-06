
/* Copyright (c) 2006-2012, Stefan Eilemann <eile@equalizergraphics.com>
 *               2014-2015, David Steiner   <steiner@ifi.uzh.ch>
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 2.1 as published
 * by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef EQFABRIC_RENDERCONTEXT_H
#define EQFABRIC_RENDERCONTEXT_H

#include <eq/fabric/colorMask.h>        // member
#include <eq/fabric/eye.h>              // member
#include <eq/fabric/pixel.h>            // member
#include <eq/fabric/pixelViewport.h>    // member
#include <eq/fabric/range.h>            // member
#include <eq/fabric/subPixel.h>         // member
#include <eq/fabric/types.h>
#include <eq/fabric/zoom.h>             // member

#include <co/objectVersion.h>
#include <eq/fabric/api.h>

namespace eq
{
namespace fabric
{
/** The context applied to a channel during rendering operations. */
class RenderContext
{
public:
    EQFABRIC_API RenderContext();
    EQFABRIC_API void apply( const Tile& tile ); //!< @internal
    EQFABRIC_API void apply( const Chunk& chunk ); //!< @internal

    Frustumf       frustum;        //!< frustum for projection matrix
    Frustumf       ortho;          //!< ortho frustum for projection matrix

    Matrix4f       headTransform;  //!< frustum transform for modelview
    Matrix4f       orthoTransform; //!< orthographic frustum transform

    co::ObjectVersion view;        //!< destination view id and version
    uint128_t      frameID;        //!< identifier from Config::beginFrame
    PixelViewport  pvp;            //!< pixel viewport of channel wrt window
    Pixel          pixel;          //!< pixel decomposition wrt to dest
    Vector4i       overdraw;       //!< @internal for pw pp filters
    Viewport       vp;             //!< fractional viewport wrt dest view

    Vector2i       offset;         //!< absolute position wrt dest channel
    Range          range;          //!< database-range wrt to dest channel
    SubPixel       subpixel;       //!< subpixel decomposition wrt to dest
    Zoom           zoom;           //!< up/downsampling wrt to dest

    uint32_t       buffer;         //!< buffer as passed to glDrawBuffer()
    uint32_t       taskID;         //!< @internal per-channel task counter
    uint32_t       period;         //!< DPlex period
    uint32_t       phase;          //!< DPlex phase
    Eye            eye;            //!< current eye pass
    uint32_t       alignToEight;   //!< @internal padding

    ColorMask      bufferMask;     //!< color mask for anaglyph stereo

    Frustumf       frustumGlobal;  //!< frustum for projection matrix
    Frustumf       orthoGlobal;    //!< ortho frustum for projection matrix
    PixelViewport  pvpGlobal;      //!< pixel viewport of channel wrt window
    Viewport       vpGlobal;       //!< fractional viewport wrt dest view
    bool           stolen;         //!< has been stolen

    bool           alignDummy[16]; //!< @internal padding
};

    EQFABRIC_API std::ostream& operator << ( std::ostream& os,
                                             const RenderContext& ctx );
}
}

namespace lunchbox
{
template<> inline void byteswap( eq::fabric::RenderContext& value )
{
    byteswap( value.frustum );
    byteswap( value.frustumGlobal );
    byteswap( value.ortho );
    byteswap( value.orthoGlobal );

    byteswap( value.headTransform );
    byteswap( value.orthoTransform );

    byteswap( value.view );
    byteswap( value.frameID );
    byteswap( value.pvp );
    byteswap( value.pvpGlobal );
    byteswap( value.pixel );
    byteswap( value.overdraw );
    byteswap( value.vp );
    byteswap( value.vpGlobal );

    byteswap( value.offset );
    byteswap( value.range );
    byteswap( value.subpixel );
    byteswap( value.zoom );

    byteswap( value.buffer );
    byteswap( value.taskID );
    byteswap( value.period );
    byteswap( value.phase );
    byteswap( value.eye );

    byteswap( value.bufferMask );
    byteswap( value.stolen );
}
}

#endif // EQFABRIC_RENDERCONTEXT_H
