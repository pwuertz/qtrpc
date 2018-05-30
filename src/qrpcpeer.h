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

#endif // QRPCPEER_H
