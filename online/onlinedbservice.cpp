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

#include "onlinedbservice.h"
#include "db/onlinedb.h"
#include "gui/covers.h"
#include "models/roles.h"
#include "network/networkaccessmanager.h"
#include <QBuffer>
#include <QXmlStreamReader>
#ifdef BUNDLED_KARCHIVE
#include <kcompressiondevice.h>
#else
#include <KCompressionDevice>
#endif

OnlineXmlParser::OnlineXmlParser()
{
	thread = new Thread(metaObject()->className());
	moveToThread(thread);
	thread->start();
	connect(this, &OnlineXmlParser::startParsing, this, &OnlineXmlParser::doParsing);
}

OnlineXmlParser::~OnlineXmlParser()
{
}

void OnlineXmlParser::start(NetworkJob* job)
{
	emit startParsing(job);
}

void OnlineXmlParser::doParsing(NetworkJob* job)
{
    QByteArray compressed = job->actualJob()->readAll();

    QBuffer buffer(&compressed);
    buffer.open(QIODevice::ReadOnly);

    KCompressionDevice dev(&buffer, false, KCompressionDevice::GZip);
    if (!dev.open(QIODevice::ReadOnly)) {
        emit error(tr("Failed to parse"));
        emit abortUpdate();
        emit complete();
        return;
    }

    QByteArray decompressed = dev.readAll();
    QXmlStreamReader reader(decompressed);

    emit startUpdate();

    int artistCount = parse(reader);

    if (artistCount > 0) {
        emit endUpdate();
        emit stats(artistCount);
    } else {
        emit error(tr("Failed to parse"));
        emit abortUpdate();
    }

    emit complete();
}

OnlineDbService::OnlineDbService(LibraryDb* d, QObject* p)
	: SqlLibraryModel(d, p, T_Genre), lastPc(-1), job(nullptr)
{
	connect(Covers::self(), &Covers::cover, this, &OnlineDbService::cover);
}

QVariant OnlineDbService::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) {
		switch (role) {
		case Cantata::Role_TitleText:
			return title();
		case Cantata::Role_SubText:
			if (!status.isEmpty()) {
				return status;
			}
			if (!stats.isEmpty()) {
				return stats;
			}
			return descr();
		case Qt::DecorationRole:
			return icon();
		}
	}
	return SqlLibraryModel::data(index, role);
}

bool OnlineDbService::previouslyDownloaded() const
{
	// Create DB, if it does not already exist
	static_cast<OnlineDb*>(db)->create();
	return 0 != db->getCurrentVersion();
}

void OnlineDbService::open()
{
	if (0 == rowCount(QModelIndex())) {
		// Create DB, if it does not already exist
		static_cast<OnlineDb*>(db)->create();
		libraryUpdated();
		updateStats();
	}
}

void OnlineDbService::download(bool redownload)
{
	if (job) {
		return;
	}
	if (redownload || !previouslyDownloaded()) {
		job = NetworkAccessManager::self()->get(QUrl(listingUrl()));
		connect(job, &NetworkJob::downloadPercent, this, &OnlineDbService::downloadPercent);
		connect(job, &NetworkJob::finished, this, &OnlineDbService::downloadFinished);
		lastPc = -1;
		downloadPercent(0);
	}
}

void OnlineDbService::abort()
{
	if (job) {
		job->cancelAndDelete();
		job = nullptr;
	}
	db->abortUpdate();
}

void OnlineDbService::cover(const Song& song, const QImage& img, const QString& file)
{
	if (file.isEmpty() || img.isNull() || !song.isFromOnlineService() || song.onlineService() != name()) {
		return;
	}

	const Item* genre = root ? root->getChild(song.genres[0]) : nullptr;
	if (genre) {
		const Item* artist = static_cast<const CollectionItem*>(genre)->getChild(song.albumArtistOrComposer());
		if (artist) {
			const Item* album = static_cast<const CollectionItem*>(artist)->getChild(song.albumId());
			if (album) {
				QModelIndex idx = index(album->getRow(), 0, index(artist->getRow(), 0, index(genre->getRow(), 0, QModelIndex())));
				emit dataChanged(idx, idx);
			}
		}
	}
}

void OnlineDbService::updateStatus(const QString& msg)
{
	status = msg;
	emit dataChanged(QModelIndex(), QModelIndex());
}

void OnlineDbService::downloadPercent(int pc)
{
	if (lastPc != pc) {
		lastPc = pc;
		updateStatus(tr("Downloading...%1%").arg(pc));
	}
}

void OnlineDbService::downloadFinished()
{
	NetworkJob* reply = qobject_cast<NetworkJob*>(sender());
	if (!reply) {
		return;
	}

	if (reply != job) {
		reply->deleteLater();
		return;
	}

	if (reply->ok()) {
		// Ensure DB is created
		static_cast<OnlineDb*>(db)->create();
		updateStatus(tr("Parsing music list...."));
		OnlineXmlParser* parser = createParser();
		db->clear();
		connect(parser, &OnlineXmlParser::startUpdate, static_cast<OnlineDb*>(db), &OnlineDb::startUpdate);
		connect(parser, &OnlineXmlParser::endUpdate, static_cast<OnlineDb*>(db), &OnlineDb::endUpdate);
		connect(parser, &OnlineXmlParser::abortUpdate, static_cast<OnlineDb*>(db), &OnlineDb::abortUpdate);
		connect(parser, &OnlineXmlParser::stats, static_cast<OnlineDb*>(db), &OnlineDb::insertStats);
		connect(parser, &OnlineXmlParser::coverUrl, static_cast<OnlineDb*>(db), &OnlineDb::storeCoverUrl);
		connect(parser, &OnlineXmlParser::songs, static_cast<OnlineDb*>(db), &OnlineDb::insertSongs);
		connect(parser, &OnlineXmlParser::complete, job, &NetworkJob::deleteLater);
		connect(parser, &OnlineXmlParser::complete, this, &OnlineDbService::updateStats);
		connect(parser, &OnlineXmlParser::error, this, &OnlineDbService::error);
		connect(parser, &OnlineXmlParser::complete, parser, &OnlineXmlParser::deleteLater);
		parser->start(reply);
	}
	else {
		reply->deleteLater();
		updateStatus(QString());
		emit error(tr("Failed to download"));
	}
	job = nullptr;
}

void OnlineDbService::updateStats()
{
	int numArtists = static_cast<OnlineDb*>(db)->getStats();
	if (numArtists > 0) {
		stats = tr("%n Artist(s)", "", numArtists);
	}
	else {
		stats = QString();
	}
	updateStatus(QString());
}

#include "moc_onlinedbservice.cpp"
