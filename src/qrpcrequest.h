#ifndef QRPCREQUEST_H
#define QRPCREQUEST_H

#include <QObject>
#include <QString>
#include <QVariant>

class QRpcPeer;


/**
 * @brief The QRpcRequest class is a promise type representation of an RPC request.
 */
class QRpcRequest : public QObject
{
    Q_OBJECT
public:
    ~QRpcRequest() override;

signals:
    /**
     * @brief finished Signal emitted if the request finished.
     */
    void finished();

    /**
     * @brief error Signal emitted if the request returned an error.
     */
    void error(const QString& e);

public slots:
    /**
     * @brief method Request method name.
     */
    const QString& method();

    /**
     * @brief data Request call data.
     */
    const QVariant& data();

    /**
     * @brief setResult Resolve request by setting a value.
     */
    void setResult(const QVariant& v);

    /**
     * @brief setError Reject request by setting an error description.
     */
    void setError(const QString& e);

    /**
     * @brief peer Get the RPC peer associated with the request.
     */
    QRpcPeer* peer();

protected:
    friend class QRpcPeer;
    explicit QRpcRequest(QString method, QVariant data, quint64 id, QRpcPeer* peer, QObject *parent = nullptr);
    const QString m_method;
    const QVariant m_data;
    quint64 m_id = 0;
    QRpcPeer* m_peer = nullptr;
    bool m_result_set = false;
    QVariant m_result;
    QString m_error;
};

#endif // QRPCREQUEST_H
