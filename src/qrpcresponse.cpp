#include "qrpcresponse.h"
#include "qrpcpeer.h"
#include <QTimer>
#include <QDebug>

QRpcResponse::QRpcResponse(quint64 id, QRpcPeer* peer, QObject *parent) :
    QObject(parent), m_id(id), m_peer(peer), m_result_set(false)
{
    connect(peer, &QRpcPeer::destroyed, this, [this](){
        m_peer = nullptr;
    });
}

QRpcResponse::~QRpcResponse()
{

}

void QRpcResponse::setResult(const QVariant &v)
{
    m_result_set = true;
    m_result = v;
    // TODO: force queued connection here?
    finished();
    disconnect(this, &QRpcResponse::error, 0, 0);
    disconnect(this, &QRpcResponse::finished, 0, 0);
}

void QRpcResponse::setError(const QString &e)
{
    m_error = e;
    // TODO: force queued connection here?
    error(m_error);
    finished();
    disconnect(this, &QRpcResponse::error, 0, 0);
    disconnect(this, &QRpcResponse::finished, 0, 0);
}

QVariant QRpcResponse::result()
{
    return m_result;
}

QString QRpcResponse::errorString()
{
    return m_error;
}

QRpcPeer *QRpcResponse::peer()
{
    return m_peer;
}
