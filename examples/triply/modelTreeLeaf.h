
/* Copyright (c) 2007-2015, Tobias Wolf <twolf@access.unizh.ch>
 *                          Stefan Eilemann <eile@equalizergraphics.com>
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

#ifndef TRIPLY_MODELTREELEAF_H
#define TRIPLY_MODELTREELEAF_H

#include "modelTreeBase.h"

namespace triply
{

/*  The class for kd-tree leaf nodes.  */
class ModelTreeLeaf : public ModelTreeBase
{
public:
    explicit ModelTreeLeaf( ModelTreeData& treeData );
    explicit ModelTreeLeaf( ModelTreeData& treeData,
                            Index indexStart, Index indexLength,
                            Index vertexStart, ShortIndex vertexLength );

    virtual void clear() override;

    virtual void draw( RenderState& state ) const override;
    virtual Index getNumberOfVertices() const override { return _indexLength; }

protected:
    virtual void toStream( std::ostream& os ) override;
    virtual void fromMemory( char** addr, ModelTreeData& treeData ) override;

    virtual const BoundingSphere& updateBoundingSphere() override;
    virtual void updateRange() override;

private:
    void setupRendering(RenderState& state, GLuint* glBuffers ) const;
    void loadLeafData( bool useColors ) const;

    void renderImmediate( RenderState& state ) const;
    void renderDisplayList( RenderState& state ) const;
    void renderBufferObject( RenderState& state ) const;
    void renderVAObject( RenderState& state ) const;

    friend class ModelTreeDist;

    ModelTreeData&              _treeData;
    BoundingBox                 _boundingBox;
    Index                       _indexStart;
    Index                       _indexLength;
    Index                       _vertexStart;
    ShortIndex                  _vertexLength;

    // Mutable for out-of-core rendering
    mutable char*               _dataBuffers[4];
    mutable bool                _dataLoaded;
};

} // namespace

#endif // TRIPLY_MODELTREELEAF_H
