// Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "sslechoserver.h"
#include "QtWebSockets/QWebSocketServer"
#include "QtWebSockets/QWebSocket"
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

QT_USE_NAMESPACE

//! [constructor]
SslEchoServer::SslEchoServer(quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(nullptr)
{
    m_pWebSocketServer = new QWebSocketServer(QStringLiteral("SSL Echo Server"),
                                              QWebSocketServer::SecureMode,
                                              this);


    if (!prepareSsl("/home/pierre/.keys/live.uuu.ee.pem", "/home/pierre/.keys/private.key")) {
        qFatal("Failed to prepare SSL configuration.");
        return;
    }

    // set SSL configuration to server before listening
    m_pWebSocketServer->setSslConfiguration(m_sslConfig);


    if (m_pWebSocketServer->listen(QHostAddress::Any, port))
    {
        qDebug() << "SSL Echo Server listening on port" << port;
        connect(m_pWebSocketServer, &QWebSocketServer::newConnection,
                this, &SslEchoServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::sslErrors,
                this, &SslEchoServer::onSslErrors);
    }
}
//! [constructor]

SslEchoServer::~SslEchoServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

bool SslEchoServer::prepareSsl(const QString &certPath, const QString &keyPath) {
    QFile certFile(certPath);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Cannot open certificate file:" << certPath;
        return false;
    }
    const QByteArray certPem = certFile.readAll();
    certFile.close();

    QFile keyFile(keyPath);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qCritical() << "Cannot open private key file:" << keyPath;
        return false;
    }
    const QByteArray keyPem = keyFile.readAll();
    keyFile.close();

    QList<QSslCertificate> certList = QSslCertificate::fromData(certPem, QSsl::Pem);
    if (certList.isEmpty()) {
        qCritical() << "Failed to parse certificate from PEM.";
        return false;
    }
    QSslCertificate localCert = certList.first();

    QSslKey privateKey(keyPem, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (privateKey.isNull()) {
        // Try EC key if RSA failed
        privateKey = QSslKey(keyPem, QSsl::Ec, QSsl::Pem, QSsl::PrivateKey);
    }
    if (privateKey.isNull()) {
        qCritical() << "Failed to parse private key from PEM.";
        return false;
    }

    // Build SSL configuration
    m_sslConfig = QSslConfiguration::defaultConfiguration();
    m_sslConfig.setLocalCertificate(localCert);
    m_sslConfig.setPrivateKey(privateKey);

    // Optional: set other parameters
    m_sslConfig.setProtocol(QSsl::TlsV1_2OrLater);

    // Optionally set the ciphers (leave default for now)
    // m_sslConfig.setCiphers(QList<QSslCipher>()); // defaultg
    // By default the server does not require client certs:
    m_sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

    return true;
}

//! [onNewConnection]
void SslEchoServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    qDebug() << "Client connected:" << pSocket->peerName() << pSocket->origin();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &SslEchoServer::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &SslEchoServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &SslEchoServer::socketDisconnected);

    m_clients << pSocket;
}
//! [onNewConnection]

//! [processTextMessage]
void SslEchoServer::processTextMessage(QString message)
{
    //QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug()  << "Message received: " << message;
    for (QWebSocket * client : m_clients) {
        if (client) {
            client->sendTextMessage(message);
        }
    }

}
//! [processTextMessage]

//! [processBinaryMessage]
void SslEchoServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        pClient->sendBinaryMessage(message);
    }
}
//! [processBinaryMessage]

//! [socketDisconnected]
void SslEchoServer::socketDisconnected()
{
    qDebug() << "Client disconnected";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

void SslEchoServer::onSslErrors(const QList<QSslError> &)
{
    qDebug() << "Ssl errors occurred";
}
//! [socketDisconnected]
