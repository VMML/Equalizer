
/* Copyright (c) 2011-2015, Stefan Eilemann <eile@equalizergraphics.com>
 *               2011, Carsten Rohn <carsten.rohn@rtt.ag>
 *               2011, Daniel Nachbaur <danielnachbaur@gmail.com>
 *               2015, David Steiner <steiner@ifi.uzh.ch>
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

#include "tileEqualizer.h"

#include "../compound.h"
#include "../compoundVisitor.h"
#include "../config.h"
#include "../server.h"
#include "../tileQueue.h"
#include "../view.h"

#include <co/global.h>

namespace eq
{
namespace server
{

TileEqualizer::TileEqualizer()
    : PackageEqualizer<eq::fabric::Tile, eq::fabric::Vector2i>()
{
    setName("TileEqualizer");
//     co::Global::setIAttribute( co::Global::IATTR_QUEUE_MIN_SIZE, 5 );
//     co::Global::setIAttribute( co::Global::IATTR_QUEUE_REFILL, 10 );
}

TileEqualizer::TileEqualizer( const TileEqualizer& from )
    : PackageEqualizer<eq::fabric::Tile, eq::fabric::Vector2i>( from )
{
}

std::ostream& operator << ( std::ostream& os, const TileEqualizer* lb )
{
    if( lb )
    {
        os << lunchbox::disableFlush
           << "tile_equalizer" << std::endl
           << "{" << std::endl
           << "    name \"" << lb->getName() << "\"" << std::endl
           << "    size " << lb->getTileSize() << std::endl
           << "}" << std::endl << lunchbox::enableFlush;
    }
    return os;
}

} //server
} //eq
