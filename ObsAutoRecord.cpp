#include "ObsAutoRecord.h"
#include <QJsonDocument>
#include <QtCore/QDebug>
#include <QTimer>

QT_USE_NAMESPACE

ObsAutoRecord::ObsAutoRecord(const QUrl &url, bool debug, QObject *parent) :
    QObject(parent),
    m_obsWebSocket(url, debug, this),
    m_url(url),
    m_debug(debug)
{
    QObject::connect(&m_obsWebSocket, SIGNAL(onResponse(QJsonObject)),
                     this, SLOT(onStatus(QJsonObject)));
    QTimer *timer = new QTimer;
    timer->setInterval(5000);
    timer->start();
    connect(timer, SIGNAL(timeout()), this, SLOT(pingStatus()));
}

BOOL CALLBACK ObsAutoRecord::EnumWindowsProc(HWND hwnd, LPARAM lParam)
{
    if ((!IsWindowVisible(hwnd) && !IsIconic(hwnd))) {
        return TRUE;
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (hProcess == 0) {
        return TRUE;
    }
    DWORD exe_size = 1024;
    CHAR exe[1024];
    QueryFullProcessImageNameA(hProcess, 0, exe, &exe_size);
    std::set<std::string>& filenames =
        *reinterpret_cast<std::set<std::string>*>(lParam);
    std::string filename(&exe[0]);
    filenames.insert(filename);
    CloseHandle(hProcess);

    DWORD dwHandle, dwFileVersionInfoSize = GetFileVersionInfoSizeA(exe, &dwHandle);
    if (dwFileVersionInfoSize == 0)
        return TRUE;
    std::vector<unsigned char> buf(dwFileVersionInfoSize);
    if (!GetFileVersionInfoA(exe, dwHandle, dwFileVersionInfoSize, &buf[0]))
        return TRUE;

    // catch default information
    LPVOID lpInfo;
    UINT unInfoLen;
    if (VerQueryValue(lpData, _T("\\"), &lpInfo, &unInfoLen))
    {
        //ASSERT(unInfoLen == sizeof(m_FileInfo));
        if (unInfoLen == sizeof(m_FileInfo))
            memcpy(&m_FileInfo, lpInfo, unInfoLen);
    }

    // find best matching language and codepage
    VerQueryValue(lpData, _T("\\VarFileInfo\\Translation"), &lpInfo, &unInfoLen);

    DWORD   dwLangCode = 0;
    if (!GetTranslationId(lpInfo, unInfoLen, GetUserDefaultLangID(), dwLangCode, FALSE))
    {
        if (!GetTranslationId(lpInfo, unInfoLen, GetUserDefaultLangID(), dwLangCode, TRUE))
        {
            if (!GetTranslationId(lpInfo, unInfoLen, MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), dwLangCode, TRUE))
            {
                if (!GetTranslationId(lpInfo, unInfoLen, MAKELANGID(LANG_ENGLISH, SUBLANG_NEUTRAL), dwLangCode, TRUE))
                    // use the first one we can get
                    dwLangCode = *((DWORD*)lpInfo);
            }
        }
    }

    stlString   strSubBlock;
    TCHAR buf[1024];
    _stprintf(buf, _T("\\StringFileInfo\\%04X%04X\\"), dwLangCode & 0x0000FFFF, (dwLangCode & 0xFFFF0000) >> 16);
    strSubBlock = buf;


    // catch string table
    stlString sBuf;

    sBuf.clear();
    sBuf = strSubBlock;
    sBuf += _T("FileDescription");
    if (VerQueryValue(lpData, sBuf.c_str(), &lpInfo, &unInfoLen))
        std::string filename(&exe[0]);
        filenames.insert(filename);
        m_strFileDescription = std::string(lpInfo);

    return TRUE;
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
        
void ObsAutoRecord::setFilenameFormatting(QString appName)
{
    QString filenameFormatting =
        QString::fromStdString(
            appName.toStdString() + " - " + defaultFilenameFormatting);
    QJsonObject object
    {
        {"filename-formatting", filenameFormatting}
    };
    m_msgid++;
    m_obsWebSocket.sendRequest("SetFilenameFormatting", m_msgid, object);
}

void ObsAutoRecord::changeFolderBack()
{
    std::string folder = "Test folder";
    QJsonObject object
    {
        {"rec-folder", QString::fromStdString(folder)}
    };
    m_msgid++;
    m_obsWebSocket.sendRequest("SetRecordingFolder", m_msgid, object);
    QJsonObject object2
    {
        {"filename-formatting", QString::fromStdString(defaultFilenameFormatting)}
    };
    m_msgid++;
    m_obsWebSocket.sendRequest("SetFilenameFormatting", m_msgid, object2);
}

void ObsAutoRecord::onStatus(QJsonObject msg)
{
    if (msg.contains("recording")) {
        bool recording = msg.value("recording").toBool();
        filenamesOpen.clear();
        EnumWindows(ObsAutoRecord::EnumWindowsProc, reinterpret_cast<LPARAM>(&filenamesOpen));
        // At this point, titles if fully populated and could be displayed, e.g.:
        qDebug() << "-----------------------------------" << endl;
        for ( const auto& filename : filenamesOpen )
            if (m_debug)
                qDebug() << "Filename: " << QString::fromStdString(filename);
        // open_app = ""
        // std::string folder = "Test folder"
        // if (!recording && open_app is not None) {
        //     rec_folder = os.path.join(folder, open_app).replace('\\', '/');
        //     QJsonObject object
        //     {
        //         {"rec-folder", rec_folder}
        //     };
        //     m_obsWebSocket.sendRequest("SetRecordingFolder", object);
        //     setFilenameFormatting(open_app);
        // } else if (recording && open_app is None) {
        //     m_obsWebSocket.send("StopRecording");
        //     changeFolderBack();
        // }
    }
}
