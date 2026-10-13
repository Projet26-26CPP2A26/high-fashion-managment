/****************************************************************************
** Meta object code from reading C++ file 'orderwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.7.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../orders/orderwindow.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'orderwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.7.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {

#ifdef QT_MOC_HAS_STRINGDATA
struct qt_meta_stringdata_CLASSOrderWindowENDCLASS_t {};
constexpr auto qt_meta_stringdata_CLASSOrderWindowENDCLASS = QtMocHelpers::stringData(
    "OrderWindow",
    "updateClock",
    "",
    "showHelp",
    "addCommande",
    "editCommande",
    "deleteCommande",
    "applyCommandeFilter",
    "applyCommandeSort",
    "exportCommandesPdf",
    "onDevisClientChanged",
    "addDevisLine",
    "removeDevisLine",
    "updateDevis",
    "exportDevisPdf",
    "createCommandeFromDevis"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA
} // unnamed namespace

Q_CONSTINIT static const uint qt_meta_data_CLASSOrderWindowENDCLASS[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   98,    2, 0x08,    1 /* Private */,
       3,    0,   99,    2, 0x08,    2 /* Private */,
       4,    0,  100,    2, 0x08,    3 /* Private */,
       5,    0,  101,    2, 0x08,    4 /* Private */,
       6,    0,  102,    2, 0x08,    5 /* Private */,
       7,    0,  103,    2, 0x08,    6 /* Private */,
       8,    0,  104,    2, 0x08,    7 /* Private */,
       9,    0,  105,    2, 0x08,    8 /* Private */,
      10,    0,  106,    2, 0x08,    9 /* Private */,
      11,    0,  107,    2, 0x08,   10 /* Private */,
      12,    0,  108,    2, 0x08,   11 /* Private */,
      13,    0,  109,    2, 0x08,   12 /* Private */,
      14,    0,  110,    2, 0x08,   13 /* Private */,
      15,    0,  111,    2, 0x08,   14 /* Private */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject OrderWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_CLASSOrderWindowENDCLASS.offsetsAndSizes,
    qt_meta_data_CLASSOrderWindowENDCLASS,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_stringdata_CLASSOrderWindowENDCLASS_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<OrderWindow, std::true_type>,
        // method 'updateClock'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'showHelp'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'addCommande'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'editCommande'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'deleteCommande'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'applyCommandeFilter'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'applyCommandeSort'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'exportCommandesPdf'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onDevisClientChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'addDevisLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'removeDevisLine'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'updateDevis'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'exportDevisPdf'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'createCommandeFromDevis'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void OrderWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<OrderWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->updateClock(); break;
        case 1: _t->showHelp(); break;
        case 2: _t->addCommande(); break;
        case 3: _t->editCommande(); break;
        case 4: _t->deleteCommande(); break;
        case 5: _t->applyCommandeFilter(); break;
        case 6: _t->applyCommandeSort(); break;
        case 7: _t->exportCommandesPdf(); break;
        case 8: _t->onDevisClientChanged(); break;
        case 9: _t->addDevisLine(); break;
        case 10: _t->removeDevisLine(); break;
        case 11: _t->updateDevis(); break;
        case 12: _t->exportDevisPdf(); break;
        case 13: _t->createCommandeFromDevis(); break;
        default: ;
        }
    }
    (void)_a;
}

const QMetaObject *OrderWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OrderWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_CLASSOrderWindowENDCLASS.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int OrderWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}
QT_WARNING_POP
