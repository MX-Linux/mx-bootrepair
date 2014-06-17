#/*****************************************************************************
# * mx-bootrepair.pro
# *****************************************************************************
# * Copyright (C) 2014 MX Authors
# *
# * Authors: Adrian
# *          MEPIS Community <http://forum.mepiscommunity.org>
# *
# * This program is free software; you can redistribute it and/or modify it
# * under the terms of the GNU Lesser General Public License as published by
# * the Free Software Foundation; either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# * GNU Lesser General Public License for more details.
# *
# * You should have received a copy of the GNU Lesser General Public License
# * along with this program; if not, write to the Free Software Foundation,
# * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
# *****************************************************************************/

#-------------------------------------------------
#
# Project created by QtCreator 2014-04-02T18:30:18
#
#-------------------------------------------------

QT       += core gui webkit

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = mx-bootrepair
TEMPLATE = app


SOURCES += main.cpp\
        mxbootrepair.cpp

HEADERS  += mxbootrepair.h

FORMS    += mxbootrepair.ui

TRANSLATIONS += translations/mx-bootrepair_el.ts \
                translations/mx-bootrepair_es.ts \
                translations/mx-bootrepair_fr.ts \
                translations/mx-bootrepair_ja.ts \
                translations/mx-bootrepair_nl.ts \
                translations/mx-bootrepair_sv.ts

