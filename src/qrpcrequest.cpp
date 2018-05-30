#include "qrpcrequest.h"
#include "qrpcpeer.h"
#include <QTimer>
#include <QDebug>

QRpcRequest::QRpcRequest(QString method, QVariant data, quint64 id, QRpcPeer* peer, QObject *parent)
    : QObject(parent)
    , m_method(std::move(method))
    , m_data(std::move(data))
    , m_id(id)
    , m_peer(peer)
{
    connect(peer, &QRpcPeer::destroyed, this, [this]() {
        m_peer = nullptr;
    });
}

QRpcRequest::~QRpcRequest() = default;

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
    emit finished();
    disconnect(this, &QRpcRequest::error, nullptr, nullptr);
    disconnect(this, &QRpcRequest::finished, nullptr, nullptr);
}

void QRpcRequest::setError(const QString &e)
{
    m_error = e;
    // TODO: force queued connection here?
    emit error(m_error);
    emit finished();
    disconnect(this, &QRpcRequest::error, nullptr, nullptr);
    disconnect(this, &QRpcRequest::finished, nullptr, nullptr);
}

QRpcPeer* QRpcRequest::peer()
{
    return m_peer;
}
