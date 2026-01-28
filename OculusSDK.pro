QT += core gui network widgets multimedia multimediawidgets
QT += opengl openglwidgets

greaterThan(QT_MAJOR_VERSION, 5) {

} else {
    QT += winextras
}

CONFIG -= debug_and_release debug_and_release_target

DEFINES += GL_SILENCE_DEPRECATION

CONFIG(release, debug|release){
    TARGET = oculus-sdk
    DESTDIR = ./build/release
}

CONFIG(debug, debug|release){
    TARGET = oculus-sdk-debug
    DESTDIR = ./build/debug
}

OBJECTS_DIR = $$DESTDIR/.obj
MOC_DIR = $$DESTDIR/.moc
RCC_DIR = $$DESTDIR/.rcc
UI_DIR = $$DESTDIR/.ui

win32 {
    QMAKE_CXXFLAGS += /std:c++20
    DEFINES += WIN32_LEAN_AND_MEAN
}
unix {
    QMAKE_CXXFLAGS += -fvisibility=hidden

    # Use this for GCC 8 or 9
    # QMAKE_CXXFLAGS += -std=c++2a
    # Use this for GCC > 9
    QMAKE_CXXFLAGS += -std=c++20
}

SOURCES += main.cpp\
    Oculus/OsClientCtrl.cpp \
    Oculus/OsStatusRx.cpp \
    RmUtil/RmUtil.cpp \
    RmGl/RmGlOrtho.cpp \
    RmGl/RmGlSurface.cpp \
    RmGl/RmGlWidget.cpp \
    Displays/SonarSurface.cpp \
    OculusSonar/MainView.cpp \
    OculusSonar/OnlineCtrls.cpp \
    OculusSonar/EnvCtrls.cpp \
    OculusSonar/ReviewCtrls.cpp \
    OculusSonar/SettingsCtrls.cpp \
    OculusSonar/CtrlWidget.cpp \
    RmGl/PalWidget.cpp \
    RmUtil/RmLogger.cpp \
    RmUtil/RmPlayer.cpp \
    OculusSonar/SettingsForm.cpp \
    OculusSonar/ConnectForm.cpp \
    OculusSonar/ToolsCtrls.cpp \
    OculusSonar/ModeCtrls.cpp \
    OculusSonar/OptionsCtrls.cpp \
    OculusSonar/DeviceForm.cpp \
    OculusSonar/TitleCtrls.cpp \
    OculusSonar/AppCtrls.cpp \
    OculusSonar/InfoCtrls.cpp \
    OculusSonar/CursorCtrls.cpp \
    OculusSonar/InfoForm.cpp \
    Controls/RangeSlider.cpp \
    OculusSonar/HelpForm.cpp \
    inference.cpp

HEADERS  += \
    Oculus/Oculus.h \
    Oculus/OsClientCtrl.h \
    Oculus/OsStatusRx.h \
    RmUtil/RmUtil.h \
    RmGl/RmGlOrtho.h \
    RmGl/RmGlSurface.h \
    RmGl/RmGlWidget.h \
    Displays/SonarSurface.h \
    OculusSonar/MainView.h \
    OnlineCtrls.h \
    OculusSonar/EnvCtrls.h \
    OculusSonar/ReviewCtrls.h \
    OculusSonar/SettingsCtrls.h \
    OculusSonar/CtrlWidget.h \
    RmGl/PalWidget.h \
    RmUtil/RmLogger.h \
    RmUtil/RmPlayer.h \
    OculusSonar/SettingsForm.h \
    OculusSonar/ConnectForm.h \
    Oculus/OssDataWrapper.h \
    Oculus/DataWrapper.h \
    OculusSonar/ToolsCtrls.h \
    OculusSonar/ModeCtrls.h \
    OculusSonar/OptionsCtrls.h \
    OculusSonar/DeviceForm.h \
    OculusSonar/TitleCtrls.h \
    OculusSonar/AppCtrls.h \
    OculusSonar/InfoCtrls.h \
    OculusSonar/CursorCtrls.h \
    OculusSonar/OnlineCtrls.h \
    OculusSonar/InfoForm.h \
    Controls/RangeSlider.h \
    Controls/RangeSlider_p.h \
    OculusSonar/HelpForm.h \
    inference.h

FORMS    += \
    OculusSonar/OnlineCtrls.ui \
    OculusSonar/EnvCtrls.ui \
    OculusSonar/ReviewCtrls.ui \
    OculusSonar/SettingsCtrls.ui \
    OculusSonar/SettingsForm.ui \
    OculusSonar/ConnectForm.ui \
    OculusSonar/ToolsCtrls.ui \
    OculusSonar/ModeCtrls.ui \
    OculusSonar/OptionsCtrls.ui \
    OculusSonar/DeviceForm.ui \
    OculusSonar/TitleCtrls.ui \
    OculusSonar/AppCtrls.ui \
    OculusSonar/InfoCtrls.ui \
    OculusSonar/CursorCtrls.ui \
    OculusSonar/InfoForm.ui \
    OculusSonar/HelpForm.ui

CONFIG += c++11


RESOURCES += \
    Media/Media.qrc \
    OculusSonar/shaders.qrc

RC_FILE = Oculus.rc

PATH_LIB = $$PWD/..

INCLUDEPATH = \
    "OculusSonar/" \
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.10240.0/ucrt" \
    "C:/Program Files (x86)/Windows Kits/10/Include/10.0.10586.0/um"

win32 {    

    LIBS += -lopengl32 -lglu32
    QMAKE_LFLAGS += /ignore:4099

    LIBS += \
            -luser32 \
            -lgdi32 \
            -lole32

    # OpenCV Configuration for MSVC
    OPENCV_INCLUDE = $$PATH_LIB/lib/opencv/msvc/include
    OPENCV_LIB = $$PATH_LIB/lib/opencv/msvc/x64/vc16/lib
    ONNX_INCLUDE = $$PATH_LIB/lib/onnxruntime/include
    ONNX_LIB = $$PATH_LIB/lib/onnxruntime/lib
    INCLUDEPATH += $$OPENCV_INCLUDE
    DEPENDPATH += $$OPENCV_INCLUDE
    INCLUDEPATH += $$ONNX_INCLUDE
    DEPENDPATH += $$ONNX_INCLUDE

    LIBS += -L$$ONNX_LIB -lonnxruntime

    # OpenCV DLL ve ONNX Runtime DLL'i manuel olarak build klasörüne kopyalayın
    # onnxruntime.dll -> build/release/
    # opencv_world4120.dll -> build/release/

    # OpenCV world library (tek DLL)
    CONFIG(release, debug|release) {
        LIBS += -L$$OPENCV_LIB -lopencv_world4100
    }

    CONFIG(debug, debug|release) {
        LIBS += -L$$OPENCV_LIB -lopencv_world4100d
    }

    message("✅ OpenCV configured for YOLO support")
    message("   OpenCV path: $$OPENCV_DIR")
}

# Will need to update this for dedicated x64 builds of the software
LIBS += \
    -L"C:/Program Files (x86)/Windows Kits/10/Lib/10.0.10240.0/ucrt/x86" \
    -L"C:/Program Files (x86)/Windows Kits/10/Lib/10.0.10586.0/um/x86" \
    -luser32 \
    -lgdi32 \
    -lole32

# ============================================================================
# OpenCV Configuration for Unix/Linux (YOLO)
# ============================================================================
unix {
    # pkg-config ile OpenCV'yi otomatik bul
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4

    message("✅ OpenCV configured for YOLO support (Linux/Unix)")
    message("   Using pkg-config opencv4")
}

# # 1. Eski container'ları durdur
# docker-compose down

# # 2. Temizle
# docker system prune -f

# # 3. CVAT'i klon et (yoksa)
# git clone https://github.com/opencv/cvat
# cd cvat

# # 4. Doğru komutla başlat
# docker-compose up -d

# # 5. Kontrol
# docker-compose ps
#pip uninstall torch torchvision
#pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
#pip install ultralytics
#C:\Users\MSI\AppData\Local\Programs\Python\Python311\python.exe train_sonar_yolo.py
