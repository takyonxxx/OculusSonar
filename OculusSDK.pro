QT += core gui network widgets multimedia multimediawidgets
QT += opengl openglwidgets

greaterThan(QT_MAJOR_VERSION, 5) {

} else {
    QT += winextras
}

CONFIG -= debug_and_release debug_and_release_target
CONFIG += c++20

DEFINES += GL_SILENCE_DEPRECATION

# ----------------------------------------------------------------------------
# Output layout: left entirely to Qt Creator. DESTDIR / OBJECTS_DIR / MOC_DIR /
# RCC_DIR / UI_DIR are intentionally NOT overridden, so everything is built into
# the build directory selected in Qt Creator (shadow build) with no extra nesting.
# ----------------------------------------------------------------------------
CONFIG(release, debug|release){
    TARGET = oculus-sdk
}

CONFIG(debug, debug|release){
    TARGET = oculus-sdk-debug
}

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
    DetectionParams.cpp \
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
    DetectionParams.h \
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


RESOURCES += \
    Media/Media.qrc \
    OculusSonar/shaders.qrc

RC_FILE = Oculus.rc

PATH_LIB = $$PWD/..

INCLUDEPATH = \
    "OculusSonar/"

win32 {

    LIBS += -lopengl32 -lglu32
    QMAKE_LFLAGS += /ignore:4099

    LIBS += \
            -luser32 \
            -lgdi32 \
            -lole32

    # ------------------------------------------------------------------------
    # OpenCV + ONNX Runtime (MSVC x64)
    # ------------------------------------------------------------------------
    OPENCV_INCLUDE = $$PATH_LIB/lib/opencv/msvc/include
    OPENCV_LIB     = $$PATH_LIB/lib/opencv/msvc/x64/vc16/lib
    OPENCV_BIN     = $$PATH_LIB/lib/opencv/msvc/x64/vc16/bin
    ONNX_INCLUDE   = $$PATH_LIB/lib/onnxruntime/include
    ONNX_LIB       = $$PATH_LIB/lib/onnxruntime/lib

    INCLUDEPATH += $$OPENCV_INCLUDE
    DEPENDPATH  += $$OPENCV_INCLUDE
    INCLUDEPATH += $$ONNX_INCLUDE
    DEPENDPATH  += $$ONNX_INCLUDE

    LIBS += -L$$ONNX_LIB -lonnxruntime

    CONFIG(release, debug|release) {
        LIBS += -L$$OPENCV_LIB -lopencv_world4100
        OPENCV_DLL     = opencv_world4100.dll
        WINDEPLOY_ARGS = --release
    }
    CONFIG(debug, debug|release) {
        LIBS += -L$$OPENCV_LIB -lopencv_world4100d
        OPENCV_DLL     = opencv_world4100d.dll
        WINDEPLOY_ARGS = --debug
    }

    # ------------------------------------------------------------------------
    # Copy runtime dependencies next to the executable after each link.
    # onnxruntime.dll is taken from the SAME package as $$ONNX_INCLUDE, so the
    # header API version and the DLL always match -- this is what fixes the
    # "API version [22] not available ... Current ORT Version is: 1.17.1" crash.
    # Verify these source paths match your actual SDK layout under $$PATH_LIB/lib
    # ------------------------------------------------------------------------
    # Deploy into Qt Creator's selected build dir ($$OUT_PWD), wherever the exe lands.
    DEST_NATIVE    = $$shell_path($$OUT_PWD)
    ONNX_DLL_SRC   = $$shell_path($$ONNX_LIB/onnxruntime.dll)
    OPENCV_DLL_SRC = $$shell_path($$OPENCV_BIN/$$OPENCV_DLL)
    MODEL_SRC      = $$shell_path($$PWD/AiTrain/sonar_model.onnx)
    TARGET_EXE     = $$shell_path($$OUT_PWD/$${TARGET}.exe)
    WINDEPLOYQT    = $$shell_path($$[QT_INSTALL_BINS]/windeployqt.exe)

    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$ONNX_DLL_SRC)   $$shell_quote($$DEST_NATIVE) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$OPENCV_DLL_SRC) $$shell_quote($$DEST_NATIVE) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $$QMAKE_COPY $$shell_quote($$MODEL_SRC)      $$shell_quote($$DEST_NATIVE) $$escape_expand(\\n\\t)

    # Deploy Qt runtime DLLs + plugins (platforms, styles, imageformats, ...)
    QMAKE_POST_LINK += $$shell_quote($$WINDEPLOYQT) $$WINDEPLOY_ARGS $$shell_quote($$TARGET_EXE) $$escape_expand(\\n\\t)

    message("[OK] OpenCV + ONNX Runtime configured for YOLO support (Windows)")
}

# (Removed hardcoded x86 Windows Kits lib paths. The MSVC2022 x64 kit provides
#  the correct ucrt/um libraries automatically; the old x86 paths could clash
#  with a 64-bit build.)

# ============================================================================
# OpenCV Configuration for Unix/Linux (YOLO)
# ============================================================================
unix {
    # pkg-config ile OpenCV'yi otomatik bul
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4

    message("[OK] OpenCV configured for YOLO support (Linux/Unix)")
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


#winget install Python.Python.3.11 --scope user
#pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124
#python -c "import torch; print('CUDA:', torch.cuda.is_available()); print('GPU:', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'None')"
#pip install onnx onnxruntime-gpu ultralytics opencv-python
#python -c "import onnx; print('ONNX:', onnx.__version__)"

#C:\Users\MSI\AppData\Local\Programs\Python\Python311\python.exe train_sonar_yolo.py

DISTFILES += \
    AiTrain/sonar_dataset.zip \
    AiTrain/train_sonar_yolo.py
