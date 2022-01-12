#include <QRpcService.hpp>
#include <QRpcPeer.hpp>
#include <QTcpSocket>
#include <QMetaObject>
#include <QMetaMethod>


// Call QMetaMethod with conversion from QVariant (based on https://gist.github.com/andref/2838534)
QVariant invokeAutoConvert(QObject* object, const QMetaMethod& metaMethod, const QVariantList& args)
{
    // Check if number of incoming args is sufficient or larger
    if (metaMethod.parameterCount() > args.size()) {
        qWarning() << "Insufficient arguments to call" << metaMethod.methodSignature();
        return {};
    }

    // Make a copy of incoming args and convert them to target types
    QVariantList converted;
    for (int i = 0; i < metaMethod.parameterCount(); i++) {
        const QVariant& arg = args.at(i);
        auto copy = QVariant{arg};
        const auto paramType = metaMethod.parameterMetaType(i);
        const auto argType = copy.metaType();
        bool paramIsVariant = (paramType.id() == QMetaType::QVariant);
        bool needConversion = !paramIsVariant && (argType != paramType);
        if (needConversion) {
            if (!copy.canConvert(paramType)) {
                // Special treatment for ->double conversion (e.g. long to double not allowed)
                if (paramType.id() == QMetaType::Double) {
                    bool ok;
                    copy = copy.toDouble(&ok);
                    if (!ok) {
                        qWarning() << "Cannot convert" << arg.typeName() << "to" << paramType.name();
                        return {};
                    }
                } else {
                    qWarning() << "Cannot convert" << arg.typeName() << "to" << paramType.name();
                    return {};
                }
            }
            if (!copy.convert(paramType)) {
                qWarning() << "Error converting" << arg.typeName() << "to" << paramType.name();
                return {};
            }
        }
        converted << copy;
    }

    // Build generic argument list from variant argument list
    QList<QGenericArgument> args_gen;
    for (const auto& argument: qAsConst(converted)) {
        args_gen << QGenericArgument(argument.metaType().name(), const_cast<void*>(argument.constData()));
    }

    // Build generic argument for return type
    const auto methodReturnT = metaMethod.returnMetaType();
    const auto methodReturnTID = methodReturnT.id();
    QVariant returnValue;
    QGenericReturnArgument returnArgument = [&]() -> QGenericReturnArgument {
        if (methodReturnTID != QMetaType::UnknownType && methodReturnTID != QMetaType::QVariant && methodReturnTID != QMetaType::Void) {
            // Create QVariant for known metatype and direct return value to internal data
            returnValue = QVariant(methodReturnT);
            return {metaMethod.typeName(), const_cast<void*>(returnValue.constData())};
        }
        if (methodReturnTID == QMetaType::QVariant) {
            // Write QVariant return values directly to returnValue
            return {metaMethod.typeName(), &returnValue};
        }
        // Ignore other return values
        return {"void", &returnValue};
    }();

    // Invoke method
    bool ok = metaMethod.invoke(object, Qt::DirectConnection, returnArgument,
        args_gen.value(0), args_gen.value(1), args_gen.value(2), args_gen.value(3),
        args_gen.value(4), args_gen.value(5), args_gen.value(6), args_gen.value(7),
        args_gen.value(8), args_gen.value(9)
    );

    if (!ok) {
        qWarning() << "Calling/converting" << metaMethod.methodSignature() << "failed.";
        return {};
    }
    return returnValue;
}


QRpcServiceBase::QRpcServiceBase(QTcpServer* server, QObject *parent) : QObject(parent), m_server(server)
{
    // New connection handler
    connect(server, &QTcpServer::newConnection, this, [this]() {
        QTcpSocket* socket;
        while ((socket = m_server->nextPendingConnection()) != nullptr) {
            auto* peer = new QRpcPeer(socket, socket);
            // Handle new rpc requests
            connect(peer, &QRpcPeer::newRequest, this, &QRpcService::handleNewRequest);
            // Remember peer
            m_peers.emplace(peer);
            // Delete socket (and peer) on disconnect
            connect(socket, &QTcpSocket::disconnected, this, [this, socket, peer]() {
                m_peers.erase(peer);
                socket->deleteLater();
                peer->deleteLater();
            });
        }
    });
}

QRpcServiceBase::~QRpcServiceBase()
{
    // Delete remaining peers
    for (auto* peer: m_peers) {
        peer->deleteLater();
    }
    m_peers.clear();
}

void QRpcServiceBase::registerObject(const QString& name, QObject* o)
{
    m_reg_name_to_obj.emplace(std::make_pair(name, o));
    m_reg_obj_to_name.emplace(std::make_pair(o, name));

    // Connect all signals of the registered object
    auto mo = o->metaObject();
    QMetaMethod handler = metaObject()->method(metaObject()->indexOfSlot("handleRegisteredObjectSignal()"));
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        QMetaMethod method = mo->method(i);
        if (method.methodType() == QMetaMethod::Signal && method.access() == QMetaMethod::Public) {
            connect(o, method, this, handler);
        }
    }

    // Unregister object if it is destroyed externally
    connect(o, &QObject::destroyed, this, [this, name](){
        unregisterObject(name);
    });
}

void QRpcServiceBase::unregisterObject(const QString& name)
{
    // Disconnect any signals from object to service and remove from map
    try {
        QObject* o = m_reg_name_to_obj.at(name);
        disconnect(o, nullptr, this, nullptr);
        m_reg_name_to_obj.erase(name);
        m_reg_obj_to_name.erase(o);
    } catch (const std::out_of_range&) {
        return;
    }
}

void QRpcServiceBase::handleNewRequest(
    const QString& method, const QVariant& args,
    const QRpcPromise::Resolve& resolve, const QRpcPromise::Reject& reject)
{
    const auto sep = method.indexOf('.');
    QString obj_name = (sep > 0) ? method.left(sep) : QStringLiteral("");
    QString method_name = method.mid(sep+1);

    // Find registered object or resolve with error
    auto obj_iter = m_reg_name_to_obj.find(obj_name);
    if (obj_iter == m_reg_name_to_obj.end()) {
        reject(std::runtime_error("RPC object not found"));
        return;
    }
    QObject* o = obj_iter->second;

    // Find requested method in Qt meta object
    const QMetaObject* mo = o->metaObject();
    for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
        const QMetaMethod mm = mo->method(i);
        if (mm.name() == method_name) {
            QVariant returnVal;
            QVariantList callArgs;
            if (args.isValid()) {
                if (args.metaType().id() == QMetaType::QVariantList) {
                    callArgs = args.toList();
                } else {
                    callArgs.append(args);
                }
            }
            // Try invoking method
			try {
                returnVal = invokeAutoConvert(o, mm, callArgs);
			}
            catch (const std::exception& e) {
                reject(std::runtime_error(e.what()));
            }
            catch (...) {
                reject(std::runtime_error("Unknown exception"));
            }
            // Resolve if return value is not an RPC promise or build chain
            const bool isRpcPromise = (returnVal.metaType() == QMetaType::fromType<QRpcPromise>());
            if (!isRpcPromise) {
                resolve(returnVal);
            } else {
                const auto& p = *reinterpret_cast<const QRpcPromise*>(returnVal.constData());
                p.then([=](const QVariant& result) {
                    resolve(result);
                }, [=](const std::exception& e) {
                    reject(std::runtime_error(e.what()));
                });
            }
            return;
        }
    }
    // Method was not found in meta object
    reject(std::runtime_error("RPC method not found"));
}

void QRpcServiceBase::handleRegisteredObjectSignal()
{
    // Dummy slot, handle signal in QRpcService::qt_metacall
}

int QRpcService::s_id_handleRegisteredObjectSignal = -1;

QRpcService::QRpcService(QTcpServer *server, QObject *parent) : QRpcServiceBase(server, parent)
{
    QRpcService::s_id_handleRegisteredObjectSignal = metaObject()->indexOfMethod("handleRegisteredObjectSignal()");
    Q_ASSERT(QRpcService::s_id_handleRegisteredObjectSignal != -1);
}

QRpcService::~QRpcService() = default;

int QRpcService::qt_metacall(QMetaObject::Call c, int id, void **a)
{
    // Intercept calls to handleRegisteredObjectSignal, let parent qt_metacall do the rest
    if (id != QRpcService::s_id_handleRegisteredObjectSignal) {
        return QRpcServiceBase::qt_metacall(c, id, a);
    }

    // Inspect sender and signal
    QObject* o = sender();
    QMetaMethod signal = o->metaObject()->method(senderSignalIndex());
    QString event_name = m_reg_obj_to_name.at(o) + "." + signal.name();

    // Convert signal args to QVariantList
    QVariantList args;
    for (int i = 0; i < signal.parameterCount(); ++i) {
        args << QVariant(signal.parameterMetaType(i), a[i+1]);
    }

    // Forward event to all peers
    for (auto peer: m_peers) {
        peer->sendEvent(event_name, args);
    }

    return -1;
}
