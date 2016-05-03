#ifndef QRPCRESPONSE_H
#define QRPCRESPONSE_H

#include <QObject>
#include <QString>
#include <QVariant>

class QRpcPeer;


class QRpcResponse : public QObject
{
    Q_OBJECT
public:
    virtual ~QRpcResponse();

signals:
    void error(const QString& e);
    void finished();

public slots:
    QVariant result();
    QString errorString();

    QRpcPeer* peer();

protected:
    friend class QRpcPeer;
    explicit QRpcResponse(quint64 id, QRpcPeer* peer, QObject *parent = 0);
    void setResult(const QVariant& v);
    void setError(const QString& e);
    quint64 m_id;
    QRpcPeer* m_peer;
    bool m_result_set;
    QVariant m_result;
    QString m_error;
};

#endif // QRPCRESPONSE_H
