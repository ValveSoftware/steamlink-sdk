/****************************************************************************
**
** Copyright (C) 2014 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QObject>
#include <QtTest/QtTest>

#include <Qt3DCore/private/qboundedcircularbuffer_p.h>

using namespace Qt3DCore;

class tst_QBoundedCircularBuffer : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void construction();
    void clear();
    void push();
    void pop();
    void at();
    void producerConsumer();
};

class MyComplexType
{
public:
    MyComplexType(int xx = 0)
        : x(xx),
          magicCheck(ms_magic)
    {
        ++ms_activeCount;
    }

    MyComplexType(const MyComplexType &other)
        : x(other.x),
          magicCheck(other.magicCheck)
    {
        ++ms_activeCount;
    }

    ~MyComplexType()
    {
        // Check that the constructor was actually called
        QVERIFY(ms_magic == magicCheck);
        --ms_activeCount;
    }

    MyComplexType &operator=(const MyComplexType &other)
    {
        if (this == &other)
            return *this;
        x = other.x;
        magicCheck = other.magicCheck;
        return *this;
    }

    int x;
    int magicCheck;

    static const int ms_magic = 0xfefefefe;
    static int ms_activeCount;
};

int MyComplexType::ms_activeCount = 0;

void tst_QBoundedCircularBuffer::construction()
{
    QBoundedCircularBuffer<int> buffer(10);
    QVERIFY(buffer.capacity() == 10);
    QVERIFY(buffer.freeSize() == 10);
    QVERIFY(buffer.size() == 0);
    QVERIFY(buffer.isEmpty() == true);
    QVERIFY(buffer.isFull() == false);
}

void tst_QBoundedCircularBuffer::clear()
{
    QBoundedCircularBuffer<int> buffer(10);
    buffer.clear();
    QVERIFY(buffer.capacity() == 10);
    QVERIFY(buffer.freeSize() == 10);
    QVERIFY(buffer.size() == 0);
    QVERIFY(buffer.isEmpty() == true);
    QVERIFY(buffer.isFull() == false);
}

void tst_QBoundedCircularBuffer::push()
{
    QBoundedCircularBuffer<MyComplexType> buffer(20);
    QVERIFY(buffer.freeSize() == 20);
    for (int i = 0; i < 15; i++) {
        const MyComplexType value(i);
        buffer.push(value);
        const MyComplexType testValue = buffer.back();
        QVERIFY(testValue.x == value.x);
    }
    QVERIFY(buffer.freeSize() == 5);
    QVERIFY(buffer.size() == 15);
}

void tst_QBoundedCircularBuffer::pop()
{
    QBoundedCircularBuffer<MyComplexType> buffer(20);
    for (int i = 0; i < 15; i++) {
        const MyComplexType value(i);
        buffer.push(value);
    }

    for (int j = 0; j < 10; j++) {
        const MyComplexType value = buffer.pop();
        QVERIFY(value.x == j);
    }
    QVERIFY(buffer.freeSize() == 15);
    QVERIFY(buffer.size() == 5);
}

void tst_QBoundedCircularBuffer::at()
{
    QBoundedCircularBuffer<MyComplexType> buffer(20);
    for (int i = 0; i < 10; i++) {
        const MyComplexType value(i);
        buffer.append(value);
    }

    for (int i = 0; i < 10; i++)
        QVERIFY(buffer.at(i).x == i);
}

class MyProducer : public QThread
{
    Q_OBJECT
public:
    MyProducer(QBoundedCircularBuffer<MyComplexType> *buffer)
        : QThread(),
          m_buffer(buffer)
    {
    }

    void run()
    {
        for (int i = 0; i < 10000; i++ ) {
            //qDebug() << "Producing" << i;
            const MyComplexType value(i);
            //if ( m_buffer->isFull() )
            //    qDebug() << i << "The buffer is full. Waiting for consumer...";
            m_buffer->push(value);

            // Uncomment and adjust this to slow the producer down
            // usleep(130);
        }
    }

private:
    QBoundedCircularBuffer<MyComplexType>* m_buffer;
};

class MyConsumer : public QThread
{
    Q_OBJECT
public:
    MyConsumer(QBoundedCircularBuffer<MyComplexType>* buffer)
        : QThread(),
          m_buffer(buffer)
    {}

    void run()
    {
        for (int i = 0; i < 10000; i++) {
            //qDebug() << "Consuming" << i;
            //if (m_buffer->isEmpty())
            //    qDebug() << i << "The buffer is empty. Waiting for producer...";
            const MyComplexType value = m_buffer->pop();
            QVERIFY(value.x == i);

            // Uncomment and adjust this to slow the consumer down
            //usleep(100);
        }
    }

private:
    QBoundedCircularBuffer<MyComplexType>* m_buffer;
};

void tst_QBoundedCircularBuffer::producerConsumer()
{
    QBoundedCircularBuffer<MyComplexType> *buffer = new QBoundedCircularBuffer<MyComplexType>(20);
    MyProducer producer(buffer);
    MyConsumer consumer(buffer);

    // Produce and consume...
    producer.start();
    consumer.start();

    // Wait until both threads are done
    producer.wait();
    consumer.wait();

    // Check all items have been consumed
    QVERIFY(buffer->freeSize() == 20);
    QVERIFY(buffer->size() == 0);

    // Cleanup
    delete buffer;
}

QTEST_APPLESS_MAIN(tst_QBoundedCircularBuffer)
#include "tst_qboundedcircularbuffer.moc"
