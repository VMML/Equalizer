
/* Copyright (c) 2006-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
 *                    2015, David Steiner <steiner@ifi.uzh.ch> 
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

#ifndef EQ_PLY_INITDATA_H
#define EQ_PLY_INITDATA_H

#include "eqPly.h"

namespace eqPly
{
    class InitData : public co::Object
    {
    public:
        InitData();
        virtual ~InitData();

        void setFrameDataID( const eq::uint128_t& id )
            { _frameDataID = id; }

        eq::uint128_t getFrameDataID() const        { return _frameDataID; }
        const std::string& getWindowSystem() const  { return _windowSystem; }
        triply::RenderMode getRenderMode() const    { return _renderMode; }
        bool               useGLSL() const          { return _useGLSL; }
        const std::string& getGLSLVertexSource() const
            { return _glslVertexSource; }
        const std::string& getGLSLFragmentSource() const
            { return _glslFragmentSource; }
        uint32_t getMaxGlMemory() const
            { return _maxGlMemory; }
        bool               useInvertedFaces() const { return _invFaces; }
        bool               showLogo() const         { return _logo; }
        bool               useROI() const           { return _roi; }
        bool               useOutOfCore() const     { return _outOfCore; }

    protected:
        virtual void getInstanceData( co::DataOStream& os );
        virtual void applyInstanceData( co::DataIStream& is );

        void setWindowSystem( const std::string& windowSystem )
            { _windowSystem = windowSystem; }
        void setRenderMode( const triply::RenderMode renderMode )
            { _renderMode = renderMode; }
        void setGLSLVertexSource( const std::string& glslVertexSource )
            { _glslVertexSource = glslVertexSource; }
        void setGLSLFragmentSource( const std::string& glslFragmentSource )
            { _glslFragmentSource = glslFragmentSource; }
        void setMaxGlMemory( uint32_t maxGlMemory )
            { _maxGlMemory = maxGlMemory; }
        void enableGLSL()          { _useGLSL  = true; }
        void enableInvertedFaces() { _invFaces = true; }
        void disableLogo()         { _logo     = false; }
        void disableROI()          { _roi      = false; }
        void enableOutOfCore()     { _outOfCore = true; }

    private:
        eq::uint128_t      _frameDataID;
        std::string        _windowSystem;
        triply::RenderMode _renderMode;
        std::string        _glslVertexSource;
        std::string        _glslFragmentSource;
        uint32_t           _maxGlMemory;
        bool               _useGLSL;
        bool               _invFaces;
        bool               _logo;
        bool               _roi;
        bool               _outOfCore;
    };
}


#endif // EQ_PLY_INITDATA_H

