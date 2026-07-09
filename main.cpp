/*
 * This file is part of USim (https://github.com/muhammad23012009/USim)
 * Copyright (c) 2026 Muhammad Asif  <thevancedgamer@mentallysanemainliners.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <QObject>
#include <QGuiApplication>
#include <QQuickView>

#include "usim.h"
#include "types.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QQuickView view;
    USim sim;

    qmlRegisterSingletonType<USim>("USim", 1, 0, "USim", [&sim](QQmlEngine *engine, QJSEngine *scriptEngine) -> QObject* {
        Q_UNUSED(engine)
        Q_UNUSED(scriptEngine)
        return &sim;
    });

    qmlRegisterUncreatableMetaObject(USimNamespace::staticMetaObject, "USim", 1, 0, "USimEnums", "Namespace");
    qRegisterMetaType<QList<eSIMInfo>>("QList<eSIMInfo>");

    view.setSource(QUrl(QStringLiteral("qrc:/ui/Main.qml")));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    view.show();
    return app.exec();
}
