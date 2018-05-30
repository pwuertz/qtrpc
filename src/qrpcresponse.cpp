#include "qrpcresponse.h"
#include "qrpcpeer.h"
#include <QTimer>
#include <QDebug>

QRpcResponse::QRpcResponse(quint64 id, QRpcPeer* peer, QObject* parent)
    : QObject(parent)
    , m_id(id)
    , m_peer(peer)
{
    connect(peer, &QRpcPeer::destroyed, this, [this](){
        m_peer = nullptr;
    });
}

QRpcResponse::~QRpcResponse() = default;

void QRpcResponse::setResult(const QVariant &v)
{
    m_result_set = true;
    m_result = v;
    // TODO: force queued connection here?
    emit finished();
    disconnect(this, &QRpcResponse::error, nullptr, nullptr);
    disconnect(this, &QRpcResponse::finished, nullptr, nullptr);
}

void QRpcResponse::setError(const QString &e)
{
    m_error = e;
    // TODO: force queued connection here?
    emit error(m_error);
    emit finished();
    disconnect(this, &QRpcResponse::error, nullptr, nullptr);
    disconnect(this, &QRpcResponse::finished, nullptr, nullptr);
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
