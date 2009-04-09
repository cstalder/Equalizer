
/* Copyright (c) 2008-2009, Stefan Eilemann <eile@equalizergraphics.com> 
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
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

#include "frustum.h"

#include "config.h"
#include "frustumData.h"

#include <eq/net/dataIStream.h>
#include <eq/net/dataOStream.h>

using namespace eq::base;

namespace eq
{
namespace server
{
Frustum::Frustum( FrustumData& data )
        : _data( data )
{
    _updateFrustum();
}

Frustum::Frustum( const Frustum& from, FrustumData& data )
        : eq::Frustum( from )
        , _data( data )
{
    _updateFrustum();
}

void Frustum::setWall( const eq::Wall& wall )
{
    eq::Frustum::setWall( wall );
    _updateFrustum();
}
        
void Frustum::setProjection( const eq::Projection& projection )
{
    eq::Frustum::setProjection( projection );
    _updateFrustum();
}

void Frustum::_updateFrustum()
{
    switch( getCurrentType( ))
    {
        case TYPE_WALL:
            _data.applyWall( getWall( ));
            break;
        case TYPE_PROJECTION:
            _data.applyProjection( getProjection( ));
            break;

        case TYPE_NONE:
            _data.invalidate();
            break;
        default:
            EQUNREACHABLE;
    }
}

}
}