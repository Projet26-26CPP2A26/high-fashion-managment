#ifndef CLIENT_H
#define CLIENT_H

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

// Classe métier « Client » (MODÈLE).
// Toutes les requêtes SQL du module clients sont ici. La fenêtre (Vue) appelle
// uniquement ces méthodes : aucune requête derrière un bouton.
class Client
{
public:
    static constexpr int SEUIL_VIP = 100;   // score à partir duquel le client devient VIP

    // --- petites structures de résultat (pour l'affichage) ---
    struct Mesure {
        QString date;                        // yyyy-MM-dd
        double poitrine = 0, taille = 0, hanches = 0;
        double epaules = 0, manche = 0, hauteur = 0;
    };
    struct LigneFidelite {
        int id = 0;
        QString nom;
        int nbCommandes = 0;
        double montant = 0;
        int score = 0;
        QString categorie;
    };
    struct Stats {
        int total = 0;
        int vip = 0;
        int inactifs = 0;                    // clients sans commande depuis 6 mois
        double scoreMoyen = 0;
    };
    struct ClientInactif {
        int id = 0;
        QString nom, mail, derniereCommande; // derniereCommande vide = jamais commandé
    };

    // --- constructeurs ---
    Client() {}
    Client(int id, const QString &nom, const QString &mail, const QString &tel,
           const QString &naissance, const QString &mensuration,
           const QString &categorie = QStringLiteral("Standard"));

    // --- getters ---
    int getID() const                { return id; }
    QString getNom() const           { return nom; }
    QString getMail() const          { return mail; }
    QString getTel() const           { return tel; }
    QString getNaissance() const     { return naissance; }     // yyyy-MM-dd
    QString getMensuration() const   { return mensuration; }
    QString getCategorie() const     { return categorie; }
    int getScore() const             { return score; }

    // --- setters ---
    void setID(int v)                       { id = v; }
    void setNom(const QString &v)           { nom = v; }
    void setMail(const QString &v)          { mail = v; }
    void setTel(const QString &v)           { tel = v; }
    void setNaissance(const QString &v)     { naissance = v; }
    void setMensuration(const QString &v)   { mensuration = v; }
    void setCategorie(const QString &v)     { categorie = v; }
    void setScore(int v)                    { score = v; }

    // --- contrôle de saisie (en C++) ---
    bool valider(QString &erreur) const;

    // --- les 4 CRUDs ---
    bool ajouter();                                            // Create
    static QList<Client> afficher(const QString &recherche = QString(),
                                  const QString &categorie = QString(),
                                  int tri = 0);                // Read (+ recherche, filtre, tri)
    bool modifier();                                           // Update
    static bool supprimer(int id);                             // Delete

    static bool charger(int id, Client &resultat);             // lit un client par son id

    // --- métiers ---
    static Stats statistiques();                               // KPI
    static QList<QPair<int, QString>> listeClients();          // (id, nom) pour les listes déroulantes

    static QList<Mesure> mesures(int clientId);                // historique des mensurations
    static bool ajouterMesure(int clientId, const Mesure &m);

    static bool ajouterCommande(int clientId, double montant, bool *devientVip = nullptr);
    static bool recalculerScore(int clientId);                 // true si le client vient de devenir VIP
    static QList<LigneFidelite> classementFidelite();

    static QList<QPair<QString, int>> repartitionCategories(); // pour le camembert
    static QList<QPair<QString, int>> topScores(int n);        // pour l'histogramme

    // --- métiers avancés ---
    static QString tailleRecommandee(int clientId, QString *detail = nullptr);  // XS..XXL selon la dernière mensuration
    static QList<ClientInactif> clientsInactifs(int mois = 6);                  // clients à relancer

    // dernier message d'erreur (lisible) en cas d'échec
    static QString derniereErreur();

private:
    int id = -1;
    QString nom, mail, tel, naissance, mensuration;
    QString categorie = QStringLiteral("Standard");
    int score = 0;
};

#endif // CLIENT_H
