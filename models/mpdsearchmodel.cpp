/*
 * Cantata
 *
 * Copyright (c) 2011-2022 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "mpdsearchmodel.h"
#include "gui/covers.h"
#include "mpd-interface/mpdconnection.h"
#include "roles.h"

MpdSearchModel::MpdSearchModel(QObject* parent)
	: SearchModel(parent), currentId(0)
{
	connect(this, &MpdSearchModel::getRating, MPDConnection::self(), &MPDConnection::getRating);
	connect(this, qOverload<const QString&, const QString&, int>(&MpdSearchModel::search), MPDConnection::self(), qOverload<const QString&, const QString&, int>(&MPDConnection::search));
	connect(MPDConnection::self(), qOverload<int, const QList<Song>&>(&MPDConnection::searchResponse), this, &MpdSearchModel::searchFinished);
	connect(MPDConnection::self(), &MPDConnection::rating, this, &MpdSearchModel::ratingResult);
	connect(Covers::self(), qOverload<const Song&, int>(&Covers::loaded), this, qOverload<const Song&, int>(&MpdSearchModel::coverLoaded));
}

MpdSearchModel::~MpdSearchModel()
{
}

QVariant MpdSearchModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() && Cantata::Role_RatingCol == role) {
		return COL_RATING;
	}

	const Song* song = toSong(index);

	if (!song) {
		return QVariant();
	}

	switch (role) {
	case Cantata::Role_SongWithRating: {
		QVariant var;
		if (Song::Standard == song->type && Song::Rating_Null == song->rating) {
			emit getRating(song->file);
			song->rating = Song::Rating_Requested;
		}
		var.setValue(*song);
		return var;
	}
	default:
		return SearchModel::data(index, role);
	}
	return QVariant();
}

void MpdSearchModel::clear()
{
	SearchModel::clear();
	currentId++;
}

void MpdSearchModel::search(const QString& key, const QString& value)
{
	if (key == currentKey && value == currentValue) {
		return;
	}
	emit searching();
	clear();
	currentKey = key;
	currentValue = value;
	currentId++;
	emit search(key, value, currentId);
}

void MpdSearchModel::searchFinished(int id, const QList<Song>& result)
{
	if (id != currentId) {
		return;
	}

	results(result);
}

void MpdSearchModel::coverLoaded(const Song& song, int s)
{
	Q_UNUSED(s)
	if (!song.isArtistImageRequest() && !song.isComposerImageRequest()) {
		int row = 0;
		for (const Song& s : songList) {
			if (s.albumArtist() == song.albumArtist() && s.album == song.album) {
				QModelIndex idx = index(row, 0, QModelIndex());
				emit dataChanged(idx, idx);
			}
			row++;
		}
	}
}

void MpdSearchModel::ratingResult(const QString& file, quint8 r)
{
	QList<Song>::iterator it = songList.begin();
	QList<Song>::iterator end = songList.end();
	int numCols = columnCount(QModelIndex()) - 1;

	for (int row = 0; it != end; ++it, ++row) {
		if (Song::Standard == (*it).type && r != (*it).rating && (*it).file == file) {
			(*it).rating = r;
			emit dataChanged(index(row, 0), index(row, numCols));
		}
	}
}

#include "moc_mpdsearchmodel.cpp"
