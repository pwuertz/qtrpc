#ifndef QRPCSERVICE_H
#define QRPCSERVICE_H

#include <QObject>
#include <QTcpServer>
#include <map>
#include <set>
#include "qrpcpeer.h"


class QRpcServiceBase : public QObject
{
    Q_OBJECT
public:
    ~QRpcServiceBase() override;

public slots:
    /**
     * @brief registerObject Register a QObject for dispatching received RPC requests to it.
     * @param name Name for routing RPC requests.
     * @param o Object to be registered.
     */
    void registerObject(const QString& name, QObject* o);

    /**
     * @brief unregisterObject Unregister a previously registered name.
     * @param name Name to unregister.
     */
    void unregisterObject(const QString& name);

    /**
     * @brief numberOfPeers Return the current number of connected peers.
     * @return Number of peers.
     */
    size_t numberOfPeers() { return m_peers.size(); }

protected:
    explicit QRpcServiceBase(QTcpServer* server, QObject* parent = nullptr);

    void handleNewRequest(const QString& method, const QVariant& args,
                          const QRpcPromise::Resolve& resolve, const QRpcPromise::Reject& reject);

    QTcpServer* m_server = nullptr;
    std::map<QString, QObject*> m_reg_name_to_obj;
    std::map<QObject*, QString> m_reg_obj_to_name;
    std::set<QRpcPeer*> m_peers;

protected slots:
    void handleRegisteredObjectSignal();
};

class QRpcService : public QRpcServiceBase
{
public:
    explicit QRpcService(QTcpServer* server, QObject* parent = nullptr);
    ~QRpcService() override;

    int qt_metacall(QMetaObject::Call c, int id, void** a) override;
private:
    static int s_id_handleRegisteredObjectSignal;
};

#endif // QRPCSERVICE_H
