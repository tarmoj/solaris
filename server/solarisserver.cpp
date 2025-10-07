// Copyright (C) 2016 Kurt Pattyn <pattyn.kurt@gmail.com>.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause
#include "solarisserver.h"
#include "QtWebSockets/QWebSocketServer"
#include "QtWebSockets/QWebSocket"
#include <QtCore/QDebug>
#include <QtCore/QFile>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QTextStream>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>

QT_USE_NAMESPACE

//! [constructor]
SolarisServer::SolarisServer(quint16 port, QObject *parent) :
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
                this, &SolarisServer::onNewConnection);
        connect(m_pWebSocketServer, &QWebSocketServer::sslErrors,
                this, &SolarisServer::onSslErrors);
    }
}
//! [constructor]

SolarisServer::~SolarisServer()
{
    m_pWebSocketServer->close();
    qDeleteAll(m_clients.begin(), m_clients.end());
}

bool SolarisServer::prepareSsl(const QString &certPath, const QString &keyPath) {
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
void SolarisServer::onNewConnection()
{
    QWebSocket *pSocket = m_pWebSocketServer->nextPendingConnection();

    qDebug() << "Client connected:" << pSocket->peerName() << pSocket->origin();

    connect(pSocket, &QWebSocket::textMessageReceived, this, &SolarisServer::processTextMessage);
    connect(pSocket, &QWebSocket::binaryMessageReceived,
            this, &SolarisServer::processBinaryMessage);
    connect(pSocket, &QWebSocket::disconnected, this, &SolarisServer::socketDisconnected);

    m_clients << pSocket;
}
//! [onNewConnection]

//! [processTextMessage]
void SolarisServer::processTextMessage(QString message)
{
    //QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug()  << "Message received: " << message;
    
    // Check if the message is in the format "generate | text | filename | channel | time"
    if (message.startsWith("generate") ) {
        QStringList parts = message.split("|");
        
        // Trim whitespace from each part
        for (int i = 0; i < parts.size(); ++i) {
            parts[i] = parts[i].trimmed();
        }
        
        if (parts.size() >= 5 && parts[0] == "generate") {
            QString text = parts[1];
            QString filename = parts[2];
            QString channel = parts[3];
            QString time = parts[4];
            
            qDebug() << "Processing TTS request - text:" << text << "filename:" << filename 
                     << "channel:" << channel << "time:" << time;
            
            // Get the audio directory path (assuming it's ../audio relative to the executable)
            QString audioDir = QDir::currentPath() + "/../../../audio";
            QDir dir(audioDir);
            if (!dir.exists()) {
                // Try alternative path
                audioDir = QDir::currentPath() + "/../../audio";
                dir.setPath(audioDir);
                if (!dir.exists()) {
                    qWarning() << "Audio directory not found:" << audioDir;
                    return;
                }
            }
            audioDir = dir.absolutePath();
            
            // Path to the generator script
            QString generatorScript = audioDir + "/generator.py";
            
            // Path to the API key script
            QString apiKeyScript = audioDir + "/elevenlabs-api-key.sh";
            
            // Create the command to source the API key and run the generator
            // We'll use bash to source the script and run python
            QString command = QString("bash -c 'source %1 && python3 %2 \"%3\" \"%4\" \"%5\"'")
                .arg(apiKeyScript)
                .arg(generatorScript)
                .arg(text)
                .arg(channel)
                .arg(filename);
            
            qDebug() << "Executing command:" << command;
            
            // Execute the command
            QProcess process;
            process.start(command);
            
            // Wait for the process to finish (with timeout)
            if (process.waitForFinished(30000)) { // 30 second timeout
                int exitCode = process.exitCode();
                QString output = process.readAllStandardOutput();
                QString errors = process.readAllStandardError();
                
                qDebug() << "Process output:" << output;
                if (!errors.isEmpty()) {
                    qDebug() << "Process errors:" << errors;
                }
                
                if (exitCode == 0) {
                    // Success! Add entry to events.txt
                    QString eventsFile = audioDir + "/events.txt";
                    QFile file(eventsFile);
                    
                    if (file.open(QIODevice::Append | QIODevice::Text)) {
                        QTextStream out(&file);
                        out << time << "|" << channel << "|" << filename << ".mp3|" << text << "\n";
                        file.close();
                        qDebug() << "Successfully added entry to events.txt";
                    } else {
                        qWarning() << "Failed to open events.txt for writing:" << eventsFile;
                    }
                } else {
                    qWarning() << "Generator script failed with exit code:" << exitCode;
                }
            } else {
                qWarning() << "Generator script timed out or failed to start";
            }
        } else {
            qWarning() << "Invalid generate message format. Expected 5 parts, got:" << parts.size();
        }
    } else {

        // Echo message to all clients (keep existing behavior)
        for (QWebSocket * client : m_clients) {
            if (client) {
                client->sendTextMessage(message);
            }
        }
    }

}
//! [processTextMessage]

//! [processBinaryMessage]
void SolarisServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        pClient->sendBinaryMessage(message);
    }
}
//! [processBinaryMessage]

//! [socketDisconnected]
void SolarisServer::socketDisconnected()
{
    qDebug() << "Client disconnected";
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        m_clients.removeAll(pClient);
        pClient->deleteLater();
    }
}

void SolarisServer::onSslErrors(const QList<QSslError> &)
{
    qDebug() << "Ssl errors occurred";
}
//! [socketDisconnected]
