#ifndef QRPCSERVICE_H
#define QRPCSERVICE_H

#include <QObject>
#include <QTcpServer>
#include <QString>
#include <map>
#include <set>

class QRpcRequest;
class QRpcPeer;

class QRpcServiceBase : public QObject
{
    Q_OBJECT
public:
    virtual ~QRpcServiceBase();

public slots:
    void registerObject(QString name, QObject* o);
    void unregisterObject(QString name);

protected:
    explicit QRpcServiceBase(QTcpServer* server, QObject *parent = 0);
    void handleNewRequest(QRpcRequest* request);
    QTcpServer* m_server;
    std::map<QString, QObject*> m_reg_name_to_obj;
    std::map<QObject*, QString> m_reg_obj_to_name;
    std::set<QRpcPeer*> m_peers;

protected slots:
    void handleRegisteredObjectSignal();
};

class QRpcService : public QRpcServiceBase
{
public:
    explicit QRpcService(QTcpServer* server, QObject *parent = 0);
    virtual ~QRpcService();

    virtual int qt_metacall(QMetaObject::Call, int, void**);
private:
    static int s_id_handleRegisteredObjectSignal;
};

#endif // QRPCSERVICE_H
