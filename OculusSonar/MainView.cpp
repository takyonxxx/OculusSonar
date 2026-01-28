#include <fstream>

#include <QDesktopServices>
#include <QUrl>
#include <QSettings>
#include <QStandardPaths>
#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QApplication>
#include <QStatusBar>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWinExtras/qwinfunctions.h>
#else
#include <windows.h>
#include <dwmapi.h>
#endif

#include <QKeyEvent>
#include <QMessageBox>

#include "MainView.h"
#include "../Displays/SonarSurface.h"
#include "ConnectForm.h"
#include "ModeCtrls.h"

#define WIN32_LEAN_AND_MEAN
#include "Windows.h"
#include "qt_windows.h"

double MainView::NAVIGATION_RANGES[] = { 1, 2, 5, 7.5, 10, 20, 30, 40, 50, 75, 100, 120, 140, 160, 180, 200 };
double MainView::INSPECTION_RANGES[] = { 0.3, 0.5, 1, 2, 3, 4, 5, 7.5, 10, 20, 40 };

MainView::MainView(QWidget *parent) :
    QMainWindow(parent),
    m_titleCtrls(this),
    m_modeCtrls(this),
    m_optionsCtrls(this),
    m_onlineCtrls(this),
    m_reviewCtrls(this),
    m_toolsCtrls(this),
    m_palSelect(this),
    m_settings(this),
    m_fanDisplay(this),
    m_connectForm(this),
    m_info(this),
    m_deviceForm(this),
    m_infoCtrls(this),
    m_cursorCtrls(this),
    m_infoForm(this),
    m_timeout(false),
    m_reconnect(false),
    m_helpForm(this),
    m_hexViewer(nullptr),
    m_hexContainer(nullptr),
    m_showHexViewer(false),
    m_maxHexBytes(64),
    m_yoloCheckbox(nullptr),
    m_yoloDetector(nullptr),
    m_yoloEnabled(false),
    m_renderCounter(0)
{
    // Allow the size grip to resize the form when in "normal" mode
    counter = 0;
    statusBar()->setSizeGripEnabled(true);

    m_playSpeed = 1;

    m_partNumber = partNumberUndefined;

    m_indexLower = 0;

    // Default settings mode
    m_displayMode = offline;

    // Make sure the fan display is on the bottom
    m_fanDisplay.lower();

    // Add a sonar surface to the fan display widget
    m_pSonarSurface = new SonarSurface;
    // Apply background image to surface display here
    m_pSonarSurface->m_background = ":/Background.png";
    m_pSonarSurface->m_dwGridText = true;
    //m_pSonarSurface->m_clearColour.setRgb(66, 66, 66, 66);
    m_pSonarSurface->m_clearColour.setRgb(81, 81, 81);
    m_fanDisplay.RmglSetupSurface(m_pSonarSurface);

    // Diagnostic info label
    //m_modeCtrls.setInfo("hhhiuhiufegfiqgfeq");
    m_info.setText("hhhiuhiufegfiqgfeq");
    m_info.setStyleSheet("QLabel { color : gray; }");
    m_info.setAlignment(Qt::AlignHCenter);
    m_info.setVisible(true);

    // Palette selection is hidden by default
    m_palSelect.setVisible(false);
    m_palSelect.m_palette = m_pSonarSurface->m_palIndex;

    // Replay index initialisation
    m_nEntries = 0;
    m_pEntries = nullptr;
    m_useRawSonar = false;

    m_nViewInfoEntries = 0;
    m_pViewInfoEntries = nullptr;

    m_sonarList.clear();
    m_statusMessageTimes.clear();

    // Set the default playback speed
    m_replay.setInterval(100); // 10Hz ?

    m_measureMode = false;

    // NULL the sonar information
    m_pSonarInfo = NULL;

    CreateHexViewer();

    // Update the fan when the palette is selected
    connect(&m_palSelect, &PalWidget::PalSelected, this, &MainView::PalSelected);

    // Connect the status output from the oculus status recieve
    connect(&m_oculusStatus, &OsStatusRx::NewStatusMsg, this, &MainView::NewStatusMsg);

    // Connect a connection failure to clear the connect button
    connect(&m_oculusClient.m_readData, &OsReadThread::NotifyConnectionFailed, this, &MainView::ConnectionFailed);

    // Connect a successful read to push the data into the system
    connect(&m_oculusClient.m_readData, &OsReadThread::NewReturnFire, this, &MainView::NewReturnFire);

    // Connect updated log directory to the logger
    connect(&m_settings.m_settingsCtrls, &SettingsCtrls::NewLogDirectory, &m_logger, &RmLogger::SetLogDirectory);
    connect(&m_settings.m_settingsCtrls, &SettingsCtrls::MaxLogSize, &m_logger, &RmLogger::SetMaxLogSize);

    connect(&m_settings.m_appCtrls, &AppCtrls::StyleChanged, this, &MainView::StyleChanged);

    // Link the player output to the payload slot
    connect(&m_player, &RmPlayer::NewPayload, this, &MainView::OnNewPayload);

    // Link the review slider with the show entry
    connect(&m_reviewCtrls, &ReviewCtrls::EntryChanged, this, &MainView::ReviewEntryChanged);
    connect(&m_reviewCtrls, &ReviewCtrls::LowerEntryChanged, this, &MainView::ReviewLowerEntryChanged);
    connect(&m_reviewCtrls, &ReviewCtrls::UpperEntryChanged, this, &MainView::ReviewUpperEntryChanged);
    connect(&m_reviewCtrls, &ReviewCtrls::OnPlay, this, &MainView::StartReplay);
    connect(&m_reviewCtrls, &ReviewCtrls::OnStop, this, &MainView::StopReplay);

    // Link the replay next item
    connect(&m_replay, &QTimer::timeout, this, &MainView::PlayNext);
    connect(&m_player, &RmPlayer::EndOfFile, this, &MainView::StopReplay);


    connect(&m_oculusClient.m_readData, &OsReadThread::socketTimeout, this, &MainView::SocketTimeout);
    connect(&m_oculusClient.m_readData, &OsReadThread::socketReconnected, this, &MainView::SocketReconnecting);
    connect(&m_oculusClient.m_readData, &OsReadThread::socketDisconnect, this, &MainView::SocketDisconnected);


    // Connect a user config
    connect(&m_oculusClient, &OsClientCtrl::NewUserConfig, &m_deviceForm, &DeviceForm::NewUserConfig);

    connect(&m_connectForm, &ConnectForm::RebuildSonarList, this, &MainView::RebuildSonarList);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainView::MonitorAvailableSonars);
    timer->start(1000);

    connect(m_pSonarSurface, &SonarSurface::MouseInfo, this, &MainView::MouseInfo);
    connect(m_pSonarSurface, &SonarSurface::MouseEnter, this, &MainView::MouseEnterFan);
    connect(m_pSonarSurface, &SonarSurface::MouseLeave, this, &MainView::MouseLeaveFan);

    connect(&m_infoForm, &InfoForm::AbortReconnect, this, &MainView::AbortReconnect);
    ReadSettings();

    // Create SonarImage folder in standard desktop directory
    desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    sonarImageDir = QDir(desktopPath).absoluteFilePath("SonarImage");

    // Create the directory if it doesn't exist
    QDir dir;
    if (!dir.exists(sonarImageDir)) {
        dir.mkpath(sonarImageDir);
    }

    // YOLO Detection initialization
    m_yoloDetector = new YOLO_V8();
    QString modelPath = QCoreApplication::applicationDirPath() + "/sonar_model.onnx";

    if (QFile::exists(modelPath)) {
        // Roboflow validation: max threshold 38% for all objects
        m_yoloParams.rectConfidenceThreshold = 0.25;
        m_yoloParams.iouThreshold = 0.45f;
        m_yoloParams.modelPath = modelPath.toStdString();
        m_yoloParams.imgSize = { 640, 640 };
        m_yoloParams.modelType = YOLO_DETECT_V8;
        m_yoloParams.classNames = {"kutu"};

        qDebug() << "YOLO Config: confidence=" << m_yoloParams.rectConfidenceThreshold
                 << "iou=" << m_yoloParams.iouThreshold;

        bool success = m_yoloDetector->CreateSession(m_yoloParams);

        if (success) {
            m_yoloEnabled = true;
            qDebug() << "YOLO detection enabled";
        } else {
            delete m_yoloDetector;
            m_yoloDetector = nullptr;
            m_yoloEnabled = false;
        }
    } else {
        delete m_yoloDetector;
        m_yoloDetector = nullptr;
        m_yoloEnabled = false;
    }

    // Create YOLO checkbox
    CreateYoloCheckbox();
}

MainView::~MainView()
{
    WriteSettings();

    // YOLO cleanup
    if (m_yoloDetector) {
        delete m_yoloDetector;
        m_yoloDetector = nullptr;
    }

    // If there is an entry table then clena up
    if (m_pEntries)
    {
        delete m_pEntries;
        m_nEntries = 0;
    }

    if (m_pViewInfoEntries) {
        delete m_pViewInfoEntries;
        m_nViewInfoEntries = 0;
    }

    if (m_hexContainer) {
        delete m_hexContainer;
        m_hexContainer = nullptr;
    }
}

// ----------------------------------------------------------------------------
void MainView::UpdateLogFileName()
{
    m_onlineCtrls.UpdateLogFileName();
}

// ----------------------------------------------------------------------------
void MainView::keyPressEvent(QKeyEvent* event) {
    char key = (char)event->key();

    if (key == 'C') {
        // Connect/Disconnect
        m_modeCtrls.ToggleConnect();
    }
    else if (key == 'O') {
        // Open/Close
        m_modeCtrls.ToggleOpen();
    }
    else if (key == 'X') {
        ToggleHexViewer();
    }
    else if ((key >= '1') && (key <= '6')) {
        int index = key - (int)('1');
        // Change the palette
        this->PalSelected(index);
        m_palSelect.m_palette = index;
        m_palSelect.update();
    }

    if (m_displayMode == online) {
        if (key == 'Q') {
            //qDebug() << "Change frequency";
            m_onlineCtrls.ToogleFrequency();
        }
        else if (key == 'W') {
            //qDebug() << "Increase range";
            m_onlineCtrls.IncreaseRange();
        }
        else if (key == 'S') {
            qDebug() << "Decrease range";
            m_onlineCtrls.DecreaseRange();
        }
        else if (key == 'A') {
            //qDebug() << "Reduce gain";
            m_onlineCtrls.DecreaseGain();
        }
        else if (key == 'D') {
            //qDebug() << "Increase gain";
            m_onlineCtrls.IncreaseGain();
        }
        else if (key == 'R') {
            //qDebug() << "Record";
            m_onlineCtrls.ToggleRecord();
        }
        else if (key == 'T') {
            // qDebug() << "Device setup";
        }
    }

    if ((m_displayMode == online) || (m_displayMode == review)) {
        if (key == 'P') {
            //qDebug() << "Snapshot";
            m_toolsCtrls.on_snapshot_clicked();
        }
        else if (key == 'H') {
            m_toolsCtrls.ToggleFlipHoriz();
        }
        else if (key == 'V') {
            m_toolsCtrls.ToggleFlipVert();
        }
        else if (key == 'M') {
            m_toolsCtrls.ToggleMeasure();
        }
        else if (key == 'G') {
            m_pSonarSurface->ToggleGridLines();
        }
    }

    if (m_displayMode == review) {
        if (key == ' ') {
            //qDebug() << "Play/Pause";
        }
    }
}

// ----------------------------------------------------------------------------
// Layout and set the visibility of the control panels
// LayoutCtrls fonksiyonunda değişiklik:
void MainView::LayoutCtrls()
{
    QRect r = rect();

    // Position the info string - TÜM GENİŞLİKTE
    m_info.setGeometry(0, 0, r.width(), 16);

    // Position the title controls - TÜM GENİŞLİKTE
    m_titleCtrls.setGeometry(0, 0, r.width(), 32);
    // Layout the title controls
    m_titleCtrls.LayoutCtrls(r);

    // Mode controls - sol tarafta (hex container'dan etkilenmez)
    m_modeCtrls.setGeometry(0, 32, m_modeCtrls.width(), m_modeCtrls.height());

    // Options controls - TÜM GENİŞLİĞİN sağ tarafına
    m_optionsCtrls.setGeometry(r.right() - m_optionsCtrls.width(), 32, m_optionsCtrls.width(), m_optionsCtrls.height());

    // Cursor controls - TÜM GENİŞLİĞİN sağ tarafına
    m_cursorCtrls.setGeometry(r.right() - m_cursorCtrls.width(), m_optionsCtrls.height() + m_titleCtrls.height() + 10, m_cursorCtrls.width(), m_cursorCtrls.height());

    // Info controls - sol tarafta (hex container'dan etkilenmez)
    m_infoCtrls.setGeometry(0, m_modeCtrls.height() + m_titleCtrls.height() + 10, m_infoCtrls.width(), m_infoCtrls.height());

    m_infoForm.layoutCtrls();

    // ALT KISIMLAR için hex container'ın durumunu hesapla
    int hexWidth = (m_showHexViewer && m_hexContainer && m_hexContainer->isVisible()) ? (r.width() / 5) : 0;
    int leftWidth = r.width() - hexWidth;

    QRect leftArea = r;
    leftArea.setWidth(leftWidth);

    // Palette control - SOL ALANIN içinde
    QRect paletteRect = m_palSelect.rect();
    m_palSelect.setGeometry(leftArea.width() - paletteRect.width() - 10, r.height() - paletteRect.height() - 75, paletteRect.width(), paletteRect.height());

    // Position the online controls - SOL ALANDA
    switch (m_displayMode)
    {
    case offline:
        m_fanDisplay.m_version.setVisible(true);
        m_onlineCtrls.setVisible(false);
        m_reviewCtrls.setVisible(false);
        m_toolsCtrls.setVisible(false);
        break;

    case online:
        m_fanDisplay.m_version.setVisible(false);
        m_onlineCtrls.setGeometry(10, r.height() - 110 - m_onlineCtrls.height(), m_onlineCtrls.width(), m_onlineCtrls.height());
        m_onlineCtrls.setVisible(true);
        m_reviewCtrls.setVisible(false);
        m_toolsCtrls.setVisible(true);
        m_toolsCtrls.setGeometry(leftArea.width() - 10 - m_toolsCtrls.width(), r.height() - 110 - m_toolsCtrls.height(), m_toolsCtrls.width(), m_toolsCtrls.height());
        break;

    case review:
        m_fanDisplay.m_version.setVisible(false);
        m_onlineCtrls.setVisible(false);
        m_reviewCtrls.setVisible(true);
        m_reviewCtrls.setGeometry(10, r.height() - 110 - m_reviewCtrls.height(), m_reviewCtrls.width(), m_reviewCtrls.height());
        m_toolsCtrls.setVisible(true);
        m_toolsCtrls.setGeometry(leftArea.width() - 10 - m_toolsCtrls.width(), r.height() - 110 - m_toolsCtrls.height(), m_toolsCtrls.width(), m_toolsCtrls.height());
        break;
    }
}

// ----------------------------------------------------------------------------
// Setup the controls display
void MainView::SetDisplayMode(eDisplayMode displayMode)
{
    m_displayMode = displayMode;

    // If we are set to review mode then set the sliderbar extents
    if (displayMode == review)
    {
        m_reviewCtrls.SetNEntries(m_nEntries);
    }
    else {
        if (displayMode == offline) {
            m_infoCtrls.HideInfo();
        }

    }

    m_pSonarSurface->m_disconnected = (displayMode == offline ? true : false);
    m_fanDisplay.update();

    // Update the mode controls
    m_modeCtrls.setDisplayMode(displayMode);

    LayoutCtrls();
}

// ----------------------------------------------------------------------------
// Read the persistent settings from the file
void MainView::ReadSettings()
{
    QSettings settings;

    m_pSonarSurface->m_palIndex = settings.value("PaletteIndex", 1).toInt();
    m_pSonarSurface->m_headDown = settings.value("HeadDown", 1).toBool();
    m_pSonarSurface->m_flipX    = settings.value("FlipX", 1).toBool();

    m_deviceForm.m_gainAssist   = settings.value("GainAssist", 1).toBool();
    m_deviceForm.m_gammaCorrection = settings.value("GammaCorrection", 150).toInt();
    m_deviceForm.m_netSpeedLimit = settings.value("NetSpeedLimit", 100).toInt();
    //	m_deviceForm.m_altFreq		  = settings.value("AltFreq", 0).toBool();

    m_showHexViewer = settings.value("ShowHexViewer", true).toBool();
    m_maxHexBytes = settings.value("MaxHexBytes", 64).toInt();

    if (m_hexContainer) {
        m_hexContainer->setVisible(m_showHexViewer);
    }

    m_onlineCtrls.ReadSettings();
    m_settings.ReadSettings();
    m_toolsCtrls.ReadSettings();

    m_palSelect.m_palette = m_pSonarSurface->m_palIndex;

    m_deviceForm.UpdateControls();
}

// ----------------------------------------------------------------------------
// Write the persistent settings from the file
void MainView::WriteSettings()
{
    QSettings settings;

    settings.setValue("PaletteIndex", m_pSonarSurface->m_palIndex);
    settings.setValue("HeadDown", m_pSonarSurface->m_headDown);
    settings.setValue("FlipX", m_pSonarSurface->m_flipX);
    settings.setValue("Style", m_themeName);

    settings.setValue("GainAssist", m_deviceForm.m_gainAssist);
    settings.setValue("GammaCorrection", m_deviceForm.m_gammaCorrection);
    settings.setValue("NetSpeedLimit", m_deviceForm.m_netSpeedLimit);
        //settings.setValue("AltFreq", m_deviceForm.m_altFreq);

    settings.setValue("ShowHexViewer", m_showHexViewer);
    settings.setValue("MaxHexBytes", m_maxHexBytes);

    m_onlineCtrls.WriteSettings();
    m_settings.WriteSettings();
    m_toolsCtrls.WriteSettings();
}

// ----------------------------------------------------------------------------
// (SLOT) Open the current replay file and start replay from the current item
void MainView::StartReplay()
{
    int entry = m_reviewCtrls.GetEntry();

    if (entry == (m_nEntries - 2)) {
        qDebug() << "End of file";
        entry = 0;
    }

    if (m_pEntries && entry < m_nEntries)
    {
        m_index = entry;
        m_player.OpenFileAt(m_replayFile, m_pEntries[m_index]);
        m_replay.start();

    }
}

// ----------------------------------------------------------------------------
// (SLOT) Close the current replay
void MainView::StopReplay()
{
    m_replay.stop();
    m_player.CloseFile();
    m_reviewCtrls.SetStop();
    counter = 0;
}

void MainView::UpdateSonarInfo(OculusPartNumberType pn) {

    if (!m_oculusClient.IsOpen()) {

        return;
    }

    // Find the sonar based on a direct part number match
    int index = 0;
    while (OculusSonarInfo[index].partNumber != partNumberEnd) {
        if (OculusSonarInfo[index].partNumber == pn) {
            qDebug() << "Found sonar: " << pn;
            break;
        }
        index++;
    }

    // Copy across the sonar information
    m_pSonarInfo = new OculusInfo();
    memset(m_pSonarInfo, 0, sizeof(OculusInfo));

    qDebug() << "Part no: " << pn;

    m_pSonarInfo->partNumber = pn;
    m_pSonarInfo->hasLF = OculusSonarInfo[index].hasLF;
    m_pSonarInfo->maxLF = OculusSonarInfo[index].maxLF;
    m_pSonarInfo->hasHF = OculusSonarInfo[index].hasHF;
    m_pSonarInfo->maxHF = OculusSonarInfo[index].maxHF;

    m_pSonarInfo->model = new char[64];

    switch (pn) {
    case partNumberM370s:
    case partNumberMT370s:
    case partNumberMD370s:
    case partNumberMD370s_Burton:
    case partNumberMD370s_Impulse:
        sprintf(m_pSonarInfo->model, "Oculus M370s");
        break;
    case partNumberC550d:
        sprintf(m_pSonarInfo->model, "Oculus C550d");
        break;
    case partNumberM750d:
    case partNumberMT750d:
    case partNumberMD750d:
    case partNumberMD750d_Burton:
    case partNumberMD750d_Impulse:
        sprintf(m_pSonarInfo->model, "Oculus M750d");
        break;
    case partNumberM1200d:
    case partNumberMT1200d:
    case partNumberMD1200d:
    case partNumberMD1200d_Burton:
    case partNumberMD1200d_Impulse:
        sprintf(m_pSonarInfo->model, "Oculus M1200d");
        break;
    case partNumberM3000d:
    case partNumberMT3000d:
    case partNumberMD3000d_Burton:
    case partNumberMD3000d_Impulse:
        sprintf(m_pSonarInfo->model, "Oculus M3000d");
        break;
    case partNumberUndefined:
    default:
        sprintf(m_pSonarInfo->model, "Undefined");
        break;
    }
}

// ----------------------------------------------------------------------------
// (SLOT) Respond to status messages from the Recieve UDP socket
void MainView::NewStatusMsg(OculusStatusMsg osm, quint16 valid, quint16 invalid)
{
    // This occurs when a status message is received from any sonar on the network
    m_modeCtrls.EnableConnect(true);

    m_statusMessageTimesLock.lock();
    m_statusMessageTimes[osm.deviceId] = QDateTime::currentDateTime();
    m_statusMessageTimesLock.unlock();


    // Update the "known sonar"table.
    if (!m_sonarList.contains(osm.deviceId)) {
        // Update the sonar list
        m_sonarLock.lock();
        m_sonarList[osm.deviceId] = osm;
        m_sonarLock.unlock();

        emit NewSonarDetected();
    }
    else {

        // Check here to see if a client has connected or disconnected since last time
        if (m_sonarList.contains(osm.deviceId)) {

            uint32_t client = m_sonarList[osm.deviceId].connectedIpAddr;

            // Update the sonar
            m_sonarLock.lock();
            m_sonarList[osm.deviceId] = osm;
            m_sonarLock.unlock();

            // If the connect client IP address is different, emit a signal
            if (osm.connectedIpAddr != client) {
                emit SonarClientStateChanged();
            }
        }




    }

    /*
    m_ipFromStatus = QString::number(ip1) + "." + QString::number(ip2) + "." + QString::number(ip3) + "." + QString::number(ip4);

    // Split out the connected IP address
    ip1 = (uchar)(osm.connectedIpAddr & 0xff);
    ip2 = (uchar)((osm.connectedIpAddr & 0xff00) >> 8);
    ip3 = (uchar)((osm.connectedIpAddr & 0xff0000) >> 16);
    ip4 = (uchar)((osm.connectedIpAddr & 0xff000000) >> 24);

    m_ipDevStatus = QString::number(ip1) + "." + QString::number(ip2) + "." + QString::number(ip3) + "." + QString::number(ip4);

    // Update the IP address fields
    QString statusMsg = "Sonar:0x" + QString::number(osm.hdr.oculusId, 16) + " IP:"+ m_ipFromStatus + " Dev:" + m_ipDevStatus
      + "  #:" + QString::number(valid) + "(" + QString::number(invalid) + ") ";
*/

    bool wasTimeout = m_timeout;


    // Split out the IP address
    uchar ip1 = (uchar)(osm.ipAddr & 0xff);
    uchar ip2 = (uchar)((osm.ipAddr & 0xff00) >> 8);
    uchar ip3 = (uchar)((osm.ipAddr & 0xff0000) >> 16);
    uchar ip4 = (uchar)((osm.ipAddr & 0xff000000) >> 24);

    QString addr = QString::number(ip1) + "." + QString::number(ip2) + "." + QString::number(ip3) + "." + QString::number(ip4);


    // Determine if this status message has been produced by the sonar we are presently
    // connected to. If it is our sonar, the update some on-screen labels, etc
    if (m_oculusClient.m_hostname == addr) {
        m_timeout = false;

        if (!m_pSonarInfo) {
            UpdateSonarInfo(osm.partNumber);

        }

        // Capture the information about the sonar
        if ((m_pSonarInfo) && (m_pSonarInfo->partNumber != partNumberUndefined)) {
            SetSonarInfo(osm);
            // Update the online controls to reflect the current part number
            m_onlineCtrls.UpdateControls(osm.partNumber);
        }
        m_onlineCtrls.UpdateRangeSlider(osm.partNumber);
    }


    // Show the connected ports
    QString indicators;
    indicators += (osm.status & (1 << 7) ? "Main " : "");
    //qDebug() << indicators;


    // -------------------------------------------------------------------------
    // Keep alives!
    // -------------------------------------------------------------------------

    // If the main socket is open, keep it alive
    if (m_oculusClient.IsOpen()) {
        // Send a simple fire to keep triggering the sonar
        FireSonar();
    }

    // Add some logic to determine whether a connection has been lost. If the device
    // returns (with a suitable time period), automatically reconnect
    if ((wasTimeout) && (! m_timeout)) {
        // qDebug() << "Connecting after TMO";
        m_timeout = false;
        m_oculusClient.Disconnect();
        Sleep(500);
        m_oculusClient.Connect();
    }

}

// ----------------------------------------------------------------------------
// Monitor waits for available sonars
void MainView::MonitorAvailableSonars()
{
    QDateTime timeNow = QDateTime::currentDateTime();
    m_statusMessageTimesLock.lock();
    QMap<uint32_t, QDateTime>::const_iterator i = m_statusMessageTimes.constBegin();
    bool bRemoved = false;
    while (i != m_statusMessageTimes.constEnd()) {
        if (i.value().msecsTo(timeNow) > 2000) {
            // remove the entry from the sonar list
            m_sonarLock.lock();
            m_sonarList.remove(i.key());
            m_sonarLock.unlock();
            bRemoved = true;
        }
        ++i;
    }
    m_statusMessageTimesLock.unlock();
    if (bRemoved) {
        emit NewSonarDetected();

        if (m_sonarList.count() == 0) {
            // Disable the connect button
            this->m_modeCtrls.EnableConnect(false);
        }
    }
}

// ----------------------------------------------------------------------------
// (SLOT) A new sonar signal from the oculus client

void MainView::NewReturnFire(OsBufferEntry* pEntry)
{
    uint16_t dst = 0;
    pEntry->m_mutex.lock();
    {
        int width = 0;
        int height = 0;
        double range = 0;
        uint16_t ver = 0;
        if (m_showHexViewer && m_hexViewer) {
            QString hexData = FormatHexData(pEntry);
            m_hexViewer->append(hexData);
            QTextCursor cursor = m_hexViewer->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_hexViewer->setTextCursor(cursor);
        }
        if (pEntry->m_simple) {
            if (pEntry->m_version == 2) {
                width = pEntry->m_rfm2.nBeams;
                height = pEntry->m_rfm2.nRanges;
                range = height * pEntry->m_rfm2.rangeResolution;
                dst = pEntry->m_rfm2.fireMessage.head.srcDeviceId;
            }
            else {
                width = pEntry->m_rfm.nBeams;
                height = pEntry->m_rfm.nRanges;
                range = height * pEntry->m_rfm.rangeResolution;
                dst = pEntry->m_rfm.fireMessage.head.srcDeviceId;
            }
        }
        else {
            width = pEntry->m_rff.ping.nBeams;
            height = pEntry->m_rff.ping_params.nRangeLinesBfm;
            range = pEntry->m_rff.ping.range;
            dst = pEntry->m_rff.head.srcDeviceId;
            ver = pEntry->m_rff.head.msgVersion;
        }

        // Sonar display güncelle
        m_pSonarSurface->UpdateFan(range, width, pEntry->m_pBrgs, true);
        m_pSonarSurface->UpdateImg(height, width, pEntry->m_pImage);

        // static qint64 lastSaveTime = 0;
        // qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        // if (currentTime - lastSaveTime >= 1000) {
        //     //SaveRenderedSonarImage_NoGrid();
        //     saveImageWithAutoLabel(height, width, pEntry->m_pImage,
        //                            pEntry->m_pBrgs, range, sonarImageDir);
        //     lastSaveTime = currentTime;
        // }

        // static qint64 lastSaveTime = 0;
        // qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        // if (currentTime - lastSaveTime >= 1000) {
        //     saveImageAsPng(height, width, pEntry->m_pImage,
        //                    pEntry->m_pBrgs, range, sonarImageDir);
        //     lastSaveTime = currentTime;
        // }

        // static qint64 lastSaveTime = 0;
        // qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
        // if (currentTime - lastSaveTime >= 1000) {
        //     SaveRenderedSonarImage_NoGrid();
        //     lastSaveTime = currentTime;
        // }

        // YOLO OBJECT DETECTION - FIXED VERSION
        if (m_yoloEnabled && m_yoloDetector && pEntry->m_pImage && width > 0 && height > 0) {
            try {
                // Create OpenCV Mat from sonar image data
                cv::Mat sonarImage(height, width, CV_8UC1, pEntry->m_pImage);

                // Run YOLO detection directly (preprocessing is done inside RunSession)
                // No need to resize here - inference.cpp handles it
                std::vector<DL_RESULT> results;
                m_yoloDetector->RunSession(sonarImage, results);

                if (!results.empty()) {
                    // Sort by confidence (descending)
                    std::sort(results.begin(), results.end(),
                              [](const DL_RESULT& a, const DL_RESULT& b) {
                                  return a.confidence > b.confidence;
                              });

                    // Limit to top 10 detections
                    int maxDetections = 10;
                    int numToShow = std::min((int)results.size(), maxDetections);

                    QList<SonarSurface::DetectedObject> detections;

                    for (int i = 0; i < numToShow; i++) {
                        const auto& det = results[i];

                        // Calculate center of detected object in pixel coordinates
                        float centerPixelX = det.box.x + det.box.width / 2.0f;
                        float centerPixelY = det.box.y + det.box.height / 2.0f;

                        // Validate coordinates
                        if (centerPixelX < 0 || centerPixelX >= width) {
                            qDebug() << "Skipping detection" << i << "- X out of bounds:" << centerPixelX;
                            continue;
                        }
                        if (centerPixelY < 0 || centerPixelY >= height) {
                            qDebug() << "Skipping detection" << i << "- Y out of bounds:" << centerPixelY;
                            continue;
                        }

                        // Get bearing index from pixel X coordinate
                        int bearingIndex = (int)(centerPixelX);
                        bearingIndex = std::max(0, std::min(bearingIndex, width - 1));

                        // Convert bearing index to radians
                        float bearingRad = 0.0f;
                        if (pEntry->m_pBrgs) {
                            bearingRad = pEntry->m_pBrgs[bearingIndex] * 0.01f * M_PI / 180.0f;
                        }

                        // Calculate distance from Y coordinate (Y=0 is near, Y=height is far)
                        float distance = (centerPixelY / (float)height) * range;

                        // Convert polar to Cartesian coordinates (sonar coordinate system)
                        float x = distance * sin(bearingRad);
                        float y = distance * cos(bearingRad);

                        // Calculate object size in meters
                        // Approximate: width scales with range, height is more constant
                        float objectWidthMeters = (det.box.width / (float)width) * range * 0.2f;
                        float objectHeightMeters = (det.box.height / (float)height) * range * 0.15f;

                        // Create detection object
                        SonarSurface::DetectedObject obj;
                        obj.meterPos = QPointF(x, y);
                        obj.meterWidth = objectWidthMeters;
                        obj.meterHeight = objectHeightMeters;
                        obj.confidence = det.confidence;

                        detections.append(obj);
                    }

                    // Update display
                    m_pSonarSurface->SetDetections(detections);
                }

            } catch (const std::exception& e) {
                qDebug() << "YOLO ERROR:" << e.what();
                m_yoloEnabled = false;
            }
        }

        m_logger.LogData(rt_oculusSonar, ver, false, pEntry->m_rawSize, pEntry->m_pRaw);
        m_info.setText("Logging To: '" + m_logger.m_fileName + "' Size: " + QString::number((double)m_logger.m_loggedSize / (1024 * 1024), 'f', 1));
        if (m_displayMode == review) {
            m_infoCtrls.HideInfo();
        }
    }
    pEntry->m_mutex.unlock();

    FireSonar();
    m_fanDisplay.update();
    if (m_logger.LogIsActive()) {
        UpdateLogFileName();
    }
}

void MainView::saveImageAsPng(int height, int width, uchar* image,
                              short* bearings, double range,
                              const QString& directoryPath)
{
    int size = 640;
    cv::Mat cartesian = cv::Mat::zeros(size, size, CV_8UC1);

    int centerX = size / 2;
    int centerY = size;

    // Polar → Cartesian
    for (int b = 0; b < width; b++) {
        float bearingRad = bearings[b] * 0.01f * M_PI / 180.0f;

        for (int r = 0; r < height; r++) {
            float distance = (r / (float)height) * range;
            int x = centerX + (int)(distance * sin(bearingRad) * (size / (2.0 * range)));
            int y = centerY - (int)(distance * cos(bearingRad) * (size / range));

            if (x >= 0 && x < size && y >= 0 && y < size) {
                uchar pixel = image[r * width + b];
                cartesian.at<uchar>(y, x) = std::max(cartesian.at<uchar>(y, x), pixel);
            }
        }
    }

    // Denoising (inpainting kaldırıldı)
    cv::Mat denoised;
    cv::fastNlMeansDenoising(cartesian, denoised, 5, 7, 21);

    // CLAHE
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    cv::Mat enhanced;
    clahe->apply(denoised, enhanced);

    // Kaydet
    counter++;
    QString filename = QDir(directoryPath).absoluteFilePath(
        QString("sonar_cartesian_%1.png").arg(counter, 5, 10, QChar('0'))
        );
    cv::imwrite(filename.toStdString(), enhanced);
}

void MainView::SaveRenderedSonarImage_NoGrid()
{
    if (!m_pSonarSurface) return;

    bool gridWasEnabled = m_pSonarSurface->m_showGrid;
    bool dwGridWasEnabled = m_pSonarSurface->m_dwGrid;
    bool gridTextWasEnabled = m_pSonarSurface->m_dwGridText;

    m_pSonarSurface->m_showGrid = false;
    m_pSonarSurface->m_dwGrid = false;
    m_pSonarSurface->m_dwGridText = false;
    m_fanDisplay.update();
    QCoreApplication::processEvents();

    QImage img = m_fanDisplay.grabFramebuffer();

    m_pSonarSurface->m_showGrid = gridWasEnabled;
    m_pSonarSurface->m_dwGrid = dwGridWasEnabled;
    m_pSonarSurface->m_dwGridText = gridTextWasEnabled;

    if (!img.isNull()) {
        m_renderCounter++;
        QString filename = QDir(sonarImageDir).absoluteFilePath(
            QString("sonar_clean_%1.png").arg(m_renderCounter, 5, 10, QChar('0'))
            );
        img.save(filename, "PNG");
    }
}

void MainView::SaveRenderedSonarImage()
{
    QImage img = m_fanDisplay.grabFramebuffer();

    if (!img.isNull()) {
        m_renderCounter++;
        QString filename = QDir(sonarImageDir).absoluteFilePath(
            QString("sonar_grid_%1.png").arg(m_renderCounter, 5, 10, QChar('0'))
            );
        img.save(filename, "PNG");
    }
}


// Helper function to apply sonar color mapping
QColor MainView::applySonarColorMap(float intensity)
{
    // Clamp intensity to 0-1 range
    intensity = qBound(0.0f, intensity, 1.0f);

    int red, green, blue;

    if (intensity < 0.15f) {
        // Reduced blue range - darker blue for low intensity (deep water/no return)
        red = 0;
        green = static_cast<int>(intensity * 6.67f * 80); // 0 to 80 (reduced green component)
        blue = static_cast<int>(150 + intensity * 6.67f * 105); // 150 to 255 (less intense blue)
    }
    else if (intensity < 0.35f) {
        // Shorter blue to cyan transition
        float t = (intensity - 0.15f) * 5; // 0 to 1
        red = static_cast<int>(t * 100); // Start adding red earlier
        green = static_cast<int>(80 + t * 175); // 80 to 255
        blue = static_cast<int>(255 * (1 - t * 0.6f)); // 255 to ~100 (reduce blue faster)
    }
    else if (intensity < 0.65f) {
        // Extended yellow range - cyan to yellow transition
        float t = (intensity - 0.35f) * 3.33f; // 0 to 1
        red = static_cast<int>(100 + t * 155); // 100 to 255
        green = 255;
        blue = static_cast<int>(100 * (1 - t)); // 100 to 0 (remove remaining blue)
    }
    else {
        // Extended yellow to bright yellow/white for high intensity
        float t = (intensity - 0.65f) * 2.86f; // 0 to 1
        red = 255;
        green = 255;
        blue = static_cast<int>(t * 150); // 0 to 150 (more yellow, less white)
    }

    return QColor(red, green, blue);
}


// ----------------------------------------------------------------------------
// (SLOT) A new sonar signal from the oculus client
void MainView::NewUserConfig(UserConfig config)
{
    m_oculusClient.m_wait.wakeAll();

    m_deviceForm.UpdateControls();
}

void MainView::StyleChanged(QString name)
{
    this->SetTheme(name);
}

// ----------------------------------------------------------------------------
// (SLOT) Send a simple fire message with the currents settings
void MainView::FireSonar()
{
    // If the main socket is not open, return
    if (! m_oculusClient.IsOpen()) {
        return;
    }

    // Cache the control values
    eSonarMode demand = m_onlineCtrls.m_demandMode;
    double range = m_onlineCtrls.m_demandRange;
    int gain = m_onlineCtrls.m_demandGain;
    double sos = m_settings.m_envCtrls.m_speedOfSound;
    bool gainAssist = m_deviceForm.m_gainAssist;
    uint8_t gamma = m_deviceForm.m_gammaCorrection;
    uint8_t netSpeedLimit = m_deviceForm.m_netSpeedLimit;

    if (netSpeedLimit == 100)
        netSpeedLimit = 0xff; // Will turn off the network speed limiter for 1000 baseT operation

    switch (m_settings.m_envCtrls.m_svType) {
    case freshWater:
        m_oculusClient.Fire(demand, range, gain, 0.0, 0.0, gainAssist, gamma, netSpeedLimit);
        break;
    case saltWater:
        m_oculusClient.Fire(demand, range, gain, 0.0, 35.0, gainAssist, gamma, netSpeedLimit);
        break;
    case fixedValue:
        m_oculusClient.Fire(demand, range, gain, sos, 0.0, gainAssist, gamma, netSpeedLimit);
        break;
    }

}

// ----------------------------------------------------------------------------
// (SLOT) The palette has been modified so force an update on the display
void MainView::PalSelected(int pal)
{
    m_pSonarSurface->m_palIndex = pal;
    m_pSonarSurface->Recalculate();
}

// ----------------------------------------------------------------------------
// (SLOT) Spawn an external browser with the Artemis webv page
void MainView::SpawnOculusWebView()
{
    QDesktopServices::openUrl(QUrl("http://www.blueprintsubsea.com/oculus/"));
}

// ----------------------------------------------------------------------------
// (SLOT) The connection to the sonar failed ...
void MainView::ConnectionFailed(QString error)
{
    SetDisplayMode(offline);

    // need to deal with this
    //m_modeCtrls.setInfo(tr("Connection Failed: ") + error);
    m_info.setText(tr("Connection Failed: ") + error);
}

// ----------------------------------------------------------------------------
// (SLOT) Respond to new data from the player
void MainView::OnNewPayload(unsigned short type, unsigned short version, double time, unsigned payloadSize, quint8 *pPayload)
{
    //Q_UNUSED(time)
    Q_UNUSED(version)

    m_payloadDateTime = QDateTime::fromMSecsSinceEpoch((quint64)(time * 1000.0));

    if (m_displayMode == review) {
        m_reviewCtrls.SetPlaybackTime(m_payloadDateTime);
    }

    // ------------------------------------------------------
    // If we have an oculus sonar record then push it into the system
    if (type == rt_oculusSonar)
    {
        m_entry.ProcessRaw((char*)pPayload);
        NewReturnFire(&m_entry);
    }
    // Sonar head data - initialise the sonar view and the review characteristics
    else if (type == rt_apSonarHeader)
    {
        //qDebug() << "Header";

        if (payloadSize == sizeof(ApSonarDataHeader))
        {
            memcpy(&m_sonarReplay, pPayload, sizeof(ApSonarDataHeader));

            //qDebug() << "Freq: " << m_sonarReplay.frequency;

            // Update the image extents
            m_pSonarSurface->UpdateFan(m_sonarReplay.range, m_sonarReplay.nBrgs, m_sonarReplay.pBrgs);

            if (m_displayMode == review) {
                //m_infoCtrls.UpdateModel(0);
                //m_infoCtrls.UpdateFrequency(m_sonarReplay.frequency);
            }
        }

    }
    // This is a raw image as defined by a previous rt_apSonarHeader
    else if (type == rt_rawSonarImage)
    {
        // Version 2 is 8bit uncompressed data
        if (version == 2)
        {
            if (payloadSize == (unsigned)(m_sonarReplay.nBrgs * m_sonarReplay.nRngs))
            {
                // Update the image extents
                m_pSonarSurface->UpdateFan(m_sonarReplay.range, m_sonarReplay.nBrgs, m_sonarReplay.pBrgs);
                m_pSonarSurface->m_nRngs = m_sonarReplay.nRngs;
                m_pSonarSurface->m_nBrgs = m_sonarReplay.nBrgs;

                m_pSonarSurface->m_pData = (quint8*) realloc (m_pSonarSurface->m_pData, payloadSize);

                memcpy(m_pSonarSurface->m_pData, pPayload, payloadSize);

                m_pSonarSurface->m_newImgData = true;

                // Update the dipslay
                m_fanDisplay.update();
            }
            else
                qDebug() << "The 8 bit image has incorrect size. PL:" + QString::number(payloadSize) + " IS:" + QString::number(m_sonarReplay.nBrgs * m_sonarReplay.nRngs);
        }
    }
}

// ----------------------------------------------------------------------------
// The review entry has changed
void MainView::ReviewEntryChanged(int entry)
{
    bool playing = m_replay.isActive();

    if (playing) {
        this->StopReplay();
    }

    if (m_pEntries && entry < m_nEntries)
    {
        m_index = entry;
        m_player.OpenFileAt(m_replayFile, m_pEntries[entry]);
        m_player.ReadNextItem();

        // Raw sonar records are in pairs
        if (m_useRawSonar)
            m_player.ReadNextItem();

        m_player.CloseFile();
    }

    if (playing)
        this->StartReplay();
}

// ----------------------------------------------------------------------------
void MainView::ReviewLowerEntryChanged(int entry) {

    bool playing = m_replay.isActive();
    // Stop any currently playing log
    if (playing)
        this->StopReplay();

    if (m_pEntries && entry < m_nEntries)
    {
        m_indexLower = entry;
        m_player.OpenFileAt(m_replayFile, m_pEntries[entry]);
        m_player.ReadNextItem();

        // Raw sonar records are in pairs
        if (m_useRawSonar)
            m_player.ReadNextItem();

        m_player.CloseFile();
    }
}

// ----------------------------------------------------------------------------
// The review entry has changed
void MainView::ReviewUpperEntryChanged(int entry) {

    bool playing = m_replay.isActive();
    // Stop any currently playing log
    if (playing)
        this->StopReplay();

    if (m_pEntries && entry < m_nEntries)
    {
        m_indexUpper = entry;
        m_player.OpenFileAt(m_replayFile, m_pEntries[entry]);
        m_player.ReadNextItem();

        // Raw sonar records are in pairs
        if (m_useRawSonar)
            m_player.ReadNextItem();

        m_player.CloseFile();
    }

}

// ----------------------------------------------------------------------------
void MainView::PlayNext()
{
    //qDebug() << "Play next";

    if (0 <= m_index && m_index < m_nEntries - 2)
    {
        m_index++;
        m_player.Seek(m_pEntries[m_index]);
        m_player.ReadNextItem();

        // Raw sonar records are in pairs
        if (m_useRawSonar)
            m_player.ReadNextItem();

        m_reviewCtrls.blockSignals(true);
        m_reviewCtrls.SetEntry(m_index);
        m_reviewCtrls.blockSignals(false);

        double next;
        m_player.PeekNextItem(&next);

        QDateTime nextTime = QDateTime::fromMSecsSinceEpoch((quint64)(next * 1000.0));

        //qDebug() << m_payloadDateTime;
        //qDebug() << nextTime;

        // Work out the time delta
        qint64 delta = m_payloadDateTime.msecsTo(nextTime);

        //qDebug() << "Delta time: " << delta;

        // Default to a 10Hz replay speed if the delta time is stupid
        if ((delta < 10) || (delta > 500))
            delta = 100;

        // Account for the play speed
        delta = (delta / m_playSpeed);

        // Update the timer interval
        m_replay.setInterval(delta);
    }

    // Check if we need to reset the index if repeat is enabled
    if ((m_index >= (m_nEntries - 2)) && (m_player.m_repeat)) {
        m_index = 0;
    }

    if (m_index == (m_nEntries-2)) {
        qDebug() << "Stop replay";
        this->StopReplay();
    }

}

// ----------------------------------------------------------------------------
bool MainView::Snapshot()
{
    // Get the primary screen
    QScreen* screen = QGuiApplication::primaryScreen();
    //QPixmap pixmap = screen->grabWindow(0);
    QPixmap pixmap = this->grab();

    // Grab the framebuffer
    //QImage snapshot = m_pMainWnd->m_fanDisplay.grabFramebuffer();

    // Create the output filename
    QDateTime dt = QDateTime::currentDateTime();

    // Source file
    QString srcFile;
    QDateTime srcDate;

    // Work out what we're going to call our image file
    if (m_logger.m_fileName != "") {
        // Logging
        srcFile = m_logger.m_fileName;
    }
    else if (m_replayFile != "") {
        // Replaying
        srcFile = m_replayFile;
        srcDate = m_payloadDateTime;
    }
    else {
        // Online but not logging
        srcFile = m_logger.m_logDir + QDir::separator() + QString(m_logger.s_source);
    }




    // Check the date
    if (!srcDate.isValid()) {
        srcDate = QDateTime::currentDateTime();
    }

    // Compile a file name
    QFileInfo info(srcFile);
    QString destPath = info.dir().absolutePath();
    QString destFile = destPath + QDir::separator() + info.baseName() + srcDate.toString("_yyyyMMdd_hhmmss.png");

    // Create the log directory if it doesn't exist
    QDir ld(destPath);

    // If the log directory doesn't exist, create it
    if (!ld.exists(destPath)) {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setStandardButtons(QMessageBox::Ok);

        QString str = "Unable to save the snapshot.\r\n\r\nThe following directory does not exist:\r\n\r\n";
        str += destPath;

        msg.setText(str);
        msg.setWindowTitle("Snapshot Error");
        msg.exec();
        return false;
    }

    // Save the snapshot
    pixmap.save(destFile, "PNG");

    return true;
}

// ----------------------------------------------------------------------------
void MainView::StartLog()
{
    m_logger.OpenLog();

    if (m_logger.m_state == logging) {
        //m_modeCtrls.setInfo("Logging To: " + m_logger.m_fileName);
        m_info.setText("Logging To: " + m_logger.m_fileName);
    }
    else
    {
        //m_modeCtrls.setInfo("Logging Failed! Cannot Open: " + m_logger.m_fileName);
        m_info.setText("Logging Failed! Cannot Open: " + m_logger.m_fileName);
        m_onlineCtrls.CancelRecord();
        update();


    }

    /*
    OculusViewInfo info;
    info.flipX = m_pSonarSurface->m_flipX;
    info.flipY = m_pSonarSurface->m_headDown;
    info.palette = (uint8_t)m_palSelect.m_palette;

    // Write the view information to the log file
    m_logger.LogData(rt_ocViewInfo, 1, false, sizeof(OculusViewInfo), (unsigned char *)&info);
    */
}

// ----------------------------------------------------------------------------
void MainView::StopLog()
{
    m_logger.CloseLog();

    if (m_logger.m_state == notLogging)
        m_info.setText("");
}

// ----------------------------------------------------------------------------
void MainView::FlipX(bool flip)
{
    SonarSurface* pSonar = (SonarSurface*) m_fanDisplay.RmglGetSurface();

    pSonar->m_flipX = flip;
    pSonar->Recalculate();
}

// ----------------------------------------------------------------------------
void MainView::FlipY(bool flip)
{
    SonarSurface* pSonar = (SonarSurface*)m_fanDisplay.RmglGetSurface();

    pSonar->m_headDown = !flip;
    pSonar->Recalculate();
}

// ----------------------------------------------------------------------------
void MainView::SetTheme(QString theme)
{
    // Get the current application instance
    QApplication *app = static_cast<QApplication *>(QCoreApplication::instance());

    QFile cssFile(":/" + theme + ".css");
    cssFile.open(QFile::ReadOnly);
    QString css = QString(cssFile.readAll());
    cssFile.close();

    // Apply the stylesheet
    app->setStyleSheet(css);

    m_themeName = theme;
}

// ----------------------------------------------------------------------------
void MainView::SetMeasureMode(bool enable)
{
    //m_measureMode = enable;

    // Enable the measurement tool
    m_pSonarSurface->m_measureEnable = enable;

    if (!enable) {
        m_pSonarSurface->m_showLastMeasurement = false;
        m_pSonarSurface->m_measuring = false;
        m_pSonarSurface->Update();
    }
}

// ----------------------------------------------------------------------------
void MainView::RebuildSonarList()
{
    m_sonarLock.lock();
    m_sonarList.clear();
    m_sonarLock.unlock();
}

// ----------------------------------------------------------------------------
void MainView::MouseInfo(float dist, float angle, float x, float y)
{
    // Update the mouse cursor location
    m_cursorCtrls.UpdateMouseInfo(dist, angle, x, y);
}

// ----------------------------------------------------------------------------
void MainView::SetSonarInfo(OculusStatusMsg msg)
{
    // Split out the connected IP address
    uchar ip1 = (uchar)(msg.ipAddr & 0xff);
    uchar ip2 = (uchar)((msg.ipAddr & 0xff00) >> 8);
    uchar ip3 = (uchar)((msg.ipAddr & 0xff0000) >> 16);
    uchar ip4 = (uchar)((msg.ipAddr & 0xff000000) >> 24);

    QString ipAddr = QString::number(ip1) + "." + QString::number(ip2) + "." + QString::number(ip3) + "." + QString::number(ip4);

    // Split out the connected IP address
    uchar msk1 = (uchar)(msg.ipMask & 0xff);
    uchar msk2 = (uchar)((msg.ipMask & 0xff00) >> 8);
    uchar msk3 = (uchar)((msg.ipMask & 0xff0000) >> 16);
    uchar msk4 = (uchar)((msg.ipMask & 0xff000000) >> 24);

    QString ipMask = QString::number(msk1) + "." + QString::number(msk2) + "." + QString::number(msk3) + "." + QString::number(msk4);

    // Capture the part number
    m_partNumber = msg.partNumber;

    // Update the sonar info display
    //m_infoCtrls.UpdateInfo(ipAddr, ipMask, QString::number(msg.deviceId), msg.versionInfo.masterVersion);

    if (m_displayMode == online) {
        //m_infoCtrls.setVisible(true);
        // Update the sonar info display
        m_infoCtrls.UpdateInfo(ipAddr, ipMask, QString::number(msg.deviceId));
        // Update any error status information
        m_infoCtrls.UpdateError(msg.status);

        m_infoCtrls.UpdateModel(msg.partNumber);
    }

}

// ----------------------------------------------------------------------------
void MainView::MouseLeaveFan()
{
    m_cursorCtrls.setVisible(false);
}

// ----------------------------------------------------------------------------
void MainView::MouseEnterFan()
{
    m_cursorCtrls.setVisible(true);
}

// ----------------------------------------------------------------------------
void MainView::SetTitleLogFile(QString string) {

    m_titleCtrls.SetTitle(string);

}

// ----------------------------------------------------------------------------
void MainView::SocketTimeout() {
    m_infoForm.setInfo("Oculus Ethernet Connection Timeout!");

    m_timeout = true;
}

// ----------------------------------------------------------------------------
void MainView::SocketReconnecting() {
    m_infoForm.hide();

    m_timeout = false;
}

// ----------------------------------------------------------------------------
void MainView::SocketDisconnected() {
    //qDebug() << "Timeout state: " << m_timeout;

    m_infoForm.hide();

    if (m_timeout) {
        m_modeCtrls.Disconnect();
        m_partNumber = OculusPartNumberType::partNumberUndefined;

        // Force all sonar entries to rebuild
        m_sonarLock.lock();
        m_sonarList.clear();
        m_statusMessageTimes.clear();
        m_sonarLock.unlock();

        m_modeCtrls.EnableConnect(false);

        m_timeout = false;
    }
}

void MainView::AbortReconnect() {
    this->SocketDisconnected();
}

void MainView::ToggleHexViewer()
{
    m_showHexViewer = !m_showHexViewer;

    if (m_hexContainer) {
        m_hexContainer->setVisible(m_showHexViewer);
    }

    // Layout'u yeniden hesapla
    resizeEvent(nullptr);
}

// Send Ping button click handler (MainView.h'a da eklenmeli)
void MainView::OnSendPingClicked()
{
    // Cihaza ekrandaki ayarları gönder
    if (m_oculusClient.IsOpen()) {
        // FireSonar fonksiyonunu çağır - bu zaten mevcut ayarları gönderir
        FireSonar();

        // Hex viewer'a bilgi mesajı ekle
        if (m_hexViewer) {
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            m_hexViewer->append(QString("\n--- PING SENT [%1] ---").arg(timestamp));
            m_hexViewer->append("Current settings sent to device:");

            // Mevcut ayarları göster
            eSonarMode mode = m_onlineCtrls.m_demandMode;
            double range = m_onlineCtrls.m_demandRange;
            int gain = m_onlineCtrls.m_demandGain;

            m_hexViewer->append(QString("Mode: %1").arg(mode == 1 ? "High Frequency" : "Low Frequency"));
            m_hexViewer->append(QString("Range: %1 m").arg(range));
            m_hexViewer->append(QString("Gain: %1%").arg(gain));

            // Çevre ayarları
            QString svType;
            switch (m_settings.m_envCtrls.m_svType) {
            case freshWater: svType = "Fresh Water"; break;
            case saltWater: svType = "Salt Water"; break;
            case fixedValue: svType = QString("Fixed (%1 m/s)").arg(m_settings.m_envCtrls.m_speedOfSound); break;
            }
            m_hexViewer->append(QString("Sound Velocity: %1").arg(svType));
            m_hexViewer->append("Ping command executed successfully.");
            m_hexViewer->append("-------------------------\n");

            // Auto-scroll to bottom
            QTextCursor cursor = m_hexViewer->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_hexViewer->setTextCursor(cursor);
        }
    } else {
        // Cihaz bağlı değilse uyarı ver
        if (m_hexViewer) {
            QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
            m_hexViewer->append(QString("\n--- PING FAILED [%1] ---").arg(timestamp));
            m_hexViewer->append("ERROR: No device connected!");
            m_hexViewer->append("Please connect to a sonar device first.");
            m_hexViewer->append("-------------------------\n");

            // Auto-scroll to bottom
            QTextCursor cursor = m_hexViewer->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_hexViewer->setTextCursor(cursor);
        }
    }
}

// Clear button click handler (MainView.h'a da eklenmeli)
void MainView::OnClearHexViewer()
{
    // Text browser'ı temizle
    if (m_hexViewer) {
        m_hexViewer->clear();
        m_hexViewer->setPlainText("Hex Viewer Ready - Waiting for packet data...\nPress 'X' to toggle visibility");
    }
}

// Exit button click handler (MainView.h'a da eklenmeli)
void MainView::OnExitHexViewer()
{
    // Hex container'ı toggle et
    ToggleHexViewer();
}

void MainView::CreateHexViewer()
{
    // Sağ taraf için container widget oluştur
    m_hexContainer = new QWidget(this);
    m_hexContainer->setStyleSheet(
        "QWidget { "
        "   border-left: 2px solid #0066CC; "
        "   background-color: #1a1a1a; "
        "}"
        );

    // Ana layout oluştur
    QVBoxLayout* mainLayout = new QVBoxLayout(m_hexContainer);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // ===========================================
    // SONAR DATA SECTION
    // ===========================================

    // Sonar Data başlık frame'i
    QFrame* sonarDataFrame = new QFrame(m_hexContainer);
    sonarDataFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
    sonarDataFrame->setStyleSheet(
        "QFrame {"
        "   background-color: #2a2a2a;"
        "   border: 2px solid #0066CC;"
        "   border-radius: 8px;"
        "   margin: 2px;"
        "}"
        );

    QVBoxLayout* sonarLayout = new QVBoxLayout(sonarDataFrame);
    sonarLayout->setContentsMargins(10, 8, 10, 8);
    sonarLayout->setSpacing(4);

    // Sonar Data başlığı
    QLabel* sonarTitle = new QLabel("SONAR DATA MONITOR", sonarDataFrame);
    sonarTitle->setStyleSheet(
        "QLabel {"
        "   font-weight: 700;"
        "   font-size: 14px;"
        "   font-family: 'Segoe UI', 'San Francisco', 'Helvetica Neue', 'Arial', sans-serif;"
        "   color: #00FFFF;"
        "   background-color: #003366;"
        "   padding: 6px;"
        "   border-radius: 4px;"
        "   border: 1px solid #0066CC;"
        "}"
        );
    sonarTitle->setAlignment(Qt::AlignCenter);
    sonarLayout->addWidget(sonarTitle);

    // Grid layout için widget
    QWidget* gridWidget = new QWidget();
    QGridLayout* gridLayout = new QGridLayout(gridWidget);
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(5, 5, 5, 5);

    // Label stilini tanımla
    QString labelStyle =
        "QLabel {"
        "   color: #CCCCCC;"
        "   font-weight: 600;"
        "   font-size: 14px;"
        "   font-family: 'Segoe UI', 'San Francisco', 'Helvetica Neue', 'Arial', sans-serif;"
        "   padding: 2px 6px;"
        "   background-color: #404040;"
        "   border-radius: 3px;"
        "   min-width: 80px;"
        "}";

    // Value stilini tanımla
    QString valueStyle =
        "QLabel {"
        "   color: #F8FAFC;"
        "   font-weight: 600;"
        "   font-size: 14px;"
        "   font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', 'Source Code Pro', 'Consolas', monospace;"
        "   padding: 4px 8px;"
        "   background-color: #1a1a1a;"
        "   border: 1px solid #333333;"
        "   border-radius: 4px;"
        "   min-width: 100px;"
        "}";

    // Ping ID
    QLabel* pingIdLabel = new QLabel("Ping ID:", gridWidget);
    pingIdLabel->setStyleSheet(labelStyle);
    m_pingIdValue = new QLabel("---", gridWidget);
    m_pingIdValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(pingIdLabel, 0, 0);
    gridLayout->addWidget(m_pingIdValue, 0, 1);

    // Packet Size
    QLabel* packetSizeLabel = new QLabel("Packet Size:", gridWidget);
    packetSizeLabel->setStyleSheet(labelStyle);
    m_packetSizeValue = new QLabel("--- bytes", gridWidget);
    m_packetSizeValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(packetSizeLabel, 1, 0);
    gridLayout->addWidget(m_packetSizeValue, 1, 1);

    // Ranges
    QLabel* rangesLabel = new QLabel("Ranges:", gridWidget);
    rangesLabel->setStyleSheet(labelStyle);
    m_rangesValue = new QLabel("---", gridWidget);
    m_rangesValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(rangesLabel, 2, 0);
    gridLayout->addWidget(m_rangesValue, 2, 1);

    // Beams
    QLabel* beamsLabel = new QLabel("Beams:", gridWidget);
    beamsLabel->setStyleSheet(labelStyle);
    m_beamsValue = new QLabel("---", gridWidget);
    m_beamsValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(beamsLabel, 3, 0);
    gridLayout->addWidget(m_beamsValue, 3, 1);

    // Frequency
    QLabel* frequencyLabel = new QLabel("Frequency:", gridWidget);
    frequencyLabel->setStyleSheet(labelStyle);
    m_frequencyValue = new QLabel("--- Hz", gridWidget);
    m_frequencyValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(frequencyLabel, 4, 0);
    gridLayout->addWidget(m_frequencyValue, 4, 1);

    // Temperature
    QLabel* temperatureLabel = new QLabel("Temperature:", gridWidget);
    temperatureLabel->setStyleSheet(labelStyle);
    m_temperatureValue = new QLabel("--- °C", gridWidget);
                         m_temperatureValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(temperatureLabel, 5, 0);
    gridLayout->addWidget(m_temperatureValue, 5, 1);

    // Pressure
    QLabel* pressureLabel = new QLabel("Pressure:", gridWidget);
    pressureLabel->setStyleSheet(labelStyle);
    m_pressureValue = new QLabel("--- bar", gridWidget);
    m_pressureValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(pressureLabel, 6, 0);
    gridLayout->addWidget(m_pressureValue, 6, 1);

    // Speed of Sound
    QLabel* sosLabel = new QLabel("Sound Speed:", gridWidget);
    sosLabel->setStyleSheet(labelStyle);
    m_sosValue = new QLabel("--- m/s", gridWidget);
    m_sosValue->setStyleSheet(valueStyle);
    gridLayout->addWidget(sosLabel, 7, 0);
    gridLayout->addWidget(m_sosValue, 7, 1);

    sonarLayout->addWidget(gridWidget);
    mainLayout->addWidget(sonarDataFrame);

    // ===========================================
    // HEX VIEWER SECTION
    // ===========================================

    // Hex Viewer başlık frame'i
    QFrame* hexFrame = new QFrame(m_hexContainer);
    hexFrame->setFrameStyle(QFrame::Box | QFrame::Raised);
    hexFrame->setStyleSheet(
        "QFrame {"
        "   background-color: #2a2a2a;"
        "   border: 2px solid #0066CC;"
        "   border-radius: 8px;"
        "   margin: 2px;"
        "}"
        );

    QVBoxLayout* hexLayout = new QVBoxLayout(hexFrame);
    hexLayout->setContentsMargins(10, 8, 10, 8);
    hexLayout->setSpacing(6);

    // Hex Viewer başlığı
    QLabel* hexTitle = new QLabel("RAW PACKET DATA", hexFrame);
    hexTitle->setStyleSheet(
        "QLabel {"
        "   font-weight: 700;"
        "   font-size: 14px;"
        "   font-family: 'Segoe UI', 'San Francisco', 'Helvetica Neue', 'Arial', sans-serif;"
        "   color: #00FFFF;"
        "   background-color: #003366;"
        "   padding: 6px;"
        "   border-radius: 4px;"
        "   border: 1px solid #0066CC;"
        "}"
        );
    hexTitle->setAlignment(Qt::AlignCenter);
    hexLayout->addWidget(hexTitle);

    // Hex viewer text browser'ı oluştur
    m_hexViewer = new QTextBrowser(hexFrame);
    m_hexViewer->setFont(QFont("JetBrains Mono, Fira Code, SF Mono, Monaco, Inconsolata, Roboto Mono, Source Code Pro, Consolas", 9));
    m_hexViewer->setMinimumHeight(200);

    // Dark color palette with green text
    QPalette palette = m_hexViewer->palette();
    palette.setColor(QPalette::Base, QColor(20, 20, 20));
    palette.setColor(QPalette::Text, QColor(0, 255, 100));
    m_hexViewer->setPalette(palette);

    m_hexViewer->setStyleSheet(
        "QTextBrowser {"
        "   background-color: #141414;"
        "   color: #00FF64;"
        "   border: 1px solid #333333;"
        "   border-radius: 4px;"
        "   padding: 4px;"
        "   selection-background-color: #0066CC;"
        "}"
        );

    // Set document properties to limit memory usage
    QTextDocument* doc = m_hexViewer->document();
    doc->setMaximumBlockCount(500);

    m_hexViewer->setAcceptRichText(false);
    m_hexViewer->setPlainText("Hex Viewer Ready - Waiting for packet data...\nPress 'X' to toggle visibility");

    hexLayout->addWidget(m_hexViewer);
    mainLayout->addWidget(hexFrame);

    // ===========================================
    // CONTROL BUTTONS SECTION
    // ===========================================

    // Button'lar için horizontal layout oluştur
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(8);
    buttonLayout->setContentsMargins(5, 0, 5, 5);

    // Button stili tanımla - sonar temalı
    QString buttonStyle =
        "QPushButton {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #01498E, stop:1 #003366);"
        "   color: #ffffff;"
        "   border: 2px solid #0066CC;"
        "   border-radius: 6px;"
        "   padding: 8px 16px;"
        "   font-size: 12px;"
        "   font-weight: 600;"
        "   font-family: 'Segoe UI', 'San Francisco', 'Helvetica Neue', 'Arial', sans-serif;"
        "   min-width: 70px;"
        "}"
        "QPushButton:hover {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #0066CC, stop:1 #01498E);"
        "   border-color: #0080FF;"
        "   color: #CCFFFF;"
        "}"
        "QPushButton:pressed {"
        "   background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #003366, stop:1 #001133);"
        "   border-color: #004488;"
        "}"
        "QPushButton:disabled {"
        "   background-color: #404040;"
        "   color: #808080;"
        "   border-color: #606060;"
        "}";

    // Send Ping button oluştur
    QPushButton* sendPingBtn = new QPushButton("PING", m_hexContainer);
    sendPingBtn->setStyleSheet(buttonStyle);
    sendPingBtn->setToolTip("Send ping to sonar device");

    // Clear button oluştur
    QPushButton* clearBtn = new QPushButton("CLEAR", m_hexContainer);
    clearBtn->setStyleSheet(buttonStyle);
    clearBtn->setToolTip("Clear hex viewer data");

    // Exit button oluştur
    QPushButton* exitBtn = new QPushButton("CLOSE", m_hexContainer);
    exitBtn->setStyleSheet(buttonStyle);
    exitBtn->setToolTip("Close data monitor");

    // Button'ları layout'a ekle
    buttonLayout->addWidget(sendPingBtn);
    buttonLayout->addWidget(clearBtn);
    buttonLayout->addWidget(exitBtn);

    // Ana layout'a button layout'ını ekle
    mainLayout->addLayout(buttonLayout);

    // Default olarak görünür yap
    m_showHexViewer = true;
    m_hexContainer->setVisible(true);

    // Button click connectionları
    connect(sendPingBtn, &QPushButton::clicked, this, &MainView::OnSendPingClicked);
    connect(clearBtn, &QPushButton::clicked, this, &MainView::OnClearHexViewer);
    connect(exitBtn, &QPushButton::clicked, this, &MainView::OnExitHexViewer);
}

QString MainView::FormatHexData(OsBufferEntry* pEntry)
{
    if (!pEntry) {
        // Clear all labels when no data
        if (m_pingIdValue) m_pingIdValue->setText("---");
        if (m_packetSizeValue) m_packetSizeValue->setText("--- bytes");
        if (m_rangesValue) m_rangesValue->setText("---");
        if (m_beamsValue) m_beamsValue->setText("---");
        if (m_frequencyValue) m_frequencyValue->setText("--- Hz");
        if (m_temperatureValue) m_temperatureValue->setText("--- °C");
                if (m_pressureValue) m_pressureValue->setText("--- bar");
        if (m_sosValue) m_sosValue->setText("--- m/s");

        return QString("No data available");
    }

    // Update the sonar data labels
    if (m_pingIdValue) {
        m_pingIdValue->setText(QString::number(pEntry->m_rfm.pingId));
    }

    if (m_packetSizeValue) {
        m_packetSizeValue->setText(QString("%1 bytes").arg(pEntry->m_rawSize));
    }

    if (m_rangesValue) {
        m_rangesValue->setText(QString::number(pEntry->m_rfm.nRanges));
    }

    if (m_beamsValue) {
        m_beamsValue->setText(QString::number(pEntry->m_rfm.nBeams));
    }

    if (m_frequencyValue) {
        // Format frequency with proper units
        double freq = pEntry->m_rfm.frequency;
        if (freq >= 1000000) {
            m_frequencyValue->setText(QString("%1 MHz").arg(freq / 1000000.0, 0, 'f', 2));
        } else if (freq >= 1000) {
            m_frequencyValue->setText(QString("%1 kHz").arg(freq / 1000.0, 0, 'f', 1));
        } else {
            m_frequencyValue->setText(QString("%1 Hz").arg(freq, 0, 'f', 0));
        }
    }

    if (m_temperatureValue) {
        m_temperatureValue->setText(QString("%1 °C").arg(pEntry->m_rfm.temperature, 0, 'f', 1));
    }

    if (m_pressureValue) {
        m_pressureValue->setText(QString("%1 bar").arg(pEntry->m_rfm.pressure, 0, 'f', 2));
    }

    if (m_sosValue) {
        m_sosValue->setText(QString("%1 m/s").arg(pEntry->m_rfm.speedOfSoundUsed, 0, 'f', 1));
    }

    // Generate hex dump for the text browser
    quint32 displaySize = qMin((quint32)m_maxHexBytes, (quint32)pEntry->m_rawSize);

    QString result;
    QDateTime currentTime = QDateTime::currentDateTime();
    // Basic packet info
    result += QString("Timestamp: %1\n").arg(currentTime.toString("yyyy-MM-dd hh:mm:ss.zzz"));
    result += QString("Total Size: %1 bytes (showing first %2)\n").arg(pEntry->m_rawSize).arg(displaySize);

    // Hex data section
    QByteArray rawData(reinterpret_cast<const char*>(pEntry->m_pRaw), displaySize);
    QString hexDump;

    for (int i = 0; i < displaySize; i += 16) {
        // Format offset (8-digit hex)
        hexDump += QString("%1: ").arg(i, 8, 16, QLatin1Char('0')).toUpper();

        // Hex bytes (16 per line) with color coding
        QString hexLine;
        QString asciiLine;

        for (int j = 0; j < 16; ++j) {
            int idx = i + j;
            if (idx < displaySize) {
                quint8 byte = static_cast<quint8>(rawData[idx]);
                hexLine += QString("%1 ").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
            } else {
                hexLine += "   "; // Pad for alignment
            }
        }

        hexDump += hexLine + "\n";
    }

    result += hexDump;
    return result;
}

// resizeEvent'te hex container pozisyonunu aşağı kaydır:
void MainView::resizeEvent(QResizeEvent* event)
{
    Q_UNUSED(event);

    QRect r = rect();

    // Hex container'ın görünür olup olmadığına göre alan hesapla
    int hexWidth = m_showHexViewer ? (r.width() / 5 + 50) : 0;
    int leftWidth = r.width() - hexWidth;

    // Sol alan (sonar için) - sadece sonar display için
    QRect leftArea = r;
    leftArea.setWidth(leftWidth);
    leftArea.adjust(0, 0, 0, -100);
    leftArea.translate(0, 60);

    if ((leftArea.width() % 2) != 0)
        leftArea.setWidth(leftArea.width() + 1);

    if ((leftArea.height() % 2) != 0)
        leftArea.setHeight(leftArea.height() + 1);

    // Fan display'i sol alana yerleştir
    m_fanDisplay.setGeometry(leftArea);

    // Hex container'ı sağ alana yerleştir - daha aşağıda başla
    if (m_hexContainer) {
        QRect hexArea;
        hexArea.setLeft(r.width() - hexWidth);  // Sağ taraf
        hexArea.setTop(220);
        hexArea.setWidth(hexWidth);
        hexArea.setHeight(r.height() - 250);  // Yüksekliği de ayarla

        m_hexContainer->setGeometry(hexArea);
    }

    // Control'ları yeniden düzenle
    LayoutCtrls();
}


// ============================================================================
// YOLO Checkbox
// ============================================================================

void MainView::CreateYoloCheckbox()
{
    m_yoloCheckbox = new QCheckBox("YOLO Detection", this);
    m_yoloCheckbox->setGeometry(10, 120, 150, 25);  // Sol üst, 3 satır alta
    m_yoloCheckbox->setChecked(m_yoloEnabled);

    m_yoloCheckbox->setStyleSheet(
        "QCheckBox {"
        "   color: #00FFFF;"
        "   font-weight: bold;"
        "   font-size: 12px;"
        "   background-color: rgba(0, 0, 0, 150);"
        "   padding: 4px;"
        "   border-radius: 4px;"
        "}"
        "QCheckBox::indicator {"
        "   width: 18px;"
        "   height: 18px;"
        "}"
        "QCheckBox::indicator:unchecked {"
        "   background-color: #333;"
        "   border: 2px solid #666;"
        "   border-radius: 3px;"
        "}"
        "QCheckBox::indicator:checked {"
        "   background-color: #00FF00;"
        "   border: 2px solid #00CC00;"
        "   border-radius: 3px;"
        "}"
        );

    connect(m_yoloCheckbox, &QCheckBox::toggled,
            this, &MainView::OnYoloCheckboxToggled);

    m_yoloCheckbox->show();
}

void MainView::OnYoloCheckboxToggled(bool checked)
{
    m_yoloEnabled = checked;

    // Unchecked olunca detections'ları temizle
    if (!checked && m_pSonarSurface) {
        QList<SonarSurface::DetectedObject> emptyList;
        m_pSonarSurface->SetDetections(emptyList);
    }

    qDebug() << "YOLO detection:" << (checked ? "ENABLED" : "DISABLED");
}

void MainView::saveImageWithAutoLabel(int height, int width, uchar* image,
                                      short* bearings, double range,
                                      const QString& directoryPath)
{
    static bool classesFileCreated = false;
    if (!classesFileCreated) {
        QString classesPath = QDir(directoryPath).absoluteFilePath("classes.txt");
        std::ofstream classesFile(classesPath.toStdString());
        for (const auto& className : m_yoloParams.classNames) {
            classesFile << className << std::endl;
        }
        classesFile.close();
        classesFileCreated = true;
        qDebug() << "Created classes.txt:" << classesPath;
    }

    int size = 640;
    cv::Mat cartesian = cv::Mat::zeros(size, size, CV_8UC1);
    int centerX = size / 2;
    int centerY = size;

    for (int b = 0; b < width; b++) {
        float bearingRad = bearings[b] * 0.01f * M_PI / 180.0f;
        for (int r = 0; r < height; r++) {
            float distance = (r / (float)height) * range;
            int x = centerX + (int)(distance * sin(bearingRad) * (size / (2.0 * range)));
            int y = centerY - (int)(distance * cos(bearingRad) * (size / range));
            if (x >= 0 && x < size && y >= 0 && y < size) {
                uchar pixel = image[r * width + b];
                cartesian.at<uchar>(y, x) = std::max(cartesian.at<uchar>(y, x), pixel);
            }
        }
    }

    cv::Mat denoised;
    cv::fastNlMeansDenoising(cartesian, denoised, 5, 7, 21);

    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(2.0, cv::Size(8, 8));
    cv::Mat enhanced;
    clahe->apply(denoised, enhanced);

    // ============ DAHA BÜYÜK WINDOW + MİNİMUM MORFOLOJİ ============

    cv::Mat localMean;
    cv::blur(enhanced, localMean, cv::Size(51, 51));  // 31 → 51 (DAHA BÜYÜK)

    cv::Mat enhancedFloat, meanFloat;
    enhanced.convertTo(enhancedFloat, CV_32F);
    localMean.convertTo(meanFloat, CV_32F);

    cv::Mat contrast = enhancedFloat - meanFloat;
    cv::Mat contrastAbs = cv::abs(contrast);

    double minVal, maxVal;
    cv::minMaxLoc(contrastAbs, &minVal, &maxVal);

    cv::Mat binary;
    cv::threshold(contrastAbs, binary, 20, 255, cv::THRESH_BINARY);  // 12 → 20
    binary.convertTo(binary, CV_8U);

    cv::Mat mask = enhanced > 10;
    cv::bitwise_and(binary, mask, binary);

    // MİNİMAL MORFOLOJİ - büyük blobs oluşmasın
    cv::Mat kernel_tiny = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::morphologyEx(binary, binary, cv::MORPH_OPEN, kernel_tiny, cv::Point(-1,-1), 1);  // Sadece OPEN

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    const int MIN_AREA = 100;
    const int MAX_AREA = 5000;
    const float MIN_ASPECT_RATIO = 0.2f;
    const float MAX_ASPECT_RATIO = 5.0f;
    const float MIN_Y_RATIO = 0.15f;
    const float MAX_Y_RATIO = 0.80f;
    const float MIN_X_RATIO = 0.15f;
    const float MAX_X_RATIO = 0.85f;

    std::vector<std::string> labels;
    int objectCount = 0;

    for (const auto& contour : contours) {
        cv::Rect bbox = cv::boundingRect(contour);

        int area = bbox.width * bbox.height;
        if (area < MIN_AREA || area > MAX_AREA) {
            if (area > 100000) qDebug() << "  Rejected HUGE blob: area=" << area;
            continue;
        }

        float aspectRatio = (float)bbox.width / (float)bbox.height;
        if (aspectRatio < MIN_ASPECT_RATIO || aspectRatio > MAX_ASPECT_RATIO) continue;

        float yCenterRatio = (bbox.y + bbox.height / 2.0f) / size;
        if (yCenterRatio < MIN_Y_RATIO || yCenterRatio > MAX_Y_RATIO) continue;

        float xCenterRatio = (bbox.x + bbox.width / 2.0f) / size;
        if (xCenterRatio < MIN_X_RATIO || xCenterRatio > MAX_X_RATIO) continue;

        double contourArea = cv::contourArea(contour);
        double solidity = contourArea / area;
        if (solidity < 0.20) continue;

        cv::Mat roiContrast = contrastAbs(bbox);
        double meanContrast = cv::mean(roiContrast)[0];
        if (meanContrast < 15) continue;  // 10 → 15

        objectCount++;

        qDebug() << ">>> OBJECT" << objectCount << "DETECTED <<<";
        qDebug() << "  Pixel coords: x=" << bbox.x << "y=" << bbox.y
                 << "w=" << bbox.width << "h=" << bbox.height;

        float centerX_norm = (bbox.x + bbox.width / 2.0f) / size;
        float centerY_norm = (bbox.y + bbox.height / 2.0f) / size;
        float width_norm = (float)bbox.width / size;
        float height_norm = (float)bbox.height / size;

        std::ostringstream ss;
        ss << std::fixed << std::setprecision(6);
        ss << "0 " << centerX_norm << " " << centerY_norm << " "
           << width_norm << " " << height_norm;

        labels.push_back(ss.str());
    }

    if (labels.empty()) {
        qDebug() << "No objects detected in this frame";
        qDebug() << "==========================================";
        return;
    }

    counter++;
    QString baseName = QString("sonar_%1").arg(counter, 5, 10, QChar('0'));

    qDebug() << "TOTAL OBJECTS:" << objectCount;

    QString imagePath = QDir(directoryPath).absoluteFilePath(baseName + ".png");
    cv::imwrite(imagePath.toStdString(), enhanced);

    QString labelPath = QDir(directoryPath).absoluteFilePath(baseName + ".txt");
    std::ofstream labelFile(labelPath.toStdString());
    for (const auto& label : labels) {
        labelFile << label << "\n";
    }
    labelFile.close();
}

