
/* Copyright (c) 2009, Stefan Eilemann <eile@equalizergraphics.com> 
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

#include "ddsLoadBalancer.h"

#include "compound.h"
#include "config.h"
#include "configVisitor.h"
#include "log.h"
#include "frame.h"
#include "view.h"
#include "segment.h"
#include <eq/base/debug.h>
#include <eq/client/viewport.h>
#include <eq/client/zoom.h>
#include <vmmlib/vmmlib.h>
using namespace eq::base;
using namespace std;



namespace eq
{
namespace server
{

namespace
{
class OutputFrameFinder : public ConstCompoundVisitor
{
public:
    OutputFrameFinder( const std::string& name  )
                             : _frame(0)
                             , _name( name )  {}

    virtual ~OutputFrameFinder(){}

    virtual VisitorResult visit( const Compound* compound )
        {
            const FrameVector& outputFrames = compound->getOutputFrames();
            for( FrameVector::const_iterator i = outputFrames.begin(); 
                 i != outputFrames.end(); ++i )
            {
                Frame* frame = *i;

                if ( frame->getName() == _name )
                {
                    _frame = frame;
                    return TRAVERSE_TERMINATE;
                }
            }
            return TRAVERSE_CONTINUE;
        }

    Frame* getResult() { return _frame; }

private:
    Frame* _frame;
    const std::string& _name;
};

}
DDSLoadBalancer::DDSLoadBalancer( const LoadBalancer& parent )
        : LoadBalancerIF( parent )
        , _compound( _parent.getCompound() )
{
    Channel* myChannel = _compound->getChannel();
    EQASSERT( myChannel );

    // Subscribe to channel load notification
    myChannel->addListener( this );

    const FrameVector& inputFrames = _compound->getInputFrames();
    for( FrameVector::const_iterator i = inputFrames.begin(); 
         i != inputFrames.end(); ++i )
    {
        const Frame* frame = *i;
        const Compound* root = _compound->getRoot();

        // find the output frame
        OutputFrameFinder finderOframe( frame->getName() );
        root->accept( finderOframe );
        Frame* outputFrame = finderOframe.getResult();
        _outputFrames.push_back( outputFrame );
        
        const Channel* channel = outputFrame->getChannel();
        const Segment* segment = channel->getSegment();
        const View* view =  channel->getView();
        
        Viewport viewport( segment->getViewport( ));
        viewport.intersect( view->getViewport( ));
        
        _viewports.push_back( viewport );
    }

    _updateZoomAndOffset();
    EQINFO << "New DDS LoadBalancer @" << (void*)this << endl;
}

DDSLoadBalancer::~DDSLoadBalancer()
{
    Channel*  channel   = _compound->getChannel();
    _compound->activate();
    EQASSERT( channel );
    // Unsubscribe to channel load notification
    channel->removeListener( this );
    EQINFO << "Remove DDSLoadBalancer @" << (void*)this << endl;

}

void DDSLoadBalancer::update( const uint32_t frameNumber )
{
    _updateZoomAndOffset();
}

void DDSLoadBalancer::notifyLoadData( Channel* channel,
                                      const uint32_t frameNumber, 
                                      const uint32_t nStatistics,
                                      const eq::Statistic* statistics  )
{
  
}

void DDSLoadBalancer::_updateZoomAndOffset()
{
    int count = 0;
 
    const PixelViewport pvp = _compound->getInheritPixelViewport();
        
    vector<Frame*> inputFrames = _compound->getInputFrames();
    for(  vector<Frame*>::const_iterator i  = inputFrames.begin(); 
         i != inputFrames.end(); ++i )
    {
        Frame* frame = *i;

        const Compound* compound = _outputFrames[ count ]->getCompound();
        
        // compute and apply input frame offset
        const int offsetX = static_cast< int >( static_cast< float >( pvp.w ) * 
                                                _viewports[ count ].x );
        const int offsetY = static_cast< int >( static_cast< float >( pvp.h ) * 
                                                _viewports[ count ].y );
        frame->setOffset( vmml::Vector2i( offsetX, offsetY ) );
        
        // compute and apply output frame zoo
        const int width   = static_cast< int >( static_cast< float >( pvp.w ) * 
                                                _viewports[ count ].w);
        const int height  = static_cast< int >( static_cast< float >( pvp.h ) * 
                                                _viewports[ count ].h); 

        const PixelViewport& compoundPVP = compound->getInheritPixelViewport();
        const float factorW = static_cast< float >( width ) / 
                                  static_cast< float >( compoundPVP.w );
        const float factorH = static_cast< float >( height ) / 
                                  static_cast< float >( compoundPVP.h );
            
        Zoom newZoom( factorW, factorH );
            
        _outputFrames[ count ]->setZoom( newZoom );
        count++;
            
    }
        

        
}
}
}