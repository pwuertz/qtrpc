#ifndef QRPCPEER_H
#define QRPCPEER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>
#include <QtPromise>

class QIODevice;

class QRpcPromise : public QtPromise::QPromise<QVariant>
{
public:
    using Resolve = QtPromise::QPromiseResolve<QVariant>;
    using Reject = QtPromise::QPromiseReject<QVariant>;

    QRpcPromise()
        : QtPromise::QPromise<QVariant>([](const Resolve& r) { r(QVariant{}); })
    { }

    template <typename F>
    QRpcPromise(F&& resolver) : QtPromise::QPromise<QVariant>(std::forward<F>(resolver)) { }

private:
    virtual void __compilerGuide__();
};
Q_DECLARE_METATYPE(QRpcPromise)


class QRpcPeer : public QObject
{
    Q_OBJECT
public:
    /**
     * @brief QRpcPeer Create new QRpcPeer operating on existing IO device.
     * @param device IO device for reading/writing.
     * @param parent QObject parent.
     */
    explicit QRpcPeer(QIODevice* device, QObject* parent = nullptr);

    ~QRpcPeer() override;

signals:
    /*
     * Received event from peer.
     */
    void newEvent(const QString& name, const QVariant& data);

    /**
     * @brief newRequest Received new request from connected peer.
     * @param method Request method name.
     * @param args Request arguments.
     * @param resolve Request resolver.
     * @param reject Request rejecter.
     */
    void newRequest(const QString& method, const QVariant& args,
                    const QRpcPromise::Resolve& resolve, const QRpcPromise::Reject& reject);

public slots:
    /**
     * @brief sendRequest Send request to peer.
     * @param method Request method.
     * @param arg Request argument(s).
     * @return Promise fulfilled once the request finished.
     */
    QRpcPromise sendRequest(const QString& method, const QVariant& arg=QVariant());

    /**
     * Overloaded method for sending multiple request arguments.
     */
    QRpcPromise sendRequest(const QString& method, const QVariantList& args);

    /**
     * @brief sendEvent Send event to peer.
     * @param name Event name.
     * @param data Event data.
     */
    void sendEvent(const QString& name, const QVariant& data=QVariant());

    /**
     * @brief device Return the QIODevice the rpc peer is operating on.
     * @return IO device.
     */
    QIODevice* device();

private:
    class Private;
    std::unique_ptr<Private> p;
};


/**
 * @brief QRpcRequestMap conveniently batches multiple requests and returns them as QVariantMap.
 */
struct QRpcRequestMap
{
    /**
     * @brief QRpcRequestMap Create request map for given peer.
     * @param peer RPC peer.
     * @param objname Objectname used as prefix for all requests.
     */
    QRpcRequestMap(QRpcPeer& peer, QString objname="")
        : m_peer(peer)
        , m_objname(std::move(objname))
    { }

    /**
     * @brief add Add request to map using the given name.
     * @param name Key.
     * @param request Request.
     */
    inline QRpcRequestMap& add(QString name, QtPromise::QPromise<QVariant> request)
    {
        m_keys.emplace_back(std::move(name));
        m_requests.emplace_back(std::move(request));
        return *this;
    }

    /**
     * @brief add Make and add request for given method name.
     * @param name Method name.
     */
    inline QRpcRequestMap& add(const QString& name)
    {
        return add(name, makeRequest(name));
    }

    /**
     * @brief add Make and add request for given method name.
     * @param name Method name.
     * @param converter Conversion function.
     */
    template <typename Func>
    inline QRpcRequestMap& add(const QString& name, Func converter)
    {
        return add(name, makeRequest(name).then(converter));
    }

    /**
     * @brief all Return promise for all previously added requests.
     * @return Promise for all requests.
     */
    QtPromise::QPromise<QVariantMap> all()
    {
        auto p = QtPromise::QPromise<QVariant>::all(m_requests);
        m_requests.clear();
        return p.then([keys=std::move(m_keys)](const QVector<QVariant>& vals) {
            QVariantMap map;
            for (auto [k, v]=std::make_tuple(keys.cbegin(), vals.cbegin()); k != keys.cend(); ++k, ++v) {
                map.insert(*k, *v);
            }
            return map;
        });
    }

    /**
     * @brief wait Wait until all promises are fulfilled. Return result or throw on error.
     * @return Result of all requests.
     */
    QVariantMap wait()
    {
        QVariantMap result;
        std::exception_ptr eptr;
        all().then([&](const QVariantMap& v) {
            result = v;
        }).fail([&]() {
            eptr = std::current_exception();
        }).wait();
        if (eptr) {
            std::rethrow_exception(eptr);
        }
        return result;
    }

private:
    inline QtPromise::QPromise<QVariant> makeRequest(const QString& method) {
        return m_peer.sendRequest(
            !m_objname.isEmpty() ? QStringLiteral("%1.%2").arg(m_objname, method) : method)
        .then([](const QVariant& v) {
            return v;
        });
    }

    QRpcPeer& m_peer;
    QString m_objname;
    std::list<QString> m_keys;
    std::list<QtPromise::QPromise<QVariant>> m_requests;
};

#endif // QRPCPEER_H
