#ifndef QRPCREQUEST_H
#define QRPCREQUEST_H

#include <QObject>
#include <QString>
#include <QVariant>

class QRpcPeer;


class QRpcRequest : public QObject
{
    Q_OBJECT
public:
    virtual ~QRpcRequest();

signals:
    void error(const QString& e);
    void finished();

public slots:
    const QString& method();
    const QVariant& data();
    void setResult(const QVariant& v);
    void setError(const QString& e);
    QRpcPeer* peer();

protected:
    friend class QRpcPeer;
    explicit QRpcRequest(const QString& method, const QVariant& data, quint64 id, QRpcPeer* peer, QObject *parent = 0);
    const QString m_method;
    const QVariant m_data;
    quint64 m_id;
    QRpcPeer* m_peer;
    bool m_result_set;
    QVariant m_result;
    QString m_error;
};

#endif // QRPCREQUEST_H
