
/* Copyright (c) 2009, Cedric Stalder <cedric.stalder@gmail.com> 
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
 
#include "compressorRLE3B.h"

namespace eq
{
namespace plugin
{
const uint64_t _rleMarker = 0xF3C553FF64F6477Full; // just a random number


void CompressorRLE3B::compress( void* const inData, 
                                const uint64_t inSize, 
                                const bool useAlpha )
{
    _setupResults( inSize );

    const uint32_t numResult = _results.size();
   
    const float width = static_cast< float >( inSize ) /  
                        static_cast< float >( numResult );
    
    uint8_t* const data = (uint8_t* const )inData;
    
    #pragma omp parallel for
    for ( size_t i = 0; i < numResult; i += 4 )
    {
        const uint32_t startIndex = 
            static_cast< uint32_t >( i / _numChannels * width ) * 
               _numChannels;
        const uint32_t nextIndex  =
            static_cast< uint32_t >(( i/_numChannels + 1 ) * width) * 
              _numChannels;
        const uint64_t inSizeParallel  = (nextIndex - startIndex) / 
            _numChannels;
        
        _writeHeader( &_results[i], Header( inSizeParallel, useAlpha ));

        _compress( &data[ startIndex ], inSizeParallel, 
                            &_results[i]);

    }

}

void CompressorRLE3B::_compress( const uint8_t* input, const uint64_t size, 
                                 Result** results )
{
    uint8_t* outOne(   results[ 0 ]->data + sizeof( Header ) ); 
    uint8_t* outTwo(   results[ 1 ]->data + sizeof( Header ) ); 
    uint8_t* outThree( results[ 2 ]->data + sizeof( Header ) ); 

    const uint32_t* input32   = reinterpret_cast< const uint32_t* >( input );
    uint32_t swizzleData = *input32;

    const uint8_t* constData = 
                             reinterpret_cast< const uint8_t* >( &swizzleData );

    uint8_t lastOne( constData[0] ), lastTwo( constData[1] ), 
            lastThree( constData[2] );
    uint8_t sameOne( 1 ), sameTwo( 1 ), sameThree( 1 );
    
    const uint8_t* data   = reinterpret_cast< const uint8_t* >( input ) + 1;
    uint8_t one;
    uint8_t two;
    uint8_t three;

    for( uint32_t i = 1; i < size; ++i )
    {
        input32   = reinterpret_cast< const uint32_t* >( data );
        swizzleData = *input32;
        if ( _swizzleData )
            _swizzlePixelData( &swizzleData, false );
        
        const uint8_t* word = 
                             reinterpret_cast< const uint8_t* >( &swizzleData );
        
        one = word[0];
        if( one == lastOne && sameOne != 255 )
            ++sameOne;
        else
        {
            WRITE_OUTPUT( One );
            lastOne = one;
            sameOne = 1;
        }
             
        ++data;
        two = word[1];
        if( two == lastTwo && sameTwo != 255 )
            ++sameTwo;
        else
        {
            WRITE_OUTPUT( Two );
            lastTwo = two;
            sameTwo = 1;
        }
        
        ++data;
        three = word[2];
        if( three == lastThree && sameThree != 255 )
            ++sameThree;
        else
        {
            WRITE_OUTPUT( Three );
            lastThree = three;
            sameThree = 1;
        }
        ++data;
    }

    WRITE_OUTPUT( One );
    WRITE_OUTPUT( Two );
    WRITE_OUTPUT( Three )

    _results[0]->size = outOne   - _results[0]->data;
    _results[1]->size = outTwo   - _results[1]->data;
    _results[2]->size = outThree - _results[2]->data;
}


void CompressorRLE3B::decompress( const void** const inData, 
                                  const uint64_t* const inSizes,
                                  void* const outData, 
                                  const uint64_t* const outSize )
{

    const uint8_t** inData8 = reinterpret_cast< const uint8_t** >( inData );
    uint8_t*       out   = reinterpret_cast< uint8_t* >( outData );

    // decompress 
    // On OS X the loop is sometimes slower when parallelized. Investigate this!
    uint64_t numBlock = inSizes[0] / 3;
    for( uint64_t i = 0; i < numBlock ; i++ )
    {
        const uint8_t* oneIn      = inData8[  i*4 + 0 ] + sizeof( Header );
        const uint8_t* twoIn      = inData8[  i*4 + 1 ] + sizeof( Header );  
        const uint8_t* threeIn    = inData8[  i*4 + 2 ] + sizeof( Header );
                
        uint8_t one(0), two(0), three(0);
        uint8_t oneLeft(0), twoLeft(0), threeLeft(0);


        const uint64_t* u64In = 
            reinterpret_cast< const uint64_t* >( inData8[i] );
        const uint64_t  blockSize = u64In[0];

        bool ignoreAlpha = (bool)*oneIn;
        oneIn++;
        
        for( uint32_t j = 0; j < blockSize; ++j )
        { 
           uint32_t result;
           if( oneLeft == 0 )
           {
               one = *oneIn; ++oneIn;
               if( one == _rleMarker )
               {
                   one     = *oneIn; ++oneIn;
                   oneLeft = *oneIn; ++oneIn;
               }
               else // single symbol
                        oneLeft = 1;
           }
           --oneLeft;


           if( twoLeft == 0 )
           {
               two = *twoIn; ++twoIn;
               if( two == _rleMarker )
               {
                   two     = *twoIn; ++twoIn;
                   twoLeft = *twoIn; ++twoIn;
               }
               else // single symbol
                   twoLeft = 1;
               
           }
           --twoLeft;

           if( threeLeft == 0 )
           {
               three = *threeIn; ++threeIn;
               if( three == _rleMarker )
               {
                   three     = *threeIn; ++threeIn;
                   threeLeft = *threeIn; ++threeIn;
               }
               else // single symbol
                   threeLeft = 1;

              
           }
           --threeLeft;    

           result = one + (two<<8) + (three<<16);
           if ( _swizzleData )
                _unswizzlePixelData( &result, false );
           
           uint8_t* result8 = reinterpret_cast< uint8_t* >( &result ); 
           out[j*3] = result8[0];
           out[j*3+1] = result8[0];
           out[j*3+2] = result8[0];
       }
    }
    return;
}


void CompressorRLE3B::_swizzlePixelData( uint32_t* data, const bool useAlpha )
{

    if ( useAlpha )
        *data =  ( *data &  (EQ_BIT32 | EQ_BIT31 | EQ_BIT22 | EQ_BIT21 | 
                            EQ_BIT11 | EQ_BIT12 | EQ_BIT2 | EQ_BIT1)) | 
                (( *data & ( EQ_BIT8  | EQ_BIT7 ))<<18)  | 
                (( *data & ( EQ_BIT24 | EQ_BIT23 | EQ_BIT13 | EQ_BIT14))<<6)|
                (( *data & ( EQ_BIT16 | EQ_BIT15 | EQ_BIT6  | 
                             EQ_BIT5  | EQ_BIT4  | EQ_BIT3 ))<<12 )| 
                (( *data & ( EQ_BIT28 | EQ_BIT27 |EQ_BIT26  | EQ_BIT25))>>18)|
                (( *data & ( EQ_BIT18 | EQ_BIT17 ))>>12)    |
                (( *data & ( EQ_BIT30 | EQ_BIT29 | EQ_BIT20 | 
                             EQ_BIT19 | EQ_BIT10 | EQ_BIT9 ))>>6); 
                             
    else
        *data =   ( *data & ( EQ_BIT24 | EQ_BIT23 | EQ_BIT22 | EQ_BIT13 | 
                              EQ_BIT12 | EQ_BIT3  | EQ_BIT2  | EQ_BIT1 )) |
                 (( *data & ( EQ_BIT16 | EQ_BIT15 | EQ_BIT14 )) <<5) | 
                 (( *data & ( EQ_BIT11 | EQ_BIT10 | EQ_BIT9  )) >>5) |
                 (( *data & ( EQ_BIT8  | EQ_BIT7  | EQ_BIT6   | 
                              EQ_BIT5  | EQ_BIT4  ))<<10) |
                 (( *data & ( EQ_BIT21 | EQ_BIT20 | EQ_BIT19  | 
                              EQ_BIT18 | EQ_BIT17 ))>>10 );
}
    
    
void CompressorRLE3B::_unswizzlePixelData( uint32_t* data, const bool useAlpha  )
{
    
    if ( useAlpha )
        *data =  
           (  *data &   ( EQ_BIT32 | EQ_BIT31 | EQ_BIT22 | EQ_BIT21 | 
                          EQ_BIT11 | EQ_BIT12 | EQ_BIT2 | EQ_BIT1) )| 
           (( *data & ( EQ_BIT26 | EQ_BIT25 )) >>18 )  | 
           (( *data & ( EQ_BIT30 | EQ_BIT29 | EQ_BIT20 | EQ_BIT19 ))>>6)|
           (( *data & ( EQ_BIT28 | EQ_BIT27 | EQ_BIT18 | 
                        EQ_BIT17 | EQ_BIT16 |EQ_BIT15 ))>>12 )| 
           (( *data & ( EQ_BIT10 | EQ_BIT9  | EQ_BIT8  | EQ_BIT7 ))<<18)|
           (( *data & ( EQ_BIT6  | EQ_BIT5 ))<<12 )+
           (( *data & ( EQ_BIT24 | EQ_BIT23 | EQ_BIT14 | 
                        EQ_BIT13 | EQ_BIT4 | EQ_BIT3 ))<<6);
    else
        *data =   
              (  *data & ( EQ_BIT24 | EQ_BIT23 | EQ_BIT22 | EQ_BIT13 | 
                           EQ_BIT12 | EQ_BIT3  | EQ_BIT2  | EQ_BIT1 )) |
              (( *data & ( EQ_BIT21 | EQ_BIT20 | EQ_BIT19 ))>>5) |
              (( *data & ( EQ_BIT6  | EQ_BIT5  | EQ_BIT4 ))<<5) | 
              (( *data & ( EQ_BIT18 | EQ_BIT17 | EQ_BIT16 | 
                           EQ_BIT15 | EQ_BIT14 ))>>10)|
              (( *data & ( EQ_BIT11 | EQ_BIT10 | EQ_BIT9  | 
                           EQ_BIT8  | EQ_BIT7 ))<<10 );     



}
}
}