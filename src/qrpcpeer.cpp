#include "qrpcpeer.h"
#include "msgpackrpcprotocol.h"
#include "qtmsgpackadaptor.h"

#include <cstdint>
#include <QIODevice>
#include <QTimer>
#include <QByteArray>
#include <QDebug>


class WriteBuffer {
    // TODO: This should be implemented as ring buffer
public:
    WriteBuffer(QIODevice* device, QObject* ctx) : m_device(device) {
        QObject::connect(device, &QIODevice::bytesWritten, ctx, [this](){
            // Device finished writing, check for buffered data
            if (!m_buffer.isEmpty()) {
                // Try to write buffered data to device
                int n_buffer = m_buffer.size();
                const char* p = m_buffer.constData() + m_buffer_pos;
                qint64 n_written = m_device->write(p, n_buffer - m_buffer_pos);
                m_buffer_pos += n_written;
                // Clear buffer when done
                if (m_buffer_pos == n_buffer) {
                    m_buffer.clear();
                    m_buffer_pos = 0;
                }
            }
        });
    }

    void write(const char *data, qint64 n_data) {
        if (!m_buffer.isEmpty()) {
            // Append new data to buffer if there is queued data
            m_buffer.append(data, static_cast<int>(n_data));
        } else {
            // Try to write new data to device
            auto n_written = m_device->write(data, n_data);
            // Append residual data to buffer
            if (n_written < n_data) {
                m_buffer.append(data + n_written, static_cast<int>(n_data - n_written));
            }
        }
    }

    qint64 m_buffer_pos = 0;
    QIODevice* m_device;
    QByteArray m_buffer;
};


class QRpcPeer::Private {
public:
    Private(QRpcPeer* base, QIODevice* device)
        : b(base)
        , m_device(device)
        , m_buffered_device(device, base)
        , m_protocol(*device, m_buffered_device, *this)
    {
        // Register QRpcPromise once
        [[maybe_unused]] static int promiseTypeId = qRegisterMetaType<QRpcPromise>();
    }

    void handleRequest(const std::string& method, const msgpack::object& o, std::uint64_t id);
    void handleResponse(std::uint64_t id, const msgpack::object& o);
    void handleError(std::uint64_t id, const std::string& e);
    void handleEvent(const std::string& name, const msgpack::object& o);

    void cancelPendingResponses();

    QRpcPeer* b = nullptr;
    QIODevice* m_device = nullptr;
    WriteBuffer m_buffered_device;
    MsgpackRpcProtocol<QIODevice, WriteBuffer, Private> m_protocol;
    std::uint64_t m_id_count = 1;

    using Resolvers = std::tuple<QRpcPromise::Resolve, QRpcPromise::Reject>;
    std::map<std::uint64_t, Resolvers> m_pending_responses;
};

QRpcPeer::QRpcPeer(QIODevice* device, QObject *parent)
    : QObject(parent)
    , p(std::make_unique<Private>(this, device))
{
    connect(device, &QIODevice::readyRead, this, [this]() {
        try {
            p->m_protocol.readAvailableBytes();
        } catch (const std::runtime_error& e) {
            // Close stream on error
            qWarning() << "QRpcPeer:" << e.what();
            p->m_device->close();
        }
    });
    // TODO: Cancel pending responses if device is closed/finished
}

QRpcPeer::~QRpcPeer()
{
    p->cancelPendingResponses();
}

QRpcPromise QRpcPeer::sendRequest(const QString& method, const QVariantList& args)
{
    return sendRequest(method, QVariant::fromValue(args));
}

QRpcPromise QRpcPeer::sendRequest(const QString& method, const QVariant& arg)
{
    // Send request to peer
    std::uint64_t id = p->m_id_count++;
    p->m_protocol.sendRequest(method.toStdString(), arg, id);

    // Create promise for pending response
    return QRpcPromise([&](const QRpcPromise::Resolve& resolve, const QRpcPromise::Reject& reject) {
        p->m_pending_responses.try_emplace(id, resolve, reject);
    });
}

void QRpcPeer::sendEvent(const QString& name, const QVariant& data)
{
    p->m_protocol.sendEvent(name.toStdString(), data);
}

QIODevice* QRpcPeer::device()
{
    return p->m_device;
}

void QRpcPeer::Private::handleRequest(const std::string& method, const msgpack::object& o, std::uint64_t id)
{
    QPointer<QRpcPeer> peer(b);
    auto p = QRpcPromise([&](const QRpcPromise::Resolve& resolve, const QRpcPromise::Reject& reject) {
        // Emit signal for new request, forwarding resolvers
        emit b->newRequest(QString::fromStdString(method), o.as<QVariant>(), resolve, reject);
    }).then([peer, id](const QVariant& result) {
        // Send reply once resolved
        if (!peer.isNull()) {
            peer->p->m_protocol.sendResponse(id, result);
        }
    }).fail([peer, id](const std::exception& e) {
        // Send error if request was rejected
        if (!peer.isNull()) {
            peer->p->m_protocol.sendError(id, e.what());
        }
    });
    // TODO: Track pending requests somewhere?
}

void QRpcPeer::Private::handleResponse(std::uint64_t id, const msgpack::object& o) {
    // Find pending response, ignore response if response ID is unknown
    auto response_iter = m_pending_responses.find(id);
    if (response_iter == m_pending_responses.end()) {
        return;
    }
    std::get<0>(response_iter->second)(o.as<QVariant>());  // Call resolve
    m_pending_responses.erase(response_iter);
}

void QRpcPeer::Private::handleError(std::uint64_t id, const std::string& e) {
    // Find pending response, ignore response if response ID is unknown
    auto response_iter = m_pending_responses.find(id);
    if (response_iter == m_pending_responses.end()) {
        return;
    }
    std::get<1>(response_iter->second)(std::runtime_error(e));  // Call reject
    m_pending_responses.erase(response_iter);
}

void QRpcPeer::Private::handleEvent(const std::string& name, const msgpack::object& o) {
    QVariant v = o.as<QVariant>();
    // TODO: force queued connection here?
    emit b->newEvent(QString::fromStdString(name), v);
}

void QRpcPeer::Private::cancelPendingResponses()
{
    for (const auto& kv: m_pending_responses) {
        std::get<1>(kv.second)(std::runtime_error("QRpcPeer destroyed before response"));
    }
    m_pending_responses.clear();
}

void QRpcPromise::__compilerGuide__()
{
    /* This method is only a hint for the compiler to find a translation unit for QRpcPromise */
}
