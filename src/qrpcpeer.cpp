#include "qrpcpeer.h"
#include "msgpackrpcprotocol.h"
#include "qtmsgpackadaptor.h"

#include <cstdint>
#include <QIODevice>
#include <QTimer>
#include <QByteArray>
#include <QDebug>
#include "qrpcrequest.h"
#include "qrpcresponse.h"


class WriteBuffer {
    // TODO: this should be implemented as ring buffer
public:
    WriteBuffer(QIODevice* device, QObject* ctx) : m_device(device) {
        QObject::connect(device, &QIODevice::bytesWritten, ctx, [this](){
            // device finished writing, check for buffered data
            if (!m_buffer.isEmpty()) {
                // try to write buffered data to device
                auto n_buffer = m_buffer.size();
                const char* p = m_buffer.constData() + m_buffer_pos;
                auto n_written = m_device->write(p, n_buffer - m_buffer_pos);
                m_buffer_pos += n_written;
                // clear buffer when done
                if (m_buffer_pos == n_buffer) {
                    m_buffer.clear();
                    m_buffer_pos = 0;
                }
            }
        });
    }

    void write(const char *data, qint64 n_data) {
        if (!m_buffer.isEmpty()) {
            // append new data to buffer if there is queued data
            m_buffer.append(data, n_data);
        } else {
            // try to write new data to device
            auto n_written = m_device->write(data, n_data);
            // append residual data to buffer
            if (n_written < n_data)
                m_buffer.append(data + n_written, n_data - n_written);
        }
    }

    QIODevice* m_device;
    QByteArray m_buffer;
    int m_buffer_pos = 0;
};


class QRpcPeer::Private {
public:
    Private(QRpcPeer* base, QIODevice* device) :
        b(base), m_device(device), m_buffered_device(device, base), m_protocol(*device, m_buffered_device, *this) {}

    void handleRequest(const std::string& method, const msgpack::object& o, std::uint64_t id);
    void handleResponse(std::uint64_t id, const msgpack::object& o);
    void handleError(std::uint64_t id, const std::string& e);
    void handleEvent(const std::string& name, const msgpack::object& o);

    QRpcPeer* b;
    QIODevice* m_device;
    WriteBuffer m_buffered_device;
    MsgpackRpcProtocol<QIODevice, WriteBuffer, Private> m_protocol;
    std::uint64_t m_id_count = 1;
    std::map<std::uint64_t, QRpcResponse*> m_pending_responses;
    std::map<std::uint64_t, QRpcRequest*> m_pending_requests;
};

QRpcPeer::QRpcPeer(QIODevice* device, QObject *parent) : QObject(parent)
{
    p = std::unique_ptr<Private>(new Private(this, device));

    connect(device, &QIODevice::readyRead, this, [this]() {
        try {
            p->m_protocol.readAvailableBytes();
        } catch (const std::runtime_error& e) {
            // close stream on error
            qWarning() << "QRpcPeer:" << e.what();
            p->m_device->close();
        }
    });
}

QRpcPeer::~QRpcPeer()
{
}

QRpcResponse* QRpcPeer::sendRequest(QString method, QVariantList data)
{
    return sendRequest(method, QVariant(data));
}

QRpcResponse* QRpcPeer::sendRequest(QString method, QVariant data)
{
    // send request and create response
    std::uint64_t id = p->m_id_count++;
    p->m_protocol.sendRequest(method.toStdString(), data, id);
    QRpcResponse* response = new QRpcResponse(id, this);

    // add to pending responses, remove on object destruction
    p->m_pending_responses.emplace(std::make_pair(id, response));
    connect(response, &QRpcResponse::destroyed, this, [this](QObject* o) {
        p->m_pending_responses.erase(static_cast<QRpcResponse*>(o)->m_id);
    });

    return response;
}

void QRpcPeer::sendEvent(QString name, QVariant data)
{
    p->m_protocol.sendEvent(name.toStdString(), data);
}

QIODevice* QRpcPeer::device()
{
    return p->m_device;
}

void QRpcPeer::Private::handleRequest(const std::string& method, const msgpack::object& o, std::uint64_t id)
{
    QRpcRequest* request;
    try {
        // remove and disconnect previous requests in case of id conflicts
        request = m_pending_requests.at(id);
        request->disconnect(request, 0, b, 0);
    } catch (const std::out_of_range& e) {
    }

    // create new pending request
    request = new QRpcRequest(QString::fromStdString(method), o.as<QVariant>(), id, b);
    m_pending_requests.emplace(std::make_pair(id, request));

    // send reply/error when request is finished
    connect(request, &QRpcRequest::finished, b, [this, request]() {
        m_pending_requests.erase(request->m_id);
        request->disconnect(request, 0, b, 0);
        if (request->m_result_set)
            m_protocol.sendResponse(request->m_id, request->m_result);
        else
            m_protocol.sendError(request->m_id, request->m_error.toStdString());
    });

    // remove request when deleted by the user
    connect(request, &QRpcRequest::destroyed, b, [this](QObject* o) {
        m_pending_requests.erase(static_cast<QRpcRequest*>(o)->m_id);
    });

    b->newRequest(request);
}

void QRpcPeer::Private::handleResponse(std::uint64_t id, const msgpack::object& o) {
    QRpcResponse* response;
    try {
        response = m_pending_responses.at(id);
        m_pending_responses.erase(id);
    } catch (std::out_of_range) {
        return;
    }
    response->setResult(o.as<QVariant>());
}

void QRpcPeer::Private::handleError(std::uint64_t id, const std::string& e) {
    QRpcResponse* response;
    try {
        response = m_pending_responses.at(id);
        m_pending_responses.erase(id);
    } catch (std::out_of_range) {
        return;
    }
    response->setError(QString::fromStdString(e));
}

void QRpcPeer::Private::handleEvent(const std::string& name_, const msgpack::object& o) {
    QString name = QString::fromStdString(name_);
    QVariant v = o.as<QVariant>();
    // TODO: force queued connection here?
    b->newEvent(name, v);
}
