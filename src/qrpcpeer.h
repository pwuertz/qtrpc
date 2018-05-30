#ifndef QRPCPEER_H
#define QRPCPEER_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <memory>

class QRpcRequest;
class QRpcResponse;
class QIODevice;


class QRpcPeer : public QObject
{
    Q_OBJECT
public:

    /*
     * Create QRpcPeer on top of device.
     */
    explicit QRpcPeer(QIODevice* device, QObject* parent = nullptr);

    ~QRpcPeer() override;

signals:
    /*
     * Received event from peer.
     */
    void newEvent(const QString& name, const QVariant& data);

    /*
     * Received request from peer.
     * After processing the request the result or an error is returned to
     * the peer by calling QRpcRequest::setResult or QRpcRequest::setError.
     * After that the request object should be deleted.
     */
    void newRequest(QRpcRequest* request);

public slots:

    /*
     * Send request to the peer.
     * When the peer replies to the request the QRpcResponse::finished signal
     * of the returned response object is emitted. This object should be destroyed
     * after that. It may also be destroyed before if the response is of no interest.
     */
    QRpcResponse* sendRequest(const QString& method, const QVariant& data);
    QRpcResponse* sendRequest(const QString& method, const QVariantList& data);

    /*
     * Send event to the peer.
     */
    void sendEvent(const QString& name, const QVariant& data=QVariant());

    /*
     * Return the QIODevice the rpc peer is operating on.
     */
    QIODevice* device();

private:
    class Private;
    std::unique_ptr<Private> p;
};

#endif // QRPCPEER_H
