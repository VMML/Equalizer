
/* Copyright (c)      2007, Tobias Wolf <twolf@access.unizh.ch>
 *               2008-2013, Stefan Eilemann <eile@equalizergraphics.com>
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

#ifndef TRIPLY_MODELTREENODE_H
#define TRIPLY_MODELTREENODE_H

#include "typedefs.h"
#include "modelTreeBase.h"
#include <triply/api.h>

namespace triply
{
/* The class for regular (non-leaf) tree nodes.  */
class ModelTreeNode : public ModelTreeBase
{
public:
    ModelTreeNode( )
        : _arity( 0 ), _children( 0 )
    {}

    ModelTreeNode( unsigned arity );

    TRIPLY_API virtual ~ModelTreeNode();

    TRIPLY_API void draw( RenderState& state ) const override;
    TRIPLY_API Index getNumberOfVertices() const override;

    virtual const ModelTreeBase* getChild( unsigned char childId ) const override
    {
        return ( _children[childId] != 0 )? _children[childId]: 0;
    }

    virtual ModelTreeBase* getChild( unsigned char childId ) override
    {
        return ( _children[childId] != 0 )? _children[childId]: 0;
    }

    TRIPLY_API virtual unsigned getNumberOfChildren() const override;

    TRIPLY_API unsigned getArity() const { return _arity; }

protected:

    virtual void toStream( std::ostream& os ) override;
    virtual void fromMemory( char** addr, ModelTreeData& treeData ) override;

    virtual void setupMKDTree( VertexData& modelData,
                               const Index start, const Index length,
                               const Axis axis, const size_t depth,
                               ModelTreeData& treeData,
                               boost::progress_display& progress) override;

    virtual void setupZOctree( VertexData& modelData,
                               const std::vector< ZKeyIndexPair >& zKeys,
                               const ZKey beginKey, const ZKey endKey,
                               const Vertex center, const size_t depth,
                               ModelTreeData& treeData,
                               boost::progress_display& progress ) override;

    virtual const BoundingSphere& updateBoundingSphere() override;
    virtual void updateRange() override;

private:
    friend class ModelTreeDist;
    friend class ModelTreeRoot;

    void allocateChildArray();
    void deallocateChildArray();

    unsigned _arity;
    ModelTreeBase** _children;
};
}

#endif // TRIPLY_MODELTREENODE_H
