#include <QtTest/QtTest>
#include <QRpcPeer.hpp>
#include <QRpcService.hpp>


class RpcObject : public QObject
{
    Q_OBJECT

public slots:
    int method1(int a, int b) { return a + b; }
    QString method2(const QString& s) { return s.toUpper(); }
    QRpcPromise method3() { return QRpcPromise::resolve(42).delay(10); }

signals:
    void signal1(int value);
    void signal2(int value1, const QString& value2);
};

/**
 * @brief Test class
 */
class TestRpc : public QObject
{
    Q_OBJECT

    RpcObject rpcObj;
    QTcpServer server;
    QRpcService* service = nullptr;

private slots:
    void initTestCase()
    {
        server.listen();
        service = new QRpcService(&server, this);
        service->registerObject("obj", &rpcObj);
    }

    void testRpcRequests()
    {
        QTRY_VERIFY(service->numberOfPeers() == 0);
        QTcpSocket socket;
        socket.connectToHost(server.serverAddress(), server.serverPort());
        QVERIFY(socket.waitForConnected());
        auto peer = std::make_unique<QRpcPeer>(&socket);
        {
            // Call `method1`
            int result = 0;
            peer->sendRequest("obj.method1", {1, 2}).then([&](const QVariant& r) {
                result = r.toInt();
            }).wait();
            QVERIFY(result == 3);
        }
        {
            // Call `method2`
            QString result;
            peer->sendRequest("obj.method2", "Test").then([&](const QVariant& r) {
                result = r.toString();
            }).wait();
            QVERIFY(result == "TEST");
        }
        {
            // Call async `method3`
            int result = 0;
            peer->sendRequest("obj.method3").then([&](const QVariant& r) {
                result = r.toInt();
            }).wait();
            QVERIFY(result == 42);
        }
        {
            // RPC promise should reject on peer destruction
            auto p = peer->sendRequest("fail");
            peer.reset();
            p.wait();
            QVERIFY(p.isRejected());
        }
    }

    void testRpcEvents()
    {
        QTRY_VERIFY(service->numberOfPeers() == 0);
        QTcpSocket socket;
        socket.connectToHost(server.serverAddress(), server.serverPort());
        QVERIFY(socket.waitForConnected());
        QTRY_VERIFY(service->numberOfPeers() == 1);
        auto peer = std::make_unique<QRpcPeer>(&socket);

        QSignalSpy spy(peer.get(), &QRpcPeer::newEvent);
        {
            // Emit `signal1`
            emit rpcObj.signal1(42);
            QVERIFY(spy.wait());
            QVariantList result = spy.takeFirst().at(1).toList();
            QVERIFY(result.at(0) == 42);
        }
        {
            // Emit `signal2`
            emit rpcObj.signal2(42, "Hello World");
            QVERIFY(spy.wait());
            QVariantList result = spy.takeFirst().at(1).toList();
            QVERIFY(result.at(0) == 42);
            QVERIFY(result.at(1) == "Hello World");
        }
    }
};

QTEST_MAIN(TestRpc)
#include "test_rpc.moc"
