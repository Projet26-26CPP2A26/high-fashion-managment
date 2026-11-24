#pragma once
#include <QPointF>
#include <QRectF>

class QPainter;

// Fils de soie en 3D : ils traversent l'écran puis viennent s'enrouler autour du logo.
class LinesAnimation {
public:
    struct Scene {
        QRectF area;
        QPointF center;
        double radius = 100;   // rayon de l'anneau
        double time = 0;       // secondes
        double drawn = 1;      // 0..1 : partie tissée
        double gather = 0;     // 0 = fils libres, 1 = anneau
        double alpha = 1;
    };
    static void paint(QPainter &p, const Scene &s, bool front);
};
