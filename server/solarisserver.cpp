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
#include <QtCore/QStringList>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslKey>
#include <algorithm>

QT_USE_NAMESPACE

SolarisServer::SolarisServer(quint16 port, QObject *parent) :
    QObject(parent),
    m_pWebSocketServer(nullptr),
    audioDir(QString()),
    counter(START_FROM)
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

        float m_speed = 1; // be ready set the speed, if needed
        timer.setInterval(1000/m_speed);
        connect(&timer, SIGNAL(timeout()), this, SLOT(counterChanged()) );

        // Get the audio directory path (assuming it's ../audio relative to the executable)
        audioDir = QDir::currentPath() + "/../../../audio";
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
        QDir audioDirObj(audioDir);
        audioDirObj.cdUp();  // Go to parent directory
        eventsFile = audioDirObj.absolutePath() + "/events.txt";
        solarisJSONFile = audioDirObj.absolutePath() + "/solaris.json";

        loadEntries();
        loadSolarisJSON();

    }
}


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

void SolarisServer::processTextMessage(QString message)
{
    //QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    qDebug()  << "Message received: " << message;
    
    QStringList messageParts = message.split("|");
    QString command = messageParts.count()>0 ? messageParts[0].trimmed(): "";


    if (command=="start") {
        timer.start();
    }
    if (command=="stop") {
        timer.stop();
        counter = START_FROM;
    }
    if (command=="test") {
        sendTest();
    }
    if (command=="seek" && messageParts.count()>2) {
        bool ok;
        int time = messageParts[2].toInt(&ok);
        if (ok) {
            counter = time;
            qDebug()<< "Set time to: " << time;
        }
    }

    // Check if the message is in the format "generate | text | filename | channel | time"
    if ( command == "generate") {
        
        // Trim whitespace from each part
        for (int i = 0; i < messageParts.size(); ++i) {
            messageParts[i] = messageParts[i].trimmed();
        }
        
        if (messageParts.size() >= 5 && messageParts[0] == "generate") {
            QString text = messageParts[1];
            QString filename = messageParts[2];
            QString channel = messageParts[3];
            QString time = messageParts[4];
            
            qDebug() << "Processing TTS request - text:" << text << "filename:" << filename 
                     << "channel:" << channel << "time:" << time;
            
            
            // Path to the generator script
            QString generatorScript = audioDir + "/generator.py";
            
            // Path to the API key script
            QString apiKeyScript = audioDir + "/elevenlabs-api-key.sh";
            
            // Create the command to source the API key and run the generator
            // We need to properly escape arguments for bash
            // Single quotes prevent any interpretation by bash, but we need to escape single quotes in the text
            auto bashEscape = [](const QString &str) -> QString {
                QString escaped = str;
                escaped.replace("'", "'\\''");  // Replace ' with '\''
                return "'" + escaped + "'";
            };
            
            QString bashCommand = QString("source %1 && python3 %2 %3 %4 %5")
                .arg(apiKeyScript)
                .arg(generatorScript)
                .arg(bashEscape(text))
                .arg(bashEscape(channel))
                .arg(bashEscape(filename));
            
            qDebug() << "Executing bash command:" << bashCommand;
            
            // Execute the command
            QProcess process;
            QStringList arguments;
            arguments << "-c" << bashCommand;
            process.start("bash", arguments);
            
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

                    // Create the new entry
                    QString newEntry = QString("%1|%2|%3.mp3|%4").arg(time).arg(channel).arg(filename).arg(text);

                    // Check if the exact same entry already exists
                    if (!entries.contains(newEntry)) {
                        // Add the new entry
                        entries.append(newEntry);
                        sortAndSaveEntries();
                    } else {
                        qDebug() << "Entry already exists in events.txt, skipping duplicate";
                    }
                } else {
                    qWarning() << "Generator script failed with exit code:" << exitCode;
                }
            } else {
                qWarning() << "Generator script timed out or failed to start";
            }
        } else {
            qWarning() << "Invalid generate message format. Expected 5 messageParts, got:" << messageParts.size();
        }
    } else if (command == "generateCommand") {
        // New format: "generateCommand | text | commandName"
        // Trim whitespace from each part
        for (int i = 0; i < messageParts.size(); ++i) {
            messageParts[i] = messageParts[i].trimmed();
        }
        
        if (messageParts.size() >= 3) {
            QString text = messageParts[1];
            QString commandName = messageParts[2];
            
            qDebug() << "Processing command generation - text:" << text << "commandName:" << commandName;
            
            // Path to the generator script
            QString generatorScript = audioDir + "/generator.py";
            
            // Path to the API key script
            QString apiKeyScript = audioDir + "/elevenlabs-api-key.sh";
            
            // Create the command to source the API key and run the generator
            auto bashEscape = [](const QString &str) -> QString {
                QString escaped = str;
                escaped.replace("'", "'\\''");  // Replace ' with '\''
                return "'" + escaped + "'";
            };
            
            // Use "audiofiles" as the channel/directory
            QString bashCommand = QString("source %1 && python3 %2 %3 %4 %5")
                .arg(apiKeyScript)
                .arg(generatorScript)
                .arg(bashEscape(text))
                .arg(bashEscape("audiofiles"))
                .arg(bashEscape(commandName));
            
            qDebug() << "Executing bash command:" << bashCommand;
            
            // Execute the command
            QProcess process;
            QStringList arguments;
            arguments << "-c" << bashCommand;
            process.start("bash", arguments);
            
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
                    // Update solaris.json
                    QJsonArray commands = solarisData["commands"].toArray();
                    
                    // Check if command already exists
                    int existingIndex = -1;
                    for (int i = 0; i < commands.size(); ++i) {
                        QJsonObject cmd = commands[i].toObject();
                        if (cmd["name"].toString() == commandName) {
                            existingIndex = i;
                            break;
                        }
                    }
                    
                    // Create command object
                    QJsonObject commandObj;
                    commandObj["name"] = commandName;
                    commandObj["fileName"] = commandName + ".mp3";
                    commandObj["text"] = text;
                    
                    if (existingIndex != -1) {
                        // Replace existing command
                        commands[existingIndex] = commandObj;
                        qDebug() << "Replaced existing command:" << commandName;
                    } else {
                        // Add new command
                        commands.append(commandObj);
                        qDebug() << "Added new command:" << commandName;
                    }
                    
                    solarisData["commands"] = commands;
                    saveSolarisJSON();
                } else {
                    qWarning() << "Generator script failed with exit code:" << exitCode;
                }
            } else {
                qWarning() << "Generator script timed out or failed to start";
            }
        } else {
            qWarning() << "Invalid generateCommand message format. Expected 3 parts, got:" << messageParts.size();
        }
    } else if (command == "updateJSON") {
        // Format: "updateJSON | <json_string>"
        if (messageParts.size() >= 2) {
            QString jsonString = message.section('|', 1).trimmed();
            QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8());
            
            if (!doc.isNull() && doc.isObject()) {
                solarisData = doc.object();
                saveSolarisJSON();
                qDebug() << "Updated solaris.json from client";
            } else {
                qWarning() << "Invalid JSON data received for updateJSON";
            }
        }
    } else if (messageParts[0] == "sendCommand") { // send  command to all connected clients

    } else {

        // Echo message to all clients (keep existing behavior)
        for (QWebSocket * client : m_clients) {
            if (client) {
                client->sendTextMessage(message);
            }
        }
    }

}

void SolarisServer::processBinaryMessage(QByteArray message)
{
    QWebSocket *pClient = qobject_cast<QWebSocket *>(sender());
    if (pClient)
    {
        pClient->sendBinaryMessage(message);
    }
}



void SolarisServer::sendTest()
{
    // format: 'play|channel|fileName|text' to players
    qDebug() << "Sending test command";
    sendToAll("play|0|test.mp3|Test. Test? Test!");
}

void SolarisServer::sendToAll(QString message )
{
    foreach(QWebSocket *socket, m_clients) {
        if (socket)
        {
            socket->sendTextMessage(message);
        }
    }
}


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


void SolarisServer::loadEntries()
{
    // Read existing entries
    entries.clear();
    QFile file(eventsFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                entries.append(line);
            }
        }
        file.close();
    }
}

void SolarisServer::sortAndSaveEntries()
{
    // Sort entries by time field (first field before first |)
    std::sort(entries.begin(), entries.end(), [](const QString &a, const QString &b) {
        QString timeA = a.section('|', 0, 0);
        QString timeB = b.section('|', 0, 0);
        return timeA < timeB;
    });

    // Write all sorted entries back to file
    QFile file(eventsFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QTextStream out(&file);
        for (const QString &entry : entries) {
            out << entry << "\n";
        }
        file.close();
        qDebug() << "Successfully added and sorted entry in events.txt";
    } else {
        qWarning() << "Failed to open events.txt for writing:" << eventsFile;
    }
}

void SolarisServer::loadSolarisJSON()
{
    QFile file(solarisJSONFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            solarisData = doc.object();
            qDebug() << "Successfully loaded solaris.json";
        } else {
            qWarning() << "Failed to parse solaris.json";
            // Initialize with empty structure
            solarisData = QJsonObject();
            solarisData["commands"] = QJsonArray();
            solarisData["events"] = QJsonArray();
        }
    } else {
        qDebug() << "solaris.json not found, creating new structure";
        // Initialize with empty structure
        solarisData = QJsonObject();
        solarisData["commands"] = QJsonArray();
        solarisData["events"] = QJsonArray();
    }
}

void SolarisServer::saveSolarisJSON()
{
    QFile file(solarisJSONFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QJsonDocument doc(solarisData);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        qDebug() << "Successfully saved solaris.json";
    } else {
        qWarning() << "Failed to open solaris.json for writing:" << solarisJSONFile;
    }
}



void SolarisServer::counterChanged() // timer timeOut slot
{

    qDebug() << "Counter: " << counter;
    counter++;
    if (counter>1200) {
        timer.stop();
        qDebug()<< "Should be finished";
        counter = START_FROM;
    }

    // Use JSON events from solaris.json
    QJsonArray events = solarisData["events"].toArray();
    QJsonArray commands = solarisData["commands"].toArray();
    
    for (const QJsonValue &eventValue : events) {
        QJsonObject event = eventValue.toObject();
        int eventTime = event["time"].toInt();
        
        if (eventTime == counter) {
            QString commandName = event["name"].toString();
            QString channel = event["channel"].toString();
            
            // Find the command to get the text and filename
            QString text = "";
            QString fileName = commandName + ".mp3";
            
            for (const QJsonValue &cmdValue : commands) {
                QJsonObject cmd = cmdValue.toObject();
                if (cmd["name"].toString() == commandName) {
                    text = cmd["text"].toString();
                    fileName = cmd["fileName"].toString();
                    break;
                }
            }
            
            // Send play command: play|channel|fileName|text
            sendToAll(QString("play|%1|%2|%3").arg(channel).arg(fileName).arg(text));
        }
    }

    sendToAll("time|" + QString::number(counter)); // or rather send as a string 00:00

}
