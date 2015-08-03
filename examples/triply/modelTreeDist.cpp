
/* Copyright (c) 2008-2013, Stefan Eilemann <eile@equalizergraphics.com>
 *                    2010, Cedric Stalder <cedric.stalder@gmail.com>
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

#include "modelTreeDist.h"

#include "modelTreeLeaf.h"
#include "modelTreeRoot.h"

namespace triply
{

ModelTreeDist::ModelTreeDist()
    : _treeRoot( 0 )
    , _treeNode( 0 )
    , _children( 0 )
    , _isRoot( false )
{ }

ModelTreeDist::ModelTreeDist( ModelTreeRoot* treeRoot )
    : _treeRoot( treeRoot )
    , _treeNode( treeRoot )
    , _children( 0 )
    , _isRoot( true )
{
    allocateChildArray();
    for( unsigned i=0; i < treeRoot->getArity(); ++i )
    {
        if( treeRoot->getChild( i ))
        {
            _children[i] = new ModelTreeDist( treeRoot, treeRoot->getChild( i ));
        }
    }
}

ModelTreeDist::ModelTreeDist( ModelTreeRoot* treeRoot, ModelTreeBase* treeNode )
        : _treeRoot( treeRoot )
        , _treeNode( treeNode )
        , _children( 0 )
        , _isRoot( false )
{
    if( !treeNode || treeNode->getNumberOfChildren() == 0)
        return;

    allocateChildArray();
    for( unsigned i=0; i < treeRoot->getArity(); ++i )
    {
        if( treeNode->getChild( i ) )
        {
            _children[i] = new ModelTreeDist( treeRoot, treeNode->getChild( i ));
        }
    }
}

ModelTreeDist::~ModelTreeDist()
{
    deallocateChildArray();
}

void ModelTreeDist::registerTree(co::LocalNodePtr localNode )
{
    LBASSERT( _treeRoot && _treeNode );
    LBASSERT( !isAttached() );
    LBCHECK( localNode->registerObject( this ));

    if( _treeNode->getNumberOfChildren() > 0 ) // if( !_treeNode->isLeaf() )
    {
        LBASSERT( _children );
        for( unsigned i=0; i < _treeRoot->getArity(); ++i )
        {
            if( _children[i] != 0 )
            {
                _children[i]->registerTree( localNode );
            }
        }
    }
}

void ModelTreeDist::deregisterTree()
{
    LBASSERT( _treeRoot && _treeNode );
    LBASSERT( isAttached() );
    LBASSERT( isMaster( ));

    getLocalNode()->deregisterObject( this );

    if( _treeNode->getNumberOfChildren() > 0 ) // if( !_treeNode->isLeaf() )
    {
        LBASSERT( _children );
        for( unsigned i=0; i < _treeRoot->getArity(); ++i )
        {
            if( _children[i] != 0 )
            {
                _children[i]->deregisterTree();
            }
        }
    }
}

ModelTreeRoot* ModelTreeDist::loadModel( co::NodePtr masterNode,
                                         co::LocalNodePtr localNode,
                                         const eq::uint128_t& modelID )
{
    LBASSERT( !_treeRoot && !_treeNode );

    if( !localNode->syncObject( this, masterNode, modelID ))
    {
        LBWARN << "Mapping of model failed" << std::endl;
        return 0;
    }
    return _treeRoot;
}

unsigned ModelTreeDist::getNumberOfChildren() const
{
    if ( _children == 0 )
        return 0;
    LBASSERT( _treeRoot );
    unsigned result = 0;
    for( unsigned i=0; i < _treeRoot->getArity(); ++i )
    {
        if( _children[i] != 0 )
            result++;
    }
    return result;
}

void ModelTreeDist::getInstanceData( co::DataOStream& os )
{
    LBASSERT( _treeRoot && _treeNode );
    unsigned arity = _treeRoot->getArity();
    unsigned numChildren = _treeNode->getNumberOfChildren();
    os << _isRoot << arity << numChildren;

    if( numChildren > 0 )
    {
        for( unsigned i=0; i < arity; ++i)
        {
            if( _children[i] != 0 )
            {
                os << i << _children[i]->getID();
            }
        }

        if( _isRoot )
        {
            LBASSERT( _treeRoot );

            os << int32_t( _treeRoot->_partition );
            os << _treeRoot->_invertFaces;
            os << _treeRoot->_inCoreData;
            if( _treeRoot->_inCoreData )
            {
                const ModelTreeData& treeData = _treeRoot->_treeData;
                os << treeData.vertices << treeData.colors
                   << treeData.normals << treeData.indices;
            }
            os << _treeRoot->_name;
        }
    }
    else
    {
        LBASSERT( dynamic_cast< const ModelTreeLeaf* >( _treeNode ));
        const ModelTreeLeaf* leaf =
            static_cast< const ModelTreeLeaf* >( _treeNode );

        os << leaf->_boundingBox[0] << leaf->_boundingBox[1]
           << uint64_t( leaf->_vertexStart ) << uint64_t( leaf->_indexStart )
           << uint64_t( leaf->_indexLength ) << leaf->_vertexLength;
    }

    os << _treeNode->_boundingSphere << _treeNode->_range;
}

void ModelTreeDist::applyInstanceData( co::DataIStream& is )
{
    LBASSERT( !_treeNode );

    ModelTreeNode* node = 0;
    ModelTreeBase* base = 0;
    unsigned arity, numChildren;
    is >> _isRoot >> arity >> numChildren ;

    if( numChildren > 0 )
    {
        eq::uint128_t* childIDs = new eq::uint128_t[arity];
        for( unsigned i=0; i < arity; ++i)
        {
            childIDs[i] = 0;
        }

        for( unsigned i=0; i < numChildren; ++i)
        {
            unsigned childNumber;
            is >> childNumber;
            is >> childIDs[ childNumber ];
        }

        if( _isRoot )
        {
            ModelTreeRoot* root = new ModelTreeRoot( arity );

            int32_t partition;
            is >> partition;
            root->_partition = TreePartitionRule( partition );
            is >> root->_invertFaces;
            is >> root->_inCoreData;
            if( root->_inCoreData)
            {
                ModelTreeData& treeData = root->_treeData;
                is >> treeData.vertices >> treeData.colors
                   >> treeData.normals >> treeData.indices;
            }
            is >> root->_name;

            node  = root;
            _treeRoot = root;
        }
        else
        {
            LBASSERT( _treeRoot );
            node = new ModelTreeNode( arity );
        }

        base   = node;

        if ( _children == 0 )
            allocateChildArray();
        co::NodePtr remoteNode = is.getRemoteNode();
        co::LocalNodePtr localNode = is.getLocalNode();
        for( unsigned i=0; i < arity; ++i)
        {
            if( childIDs[i] != 0 )
            {
                _children[i] = new ModelTreeDist( _treeRoot, 0 );
                co::f_bool_t childSync =
                    localNode->syncObject( _children[i], remoteNode, childIDs[i] );
                LBCHECK( childSync.wait( ) );
            }
        }

        for( unsigned i=0; i < arity; ++i)
        {
            if( _children[i] != 0 )
                node->_children[i] = _children[i]->_treeNode;
        }
    }
    else
    {
        LBASSERT( !_isRoot );
        ModelTreeLeaf* leaf = new ModelTreeLeaf( _treeRoot->_treeData );

        uint64_t i1, i2, i3;
        is >> leaf->_boundingBox[0] >> leaf->_boundingBox[1]
           >> i1 >> i2 >> i3 >> leaf->_vertexLength;
        leaf->_vertexStart = size_t( i1 );
        leaf->_indexStart = size_t( i2 );
        leaf->_indexLength = size_t( i3 );

        base = leaf;
    }

    LBASSERT( base );
    is >> base->_boundingSphere >> base->_range;

    _treeNode = base;
}

void ModelTreeDist::allocateChildArray()
{
    LBASSERT( _treeRoot );
    LBASSERT( !_children );
    _children = new ModelTreeDist*[ _treeRoot->getArity() ];
    for( unsigned i=0; i < _treeRoot->getArity(); ++i )
    {
        _children[i] = 0;
    }
}

void ModelTreeDist::deallocateChildArray()
{
    LBASSERT( _treeRoot );
    if( _children != 0 )
    {
        for( unsigned i=0;  i < _treeRoot->getArity(); ++i )
        {
            if( _children[i] != 0 )
            {
                delete _children[i];
            }
        }
        delete[] _children;
        _children = 0;
    }
}

}
