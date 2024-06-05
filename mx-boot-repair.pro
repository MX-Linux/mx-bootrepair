#/*****************************************************************************
# * mx-boot-repair.pro
# *****************************************************************************
# * Copyright (C) 2014 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This program is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * MX Boot Repair is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with MX Boot Repair.  If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += widgets
CONFIG   += debug_and_release warn_on strict_c++ c++17
CONFIG(release, debug|release) {
    DEFINES += NDEBUG
    QMAKE_CXXFLAGS += -flto=auto
    QMAKE_LFLAGS += -flto=auto
}

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

TARGET = mx-boot-repair
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    about.cpp \
    cmd.cpp

HEADERS  += \
    common.h \
    mainwindow.h \
    about.h \
    cmd.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-boot-repair_en.ts

RESOURCES += \
    images.qrc
