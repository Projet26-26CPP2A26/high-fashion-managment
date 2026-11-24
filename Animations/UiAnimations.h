#pragma once
#include "AnimationSpeed.h"

#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVariantAnimation>
#include <QVector>
#include <QWidget>
#include <algorithm>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>
#endif

namespace Fx {
int frameInterval(const QWidget *w);
void steady(QWidget *w);
void openWindow(QWidget *w);
void shake(QWidget *w);
void switchPage(QStackedWidget *stack, int index);
void switchPage(QTabWidget *stack, int index);

void countTo(QLabel *label, int value);
void toast(QWidget *window, const QString &text);
void navIndicator(QWidget *sidebar, const QVector<QPushButton *> &buttons);
void glowButtons(QWidget *);
void fullScreenKey(QWidget *window);
void reveal(QWidget *w, int delay, int duration = Motion::Slow);
}
