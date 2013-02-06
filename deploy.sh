#!/bin/bash -i

adb push src/platforms/ubuntu/libqubuntu.so /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/plugins/platforms
adb push src/platforms/ubuntulegacy/libqubuntulegacy.so /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/plugins/platforms
adb push src/modules/application/libubuntuapplicationplugin.so /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/imports/Ubuntu/Application
adb push src/modules/application/qmldir /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/imports/Ubuntu/Application
adb push examples/qmlscene-ubuntu /data/ubuntu/usr/bin
adb push examples/Logo.qml /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/MovingLogo.qml /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/WarpingLogo.qml /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/Input.qml /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/Application.qml /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/logo.png /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
adb push examples/noise.png /data/ubuntu/usr/lib/arm-linux-gnueabihf/qt5/examples
