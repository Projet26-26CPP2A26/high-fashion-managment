#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class TaskModel {
public:
    QString title, details;
    QStringList steps;
    int minutes;
};

namespace TaskCatalog {
inline QVector<TaskModel> forSpecialty(const QString &s)
{
    if (s == QStringLiteral("Couture haute précision")) return {
        {QStringLiteral("Assembler le corsage"), QStringLiteral("Corsage baleiné de la robe de soirée, tolérance 1 mm."), {QStringLiteral("Épingler et bâtir les panneaux"), QStringLiteral("Piquer les coutures princesse"), QStringLiteral("Poser les baleines"), QStringLiteral("Contrôler la symétrie")}, 90},
        {QStringLiteral("Poser une fermeture invisible"), QStringLiteral("Fermeture milieu dos, aucune surpiqûre visible."), {QStringLiteral("Entoiler les bords"), QStringLiteral("Piquer au pied spécial"), QStringLiteral("Tester 10 ouvertures")}, 40},
        {QStringLiteral("Monter les manches"), QStringLiteral("Manches montées avec embu réparti."), {QStringLiteral("Répartir l'embu"), QStringLiteral("Bâtir et essayer"), QStringLiteral("Piquer et surfiler")}, 60},
        {QStringLiteral("Doubler la jupe"), QStringLiteral("Doublure en soie, ourlet libre."), {QStringLiteral("Couper la doublure"), QStringLiteral("Assembler à la taille"), QStringLiteral("Repasser et vérifier l'aplomb")}, 55}};
    if (s == QStringLiteral("Broderie main")) return {
        {QStringLiteral("Broder le col"), QStringLiteral("Motif floral au fil de soie."), {QStringLiteral("Reporter le dessin"), QStringLiteral("Broder les contours"), QStringLiteral("Remplir au passé plat")}, 80},
        {QStringLiteral("Poser perles et sequins"), QStringLiteral("Semis de perles sur la taille."), {QStringLiteral("Trier les perles"), QStringLiteral("Fixer au point arrière"), QStringLiteral("Tester la tenue")}, 70}};
    if (s == QStringLiteral("Patronage")) return {
        {QStringLiteral("Tracer le patron de base"), QStringLiteral("Patron aux mesures de la cliente."), {QStringLiteral("Vérifier les mesures"), QStringLiteral("Tracer devant et dos"), QStringLiteral("Ajouter les crans")}, 75},
        {QStringLiteral("Grader trois tailles"), QStringLiteral("Gradation 36 à 40."), {QStringLiteral("Définir les évolutions"), QStringLiteral("Tracer le nid de gradation"), QStringLiteral("Contrôler les raccords")}, 60}};
    if (s == QStringLiteral("Drapé et moulage")) return {
        {QStringLiteral("Mouler le buste"), QStringLiteral("Toile sur mannequin."), {QStringLiteral("Préparer la toile"), QStringLiteral("Draper et épingler"), QStringLiteral("Relever les lignes")}, 85},
        {QStringLiteral("Draper la jupe asymétrique"), QStringLiteral("Drapé en biais fixé à la taille."), {QStringLiteral("Trouver le biais"), QStringLiteral("Former les plis"), QStringLiteral("Fixer et photographier")}, 70}};
    if (s == QStringLiteral("Finitions")) return {
        {QStringLiteral("Ourlet roulotté main"), QStringLiteral("Ourlet de 3 mm sur mousseline."), {QStringLiteral("Égaliser le bas"), QStringLiteral("Rouler et coudre"), QStringLiteral("Repasser sans lustrer")}, 50},
        {QStringLiteral("Poser les boutons"), QStringLiteral("Boutons bijoux et brides."), {QStringLiteral("Marquer les positions"), QStringLiteral("Coudre avec tige"), QStringLiteral("Vérifier l'alignement")}, 35}};
    return {
        {QStringLiteral("Contrôler la robe terminée"), QStringLiteral("Contrôle avant livraison."), {QStringLiteral("Vérifier les coutures"), QStringLiteral("Mesurer les tolérances"), QStringLiteral("Remplir la fiche qualité")}, 30},
        {QStringLiteral("Audit d'une série"), QStringLiteral("Échantillon de 5 pièces."), {QStringLiteral("Prélever l'échantillon"), QStringLiteral("Noter les défauts"), QStringLiteral("Conclure")}, 45}};
}
}
