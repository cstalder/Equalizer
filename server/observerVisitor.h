
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

#ifndef EQSERVER_OBSERVERVISITOR_H
#define EQSERVER_OBSERVERVISITOR_H

#include "visitorResult.h"  // enum

namespace eq
{
namespace server
{
    class Observer;

    /**
     * A visitor to traverse non-const observers.
     */
    class ObserverVisitor
    {
    public:
        /** Constructs a new ObserverVisitor. */
        ObserverVisitor(){}
        
        /** Destruct the ObserverVisitor */
        virtual ~ObserverVisitor(){}

        /** Visit a observer. */
        virtual VisitorResult visit( Observer* observer )
            { return TRAVERSE_CONTINUE; }
    };

    /**
     * A visitor to traverse const observers.
     */
    class ConstObserverVisitor
    {
    public:
        /** Constructs a new ObserverVisitor. */
        ConstObserverVisitor(){}
        
        /** Destruct the ObserverVisitor */
        virtual ~ConstObserverVisitor(){}

        /** Visit a observer. */
        virtual VisitorResult visit( const Observer* observer )
            { return TRAVERSE_CONTINUE; }
    };
}
}
#endif // EQSERVER_OBSERVERVISITOR_H