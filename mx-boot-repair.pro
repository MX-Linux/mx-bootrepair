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

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-boot-repair
TEMPLATE = app


SOURCES += main.cpp \
    mainwindow.cpp

HEADERS  += \
    version.h \
    mainwindow.h

FORMS    += \
    mainwindow.ui

TRANSLATIONS += translations/mx-bootrepair_am.ts \
                translations/mx-bootrepair_ar.ts \
                translations/mx-bootrepair_bg.ts \
                translations/mx-bootrepair_ca.ts \
                translations/mx-bootrepair_cs.ts \
                translations/mx-bootrepair_da.ts \
                translations/mx-bootrepair_de.ts \
                translations/mx-bootrepair_el.ts \
                translations/mx-bootrepair_es.ts \
                translations/mx-bootrepair_et.ts \
                translations/mx-bootrepair_eu.ts \
                translations/mx-bootrepair_fa.ts \
                translations/mx-bootrepair_fi.ts \
                translations/mx-bootrepair_fr.ts \
                translations/mx-bootrepair_he_IL.ts \
                translations/mx-bootrepair_hi.ts \
                translations/mx-bootrepair_hr.ts \
                translations/mx-bootrepair_hu.ts \
                translations/mx-bootrepair_id.ts \
                translations/mx-bootrepair_is.ts \
                translations/mx-bootrepair_it.ts \
                translations/mx-bootrepair_ja.ts \
                translations/mx-bootrepair_ja_JP.ts \
                translations/mx-bootrepair_kk.ts \
                translations/mx-bootrepair_ko.ts \
                translations/mx-bootrepair_lt.ts \
                translations/mx-bootrepair_mk.ts \
                translations/mx-bootrepair_mr.ts \
                translations/mx-bootrepair_nb.ts \
                translations/mx-bootrepair_nl.ts \
                translations/mx-bootrepair_pl.ts \
                translations/mx-bootrepair_pt.ts \
                translations/mx-bootrepair_pt_BR.ts \
                translations/mx-bootrepair_ro.ts \
                translations/mx-bootrepair_ru.ts \
                translations/mx-bootrepair_sk.ts \
                translations/mx-bootrepair_sl.ts \
                translations/mx-bootrepair_sq.ts \
                translations/mx-bootrepair_sr.ts \
                translations/mx-bootrepair_sv.ts \
                translations/mx-bootrepair_tr.ts \
                translations/mx-bootrepair_uk.ts \
                translations/mx-bootrepair_zh_CN.ts \
                translations/mx-bootrepair_zh_TW.ts

RESOURCES += \
    images.qrc

