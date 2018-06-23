#include "ObsAutoRecord.h"
#include <set>
#include <QJsonDocument>
#include <QtCore/QDebug>

QT_USE_NAMESPACE

ObsAutoRecord::ObsAutoRecord(
    const QUrl &url,
    const int interval,
    const QString &folder,
    std::set<std::string> appsToWatch,
    bool debug,
    QObject *parent) :
        QObject(parent),
        m_obsWebSocket(url, debug, this),
        m_url(url),
        m_folder(folder),
        m_appsToWatch(appsToWatch),
        m_debug(debug)
{
    QObject::connect(&m_obsWebSocket, SIGNAL(onResponse(QJsonObject)),
                     this, SLOT(onStatus(QJsonObject)));
    QObject::connect(&m_obsWebSocket, SIGNAL(connected(bool)),
                     this, SLOT(setIsConnected(bool)));
    timer = new QTimer;
    timer->setInterval(interval * 1000);
    timer->start();
    connect(timer, SIGNAL(timeout()), this, SLOT(pingStatus()));
}

void ObsAutoRecord::setAddress(const QUrl &url)
{
    m_url = url;
    m_obsWebSocket.setAddress(m_url);
}

void ObsAutoRecord::setInterval(const int interval)
{
    timer->setInterval(interval * 1000);
}

void ObsAutoRecord::setFolder(const QString &folder)
{
    m_folder = folder;
}

void  ObsAutoRecord::setAppsToWatch(std::set<std::string> appsToWatch)
{
    m_appsToWatch = appsToWatch;
}

void ObsAutoRecord::pingStatus()
{
    if (m_obsWebSocket.isConnected()) {
        m_msgid++;
        m_obsWebSocket.sendRequest("GetStreamingStatus", m_msgid);
    }
}

void ObsAutoRecord::startRecording()
{
    m_msgid++;
    m_obsWebSocket.sendRequest("StartRecording", m_msgid);
}
        
void ObsAutoRecord::setFilenameFormatting(QString appName, int msgid)
{
    QString filenameFormatting = appName + " - " + defaultFilenameFormatting;
    QJsonObject object
    {
        {"filename-formatting", filenameFormatting}
    };
    m_obsWebSocket.sendRequest("SetFilenameFormatting", msgid, object);
}

void ObsAutoRecord::changeFolderBack()
{
    if (!m_folder.isEmpty()) {
        QJsonObject object
        {
            {"rec-folder", m_folder}
        };
        m_msgid++;
        m_obsWebSocket.sendRequest("SetRecordingFolder", m_msgid, object);
    }
    QJsonObject object2
    {
        {"filename-formatting", defaultFilenameFormatting}
    };
    m_msgid++;
    m_obsWebSocket.sendRequest("SetFilenameFormatting", m_msgid, object2);
}

void ObsAutoRecord::onStatus(QJsonObject msg)
{
    if (msg.contains("message-id")) {
        int msgid = msg.value("message-id").toString().toInt();
        std::set<int>::iterator it = idsWaitToRecord.find(msgid);
        if (it != idsWaitToRecord.end()) {
            idsWaitToRecord.erase(msgid);
            if (idsWaitToRecord.empty()) {
                startRecording();
            }
        }
    }
    if (msg.contains("recording") && idsWaitToRecord.empty()) {
        bool recording = msg.value("recording").toBool();
        QString openApp = QString::fromStdString(m_obsUtils.getOpenApp(m_appsToWatch));
        if (!recording && !openApp.isEmpty()) {
            if (m_debug) {
                qDebug() << "App found: " << openApp;
            }
            if (!m_folder.isEmpty()) {
                QString recFolder = m_folder;
                if (m_folder.contains("\\")) {
                    // windows paths
                    if (m_folder.endsWith("\\")) {
                        recFolder.append(openApp);
                    } else {
                        recFolder.append("\\" + openApp);
                    }
                } else if (m_folder.contains("/")) {
                    // unix paths
                    if (m_folder.endsWith("/")) {
                        recFolder.append(openApp);
                    } else {
                        recFolder.append("/" + openApp);
                    }
                } else {
                    // not a valid path
                }
                QJsonObject object
                {
                    {"rec-folder", recFolder}
                };
                m_msgid++;
                idsWaitToRecord.insert(m_msgid);
                m_obsWebSocket.sendRequest("SetRecordingFolder", m_msgid, object);
            }
            m_msgid++;
            idsWaitToRecord.insert(m_msgid);
            setFilenameFormatting(openApp, m_msgid);
        } else if (recording && openApp.isEmpty()) {
            m_msgid++;
            m_obsWebSocket.sendRequest("StopRecording", m_msgid);
            changeFolderBack();
        }
    }
}

void ObsAutoRecord::setIsConnected(bool isConnected)
{
    if (isConnected) {
        emit onStateUpdate(ObsAutoRecordState::CONNECTED);
    } else {
        idsWaitToRecord.clear();
        emit onStateUpdate(ObsAutoRecordState::DISCONNECTED);
    }
}
