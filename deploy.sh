#!/bin/bash -i

adb push src/platforms/ubuntu/libqubuntu.so /data/ubuntu/opt/qt5/plugins/platforms
adb push src/platforms/ubuntulegacy/libqubuntulegacy.so /data/ubuntu/opt/qt5/plugins/platforms
adb push src/modules/application/libubuntuapplicationplugin.so /data/ubuntu/opt/qt5/imports/Ubuntu/Application
adb push src/modules/application/qmldir /data/ubuntu/opt/qt5/imports/Ubuntu/Application
adb push examples/qmlscene-ubuntu /data/ubuntu/opt/qt5/bin
adb push examples/Logo.qml /data/ubuntu/opt/qt5/examples
adb push examples/MovingLogo.qml /data/ubuntu/opt/qt5/examples
adb push examples/WarpingLogo.qml /data/ubuntu/opt/qt5/examples
adb push examples/Input.qml /data/ubuntu/opt/qt5/examples
adb push examples/Application.qml /data/ubuntu/opt/qt5/examples
adb push examples/logo.png /data/ubuntu/opt/qt5/examples
adb push examples/noise.png /data/ubuntu/opt/qt5/examples
