#include "client.h"

#include <QDate>
#include <QDebug>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

// ---------------------------------------------------------------- outils internes
static QString s_erreur;

// Traduit les erreurs Oracle courantes en message lisible
static QString traduire(const QSqlError &e)
{
    const QString t = e.text();
    if (t.contains("ORA-00001")) return QStringLiteral("Cette valeur existe déjà (mail ou identifiant déjà utilisé).");
    if (t.contains("ORA-01400")) return QStringLiteral("Un champ obligatoire est vide.");
    if (t.contains("ORA-12899")) return QStringLiteral("Une valeur est trop longue pour sa colonne.");
    if (t.contains("ORA-02290")) return QStringLiteral("Une valeur ne respecte pas les contraintes de la base.");
    return t;
}

// Exécute une requête préparée et mémorise l'erreur éventuelle
static bool executer(QSqlQuery &q)
{
    if (q.exec())
        return true;
    s_erreur = traduire(q.lastError());
    qDebug() << "Erreur SQL :" << q.lastError().text() << "|" << q.lastQuery();
    return false;
}

QString Client::derniereErreur() { return s_erreur; }

// ---------------------------------------------------------------- constructeur
Client::Client(int id, const QString &nom, const QString &mail, const QString &tel,
               const QString &naissance, const QString &mensuration, const QString &categorie)
    : id(id), nom(nom), mail(mail), tel(tel), naissance(naissance),
      mensuration(mensuration), categorie(categorie)
{
}

// ---------------------------------------------------------------- contrôle de saisie
bool Client::valider(QString &erreur) const
{
    static const QRegularExpression rxMail(R"(^[\w.+-]+@[\w-]+\.[\w.-]+$)");
    static const QRegularExpression rxTel(R"(^\+?\d{8,12}$)");

    if (nom.trimmed().isEmpty())      { erreur = "Le nom est obligatoire."; return false; }
    if (nom.size() > 100)             { erreur = "Le nom est trop long (100 caractères maximum)."; return false; }
    if (!rxMail.match(mail).hasMatch()) { erreur = "Adresse mail invalide."; return false; }
    if (!rxTel.match(tel).hasMatch())   { erreur = "Téléphone invalide (8 à 12 chiffres)."; return false; }

    const QDate d = QDate::fromString(naissance, Qt::ISODate);
    if (!d.isValid() || d >= QDate::currentDate()) {
        erreur = "La date de naissance doit être une date valide dans le passé.";
        return false;
    }
    if (mensuration.size() > 100)     { erreur = "Le résumé des mensurations est trop long."; return false; }
    if (categorie != "Standard" && categorie != "VIP") {
        erreur = "Catégorie invalide.";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- CREATE
bool Client::ajouter()
{
    QSqlQuery q;
    // l'id est généré automatiquement (séquence + trigger Oracle)
    q.prepare("INSERT INTO clients (nom, mail, tel, naissance, mensuration, categorie) "
              "VALUES (:nom, :mail, :tel, :naissance, :mensuration, :categorie)");
    q.bindValue(":nom", nom);
    q.bindValue(":mail", mail);
    q.bindValue(":tel", tel);
    q.bindValue(":naissance", naissance);
    q.bindValue(":mensuration", mensuration);
    q.bindValue(":categorie", categorie);
    return executer(q);
}

// ---------------------------------------------------------------- READ
QList<Client> Client::afficher(const QString &recherche, const QString &categorie, int tri)
{
    QList<Client> liste;

    QString sql = "SELECT id, nom, mail, tel, naissance, mensuration, categorie, score FROM clients "
                  "WHERE (UPPER(nom) LIKE UPPER(:s) OR UPPER(mail) LIKE UPPER(:s) "
                  "OR tel LIKE :s OR TO_CHAR(id) LIKE :s)";
    if (!categorie.isEmpty())
        sql += " AND categorie = :c";

    static const QStringList orders = { "UPPER(nom) ASC", "UPPER(nom) DESC",
                                        "score DESC", "naissance ASC", "id ASC" };
    sql += " ORDER BY " + orders.value(tri < 0 ? 0 : tri, orders.first());

    QSqlQuery q;
    q.prepare(sql);
    q.bindValue(":s", "%" + recherche + "%");
    if (!categorie.isEmpty())
        q.bindValue(":c", categorie);
    if (!executer(q))
        return liste;

    while (q.next()) {
        Client c(q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(),
                 q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                 q.value(6).toString());
        c.setScore(q.value(7).toInt());
        liste << c;
    }
    return liste;
}

bool Client::charger(int id, Client &resultat)
{
    QSqlQuery q;
    q.prepare("SELECT id, nom, mail, tel, naissance, mensuration, categorie, score "
              "FROM clients WHERE id = :id");
    q.bindValue(":id", id);
    if (!executer(q))
        return false;
    if (!q.next()) {
        s_erreur = "Client introuvable.";
        return false;
    }
    resultat = Client(q.value(0).toInt(), q.value(1).toString(), q.value(2).toString(),
                      q.value(3).toString(), q.value(4).toString(), q.value(5).toString(),
                      q.value(6).toString());
    resultat.setScore(q.value(7).toInt());
    return true;
}

// ---------------------------------------------------------------- UPDATE
bool Client::modifier()
{
    if (id <= 0) {
        s_erreur = "Aucun client sélectionné pour la modification.";
        return false;
    }
    QSqlQuery q;
    q.prepare("UPDATE clients SET nom = :nom, mail = :mail, tel = :tel, naissance = :naissance, "
              "mensuration = :mensuration, categorie = :categorie WHERE id = :id");
    q.bindValue(":nom", nom);
    q.bindValue(":mail", mail);
    q.bindValue(":tel", tel);
    q.bindValue(":naissance", naissance);
    q.bindValue(":mensuration", mensuration);
    q.bindValue(":categorie", categorie);
    q.bindValue(":id", id);
    if (!executer(q))
        return false;
    if (q.numRowsAffected() == 0) {
        s_erreur = "Client introuvable : aucune ligne modifiée.";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------- DELETE
bool Client::supprimer(int id)
{
    QSqlDatabase db = QSqlDatabase::database();
    const bool tx = db.transaction();   // tout ou rien

    bool ok = true;
    for (const QString &table : { QStringLiteral("mesures"), QStringLiteral("commandes") }) {
        QSqlQuery d;
        d.prepare("DELETE FROM " + table + " WHERE client_id = :id");
        d.bindValue(":id", id);
        ok = ok && executer(d);
    }

    QSqlQuery q;
    q.prepare("DELETE FROM clients WHERE id = :id");
    q.bindValue(":id", id);
    ok = ok && executer(q);
    if (ok && q.numRowsAffected() == 0) {
        s_erreur = "Client introuvable.";
        ok = false;
    }

    if (tx) {
        if (ok) db.commit();
        else    db.rollback();
    }
    return ok;
}

// ---------------------------------------------------------------- métiers : statistiques
Client::Stats Client::statistiques()
{
    Stats s;
    QSqlQuery q;
    q.prepare("SELECT COUNT(*), "
              "NVL(SUM(CASE WHEN categorie = 'VIP' THEN 1 ELSE 0 END), 0), "
              "NVL(AVG(score), 0) FROM clients");
    if (executer(q) && q.next()) {
        s.total = q.value(0).toInt();
        s.vip = q.value(1).toInt();
        s.scoreMoyen = q.value(2).toDouble();
    }
    s.inactifs = clientsInactifs(6).size();
    return s;
}

QList<QPair<int, QString>> Client::listeClients()
{
    QList<QPair<int, QString>> liste;
    QSqlQuery q;
    q.prepare("SELECT id, nom FROM clients ORDER BY UPPER(nom)");
    if (executer(q))
        while (q.next())
            liste << qMakePair(q.value(0).toInt(), q.value(1).toString());
    return liste;
}

// ---------------------------------------------------------------- métiers : mensurations
QList<Client::Mesure> Client::mesures(int clientId)
{
    QList<Mesure> liste;
    QSqlQuery q;
    q.prepare("SELECT date_mesure, poitrine, taille, hanches, epaules, manche, hauteur FROM mesures "
              "WHERE client_id = :id ORDER BY date_mesure DESC, id DESC");
    q.bindValue(":id", clientId);
    if (!executer(q))
        return liste;
    while (q.next()) {
        Mesure m;
        m.date = q.value(0).toString();
        m.poitrine = q.value(1).toDouble();
        m.taille = q.value(2).toDouble();
        m.hanches = q.value(3).toDouble();
        m.epaules = q.value(4).toDouble();
        m.manche = q.value(5).toDouble();
        m.hauteur = q.value(6).toDouble();
        liste << m;
    }
    return liste;
}

bool Client::ajouterMesure(int clientId, const Mesure &m)
{
    QSqlQuery q;
    q.prepare("INSERT INTO mesures (client_id, date_mesure, poitrine, taille, hanches, epaules, manche, hauteur) "
              "VALUES (:id, :date, :poitrine, :taille, :hanches, :epaules, :manche, :hauteur)");
    q.bindValue(":id", clientId);
    q.bindValue(":date", QDate::currentDate().toString(Qt::ISODate));
    q.bindValue(":poitrine", m.poitrine);
    q.bindValue(":taille", m.taille);
    q.bindValue(":hanches", m.hanches);
    q.bindValue(":epaules", m.epaules);
    q.bindValue(":manche", m.manche);
    q.bindValue(":hauteur", m.hauteur);
    if (!executer(q))
        return false;

    // met à jour le résumé « mensuration » du client
    QSqlQuery u;
    u.prepare("UPDATE clients SET mensuration = :resume WHERE id = :id");
    u.bindValue(":resume", QString("P%1 / T%2 / H%3")
                               .arg(m.poitrine, 0, 'f', 0)
                               .arg(m.taille, 0, 'f', 0)
                               .arg(m.hanches, 0, 'f', 0));
    u.bindValue(":id", clientId);
    executer(u);
    return true;
}

// ---------------------------------------------------------------- métiers : fidélité
bool Client::ajouterCommande(int clientId, double montant, bool *devientVip)
{
    if (devientVip) *devientVip = false;

    QSqlQuery q;
    q.prepare("INSERT INTO commandes (client_id, montant, date_commande) VALUES (:id, :montant, :date)");
    q.bindValue(":id", clientId);
    q.bindValue(":montant", montant);
    q.bindValue(":date", QDate::currentDate().toString(Qt::ISODate));
    if (!executer(q))
        return false;

    const bool vip = recalculerScore(clientId);
    if (devientVip) *devientVip = vip;
    return true;
}

bool Client::recalculerScore(int clientId)
{
    QSqlQuery q;
    q.prepare("SELECT NVL(SUM(montant), 0), "
              "NVL(SUM(CASE WHEN date_commande >= :depuis THEN 1 ELSE 0 END), 0) "
              "FROM commandes WHERE client_id = :id");
    q.bindValue(":depuis", QDate::currentDate().addYears(-1).toString(Qt::ISODate));
    q.bindValue(":id", clientId);
    if (!executer(q) || !q.next())
        return false;

    // 1 point par 100 DT dépensés + 10 points par commande de l'année
    const int score = int(q.value(0).toDouble() / 100.0) + 10 * q.value(1).toInt();

    QSqlQuery u;
    u.prepare("UPDATE clients SET score = :score WHERE id = :id");
    u.bindValue(":score", score);
    u.bindValue(":id", clientId);
    executer(u);

    if (score >= SEUIL_VIP) {
        QSqlQuery v;
        v.prepare("UPDATE clients SET categorie = 'VIP' WHERE id = :id AND categorie <> 'VIP'");
        v.bindValue(":id", clientId);
        executer(v);
        return v.numRowsAffected() > 0;
    }
    return false;
}

QList<Client::LigneFidelite> Client::classementFidelite()
{
    QList<LigneFidelite> liste;
    QSqlQuery q;
    q.prepare("SELECT c.id, c.nom, COUNT(o.id), NVL(SUM(o.montant), 0), c.score, c.categorie "
              "FROM clients c LEFT JOIN commandes o ON o.client_id = c.id "
              "GROUP BY c.id, c.nom, c.score, c.categorie ORDER BY c.score DESC");
    if (!executer(q))
        return liste;
    while (q.next()) {
        LigneFidelite l;
        l.id = q.value(0).toInt();
        l.nom = q.value(1).toString();
        l.nbCommandes = q.value(2).toInt();
        l.montant = q.value(3).toDouble();
        l.score = q.value(4).toInt();
        l.categorie = q.value(5).toString();
        liste << l;
    }
    return liste;
}

// ---------------------------------------------------------------- métiers : graphiques
QList<QPair<QString, int>> Client::repartitionCategories()
{
    QList<QPair<QString, int>> liste;
    QSqlQuery q;
    q.prepare("SELECT categorie, COUNT(*) FROM clients GROUP BY categorie");
    if (executer(q))
        while (q.next())
            liste << qMakePair(q.value(0).toString(), q.value(1).toInt());
    return liste;
}

QList<QPair<QString, int>> Client::topScores(int n)
{
    QList<QPair<QString, int>> liste;
    QSqlQuery q;
    q.prepare("SELECT nom, score FROM (SELECT nom, score FROM clients ORDER BY score DESC) "
              "WHERE ROWNUM <= :n");
    q.bindValue(":n", n);
    if (executer(q))
        while (q.next())
            liste << qMakePair(q.value(0).toString(), q.value(1).toInt());
    return liste;
}

// ---------------------------------------------------------------- métier avancé 1 : taille recommandée
// Pour chaque mensuration (poitrine, taille, hanches) on cherche la taille correspondante,
// puis on retient la plus grande : en couture, on ajuste toujours sur la mesure la plus large.
QString Client::tailleRecommandee(int clientId, QString *detail)
{
    const QList<Mesure> m = mesures(clientId);
    if (m.isEmpty()) {
        if (detail) *detail = "Aucune mensuration enregistrée pour ce client.";
        return QString();
    }

    static const QStringList tailles = { "XS", "S", "M", "L", "XL", "XXL" };
    // compte combien de seuils la valeur dépasse -> indice 0..5 dans « tailles »
    auto indice = [](double v, const QList<double> &seuils) {
        int i = 0;
        while (i < seuils.size() && v >= seuils[i]) ++i;
        return i;
    };

    const Mesure &d = m.first();                       // dernière prise de mesures (tri DESC)
    const int iP = indice(d.poitrine, { 84, 92, 100, 108, 116 });
    const int iT = indice(d.taille,   { 68, 76, 84, 92, 100 });
    const int iH = indice(d.hanches,  { 92, 100, 108, 116, 124 });
    const int best = qMax(iP, qMax(iT, iH));

    if (detail)
        *detail = QString("Poitrine → %1\nTaille → %2\nHanches → %3\n\n(d'après les mesures du %4)")
                      .arg(tailles[iP], tailles[iT], tailles[iH])
                      .arg(QDate::fromString(d.date.left(10), Qt::ISODate).toString("dd/MM/yyyy"));
    return tailles[best];
}

// ---------------------------------------------------------------- métier avancé 2 : clients inactifs
// Clients dont la dernière commande date de plus de « mois » mois (ou qui n'ont jamais commandé).
QList<Client::ClientInactif> Client::clientsInactifs(int mois)
{
    QList<ClientInactif> liste;
    QSqlQuery q;
    q.prepare("SELECT c.id, c.nom, c.mail, MAX(o.date_commande) "
              "FROM clients c LEFT JOIN commandes o ON o.client_id = c.id "
              "GROUP BY c.id, c.nom, c.mail "
              "HAVING MAX(o.date_commande) IS NULL OR MAX(o.date_commande) < :limite "
              "ORDER BY MAX(o.date_commande)");
    q.bindValue(":limite", QDate::currentDate().addMonths(-mois).toString(Qt::ISODate));
    if (!executer(q))
        return liste;
    while (q.next()) {
        ClientInactif c;
        c.id = q.value(0).toInt();
        c.nom = q.value(1).toString();
        c.mail = q.value(2).toString();
        c.derniereCommande = q.value(3).toString().left(10);   // garde yyyy-MM-dd
        liste << c;
    }
    return liste;
}
