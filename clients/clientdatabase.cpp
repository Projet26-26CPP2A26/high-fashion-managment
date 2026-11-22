#include "clientdatabase.h"

#include <QDate>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlQuery>

bool openClientDatabase(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;

    QSqlDatabase db = QSqlDatabase::contains(QSqlDatabase::defaultConnection)
                          ? QSqlDatabase::database(QSqlDatabase::defaultConnection, false)
                          : QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(path);
    if (!db.isOpen() && !db.open())
        return false;

    QSqlQuery q;
    q.exec("CREATE TABLE IF NOT EXISTS clients ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "nom TEXT NOT NULL,"
           "mail TEXT NOT NULL,"
           "tel TEXT NOT NULL,"
           "naissance TEXT,"                 // yyyy-MM-dd
           "mensuration TEXT,"               // résumé (dernière prise de mesures)
           "categorie TEXT DEFAULT 'Standard',"
           "score INTEGER DEFAULT 0)");

    q.exec("CREATE TABLE IF NOT EXISTS mesures ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "client_id INTEGER NOT NULL,"
           "date TEXT NOT NULL,"
           "poitrine REAL, taille REAL, hanches REAL,"
           "epaules REAL, manche REAL, hauteur REAL)");

    q.exec("CREATE TABLE IF NOT EXISTS commandes ("
           "id INTEGER PRIMARY KEY AUTOINCREMENT,"
           "client_id INTEGER NOT NULL,"
           "montant REAL NOT NULL,"
           "date TEXT NOT NULL)");

    // Demo data on first launch only
    q.exec("SELECT COUNT(*) FROM clients");
    if (q.next() && q.value(0).toInt() == 0) {
        const QString today = QDate::currentDate().addYears(-30).toString(Qt::ISODate); // anniversaire aujourd'hui
        q.exec("INSERT INTO clients(nom,mail,tel,naissance,mensuration,categorie) VALUES"
               "('Amira Trabelsi','amira.trabelsi@example.com','22123456','1992-03-14','P88 / T68 / H94','Standard'),"
               "('Salma Ben Ali','salma.benali@example.com','98456123','" + today + "','P92 / T72 / H98','Standard'),"
               "('Yassine Gharbi','yassine.gharbi@example.com','55789321','1985-11-02','P104 / T88 / H100','Standard'),"
               "('Nour Haddad','nour.haddad@example.com','21654987','1990-07-21','P86 / T66 / H92','Standard'),"
               "('Meriem Jebali','meriem.jebali@example.com','50321654','1998-12-30','P90 / T70 / H96','Standard')");

        q.exec("INSERT INTO commandes(client_id,montant,date) VALUES"
               "(1,3200,date('now','-20 day')),(1,2800,date('now','-90 day')),(1,4100,date('now','-200 day')),"
               "(2,650,date('now','-15 day')),(2,480,date('now','-120 day')),"
               "(3,1500,date('now','-60 day')),"
               "(4,5200,date('now','-10 day')),(4,3900,date('now','-45 day')),"
               "(4,2100,date('now','-150 day')),(4,1800,date('now','-300 day')),"
               "(5,300,date('now','-5 day'))");

        q.exec("INSERT INTO mesures(client_id,date,poitrine,taille,hanches,epaules,manche,hauteur) VALUES"
               "(1,date('now','-200 day'),87,67,93,39,59,168),"
               "(1,date('now','-20 day'),88,68,94,39,59,168)");

        // même formule que dans l'application
        q.exec("UPDATE clients SET score = "
               "CAST((SELECT COALESCE(SUM(montant),0) FROM commandes WHERE client_id=clients.id)/100 AS INTEGER)"
               " + 10*(SELECT COUNT(*) FROM commandes WHERE client_id=clients.id AND date>=date('now','-1 year'))");
        q.exec("UPDATE clients SET categorie='VIP' WHERE score>=100");
    }
    return true;
}
