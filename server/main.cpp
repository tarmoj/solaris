// Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include <QtCore/QCoreApplication>
#include "solarisserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    SolarisServer server(1234);

    Q_UNUSED(server);

    return a.exec();
}
