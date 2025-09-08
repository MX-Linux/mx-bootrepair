#pragma once

#include <QObject>

class BootRepairEngine;

class CliController : public QObject {
    Q_OBJECT
public:
    explicit CliController(QObject* parent = nullptr);
    int run();
};
