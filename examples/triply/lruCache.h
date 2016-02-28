
/* Copyright (c)      2016, Enrique G. Paredes <egparedes@ifi.uzh.ch>
 *                    2007, Tobias Wolf <twolf@access.unizh.ch>
 *               2009-2014, Stefan Eilemann <eile@equalizergraphics.com>
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


#ifndef TRIPLY_LRUCACHE_H
#define TRIPLY_LRUCACHE_H

#include "typedefs.h"
#include <triply/api.h>
#include <list>

namespace triply
{

template < typename KeyT >
class LRUCache
{
public:
    typedef KeyT KeyType;

    TRIPLY_API void push( const KeyT& key )
    {
        auto it = _keyMap.find( key );
        if( it != _keyMap.end( ))
        {
            _lruList.erase( it->second );
            _keyMap.erase( it );
        }

        _lruList.push_back( key );
        auto lastIt =_lruList.rbegin();
        _keyMap[key] = (++lastIt).base();
    }

    TRIPLY_API void pushPop( const KeyT& key, KeyT& deletedKey )
    {
        if(_keyMap.find( key ) == _keyMap.end( ) &&
            _lruList.begin() != _lruList.end( ))
        {
            deletedKey = *( _lruList.begin( ));
            _lruList.pop_front();
            _keyMap.erase( deletedKey );
            push( key );
        }
    }

    TRIPLY_API void use( const KeyT& key )
    {
        auto it = _keyMap.find( key );
        if( it != _keyMap.end( ))
            _lruList.splice( _lruList.end(), _lruList, it->second );
    }

    TRIPLY_API void remove( const KeyT& key )
    {
        auto it = _keyMap.find( key );
        if( it != _keyMap.end( ))
        {
            _lruList.erase( it->second );
            _keyMap.erase( it );
        }
    }

    TRIPLY_API void pop( KeyT& deletedKey )
    {
        if( _keyMap.size() > 0 )
        {
            deletedKey = _lruList.front();
            _lruList.pop_front();
            _keyMap.erase( deletedKey );
        }
    }

    TRIPLY_API bool exists( const KeyT& key ) const
    {
        return _keyMap.find( key ) != _keyMap.end();
    }

    TRIPLY_API size_t size() const
    {
        return _lruList.size();
    }

private:
    std::list< KeyT > _lruList;
    stde::hash_map< KeyT, typename std::list< KeyT >::iterator > _keyMap;
};

} //namespace triply

#endif // TRIPLY_LRUCACHE_H
