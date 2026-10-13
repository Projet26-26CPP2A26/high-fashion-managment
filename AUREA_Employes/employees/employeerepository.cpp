#include "employeerepository.h"
#include <QDateTime>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace {
const QString employeeSelect=QStringLiteral("SELECT e.id,e.name,e.email,e.position,e.specialty,e.hired,e.available,e.base_load,e.base_load+COALESCE((SELECT SUM(a.load) FROM employee_assignments a WHERE a.employee_id=e.id AND a.completed=0),0) FROM employees e ");
Employee readEmployee(const QSqlQuery &q) {
    Employee e; e.id=q.value(0).toLongLong(); e.name=q.value(1).toString(); e.email=q.value(2).toString();
    e.position=q.value(3).toString(); e.specialty=q.value(4).toString(); e.hired=QDate::fromString(q.value(5).toString(),Qt::ISODate);
    e.available=q.value(6).toBool(); e.baseLoad=q.value(7).toInt(); e.load=q.value(8).toInt(); return e;
}
QString normalized(QString text) {
    text=text.normalized(QString::NormalizationForm_D).toCaseFolded(); QString result;
    for(QChar c:text) if(c.category()!=QChar::Mark_NonSpacing) result+=c;
    return result.trimmed();
}
int elapsed(const QTime &start,const QTime &end) {return start.secsTo(end)/60;}
}
int Attendance::minutes(const QTime &now) const {
    const QTime end=departure.isValid()?departure:(day==QDate::currentDate()?now:QTime());
    return end.isValid()?std::max(0,elapsed(arrival,end)-pause):0;
}
int Attendance::overtime(const QTime &now) const { return std::max(0,minutes(now)-480); }
QStringList EmployeeRepository::positions() {return {"Chef d'atelier","Modéliste","Couturier","Brodeur","Coupeur","Finisseur","Responsable qualité"};}
QStringList EmployeeRepository::specialties() {return {"Couture haute précision","Broderie main","Patronage","Drapé et moulage","Finitions","Contrôle qualité"};}
int EmployeeRepository::taskLoad(int hours) {return static_cast<int>(std::ceil(hours*100.0/40.0));}
bool EmployeeRepository::fail(const QString &message,bool rollback) {lastError=message; if(rollback) db.rollback(); return false;}
QString EmployeeRepository::validate(const Employee &e) {
    if(e.name.trimmed().isEmpty()||e.position.trimmed().isEmpty()||e.specialty.trimmed().isEmpty()) return "Le nom, le poste et la spécialité sont obligatoires.";
    if(e.name.size()>120||e.email.size()>180||e.position.size()>100||e.specialty.size()>100) return "Un champ dépasse sa longueur maximale.";
    if(!QRegularExpression(QStringLiteral("^[^@\\s]+@[^@\\s]+\\.[^@\\s]{2,}$")).match(e.email.trimmed()).hasMatch()) return "L'adresse e-mail n'est pas valide.";
    if(!e.hired.isValid()||e.hired>QDate::currentDate()||e.hired<QDate(1970,1,1)) return "La date d'embauche doit être valide et ne peut pas être future.";
    if(e.baseLoad<0||e.baseLoad>100) return "La charge initiale doit être comprise entre 0 et 100 %.";
    return {};
}
bool EmployeeRepository::initialize(bool demo) {
    lastError.clear(); QSqlQuery q(db);
    const QStringList statements={
        "CREATE TABLE IF NOT EXISTS employees(id INTEGER PRIMARY KEY AUTOINCREMENT,name TEXT NOT NULL,email TEXT NOT NULL UNIQUE COLLATE NOCASE,position TEXT NOT NULL,specialty TEXT NOT NULL,hired TEXT NOT NULL,available INTEGER NOT NULL CHECK(available IN(0,1)),base_load INTEGER NOT NULL CHECK(base_load BETWEEN 0 AND 100))",
        "CREATE TABLE IF NOT EXISTS employee_attendance(id INTEGER PRIMARY KEY AUTOINCREMENT,employee_id INTEGER REFERENCES employees(id) ON DELETE SET NULL,name TEXT NOT NULL,day TEXT NOT NULL,arrival TEXT NOT NULL,departure TEXT,pause INTEGER NOT NULL CHECK(pause BETWEEN 0 AND 240),UNIQUE(employee_id,day))",
        "CREATE TABLE IF NOT EXISTS employee_assignments(id INTEGER PRIMARY KEY AUTOINCREMENT,employee_id INTEGER REFERENCES employees(id) ON DELETE SET NULL,name TEXT NOT NULL,title TEXT NOT NULL,specialty TEXT NOT NULL,hours INTEGER NOT NULL CHECK(hours BETWEEN 1 AND 80),load INTEGER NOT NULL,created TEXT NOT NULL,completed INTEGER NOT NULL DEFAULT 0 CHECK(completed IN(0,1)))",
        "CREATE INDEX IF NOT EXISTS attendance_day_idx ON employee_attendance(day)",
        "CREATE INDEX IF NOT EXISTS assignment_employee_idx ON employee_assignments(employee_id)",
        "CREATE TABLE IF NOT EXISTS employee_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL)"};
    for(const auto &sql:statements) if(!q.exec(sql)) return fail(q.lastError().text());
    if(demo) {
        const auto flag=[&](const QString &key)->int {
            q.finish(); q.prepare("SELECT count(*) FROM employee_metadata WHERE key=?"); q.addBindValue(key);
            if(!q.exec()||!q.next()) return -1; const int value=q.value(0).toInt(); q.finish(); return value;
        };
        const int current=flag("demo_seed_v2"),legacy=flag("demo_seeded"); if(current<0||legacy<0) return fail(q.lastError().text());
        if(!current) {
            if(!q.exec("SELECT count(*) FROM employees")||!q.next()) return fail(q.lastError().text());
            const bool empty=q.value(0).toInt()==0; q.finish();
            // Base vide jamais initialisée : on charge la démo. Ancienne démo (v1) : on la remplace une seule fois par la version enrichie.
            if(empty&&!legacy&&!seed()) return false;
            if(!empty&&legacy&&!seed(true)) return false;
            if(!q.exec("INSERT OR IGNORE INTO employee_metadata(key,value) VALUES('demo_seeded','1')")||!q.exec("INSERT OR IGNORE INTO employee_metadata(key,value) VALUES('demo_seed_v2','1')")) return fail(q.lastError().text());
        }
    }
    return true;
}
QVector<Employee> EmployeeRepository::all() {
    lastError.clear(); QVector<Employee> out; QSqlQuery q(db);
    if(!q.exec(employeeSelect+"ORDER BY e.name COLLATE NOCASE")) {fail(q.lastError().text()); return out;}
    while(q.next()) out.append(readEmployee(q));
    return out;
}
Employee EmployeeRepository::find(qint64 id) {
    lastError.clear(); QSqlQuery q(db); q.prepare(employeeSelect+"WHERE e.id=?"); q.addBindValue(id);
    if(!q.exec()) {fail(q.lastError().text()); return {};}
    return q.next()?readEmployee(q):Employee{};
}
bool EmployeeRepository::save(Employee &e) {
    lastError.clear(); e.name=e.name.trimmed(); e.email=e.email.trimmed(); e.position=e.position.trimmed(); e.specialty=e.specialty.trimmed();
    const QString validation=validate(e); if(!validation.isEmpty()) return fail(validation);
    if(!db.transaction()) return fail(db.lastError().text());
    if(e.id) {
        const Employee previous=find(e.id); if(!previous.id) return fail("Cet employé n'existe plus.",true);
        if(e.baseLoad+previous.load-previous.baseLoad>100) return fail("Cette charge initiale provoquerait une surcharge avec les tâches en cours.",true);
    }
    QSqlQuery q(db); q.prepare(e.id?"UPDATE employees SET name=?,email=?,position=?,specialty=?,hired=?,available=?,base_load=? WHERE id=?":
        "INSERT INTO employees(name,email,position,specialty,hired,available,base_load) VALUES(?,?,?,?,?,?,?)");
    for(const QVariant &v:QVariantList{e.name,e.email,e.position,e.specialty,e.hired.toString(Qt::ISODate),e.available,e.baseLoad}) q.addBindValue(v);
    if(e.id) q.addBindValue(e.id);
    if(!q.exec()) return fail(q.lastError().text().contains("UNIQUE")?QStringLiteral("Cette adresse e-mail est déjà utilisée."):q.lastError().text(),true);
    const qint64 id=e.id?e.id:q.lastInsertId().toLongLong(); if(!db.commit()) return fail(db.lastError().text(),true);
    e=find(id); return e.id!=0;
}
bool EmployeeRepository::remove(qint64 id) {
    lastError.clear(); QSqlQuery q(db); q.prepare("DELETE FROM employees WHERE id=?"); q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    if(q.numRowsAffected()!=1) return fail("Cet employé n'existe plus.");
    return true;
}
QVector<Attendance> EmployeeRepository::attendance() {
    lastError.clear(); QVector<Attendance> out; QSqlQuery q(db);
    if(!q.exec("SELECT a.id,a.employee_id,COALESCE(e.name,a.name),a.day,a.arrival,a.departure,a.pause FROM employee_attendance a LEFT JOIN employees e ON e.id=a.employee_id ORDER BY a.day DESC,a.arrival DESC,a.id DESC")) {fail(q.lastError().text()); return out;}
    while(q.next()) {Attendance a; a.id=q.value(0).toLongLong(); a.employeeId=q.value(1).toLongLong(); a.name=q.value(2).toString(); a.day=QDate::fromString(q.value(3).toString(),Qt::ISODate);
        a.arrival=QTime::fromString(q.value(4).toString(),"HH:mm"); a.departure=QTime::fromString(q.value(5).toString(),"HH:mm"); a.pause=q.value(6).toInt(); out.append(a);} return out;
}
bool EmployeeRepository::insertAttendance(qint64 id,const QString &name,const QDate &day,const QTime &arrival,const QTime &departure,int pause) {
    QSqlQuery q(db); q.prepare("INSERT INTO employee_attendance(employee_id,name,day,arrival,departure,pause) VALUES(?,?,?,?,?,?) ON CONFLICT(employee_id,day) DO UPDATE SET arrival=excluded.arrival,departure=excluded.departure,pause=excluded.pause,name=excluded.name");
    for(const QVariant &v:QVariantList{id,name,day.toString(Qt::ISODate),arrival.toString("HH:mm"),departure.isValid()?QVariant(departure.toString("HH:mm")):QVariant(),pause}) q.addBindValue(v);
    return q.exec()?true:fail(q.lastError().text());
}
bool EmployeeRepository::clockIn(qint64 id,const QDate &day,const QTime &time) {
    const Employee e=find(id); if(!e.id) return fail("Sélectionnez un employé existant.");
    if(day!=QDate::currentDate()||!time.isValid()) return fail("Le pointage instantané concerne aujourd'hui.");
    for(const auto &a:attendance()) if(a.employeeId==id&&a.day==day) return fail("L'arrivée est déjà enregistrée aujourd'hui.");
    return insertAttendance(id,e.name,day,time,{},0);
}
bool EmployeeRepository::clockOut(qint64 id,const QDate &day,const QTime &time) {
    if(!find(id).id) return fail("Sélectionnez un employé existant.");
    if(day!=QDate::currentDate()||!time.isValid()) return fail("Le pointage instantané concerne aujourd'hui.");
    for(const auto &a:attendance()) if(a.employeeId==id&&a.day==day) {
        if(a.departure.isValid()) return fail("Le départ est déjà enregistré.");
        if(elapsed(a.arrival,time)<=0) return fail("Le départ doit suivre l'arrivée.");
        QSqlQuery q(db); q.prepare("UPDATE employee_attendance SET departure=? WHERE id=? AND departure IS NULL"); q.addBindValue(time.toString("HH:mm")); q.addBindValue(a.id);
        if(!q.exec()) return fail(q.lastError().text());
        return q.numRowsAffected()==1;
    }
    return fail("Aucune arrivée en cours pour cet employé aujourd'hui.");
}
bool EmployeeRepository::saveAttendance(qint64 id,const QDate &day,const QTime &arrival,const QTime &departure,int pause) {
    const Employee e=find(id); if(!e.id) return fail("Sélectionnez un employé existant.");
    if(!day.isValid()||day>QDate::currentDate()||day<e.hired) return fail("La date doit être comprise entre l'embauche et aujourd'hui.");
    if(!arrival.isValid()||!departure.isValid()||elapsed(arrival,departure)<=0) return fail("Le départ doit suivre l'arrivée, dans la même journée.");
    if(pause<0||pause>240||pause>=elapsed(arrival,departure)) return fail("La pause doit être inférieure à la durée de présence (0 à 240 min).");
    return insertAttendance(id,e.name,day,arrival,departure,pause);
}
bool EmployeeRepository::removeAttendance(qint64 id) {
    lastError.clear(); QSqlQuery q(db); q.prepare("DELETE FROM employee_attendance WHERE id=?"); q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    if(q.numRowsAffected()!=1) return fail("Ce pointage n'existe plus.");
    return true;
}
QVector<EmployeeCandidate> EmployeeRepository::rank(const QString &specialty,int hours) {
    QVector<EmployeeCandidate> out; if(hours<1||hours>80) return out; const int extra=taskLoad(hours);
    for(const auto &e:all()) if(e.available) {
        EmployeeCandidate c; c.employee=e; const int remaining=std::max(0,100-e.load); c.fits=extra<=remaining;
        c.specialtyPoints=normalized(e.specialty)==normalized(specialty)?50:0; c.loadPoints=qRound(40.0*remaining/100.0);
        c.capacityPoints=c.fits?10:qRound(10.0*remaining/extra); out.append(c);
    }
    std::sort(out.begin(),out.end(),[](const EmployeeCandidate &a,const EmployeeCandidate &b) {
        if(a.fits!=b.fits) return a.fits;
        if(a.score()!=b.score()) return a.score()>b.score();
        if(a.employee.load!=b.employee.load) return a.employee.load<b.employee.load;
        return QString::localeAwareCompare(a.employee.name,b.employee.name)<0;
    }); return out;
}
bool EmployeeRepository::assign(qint64 id,const QString &title,const QString &specialty,int hours) {
    lastError.clear(); if(title.trimmed().isEmpty()||specialty.trimmed().isEmpty()||title.size()>200||specialty.size()>100||hours<1||hours>80) return fail("Renseignez une tâche, une spécialité et une durée de 1 à 80 h.");
    if(!db.transaction()) return fail(db.lastError().text());
    const Employee e=find(id);
    if(!e.id||!e.available) return fail("L'employé doit exister et être disponible.",true);
    const int extra=taskLoad(hours); if(e.load+extra>100) return fail("Capacité insuffisante : réduisez la durée ou choisissez un autre employé.",true);
    QSqlQuery q(db); q.prepare("INSERT INTO employee_assignments(employee_id,name,title,specialty,hours,load,created) VALUES(?,?,?,?,?,?,?)");
    for(const QVariant &v:QVariantList{id,e.name,title.trimmed(),specialty.trimmed(),hours,extra,QDateTime::currentDateTime().toString(Qt::ISODate)}) q.addBindValue(v);
    if(!q.exec()) return fail(q.lastError().text(),true);
    if(!db.commit()) return fail(db.lastError().text(),true);
    return true;
}
QVector<Assignment> EmployeeRepository::assignments() {
    lastError.clear(); QVector<Assignment> out; QSqlQuery q(db);
    if(!q.exec("SELECT a.id,a.employee_id,COALESCE(e.name,a.name),a.title,a.specialty,a.hours,a.load,a.created,a.completed FROM employee_assignments a LEFT JOIN employees e ON e.id=a.employee_id ORDER BY a.id DESC")) {fail(q.lastError().text()); return out;}
    while(q.next()) {Assignment a; a.id=q.value(0).toLongLong(); a.employeeId=q.value(1).toLongLong(); a.name=q.value(2).toString(); a.title=q.value(3).toString(); a.specialty=q.value(4).toString(); a.hours=q.value(5).toInt(); a.load=q.value(6).toInt(); a.created=q.value(7).toString(); a.completed=q.value(8).toBool(); out.append(a);} return out;
}
bool EmployeeRepository::completeAssignment(qint64 id) {
    lastError.clear(); QSqlQuery q(db); q.prepare("UPDATE employee_assignments SET completed=1 WHERE id=? AND completed=0"); q.addBindValue(id);
    if(!q.exec()) return fail(q.lastError().text());
    if(q.numRowsAffected()!=1) return fail("Cette tâche est déjà terminée ou n'existe plus.");
    return true;
}
bool EmployeeRepository::seed(bool wipe) {
    // Données fictives de démonstration : onze employés, quinze jours de pointages, tâches en cours et terminées.
    struct Person {const char *name,*email,*position,*specialty,*hired; int available,baseLoad;};
    const QVector<Person> people={
        {"Sarah Ben Ali","sarah.ben.ali@aurea.tn","Couturier","Couture haute précision","2019-03-11",1,20},
        {"Mohamed Trabelsi","mohamed.trabelsi@aurea.tn","Chef d'atelier","Contrôle qualité","2016-09-05",1,40},
        {"Leila Mansouri","leila.mansouri@aurea.tn","Brodeur","Broderie main","2020-01-20",1,25},
        {"Yassine Gharbi","yassine.gharbi@aurea.tn","Modéliste","Patronage","2018-06-14",1,45},
        {"Amel Jaziri","amel.jaziri@aurea.tn","Couturier","Couture haute précision","2021-11-02",1,30},
        {"Karim Bouzid","karim.bouzid@aurea.tn","Coupeur","Patronage","2022-04-18",0,0},
        {"Nour Hammami","nour.hammami@aurea.tn","Brodeur","Broderie main","2023-02-27",1,10},
        {"Rania Chaabane","rania.chaabane@aurea.tn","Modéliste","Drapé et moulage","2017-10-09",1,20},
        {"Walid Sassi","walid.sassi@aurea.tn","Finisseur","Finitions","2021-05-31",0,15},
        {"Inès Khelifi","ines.khelifi@aurea.tn","Responsable qualité","Contrôle qualité","2019-09-23",1,30},
        {"Hatem Louati","hatem.louati@aurea.tn","Finisseur","Finitions","2024-01-15",1,15}};
    struct Task {int person; const char *title; int hours,daysAgo; bool done;};
    QVector<Task> tasks={
        {0,"Robe fourreau en crêpe — assemblage",8,24,true},{2,"Broderie de perles sur bustier",12,22,true},{3,"Patron du tailleur croisé",16,21,true},
        {7,"Drapé de la robe de gala",10,19,true},{10,"Ourlets main — collection été",6,18,true},{9,"Contrôle qualité des chemisiers",8,16,true},
        {4,"Veste smoking — montage",16,14,true},{6,"Broderie fils d'or sur col",6,12,true},{1,"Validation du prototype manteau",10,10,true},{10,"Doublures en soie — finitions",8,9,true},
        {3,"Gradation du patron de veste",16,7,false},{2,"Broderie fils d'or sur manteau",10,6,false},{0,"Robe de mariée — corsage",12,5,false},{1,"Contrôle qualité collection automne",20,4,false},
        {4,"Manteau en laine — couture",16,3,false},{7,"Drapé de la robe asymétrique",10,3,false},{9,"Inspection finale du lot manteaux",8,2,false},
        {2,"Paillettes sur robe de soirée",6,2,false},{3,"Ajustement du patron de pantalon",4,1,false},{6,"Broderie de motifs sur manche",6,1,false},{10,"Ourlets main — robe cocktail",4,1,false}};
    if(!db.transaction()) return fail(db.lastError().text());
    QSqlQuery q(db);
    if(wipe) for(const char *sql:{"DELETE FROM employee_assignments","DELETE FROM employee_attendance","DELETE FROM employees","DELETE FROM sqlite_sequence WHERE name IN('employees','employee_attendance','employee_assignments')"}) if(!q.exec(QString::fromLatin1(sql))) return fail(q.lastError().text(),true);
    q.prepare("INSERT INTO employees(name,email,position,specialty,hired,available,base_load) VALUES(?,?,?,?,?,?,?)");
    QVector<qint64> ids;
    const auto bindAll=[](QSqlQuery &query,const QVariantList &values){for(int i=0;i<values.size();++i) query.bindValue(i,values[i]);};
    for(const auto &p:people) {
        bindAll(q,QVariantList{QString::fromUtf8(p.name),QString::fromUtf8(p.email),QString::fromUtf8(p.position),QString::fromUtf8(p.specialty),QString::fromLatin1(p.hired),p.available,p.baseLoad});
        if(!q.exec()) return fail(q.lastError().text(),true);
        ids.append(q.lastInsertId().toLongLong());
    }
    // Pointages : arrivée 07 h 50 – 08 h 35, pause 45 ou 60 min, quelques heures supplémentaires, une absence de temps en temps.
    const QDate today=QDate::currentDate(); const int nowMinute=QTime::currentTime().hour()*60+QTime::currentTime().minute();
    auto mix=[](quint32 x){x=x*2654435761u+12345u; x^=x<<13; x^=x>>17; x^=x<<5; return x;};
    QSqlQuery att(db); att.prepare("INSERT INTO employee_attendance(employee_id,name,day,arrival,departure,pause) VALUES(?,?,?,?,?,?)");
    for(int back=14;back>=0;--back) {
        const QDate day=today.addDays(-back); if(back>0&&day.dayOfWeek()==7) continue;
        for(int i=0;i<people.size();++i) {
            if(!people[i].available) continue;
            const quint32 r=mix(quint32(ids[i]*7919+day.toJulianDay()));
            if(back>0&&r%14==0) continue;
            const int arrival=7*60+50+int(r%46); int pause=((r>>8)%2)?60:45; const int departure=arrival+pause+7*60+20+int((r>>12)%100);
            bool open=back==1&&i==4;                               // départ oublié hier : « À corriger »
            if(back==0) {if(arrival+15>nowMinute) continue; if(departure>nowMinute) {open=true; pause=0;}}
            const auto clock=[](int minute){return QTime(minute/60,minute%60).toString("HH:mm");};
            bindAll(att,QVariantList{ids[i],QString::fromUtf8(people[i].name),day.toString(Qt::ISODate),clock(arrival),open?QVariant():QVariant(clock(departure)),pause});
            if(!att.exec()) return fail(att.lastError().text(),true);
        }
    }
    std::stable_sort(tasks.begin(),tasks.end(),[](const Task &a,const Task &b){return a.daysAgo>b.daysAgo;});
    q.prepare("INSERT INTO employee_assignments(employee_id,name,title,specialty,hours,load,created,completed) VALUES(?,?,?,?,?,?,?,?)");
    int n=0;
    for(const auto &t:tasks) {
        const QDateTime created(today.addDays(-t.daysAgo),QTime(8+n%7,10+(n*7)%40)); ++n;
        bindAll(q,QVariantList{ids[t.person],QString::fromUtf8(people[t.person].name),QString::fromUtf8(t.title),QString::fromUtf8(people[t.person].specialty),t.hours,taskLoad(t.hours),created.toString(Qt::ISODate),t.done?1:0});
        if(!q.exec()) return fail(q.lastError().text(),true);
    }
    if(!db.commit()) return fail(db.lastError().text(),true);
    return true;
}
