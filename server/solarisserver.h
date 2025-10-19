// Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#ifndef SOLARISSERVER_H
#define SOLARISSERVER_H

#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QByteArray>
#include <QtNetwork/QSslError>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <QSslConfiguration>
#include <QTimer>

QT_FORWARD_DECLARE_CLASS(QWebSocketServer)
QT_FORWARD_DECLARE_CLASS(QWebSocket)

#define START_FROM -4

class SolarisServer : public QObject
{
    Q_OBJECT
public:
    explicit SolarisServer(quint16 port, QObject *parent = nullptr);
    ~SolarisServer() override;

    void loadEntries();
    void sortAndSaveEntries();

    void sendToAll(QString message);
    void sendTest();



private Q_SLOTS:
    void onNewConnection();
    void processTextMessage(QString message);
    void processBinaryMessage(QByteArray message);
    void socketDisconnected();
    void onSslErrors(const QList<QSslError> &errors);

    void counterChanged();

private:
    QWebSocketServer *m_pWebSocketServer;
    QList<QWebSocket *> m_clients;
    bool prepareSsl(const QString &certPath, const QString &keyPath);
    QSslConfiguration m_sslConfig;

    QTimer timer;
    int counter;
    QString audioDir;
    QStringList entries;
    QString eventsFile;

};

#endif //SOLARISSERVER_H
