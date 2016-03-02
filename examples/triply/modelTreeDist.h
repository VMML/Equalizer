
/* Copyright (c) 2008-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
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


#ifndef TRIPLY_MODELTREEDIST_H
#define TRIPLY_MODELTREEDIST_H

#include "typedefs.h"
#include "dynArrayWrappers.h"
#include <triply/api.h>

#include <co/co.h>

namespace triply
{
/** Uses co::Object to distribute a model, holds a ModelTreeBase node. */
class ModelTreeDist : public co::Object
{
public:
    TRIPLY_API ModelTreeDist();
    TRIPLY_API ModelTreeDist( triply::ModelTreeRoot* treeRoot );
    TRIPLY_API virtual ~ModelTreeDist();

    TRIPLY_API void registerTree( co::LocalNodePtr localNode );
    TRIPLY_API void deregisterTree();

    TRIPLY_API triply::ModelTreeRoot* loadModel( co::NodePtr masterNode,
                                                 co::LocalNodePtr localNode,
                                                 const eq::uint128_t& modelID );

protected:
    ModelTreeDist( ModelTreeRoot* treeRoot, ModelTreeBase* treeNode );

    void clear( );
    unsigned getNumberOfChildren( ) const;

    virtual void getInstanceData( co::DataOStream& os );
    virtual void applyInstanceData( co::DataIStream& is );

private:
    ModelTreeRoot*  _treeRoot;
    ModelTreeBase*  _treeNode;
    DynArrayWrapper< ModelTreeDist* > _children;
    bool _isRoot;
};
}


#endif // TRIPLY_MODELTREEDIST_H
