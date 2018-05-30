#include <QtTest/QtTest>
#include <memory>

/**
 * @brief Test class
 */
class TestRpc: public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
    }
};

QTEST_MAIN(TestRpc)
#include "test_rpc.moc"
