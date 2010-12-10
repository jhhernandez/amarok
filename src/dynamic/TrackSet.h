/****************************************************************************************
 * Copyright (c) 2008 Daniel Caleb Jones <danielcjones@gmail.com>                       *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) version 3 or        *
 * any later version accepted by the membership of KDE e.V. (or its successor approved  *
 * by the membership of KDE e.V.), which shall act as a proxy defined in Section 14 of  *
 * version 3 of the license.                                                            *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#ifndef AMAROK_TRACKSET_H
#define AMAROK_TRACKSET_H

#include "core/meta/Meta.h"

#include <QBitArray>
#include <QString>
#include <QSharedData>
#include <QSharedDataPointer>

namespace Dynamic
{
    class TrackSet;
    class TrackCollection;

    typedef QSharedDataPointer<TrackCollection> TrackCollectionPtr;

    /**
     * We keep a list here of the uid of every track in the set
     * collection being considered. This is unfortunately necessary
     * because the algorithm in generateInitialPlaylist performs many
     * set subtractions and intersections which would be impractical and
     * inefficient to perform using database queries. Instead we
     * represent a set of tracks as a bit list, where the n'th bit
     * indicates whether the n'th track in s_universe is included in the
     * set. Set operations can then be performed extremely quickly using
     * bitwise operations, rather than tree operations which QSet would
     * use.
     */

    /** The TrackCollection stores all the uids that a TrackSet can contain.
        Usually the dynamic playlist queries all the uids before computing a playlist.
    */
    class TrackCollection : public QSharedData
    {
        public:
            TrackCollection( const QStringList& uids );

            int count();

        private:
            QStringList m_uids;
            QHash<QString, int> m_ids;

            friend class TrackSet;
    };

    /**
     * A representation of a set of tracks as a bit array, relative to the
     * given universe set.
     * Intersecting TrackSets from different universes is not a good idea.
     * The BiasSolver uses this class to do a lot of set operations.
     * QSet is more space efficient for sparse sets, but set
     * operations generally aren't linear.
     */
    class TrackSet
    {
        public:
            /** Creates a TrackSet that is outstanding */
            TrackSet();

            /** Creates a TrackSet that represents the whole universe.
                All tracks are included.
            */
            TrackSet( Dynamic::TrackCollectionPtr collection );

            /** Removes all tracks from the set */
            void clear();

            /** Sets all tracks to the set */
            void reset();

            /** Returns true if the results of this track set are not yet available */
            bool isOutstanding() const;

            /**
             * The number of songs contained in this trackSet
             */
            int trackCount() const;

            bool isEmpty() const;
            bool isFull() const;

            /** Returns the uids of a random track contains in this set */
            QString getRandomTrack() const;

            void unite( const TrackSet& );
            void unite( const QStringList& uids );
            void intersect( const TrackSet& );
            void intersect( const QStringList& uids );
            void subtract( const TrackSet& );
            void subtract( const QStringList& uids );

            TrackSet& operator=( const TrackSet& );

        private:
            QBitArray m_bits;
            bool m_outstanding;
            TrackCollectionPtr m_collection;
    };
}

#endif

