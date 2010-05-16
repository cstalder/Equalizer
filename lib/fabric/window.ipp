
/* Copyright (c) 2010, Stefan Eilemann <eile@equalizergraphics.com>
 * Copyright (c) 2010, Cedric Stalder <cedric.stalder@gmail.com> 
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

#include "window.h"

#include "channel.h"
#include "elementVisitor.h"
#include "task.h"

#include <eq/net/dataIStream.h>
#include <eq/net/dataOStream.h>

namespace eq
{
namespace fabric
{

namespace
{
#define MAKE_WINDOW_ATTR_STRING( attr ) ( std::string("EQ_WINDOW_") + #attr )
std::string _iAttributeStrings[] = {
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_STEREO ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_DOUBLEBUFFER ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_FULLSCREEN ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_DECORATION ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_SWAPSYNC ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_DRAWABLE ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_STATISTICS ),
    MAKE_WINDOW_ATTR_STRING( IATTR_HINT_SCREENSAVER ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_COLOR ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_ALPHA ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_DEPTH ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_STENCIL ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_ACCUM ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_ACCUM_ALPHA ),
    MAKE_WINDOW_ATTR_STRING( IATTR_PLANES_SAMPLES ),
    MAKE_WINDOW_ATTR_STRING( IATTR_FILL1 ),
    MAKE_WINDOW_ATTR_STRING( IATTR_FILL2 )
};
}

template< class P, class W, class C >
Window< P, W, C >::Window( P* parent )
        : _pipe( parent )
{
    EQASSERT( parent );
    parent->_addWindow( static_cast< W* >( this ) );
    notifyViewportChanged();
}

template< class P, class W, class C >
Window< P, W, C >::~Window( )
{    
    while( !_channels.empty( ))
    {
        C* channel = _channels.back();

        EQASSERT( channel->getWindow() == this );
        _removeChannel( channel );
        delete channel;
    }
    _pipe->_removeWindow( static_cast< W* >( this ) );
}

template< class P, class W, class C >
void Window< P, W, C >::backup()
{
    Object::backup();
    _backup = _data;
}

template< class P, class W, class C >
void Window< P, W, C >::restore()
{
    _data = _backup;
    _data.drawableConfig = DrawableConfig();

    Object::restore();
    notifyViewportChanged();
    setDirty( DIRTY_VIEWPORT );
}

template< class P, class W, class C >
void Window< P, W, C >::serialize( net::DataOStream& os,
                                   const uint64_t dirtyBits )
{
    Object::serialize( os, dirtyBits );
    if( dirtyBits & DIRTY_ATTRIBUTES )
        os.write( _data.iAttributes, IATTR_ALL * sizeof( int32_t ));
    if( dirtyBits & DIRTY_CHANNELS )
        os.serializeChildren( this, _channels );
    if( dirtyBits & DIRTY_VIEWPORT )
        os << _data.vp << _data.pvp << _data.fixedVP;
    if( dirtyBits & DIRTY_DRAWABLECONFIG )
        os << _data.drawableConfig;
}

template< class P, class W, class C >
void Window< P, W, C >::deserialize( net::DataIStream& is,
                                     const uint64_t dirtyBits )
{
    Object::deserialize( is, dirtyBits );
    if( dirtyBits & DIRTY_ATTRIBUTES )
        is.read( _data.iAttributes, IATTR_ALL * sizeof( int32_t ));
    if( dirtyBits & DIRTY_CHANNELS )
    {
        if( _mapNodeObjects( ))
        {
            ChannelVector result;
            is.deserializeChildren( this, _channels, result );
            _channels.swap( result );
            EQASSERT( _channels.size() == result.size( ));
        }
        else // consume unused ObjectVersions
        {
            net::ObjectVersionVector childIDs;
            is >> childIDs;
        }
    }
    if( dirtyBits & DIRTY_VIEWPORT )
    {
        is >> _data.vp >> _data.pvp >> _data.fixedVP;
        notifyViewportChanged();
    }
    if( dirtyBits & DIRTY_DRAWABLECONFIG )
        is >> _data.drawableConfig;
}

template< class P, class W, class C >
void Window< P, W, C >::create( C** channel )
{
    *channel = _pipe->getServer()->getNodeFactory()->createChannel( 
        static_cast< W* >( this ));
}

template< class P, class W, class C >
void Window< P, W, C >::release( C* channel )
{
    _pipe->getServer()->getNodeFactory()->releaseChannel( channel );
}

template< class P, class W, class C >
void Window< P, W, C >::_addChannel( C* channel )
{
    EQASSERT( channel->getWindow() == this );
    _channels.push_back( channel );
}

template< class P, class W, class C >
bool Window< P, W, C >::_removeChannel( C* channel )
{
    typename ChannelVector::iterator i = stde::find( _channels, channel );
    if( i == _channels.end( ))
        return false;

    _channels.erase( i );
    return true;
}

template< class P, class W, class C >
C* Window< P, W, C >::_findChannel( const uint32_t id )
{
    for( typename ChannelVector::const_iterator i = _channels.begin(); 
         i != _channels.end(); ++i )
    {
        C* channel = *i;
        if( channel->getID() == id )
            return channel;
    }
    return 0;
}

template< class P, class W, class C > const std::string&
Window< P, W, C >::getIAttributeString( const IAttribute attr )
{
    return _iAttributeStrings[attr];
}

template< class P, class W, class C >
WindowPath Window< P, W, C >::getPath() const
{
    const P* pipe = getPipe();
    EQASSERT( pipe );
    WindowPath path( pipe->getPath( ));
    
    const typename std::vector< W* >& windows = pipe->getWindows();
    typename std::vector< W* >::const_iterator i = std::find( windows.begin(),
                                                              windows.end(),
                                                              this );
    EQASSERT( i != windows.end( ));
    path.windowIndex = std::distance( windows.begin(), i );
    return path;
}

namespace
{
template< class W, class V >
VisitorResult _accept( W* window, V& visitor )
{ 
    VisitorResult result = visitor.visitPre( window );
    if( result != TRAVERSE_CONTINUE )
        return result;

    const typename W::ChannelVector& channels = window->getChannels();
    for( typename W::ChannelVector::const_iterator i = channels.begin(); 
         i != channels.end(); ++i )
    {
        switch( (*i)->accept( visitor ))
        {
            case TRAVERSE_TERMINATE:
                return TRAVERSE_TERMINATE;

            case TRAVERSE_PRUNE:
                result = TRAVERSE_PRUNE;
                break;
                
            case TRAVERSE_CONTINUE:
            default:
                break;
        }
    }

    switch( visitor.visitPost( window ))
    {
        case TRAVERSE_TERMINATE:
            return TRAVERSE_TERMINATE;

        case TRAVERSE_PRUNE:
            return TRAVERSE_PRUNE;
                
        case TRAVERSE_CONTINUE:
        default:
            break;
    }

    return result;
}
}

template< class P, class W, class C >
VisitorResult Window< P, W, C >::accept( Visitor& visitor )
{
    return _accept( static_cast< W* >( this ), visitor );
}

template< class P, class W, class C >
VisitorResult Window< P, W, C >::accept( Visitor& visitor  ) const
{
    return _accept( static_cast< const W* >( this ), visitor );
}

template< class P, class W, class C >
void Window< P, W, C >::setPixelViewport( const PixelViewport& pvp )
{
    EQASSERT( pvp.hasArea( ));
    if( !pvp.hasArea( ))
        return;

    _data.fixedVP = false;

    if( pvp == _data.pvp && _data.vp.hasArea( ))
        return;

    _data.pvp = pvp;
    _data.vp.invalidate();

    notifyViewportChanged();
    setDirty( DIRTY_VIEWPORT );
}

template< class P, class W, class C >
void Window< P, W, C >::setViewport( const Viewport& vp )
{
    if( !vp.hasArea())
        return;

    _data.fixedVP = true;

    if( vp == _data.vp && _data.pvp.hasArea( ))
        return;
    _data.vp = vp;
    _data.pvp.invalidate();

    setDirty( DIRTY_VIEWPORT );
    notifyViewportChanged();
}

template< class P, class W, class C >
void Window< P, W, C >::notifyViewportChanged()
{
    const PixelViewport pipePVP = _pipe->getPixelViewport();

    if( _data.fixedVP ) // update pixel viewport
    {
        const PixelViewport oldPVP = _data.pvp;
        _data.pvp = pipePVP;
        _data.pvp.apply( _data.vp );
        if( oldPVP != _data.pvp )
        {
            setDirty( DIRTY_VIEWPORT );
        }
    }
    else           // update viewport
    {
        const Viewport oldVP = _data.vp;
        _data.vp = _data.pvp.getSubVP( pipePVP );
        if( oldVP != _data.vp )
        {
            setDirty( DIRTY_VIEWPORT );
        }
    }

    for( typename ChannelVector::const_iterator i = _channels.begin(); 
         i != _channels.end(); ++i )
    {
        (*i)->notifyViewportChanged();
    }
    EQINFO << getName() << " viewport update: " << _data.vp << ":" << _data.pvp
           << std::endl;
}

template< class P, class W, class C >
void Window< P, W, C >::_setDrawableConfig(const DrawableConfig& drawableConfig)
{ 
    _data.drawableConfig = drawableConfig;
    setDirty( DIRTY_DRAWABLECONFIG );
}

}
}
