#include "qrpcservice.h"
#include "qrpcpeer.h"
#include "qrpcrequest.h"
#include <QTcpSocket>
#include <QMetaObject>
#include <QMetaMethod>


// call QMetaMethod with conversion from QVariant (based on https://gist.github.com/andref/2838534)
QVariant invokeAutoConvert(QObject* object, QMetaMethod metaMethod, QVariantList args)
{
    // check if number of incoming args is sufficient or larger
    QList<QByteArray> methodTypes = metaMethod.parameterTypes();
    if (methodTypes.size() > args.size()) {
        qWarning() << "Insufficient arguments to call" << metaMethod.methodSignature();
        return QVariant();
    }

    // make a copy of incoming args and convert them to target types
    QVariantList converted;
    for (int i = 0; i < methodTypes.size(); i++) {
        const QVariant& arg = args.at(i);
        QByteArray methodTypeName = methodTypes.at(i);
        QByteArray argTypeName = arg.typeName();
        QVariant::Type methodType = QVariant::nameToType(methodTypeName);
        QVariant copy = QVariant(arg);
        if (copy.type() != methodType) {
            if (copy.canConvert(methodType)) {
                if (!copy.convert(methodType)) {
                    qWarning() << "Conversion from" << argTypeName << "to" << methodTypeName << "failed";
                    return QVariant();
                }
            } else {
                qWarning() << "Cannot convert" << argTypeName << "to" << methodTypeName;
                return QVariant();
            }
        }
        converted << copy;
    }

    // build generic argument list from variant argument list
    QList<QGenericArgument> args_gen;
    for (int i = 0; i < converted.size(); i++) {
        QVariant& argument = converted[i];
        QGenericArgument genericArgument(
            QMetaType::typeName(argument.userType()),
            const_cast<void*>(argument.constData())
        );
        args_gen << genericArgument;
    }

    // build generic argument for return type
    QVariant returnValue;
    if (metaMethod.returnType() != QMetaType::Void) {
        returnValue = QVariant(metaMethod.returnType(), static_cast<void*>(nullptr));
    }
    QGenericReturnArgument returnArgument(
        metaMethod.typeName(),
        const_cast<void*>(returnValue.constData())
    );

    // invoke method
    bool ok = metaMethod.invoke(object, Qt::DirectConnection, returnArgument,
        args_gen.value(0), args_gen.value(1), args_gen.value(2), args_gen.value(3),
        args_gen.value(4), args_gen.value(5), args_gen.value(6), args_gen.value(7),
        args_gen.value(8), args_gen.value(9)
    );

    if (!ok) {
        qWarning() << "Calling" << metaMethod.methodSignature() << "failed.";
        return QVariant();
    } else {
        return returnValue;
    }
}


QRpcServiceBase::QRpcServiceBase(QTcpServer* server, QObject *parent) : QObject(parent), m_server(server)
{
    // new connection handler
    connect(server, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket* socket;
        while ((socket = m_server->nextPendingConnection())) {
            auto peer = new QRpcPeer(socket);
            // handle new rpc requests
            connect(peer, &QRpcPeer::newRequest, this, &QRpcService::handleNewRequest);
            // remember peer
            m_peers.emplace(peer);
            // delete socket and peer on disconnect  // TODO: sockets and peers are probably never destroyed if service is destroyed first
            connect(socket, &QTcpSocket::disconnected, this, [this, peer, socket]() {
                peer->deleteLater();
                socket->deleteLater();
                m_peers.erase(peer);
            });
        }
    });
}

QRpcServiceBase::~QRpcServiceBase()
{

}

void QRpcServiceBase::registerObject(QString name, QObject *o)
{
    m_reg_name_to_obj.emplace(std::make_pair(name, o));
    m_reg_obj_to_name.emplace(std::make_pair(o, name));

    // connect all signals of the registered object
    auto mo = o->metaObject();
    QMetaMethod handler = metaObject()->method(metaObject()->indexOfSlot("handleRegisteredObjectSignal()"));
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod method = mo->method(i);
        if (method.methodType() == QMetaMethod::Signal && method.access() == QMetaMethod::Public) {
            connect(o, method, this, handler);
        }
    }

    // unregister object if it is destroyed externally
    connect(o, &QObject::destroyed, this, [this, name](){
        unregisterObject(name);
    });
}

void QRpcServiceBase::unregisterObject(QString name)
{
    // disconnect any signals from object to service and remove from map
    try {
        QObject* o = m_reg_name_to_obj.at(name);
        disconnect(o, 0, this, 0);
        m_reg_name_to_obj.erase(name);
        m_reg_obj_to_name.erase(o);
    } catch (const std::out_of_range&) {
        return;
    }
}

void QRpcServiceBase::handleNewRequest(QRpcRequest *request)
{
    int sep = request->method().indexOf('.');
    QString obj_name = (sep > 0) ? request->method().left(sep) : "";
    QString method_name = request->method().mid(sep+1);

    // find object
    QObject* o;
    try {
        o = m_reg_name_to_obj.at(obj_name);
    } catch (const std::out_of_range& e) {
        request->error("rpc object not found");
        return;
    }

    // find method
    const QMetaObject* mo = o->metaObject();
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod method = mo->method(i);
        if (method.name() == method_name) {
            QVariant retval;
            QVariantList args;
            QVariant reqData = request->data();
            if (reqData.isValid()) {
                if (reqData.type() == QVariant::Type::List)
                    args = reqData.toList();
                else
                    args.append(reqData);
            }
            retval = invokeAutoConvert(o, method, args);
            request->setResult(retval);
        }
    }

    // TODO: handle method not found
}

void QRpcServiceBase::handleRegisteredObjectSignal()
{
    // dummy slot, handle signal in QRpcService::qt_metacall
}

int QRpcService::s_id_handleRegisteredObjectSignal = -1;

QRpcService::QRpcService(QTcpServer *server, QObject *parent) : QRpcServiceBase(server, parent)
{
    QRpcService::s_id_handleRegisteredObjectSignal = metaObject()->indexOfMethod("handleRegisteredObjectSignal()");
    Q_ASSERT(QRpcService::s_id_handleRegisteredObjectSignal != -1);
}

QRpcService::~QRpcService()
{

}

int QRpcService::qt_metacall(QMetaObject::Call c, int id, void **a)
{
    // only handle calls to handleRegisteredObjectSignal, let parent qt_metacall do the rest
    if (id != QRpcService::s_id_handleRegisteredObjectSignal)
        return QRpcServiceBase::qt_metacall(c, id, a);

    // inspect sender and signal
    QObject* o = sender();
    QMetaMethod signal = o->metaObject()->method(senderSignalIndex());
    QString event_name = m_reg_obj_to_name.at(o) + "." + signal.name();

    // convert signal args to QVariantList
    QVariantList args;
    for (int i = 0; i < signal.parameterCount(); ++i)
        args << QVariant(signal.parameterType(i), a[i+1]);

    // forward event to all peers
    for (auto peer: m_peers)
        peer->sendEvent(event_name, args);

    return -1;
}
