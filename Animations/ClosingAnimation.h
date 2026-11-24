#pragma once
#include <QElapsedTimer>
#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWindow>
#include <QRectF>
#include <memory>

class ClosingView : public QOpenGLWindow, protected QOpenGLFunctions {
public:
    static constexpr double Length = 1.68;

    class Scene {
    public:
        QRect area;
        QRectF window, logo;
        QImage shot, mark;
        QString title, subtitle;
        double ringTime = -1;
    };
    static bool available();
    explicit ClosingView(const Scene &scene);
    ~ClosingView() override;
    void setMessage(const QString &title, const QString &subtitle);
    bool begin();
    bool over() const { return m_time >= Length; }
    QImage frameAt(double time);

protected:
    void initializeGL() override;
    void paintGL() override;
    void mousePressEvent(QMouseEvent *) override { m_rate = 2.5; }
    void keyPressEvent(QKeyEvent *) override { m_rate = 2.5; }

private:
    static QSurfaceFormat surface();
    void step();
    std::unique_ptr<QOpenGLTexture> texture(const QImage &image);
    QImage message() const;
    Scene m_scene;
    QOpenGLShaderProgram m_program;
    QOpenGLVertexArrayObject m_vao;
    std::unique_ptr<QOpenGLTexture> m_shot, m_mark, m_text;
    QElapsedTimer m_clock;
    qint64 m_last = -1;
    double m_time = 0, m_rate = 1, m_textAt = -1;
    int m_frames = 0;
    bool m_ready = false, m_started = false, m_textDirty = false, m_frozen = false;
};
