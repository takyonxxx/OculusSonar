#pragma once

#include <QMainWindow>
#include <QSlider>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QCheckBox>

#include "ModeCtrls.h"
#include "OptionsCtrls.h"
#include "OnlineCtrls.h"
#include "ReviewCtrls.h"
#include "Settings.h"
#include "ToolsCtrls.h"
#include "InfoCtrls.h"
#include "CursorCtrls.h"
#include "TitleCtrls.h"
#include "SettingsForm.h"
#include "ConnectForm.h"
#include "DeviceForm.h"
#include "InfoForm.h"
#include "HelpForm.h"

#include "../RmGl/RmGlWidget.h"
#include "../RmGl/PalWidget.h"
#include "../Oculus/OsStatusRx.h"
#include "../Oculus/OsClientCtrl.h"
#include "../Oculus/OssDataWrapper.h"
#include "../RmUtil/RmLogger.h"
#include "../RmUtil/RmPlayer.h"

// ============================================================================
// YOLO Detection (ONNX Runtime) - Direkt MainView'de
// ============================================================================
#include "inference.h"
#include <opencv2/opencv.hpp>

// forward definition for the sonar surface
class SonarSurface;

// Enumerate different display modes
enum eDisplayMode : int
{
    offline,
    online,
    review
};

#pragma pack(push, 1)
// ----------------------------------------------------------------------------
// Sonar data header with bearing table
class ApSonarDataHeader
{
public:
    float   range;
    float   gain;
    float   frequency;
    quint16 nRngs;
    quint16 nBrgs;
    qint16  pBrgs[1024];
    quint16 size;
};
#pragma pack(pop)

// ----------------------------------------------------------------------------
struct SonarType {
    OculusPartNumberType partNumber;
    bool highFreq;
    double highFreqRange;
    bool lowFreq;
    double lowFreqRange;
};

// ----------------------------------------------------------------------------
// MainView - this is the main oculus sonar window
class MainView : public QMainWindow
{
    Q_OBJECT

public:
    static const uint32_t NAVIGATION_RANGE_COUNT = 16;
    static const uint32_t INSPECTION_RANGE_COUNT = 11;

    static double NAVIGATION_RANGES[NAVIGATION_RANGE_COUNT];
    static double INSPECTION_RANGES[INSPECTION_RANGE_COUNT];

public:
    explicit MainView(QWidget *parent = 0);
    ~MainView();

    void resizeEvent(QResizeEvent* event)  Q_DECL_OVERRIDE;
    void keyPressEvent(QKeyEvent* event) Q_DECL_OVERRIDE;

    void LayoutCtrls();
    void SetDisplayMode(eDisplayMode displayMode);
    void ReadSettings();
    void WriteSettings();
    bool Snapshot();
    void FlipX(bool flip);
    void FlipY(bool flip);
    void StartLog();
    void StopLog();
    void SetTheme(QString theme);
    void SetMeasureMode(bool enable);
    void RebuildSonarList();
    void SetSonarInfo(OculusStatusMsg msg);
    void MouseLeaveFan();
    void MouseEnterFan();
    void SetTitleLogFile(QString string);
    void ShowLogEditor();

    // Controls
    TitleCtrls    m_titleCtrls;
    ModeCtrls     m_modeCtrls;
    OptionsCtrls  m_optionsCtrls;
    OnlineCtrls   m_onlineCtrls;
    ReviewCtrls   m_reviewCtrls;
    ToolsCtrls    m_toolsCtrls;
    PalWidget     m_palSelect;
    InfoCtrls     m_infoCtrls;
    CursorCtrls   m_cursorCtrls;
    InfoForm      m_infoForm;
    HelpForm      m_helpForm;
    SettingsForm  m_settings;
    ConnectForm   m_connectForm;
    DeviceForm    m_deviceForm;

    // Data
    RmGlWidget    m_fanDisplay;
    SonarSurface* m_pSonarSurface;
    OsClientCtrl  m_oculusClient;
    OsStatusRx    m_oculusStatus;
    RmLogger      m_logger;
    RmPlayer      m_player;
    QLabel        m_info;

    QString       m_themeName;
    bool          m_measureMode;
    bool          m_reconnect;
    bool          m_timeout;

    OculusVersionInfo    m_versionInfo;
    OculusPartNumberType m_partNumber;

    QMap<uint32_t, OculusStatusMsg> m_sonarList;
    QMap<uint32_t, QDateTime>       m_statusMessageTimes;
    QMutex                          m_statusMessageTimesLock;
    QMutex                          m_sonarLock;

    eDisplayMode  m_displayMode;
    QString       m_ipFromStatus;
    QString       m_ipDevStatus;
    uint32_t      m_bootVersion;
    uint32_t      m_armVersion;

    // Replay
    QString       m_replayFile;
    quint64*      m_pEntries;
    int           m_nEntries;
    int           m_index;
    int           m_indexLower;
    int           m_indexUpper;
    OsBufferEntry m_entry;
    QTimer        m_replay;
    quint64*      m_pViewInfoEntries;
    int           m_nViewInfoEntries;
    ApSonarDataHeader m_sonarReplay;
    bool              m_useRawSonar;
    QDateTime         m_payloadDateTime;
    int               m_playSpeed;
    int               counter;
    OculusInfo*       m_pSonarInfo;

    void UpdateLogFileName();
    void AbortReconnect();
    void UpdateSonarInfo(OculusPartNumberType pn);

signals:
    void NewSonarDetected();
    void SonarClientStateChanged();

public slots:
    void NewStatusMsg(OculusStatusMsg osm, quint16 valid, quint16 invalid);
    void NewReturnFire(OsBufferEntry* pEntry);   
    void analyzeImage(int height, int width, uchar* image,
                                short* bearings, double range,
                                const QString& directoryPath);
    QColor applySonarColorMap(float intensity);
    void NewUserConfig(UserConfig config);
    void FireSonar();
    void SpawnOculusWebView();
    void PalSelected(int pal);
    void ConnectionFailed(QString error);
    void OnNewPayload(unsigned short type, unsigned short version, double time, unsigned payloadSize, quint8* pPayload);
    void ReviewEntryChanged(int enrr);
    void ReviewLowerEntryChanged(int entry);
    void ReviewUpperEntryChanged(int entry);
    void StartReplay();
    void StopReplay();
    void PlayNext();
    void StyleChanged(QString name);
    void MouseInfo(float dist, float angle, float x, float y);
    void MonitorAvailableSonars();
    void SocketTimeout();
    void SocketReconnecting();
    void SocketDisconnected();

private slots:
    void OnSendPingClicked();
    void OnExitHexViewer();
    void ToggleHexViewer();
    void OnClearHexViewer();
    void OnYoloCheckboxToggled(bool checked);

private:
    void CreateHexViewer();
    void CreateYoloCheckbox();
    QString FormatHexData(OsBufferEntry* pEntry);

    QTextBrowser* m_hexViewer;
    QWidget*      m_hexContainer;
    bool          m_showHexViewer;
    int           m_maxHexBytes;

    QLabel* m_pingIdValue;
    QLabel* m_packetSizeValue;
    QLabel* m_rangesValue;
    QLabel* m_beamsValue;
    QLabel* m_frequencyValue;
    QLabel* m_temperatureValue;
    QLabel* m_pressureValue;
    QLabel* m_sosValue;
    QString desktopPath;
    QString sonarImageDir;

    // YOLO Detection UI
    QCheckBox* m_yoloCheckbox;

    // ============================================================================
    // YOLO Detection (Direkt MainView'de)
    // ============================================================================
    YOLO_V8*       m_yoloDetector;
    DL_INIT_PARAM  m_yoloParams;
    bool           m_yoloEnabled;
    int            m_renderCounter;
};
