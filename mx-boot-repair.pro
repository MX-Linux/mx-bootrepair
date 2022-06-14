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

#-------------------------------------------------
#
# Project created by QtCreator 2014-04-02T18:30:18
#
#-------------------------------------------------

QT       += core gui widgets
CONFIG   += c++1z

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

TARGET = mx-boot-repair
TEMPLATE = app

SOURCES += main.cpp \
    mainwindow.cpp \
    about.cpp \
    cmd.cpp

HEADERS  += \
    version.h \
    mainwindow.h \
    about.h \
    cmd.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-boot-repair_am.ts \
                translations/mx-boot-repair_ar.ts \
                translations/mx-boot-repair_bg.ts \
                translations/mx-boot-repair_ca.ts \
                translations/mx-boot-repair_cs.ts \
                translations/mx-boot-repair_da.ts \
                translations/mx-boot-repair_de.ts \
                translations/mx-boot-repair_el.ts \
                translations/mx-boot-repair_es.ts \
                translations/mx-boot-repair_et.ts \
                translations/mx-boot-repair_eu.ts \
                translations/mx-boot-repair_fa.ts \
                translations/mx-boot-repair_fi.ts \
                translations/mx-boot-repair_fil_PH.ts\
                translations/mx-boot-repair_fr.ts \
                translations/mx-boot-repair_he_IL.ts \
                translations/mx-boot-repair_hi.ts \
                translations/mx-boot-repair_hr.ts \
                translations/mx-boot-repair_hu.ts \
                translations/mx-boot-repair_id.ts \
                translations/mx-boot-repair_is.ts \
                translations/mx-boot-repair_it.ts \
                translations/mx-boot-repair_ja.ts \
                translations/mx-boot-repair_kk.ts \
                translations/mx-boot-repair_ko.ts \
                translations/mx-boot-repair_lt.ts \
                translations/mx-boot-repair_mk.ts \
                translations/mx-boot-repair_mr.ts \
                translations/mx-boot-repair_nb.ts \
                translations/mx-boot-repair_nl.ts \
                translations/mx-boot-repair_pl.ts \
                translations/mx-boot-repair_pt.ts \
                translations/mx-boot-repair_pt_BR.ts \
                translations/mx-boot-repair_ro.ts \
                translations/mx-boot-repair_ru.ts \
                translations/mx-boot-repair_sk.ts \
                translations/mx-boot-repair_sl.ts \
                translations/mx-boot-repair_sq.ts \
                translations/mx-boot-repair_sr.ts \
                translations/mx-boot-repair_sv.ts \
                translations/mx-boot-repair_tr.ts \
                translations/mx-boot-repair_uk.ts \
                translations/mx-boot-repair_zh_CN.ts \
                translations/mx-boot-repair_zh_TW.ts

RESOURCES += \
    images.qrc

