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

#ifndef MIRROR_MENU_H
#define MIRROR_MENU_H

#include <QList>
#include <QMenu>

class MirrorMenu : public QMenu {
	Q_OBJECT
public:
	MirrorMenu(QWidget* p);
	void addAction(QAction* act);
	QAction* addAction(const QString& text);
	QAction* addAction(const QIcon& icon, const QString& text);

	template <typename Receiver>
	QAction* addAction(const QString& text, const Receiver* receiver, void (Receiver::*slot)(), const QKeySequence& shortcut = 0)
	{
		QAction* act = QMenu::addAction(text, shortcut, receiver, slot);
		updateMenus();
		return act;
	}

	template <typename Receiver>
	QAction* addAction(const QIcon& icon, const QString& text, const Receiver* receiver, void (Receiver::*slot)(), const QKeySequence& shortcut = 0)

	{
		QAction* act = QMenu::addAction(icon, text, shortcut, receiver, slot);
		updateMenus();
		return act;
	}

	void removeAction(QAction* act);
	void clear();
	QMenu* duplicate(QWidget* p);

private:
	void updateMenus();
	void updateMenu(QMenu* menu);

private Q_SLOTS:
	void menuDestroyed(QObject* obj);

private:
	QList<QMenu*> menus;
};

#endif
