#include "qrpcrequest.h"
#include "qrpcpeer.h"
#include <QTimer>
#include <QDebug>

QRpcRequest::QRpcRequest(const QString& method, const QVariant& data, quint64 id, QRpcPeer* peer, QObject *parent) :
    QObject(parent), m_method(method), m_data(data), m_id(id), m_peer(peer), m_result_set(false)
{
    connect(peer, &QRpcPeer::destroyed, this, [this](){
        m_peer = nullptr;
    });
}

QRpcRequest::~QRpcRequest()
{

}

const QString &QRpcRequest::method()
{
    return m_method;
}

const QVariant &QRpcRequest::data()
{
    return m_data;
}

void QRpcRequest::setResult(const QVariant &v)
{
    m_result_set = true;
    m_result = v;
    // TODO: force queued connection here?
    finished();
    disconnect(this, &QRpcRequest::error, 0, 0);
    disconnect(this, &QRpcRequest::finished, 0, 0);
}

void QRpcRequest::setError(const QString &e)
{
    m_error = e;
    // TODO: force queued connection here?
    error(m_error);
    finished();
    disconnect(this, &QRpcRequest::error, 0, 0);
    disconnect(this, &QRpcRequest::finished, 0, 0);
}

QRpcPeer *QRpcRequest::peer()
{
    return m_peer;
}
