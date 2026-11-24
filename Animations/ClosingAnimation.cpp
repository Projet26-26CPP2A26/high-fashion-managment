// Module commun de l'équipe AUREA : animation de fermeture reprise telle quelle du projet AUREA_Employes
// (Animations/ClosingAnimation), écrite par le responsable du module Employés.
#include "ClosingAnimation.h"
#include <QEventLoop>
#include <QFont>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace {
const char *Vertex = R"(#version 330 core
out vec2 uv;
void main()
{
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = vec2(p.x, 1.0 - p.y);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";

const char *Fragment = R"(#version 330 core
in vec2 uv;
out vec4 color;
uniform sampler2D shot;
uniform sampler2D mark;
uniform sampler2D words;
uniform vec2 size;
uniform vec4 frame;
uniform vec3 logo;
uniform vec2 wordSize;
uniform float big;
uniform float time;
uniform float wordAt;
uniform float ringTime;

const int TAPS = 8;
const vec3 GOLD = vec3(0.886, 0.812, 0.624);

float sat(float v) { return clamp(v, 0.0, 1.0); }
float smoother(float v) { v = sat(v); return v * v * v * (v * (v * 6.0 - 15.0) + 10.0); }
float outCubic(float v) { v = 1.0 - sat(v); return 1.0 - v * v * v; }
float span(float from, float to) { return sat((time - from) / (to - from)); }
float noise(vec2 p) { return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715)))); }
vec4 over(vec4 back, vec4 front) { return front + back * (1.0 - front.a); }

float inhale(float t)
{
    float v = sat((t - 0.50) / 0.84);
    float w = smoother(v);
    return mix(v * v * v, w, 0.62);
}

vec2 spin(vec2 p, float a)
{
    float c = cos(a), s = sin(a);
    return vec2(c * p.x - s * p.y, s * p.x + c * p.y);
}

vec4 ring(vec2 d, float radius, float from, float span_, float width, float alpha)
{
    float a = degrees(atan(-d.y, d.x));
    float along = mod(a - from, 360.0);
    float gap = abs(length(d) - radius);
    if (along > span_) {
        float s = radians(from), e = radians(from + span_);
        gap = min(length(d - radius * vec2(cos(s), -sin(s))), length(d - radius * vec2(cos(e), -sin(e))));
    }
    float line = sat(width * 0.5 + 0.5 - gap) * alpha;
    return vec4(GOLD * line, line);
}

void main()
{
    vec2 p = uv * size;
    float grain = noise(gl_FragCoord.xy);
    vec2 half_ = frame.zw * 0.5;
    vec2 middle = frame.xy + half_;
    vec2 eye = middle - vec2(0.0, 30.0);
    vec2 d = p - eye;
    float dist = length(d);

    float settle = smoother(span(0.0, 0.44));
    float pull = inhale(time);
    float leave = smoother(span(1.36, 1.66));

    vec4 result = vec4(0.0);
    vec4 page = vec4(0.0);
    int taps = pull > 0.0 && pull < 1.0 ? TAPS : 1;
    for (int i = 0; i < taps; ++i) {
        float k = taps == 1 ? 0.0 : (float(i) + grain) / float(taps) - 0.5;
        float amount = inhale(time + k * 0.011);
        float near = 1.0 - amount;
        vec2 center = mix(middle, eye, amount);
        vec2 box = max(half_ * vec2(near * mix(1.0, half_.y / half_.x, smoother(amount)), near), vec2(0.5));
        float low = min(box.x, box.y);
        float corner = mix(9.0 * settle, low, smoother(amount * 1.15));
        corner = min(corner, low);
        vec2 g = abs(p - center) - box + corner;
        float edge = length(max(g, 0.0)) + min(max(g.x, g.y), 0.0) - corner;
        float body = sat(0.5 - edge);
        if (body > 0.0) {
            vec2 n = (p - center) / box;
            float m = clamp(max(abs(n.x), abs(n.y)), 0.02, 1.0);
            float draw = pow(m, -0.42 * amount);
            vec2 q = middle + spin(n * half_ * draw, 0.22 * amount * (1.0 - m) * (1.0 - m));
            vec2 st = clamp((q - frame.xy) / frame.zw, 0.0005, 0.9995);
            float lod = log2(max(half_.y / box.y, 1.0)) * 0.8 + 0.9 * sat(amount * 4.0);
            page += vec4(textureLod(shot, st, lod).rgb, 1.0) * body;
        }
    }
    page /= float(taps);
    page.rgb *= 1.0 - 0.60 * settle;
    page.rgb = mix(page.rgb, vec3(0.074, 0.070, 0.060) * page.a, settle * 0.85 * smoothstep(0.0, 0.8, pull) + 0.75 * settle * exp(-dist * dist / (big * big * 0.9)) * (1.0 - smoothstep(0.0, 0.8, pull)));
    page.rgb += GOLD * 0.055 * settle * exp(-dist * dist / (big * big * 2.2)) * page.a;
    result = page;

    float breath = sin(3.14159265 * smoother(span(0.50, 1.46)));
    float glow = (0.20 * settle + 0.30 * breath) * (1.0 - leave) * exp(-dist * dist / (big * big * 0.55));
    result = over(result, vec4(GOLD * glow * 0.40, glow * 0.40));

    vec2 home = mix(logo.xy, eye, settle);
    float wide = mix(logo.z, big, settle) * (1.0 + 0.10 * breath) * (1.0 - 0.10 * leave);
    if (ringTime >= 0.0) {
        float zoom = mix(1.0, big / logo.z, settle);
        float drawn = outCubic((ringTime - 0.15) / 1.3);
        float lift = 1.0 + 0.07 * sin(3.14159265 * span(0.10, 0.66));
        float first = smoother(span(0.58, 1.06)), second = smoother(span(0.68, 1.20));
        float turn = ringTime + time;
        vec2 away = p - home;
        float r1 = 82.0 * zoom * lift * mix(1.0, 0.10, first), r2 = 92.0 * zoom * lift * mix(1.0, 0.10, second);
        float a1 = mix(0.59, 0.85, settle) * (1.0 - smoothstep(0.62, 0.96, first));
        float a2 = mix(0.27, 0.62, settle) * (1.0 - smoothstep(0.62, 0.96, second));
        float l1 = 300.0 * drawn * mix(1.0, 0.55, first), l2 = 220.0 * drawn * mix(1.0, 0.55, second);
        result = over(result, ring(away, r2, 200.0 - turn * 9.0 + 150.0 * second * second, l2, 1.0 + 0.5 * settle, a2));
        result = over(result, ring(away, r1, 90.0 + turn * 12.0 - l1 - 170.0 * first * first, l1, 1.4 + 0.4 * settle, a1));
    }
    vec2 st = (p - home) / wide + 0.5;
    if (st.x > 0.0 && st.x < 1.0 && st.y > 0.0 && st.y < 1.0)
        result = over(result, texture(mark, st, -0.3) * (1.0 - leave));

    if (wordAt >= 0.0) {
        float in_ = outCubic((time - wordAt) / 0.34);
        float out_ = smoother(span(0.62, 0.98));
        vec2 base = vec2(eye.x, eye.y + big * 0.5 + (ringTime >= 0.0 ? 92.0 : 62.0) + 10.0 * (1.0 - in_));
        vec2 wt = (p - mix(base, eye, out_ * 0.6)) / (wordSize * (1.0 - 0.5 * out_)) + 0.5;
        if (wt.x > 0.0 && wt.x < 1.0 && wt.y > 0.0 && wt.y < 1.0)
            result = over(result, texture(words, wt) * in_ * (1.0 - out_));
    }

    result.rgb += (grain - 0.5) / 255.0 * result.a;
    result = clamp(result, 0.0, 1.0);
    color = vec4(min(result.rgb, vec3(result.a)), result.a);
}
)";
}

QSurfaceFormat ClosingView::surface()
{
    QSurfaceFormat format;
    format.setVersion(3, 3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setAlphaBufferSize(8);
    format.setDepthBufferSize(0);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    format.setSwapInterval(1);
    return format;
}

bool ClosingView::available()
{
    if (QGuiApplication::platformName() != "windows") return false;
    QOpenGLContext context;
    context.setFormat(surface());
    return context.create() && context.format().version() >= qMakePair(3, 3) && context.format().alphaBufferSize() >= 8;
}

ClosingView::ClosingView(const Scene &scene) : m_scene(scene)
{
    setFormat(surface());
    setFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setGeometry(scene.area);
    QObject::connect(this, &QOpenGLWindow::frameSwapped, this, [this] { ++m_frames; });
}

ClosingView::~ClosingView()
{
    if (!context()) return;
    makeCurrent();
    m_shot.reset();
    m_mark.reset();
    m_text.reset();
    m_vao.destroy();
    m_program.removeAllShaders();
    doneCurrent();
}

std::unique_ptr<QOpenGLTexture> ClosingView::texture(const QImage &image)
{
    auto t = std::make_unique<QOpenGLTexture>(image.convertToFormat(QImage::Format_RGBA8888_Premultiplied), QOpenGLTexture::GenerateMipMaps);
    t->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
    t->setWrapMode(QOpenGLTexture::ClampToEdge);
    return t;
}

QImage ClosingView::message() const
{
    const qreal ratio = devicePixelRatio();
    QImage image(QSize(900, 110) * ratio, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(ratio);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont title("Inter");
    title.setPointSizeF(21);
    title.setWeight(QFont::Bold);
    title.setStyleStrategy(QFont::NoSubpixelAntialias);
    p.setFont(title);
    p.setPen(QColor("#fff9ea"));
    p.drawText(QRectF(0, 16, 900, 42), Qt::AlignCenter, m_scene.title);
    QFont small("Inter");
    small.setPointSizeF(10.5);
    small.setStyleStrategy(QFont::NoSubpixelAntialias);
    p.setFont(small);
    p.setPen(QColor("#cdbd92"));
    p.drawText(QRectF(0, 60, 900, 30), Qt::AlignCenter, m_scene.subtitle);
    return image;
}

void ClosingView::setMessage(const QString &title, const QString &subtitle)
{
    m_scene.title = title;
    m_scene.subtitle = subtitle;
    m_textAt = std::max(0.14, m_time);
    m_textDirty = true;
}

bool ClosingView::begin()
{
    show();
    raise();
    QEventLoop loop;
    QTimer check;
    QElapsedTimer limit;
    limit.start();
    QObject::connect(&check, &QTimer::timeout, &loop, [&] { if (m_frames >= 2 || limit.elapsed() > 900) loop.quit(); else update(); });
    check.start(2);
    loop.exec(QEventLoop::ExcludeUserInputEvents);
    if (!m_ready || !m_frames) {
        hide();
        return false;
    }
    m_started = true;
    QMetaObject::invokeMethod(this, [this] { step(); }, Qt::QueuedConnection);
    return true;
}

void ClosingView::step()
{
    if (!isExposed()) {
        m_time = Length;
        return;
    }
    makeCurrent();
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, int(width() * devicePixelRatio()), int(height() * devicePixelRatio()));
    paintGL();
    context()->swapBuffers(this);
    emit frameSwapped();
    if (!over()) QMetaObject::invokeMethod(this, [this] { step(); }, Qt::QueuedConnection);
}

void ClosingView::initializeGL()
{
    initializeOpenGLFunctions();
    m_ready = m_program.addShaderFromSourceCode(QOpenGLShader::Vertex, Vertex) && m_program.addShaderFromSourceCode(QOpenGLShader::Fragment, Fragment) && m_program.link() && m_vao.create();
    if (!m_ready) return;
    m_shot = texture(m_scene.shot);
    m_mark = texture(m_scene.mark);
    m_text = texture(message());
}

void ClosingView::paintGL()
{
    glDisable(GL_BLEND);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    if (!m_ready) return;
    if (m_started && !m_frozen) {
        if (!m_clock.isValid()) m_clock.start();
        const qint64 now = m_clock.nsecsElapsed();
        if (m_last >= 0) m_time += std::min((now - m_last) / 1e9, 0.030) * m_rate;
        m_last = now;
    }
    if (m_textDirty) {
        m_text = texture(message());
        m_textDirty = false;
    }
    const QRectF w = m_scene.window.translated(-QPointF(m_scene.area.topLeft())), l = m_scene.logo.translated(-QPointF(m_scene.area.topLeft()));
    m_vao.bind();
    m_program.bind();
    m_shot->bind(0);
    m_mark->bind(1);
    m_text->bind(2);
    m_program.setUniformValue("shot", 0);
    m_program.setUniformValue("mark", 1);
    m_program.setUniformValue("words", 2);
    m_program.setUniformValue("size", QVector2D(width(), height()));
    m_program.setUniformValue("frame", QVector4D(w.x(), w.y(), w.width(), w.height()));
    m_program.setUniformValue("logo", QVector3D(l.center().x(), l.center().y(), l.width()));
    m_program.setUniformValue("wordSize", QVector2D(900, 110));
    m_program.setUniformValue("big", GLfloat(std::min({150.0, w.height() * 0.28, w.width() * 0.28})));
    m_program.setUniformValue("time", GLfloat(m_time));
    m_program.setUniformValue("wordAt", GLfloat(m_textAt));
    m_program.setUniformValue("ringTime", GLfloat(m_scene.ringTime));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    m_program.release();
    m_vao.release();
}

QImage ClosingView::frameAt(double time)
{
    m_frozen = true;
    m_time = time;
    makeCurrent();
    QOpenGLFramebufferObject target(size() * devicePixelRatio());
    target.bind();
    glViewport(0, 0, target.width(), target.height());
    paintGL();
    const QImage image = target.toImage();
    target.release();
    doneCurrent();
    return image;
}
