
/* Copyright (c) 2006-2009, Stefan Eilemann <eile@equalizergraphics.com> 
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

#include "frustumData.h"

#include <eq/client/projection.h>
#include <eq/client/wall.h>

#define DEG2RAD( angle ) ( (angle) * static_cast<float>(M_PI) / 180.f )

namespace eq
{
namespace server
{

FrustumData::FrustumData()
        : _width(0.f)
        , _height(0.f)
        , _type( Wall::TYPE_FIXED )
{
}

void FrustumData::applyWall( const eq::Wall& wall )
{
    vmml::Vector3f u = wall.bottomRight - wall.bottomLeft;
    vmml::Vector3f v = wall.topLeft - wall.bottomLeft;
    vmml::Vector3f w( u[1]*v[2] - u[2]*v[1],
                      u[2]*v[0] - u[0]*v[2],
                      u[0]*v[1] - u[1]*v[0] );

    _type   = wall.type;
    _width  = u.normalize();
    _height = v.normalize();
    w.normalize();

    _xfm.ml[0]  = u[0];
    _xfm.ml[1]  = v[0];
    _xfm.ml[2]  = w[0];
    _xfm.ml[3]  = 0.;
             
    _xfm.ml[4]  = u[1];
    _xfm.ml[5]  = v[1];
    _xfm.ml[6]  = w[1];
    _xfm.ml[7]  = 0.;
             
    _xfm.ml[8]  = u[2];
    _xfm.ml[9]  = v[2];
    _xfm.ml[10] = w[2];
    _xfm.ml[11] = 0.;

    const vmml::Vector3f center((wall.bottomRight[0] + wall.topLeft[0]) * 0.5f,
                                (wall.bottomRight[1] + wall.topLeft[1]) * 0.5f,
                                (wall.bottomRight[2] + wall.topLeft[2]) * 0.5f);

    _xfm.ml[12] = -(u[0]*center[0] + u[1]*center[1] + u[2]*center[2]);
    _xfm.ml[13] = -(v[0]*center[0] + v[1]*center[1] + v[2]*center[2]);
    _xfm.ml[14] = -(w[0]*center[0] + w[1]*center[1] + w[2]*center[2]);
    _xfm.ml[15] = 1.;
}

void FrustumData::applyProjection( const eq::Projection& projection )
{
    const float cosH = cosf( DEG2RAD( projection.hpr[0] ));
    const float sinH = sinf( DEG2RAD( projection.hpr[0] ));
    const float cosP = cosf( DEG2RAD( projection.hpr[1] ));
    const float sinP = sinf( DEG2RAD( projection.hpr[1] ));
    const float cosR = cosf( DEG2RAD( projection.hpr[2] ));
    const float sinR = sinf( DEG2RAD( projection.hpr[2] ));

    // HPR Matrix = -roll[z-axis] * -pitch[x-axis] * -head[y-axis]
    const float rot[9] =
        {
            sinR*sinP*sinH + cosR*cosH,  cosR*sinP*sinH - sinR*cosH,  cosP*sinH,
            cosP*sinR,                   cosP*cosR,                  -sinP,
            sinR*sinP*cosH - cosR*sinH,  cosR*sinP*cosH + sinR*sinH,  cosP*cosH 
        };

    // translation = HPR x -origin
    const vmml::Vector3f& origin   = projection.origin;
    const float           distance = projection.distance;
    const vmml::Vector3f  trans(
               -( rot[0]*origin[0] + rot[3]*origin[1] + rot[6]*origin[2] ),
               -( rot[1]*origin[0] + rot[4]*origin[1] + rot[7]*origin[2] ),
               -( rot[2]*origin[0] + rot[5]*origin[1] + rot[8]*origin[2] ));

    _xfm.ml[0]  = rot[0];
    _xfm.ml[1]  = rot[1];
    _xfm.ml[2]  = rot[2];
    _xfm.ml[3]  = 0.;

    _xfm.ml[4]  = rot[3];
    _xfm.ml[5]  = rot[4];
    _xfm.ml[6]  = rot[5];
    _xfm.ml[7]  = 0.;
            
    _xfm.ml[8]  = rot[6];                
    _xfm.ml[9]  = rot[7];
    _xfm.ml[10] = rot[8];
    _xfm.ml[11] = 0.;

    _xfm.ml[12] = trans[0];
    _xfm.ml[13] = trans[1];
    _xfm.ml[14] = trans[2] + distance;
    _xfm.ml[15] = 1.;

    _width  = distance * 2.f * tanf(DEG2RAD( .5f * projection.fov[0] ));
    _height = distance * 2.f * tanf(DEG2RAD( .5f * projection.fov[1] ));
    _type   = Wall::TYPE_FIXED;
}
 
std::ostream& operator << ( std::ostream& os, const FrustumData& frustumData )
{
    os << "size: " << frustumData.getWidth() << "x" << frustumData.getHeight()
       << " xfm: " << frustumData.getTransform() << std::endl;
    return os;
}

}
}