#pragma once
#include <QElapsedTimer>
#include <QPixmap>
#include <QWidget>

class LoginScene : public QWidget {
public:
    explicit LoginScene(QWidget *parent = nullptr);
    void skipIntro() { m_offset = 2.2; }
    void setLogoVisible(bool visible);
    QRect logoRect() const;

protected:
    void paintEvent(QPaintEvent *) override;

private:
    double wave(double x, int line) const;
    QPixmap m_logo;
    QElapsedTimer m_clock;
    bool m_logoVisible = true;
    double m_t = 0, m_offset = 0;
};
