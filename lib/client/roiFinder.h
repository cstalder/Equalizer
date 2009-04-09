/* Copyright (c) 2009       Maxim Makhinya
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

#ifndef EQ_ROI_FINDER_H
#define EQ_ROI_FINDER_H

#include "roiEmptySpaceFinder.h"

#include <eq/client/image.h>   // member

#include <vector>
#include <string>


namespace eq
{
    /**
     * Processes image with depth information and selects areas for read back.
     */
    class EQ_EXPORT ROIFinder
    {
    public:
        ROIFinder();
        virtual ~ROIFinder(){};

        PixelViewportVector findRegions( const uint32_t         buffers,
                                         const PixelViewport&   pvp,
                                         const Zoom&            zoom,
                                         Window::ObjectManager* glObjects );

        /** @return the GL function table, valid during readback. */
        GLEWContext* glewGetContext() { return _glObjects->glewGetContext(); }

    protected:

    private:
        struct Area;

        const void* _getInfoKey( ) const;

        /** Called from getReadbackInfo. Calculates per-block statistic before 
            actuall read-back */
        void _readbackInfo();

        void _dumpDebug( const std::string file );

        void _init( );

        /** For debugging purposes */
        void _fillWithColor( const PixelViewport& pvp, uint8_t* dst,
                             const uint8_t val );

        /** updates dimensions and resizes arrays */
        void _resize( const PixelViewport& pvp );

        void _calcHist( const PixelViewport& pvp, const uint8_t* src );

        PixelViewport _getObjectPVP( const PixelViewport& pvp,
                                     const uint8_t* src );

        /** result is returned via _finalAreas array */
        uint8_t _splitArea( Area& area );

        void _updateSubArea( const uint8_t type );

        /** find areas in current mask*/
        void _findAreas( PixelViewportVector& resultPVPs );

#ifndef NDEBUG
        void _invalidateAreas( Area* areas, uint8_t num );
#endif

        struct Dims
        {
            uint8_t x1, x2, x3;
            uint8_t y1, y2, y3;
            uint8_t w1, w2, w3, w4, w5, w6, w7, w8;
            uint8_t h1, h2, h3, h4, h5, h6, h7, h8;
        } _dim;

        struct Area
        {
            Area(){};
            Area( PixelViewport pvp_ ) : emptySize( 0 ), pvp ( pvp_ ) {}

            int32_t       emptySize;
            PixelViewport pvp;
            PixelViewport hole;
#ifndef NDEBUG
            bool          valid;
#endif
        };

        Area  _tmpAreas[17];  //!< possible arreas
        Area* _finalAreas[4]; //!< links to picked areas from _tmpAreas

        std::vector< Area > _areasToCheck;

        PixelViewport       _pvp;       //! current pvp

        ROIEmptySpaceFinder   _emptyFinder;

        int32_t _w;
        int32_t _h;
        int32_t _wh;
        int32_t _wb;
        int32_t _hb;
        int32_t _wbhb;

        byteVec _tmpMask;
        byteVec _mask;

        std::vector<float> _perBlockInfo;

        uint8_t _histX[256];
        uint8_t _histY[256];

        Image _tmpImg; //! used for dumping debug info

        /** The GL object manager, valid during a readback operation. */
        Window::ObjectManager* _glObjects;
    };
}

#endif // EQ_ROI_FINDER_H
