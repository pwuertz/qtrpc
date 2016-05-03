#include "qrpcpeer.h"
#include "msgpackrpcprotocol.h"
#include "qtmsgpackadaptor.h"

#include <cstdint>
#include <QIODevice>
#include <QTimer>
#include <QDebug>
#include "qrpcrequest.h"
#include "qrpcresponse.h"

class QRpcPeer::Private {
public:
    // TODO: QIODevice is not a sufficient Stream implementation for MsgpackRpcProtocol since write() may transfer fewer bytes than supplied
    // -> need write buffer on top of QIODevice
    Private(QRpcPeer* base, QIODevice* device) :
        b(base), m_device(device), m_protocol(*device, *this) {}

    void handleRequest(const std::string& method, const msgpack::object& o, std::uint64_t id);
    void handleResponse(std::uint64_t id, const msgpack::object& o);
    void handleError(std::uint64_t id, const std::string& e);
    void handleEvent(const std::string& name, const msgpack::object& o);

    QRpcPeer* b;
    QIODevice* m_device;
    MsgpackRpcProtocol<QIODevice, Private> m_protocol;
    std::uint64_t m_id_count = 1;
    std::map<std::uint64_t, QRpcResponse*> m_pending_responses;
    std::map<std::uint64_t, QRpcRequest*> m_pending_requests;
};

QRpcPeer::QRpcPeer(QIODevice* device, QObject *parent) : QObject(parent)
{
    p = std::unique_ptr<Private>(new Private(this, device));

    connect(device, &QIODevice::readyRead, this, [this]() {
        p->m_protocol.readAvailableBytes();
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
