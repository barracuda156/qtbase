// Copyright (C) 2021 The Qt Company Ltd.
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include <QTest>
#include <QTestEventLoop>
#include <QSignalSpy>

#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QElapsedTimer>
#include <QtCore/QFile>
#include <QtCore/QThread>
#include <QtCore/QTemporaryDir>
#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>
#include <QtCore/QMetaType>
#include <QtCore/QScopeGuard>
#include <QtNetwork/QHostInfo>

#include <qplatformdefs.h>
#ifdef Q_OS_UNIX
#  include <private/qcore_unix_p.h>
#  include <sys/wait.h>
#endif

#include <QtTest/private/qemulationdetector_p.h>

#include <stdlib.h>

#include "crasher.h"

using namespace Qt::StringLiterals;

typedef void (QProcess::*QProcessErrorSignal)(QProcess::ProcessError);

class tst_QProcess : public QObject
{
    Q_OBJECT

public slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

private slots:
    void getSetCheck();
    void constructing();
    void simpleStart();
    void startCommand();
    void startCommandEmptyString();
    void startWithOpen();
    void startWithOldOpen();
    void execute();
    void startDetached();
    void crashTest();
    void crashTest2();
    void echoTest_data();
    void echoTest();
    void echoTest2();
#ifdef Q_OS_WIN
    void echoTestGui();
    void testSetNamedPipeHandleState();
    void batFiles_data();
    void batFiles();
#endif
    void loopBackTest();
    void readTimeoutAndThenCrash();
    void deadWhileReading();
    void restartProcessDeadlock();
    void closeWriteChannel();
    void closeReadChannel();
    void openModes();
    void emitReadyReadOnlyWhenNewDataArrives();
    void softExitInSlots_data();
    void softExitInSlots();
    void mergedChannels();
    void forwardedChannels_data();
    void forwardedChannels();
    void atEnd();
    void atEnd2();
    void waitForFinishedWithTimeout();
    void waitForReadyReadInAReadyReadSlot();
    void waitForBytesWrittenInABytesWrittenSlot();
    void setEnvironment_data();
    void setEnvironment();
    void setProcessEnvironment_data();
    void setProcessEnvironment();
    void environmentIsSorted();
    void spaceInName();
    void setStandardInputFile();
    void setStandardInputFileFailure();
    void setStandardOutputFile_data();
    void setStandardOutputFile();
    void setStandardOutputFileFailure_data() { setStandardOutputFile_data(); }
    void setStandardOutputFileFailure();
    void setStandardOutputFileNullDevice();
    void setStandardOutputFileAndWaitForBytesWritten();
    void setStandardOutputProcess_data();
    void setStandardOutputProcess();
    void removeFileWhileProcessIsRunning();
    void fileWriterProcess();
    void switchReadChannels();
    void discardUnwantedOutput();
    void setWorkingDirectory();
    void setNonExistentWorkingDirectory();
    void detachedSetNonExistentWorkingDirectory();

    void exitStatus_data();
    void exitStatus();
    void waitForFinished();
    void hardExit();
    void softExit();
    void processInAThread();
    void processesInMultipleThreads();
    void spaceArgsTest_data();
    void spaceArgsTest();
#if defined(Q_OS_WIN)
    void nativeArguments();
    void createProcessArgumentsModifier();
#endif // Q_OS_WIN
#if defined(Q_OS_UNIX)
    void setChildProcessModifier_data();
    void setChildProcessModifier();
    void failChildProcessModifier_data() { setChildProcessModifier_data(); }
    void failChildProcessModifier();
    void throwInChildProcessModifier();
    void terminateInChildProcessModifier_data();
    void terminateInChildProcessModifier();
    void raiseInChildProcessModifier();
    void unixProcessParameters_data();
    void unixProcessParameters();
    void impossibleUnixProcessParameters_data();
    void impossibleUnixProcessParameters();
    void unixProcessParametersAndChildModifier();
    void unixProcessParametersOtherFileDescriptors();
#endif
    void exitCodeTest();
    void systemEnvironment();
    void lockupsInStartDetached();
    void waitForReadyReadForNonexistantProcess();
    void detachedProcessParameters_data();
    void detachedProcessParameters();
    void startFinishStartFinish();
    void invalidProgramString_data();
    void invalidProgramString();
    void onlyOneStartedSignal();
    void finishProcessBeforeReadingDone();
    void waitForStartedWithoutStart();
    void startStopStartStop();
    void startStopStartStopBuffers_data();
    void startStopStartStopBuffers();
    void processEventsInAReadyReadSlot_data();
    void processEventsInAReadyReadSlot();
    void startFromCurrentWorkingDir_data();
    void startFromCurrentWorkingDir();

    // keep these at the end, since they use lots of processes and sometimes
    // caused obscure failures to occur in tests that followed them (esp. on the Mac)
    void failToStart();
    void failToStartWithWait();
    void failToStartWithEventLoop();
    void failToStartEmptyArgs_data();
    void failToStartEmptyArgs();

protected slots:
    void readFromProcess();
    void exitLoopSlot();
    void processApplicationEvents();
    void restartProcess();
    void waitForReadyReadInAReadyReadSlotSlot();
    void waitForBytesWrittenInABytesWrittenSlotSlot();

private:
    QString nonExistentFileName = u"/this/file/cant/exist/hopefully"_s;

    qint64 bytesAvailable;
    QTemporaryDir m_temporaryDir;
    bool haveWorkingVFork = false;
};

void tst_QProcess::initTestCase()
{
#if defined(QT_ASAN_ENABLED)
    QSKIP("Skipping QProcess tests under ASAN as they are flaky (QTBUG-109329)");
#endif
    QVERIFY2(m_temporaryDir.isValid(), qPrintable(m_temporaryDir.errorString()));
    // chdir to our testdata path and execute helper apps relative to that.
    QString testdata_dir = QFileInfo(QFINDTESTDATA("testProcessNormal")).absolutePath();
    QVERIFY2(QDir::setCurrent(testdata_dir), qPrintable("Could not chdir to " + testdata_dir));

#if defined(Q_OS_LINUX) && QT_CONFIG(forkfd_pidfd)
    // see detect_clone_pidfd_support() in forkfd_linux.c for explanation
    waitid(/*P_PIDFD*/ idtype_t(3), INT_MAX, NULL, WEXITED|WNOHANG);
    haveWorkingVFork = (errno == EBADF);
#endif
}

void tst_QProcess::cleanupTestCase()
{
}

void tst_QProcess::init()
{
    bytesAvailable = 0;
}

// Testing get/set functions
void tst_QProcess::getSetCheck()
{
    QProcess obj1;
    // ProcessChannelMode QProcess::readChannelMode()
    // void QProcess::setProcessChannelMode(ProcessChannelMode)
    obj1.setProcessChannelMode(QProcess::ProcessChannelMode(QProcess::SeparateChannels));
    QCOMPARE(QProcess::ProcessChannelMode(QProcess::SeparateChannels), obj1.processChannelMode());
    obj1.setProcessChannelMode(QProcess::ProcessChannelMode(QProcess::MergedChannels));
    QCOMPARE(QProcess::ProcessChannelMode(QProcess::MergedChannels), obj1.processChannelMode());
    obj1.setProcessChannelMode(QProcess::ProcessChannelMode(QProcess::ForwardedChannels));
    QCOMPARE(QProcess::ProcessChannelMode(QProcess::ForwardedChannels), obj1.processChannelMode());

    // ProcessChannel QProcess::readChannel()
    // void QProcess::setReadChannel(ProcessChannel)
    obj1.setReadChannel(QProcess::ProcessChannel(QProcess::StandardOutput));
    QCOMPARE(QProcess::ProcessChannel(QProcess::StandardOutput), obj1.readChannel());
    obj1.setReadChannel(QProcess::ProcessChannel(QProcess::StandardError));
    QCOMPARE(QProcess::ProcessChannel(QProcess::StandardError), obj1.readChannel());
}

void tst_QProcess::constructing()
{
    QProcess process;
    QCOMPARE(process.readChannel(), QProcess::StandardOutput);
    QCOMPARE(process.workingDirectory(), QString());
    QCOMPARE(process.environment(), QStringList());
    QCOMPARE(process.error(), QProcess::UnknownError);
    QCOMPARE(process.state(), QProcess::NotRunning);
    QCOMPARE(process.processId(), 0);
    QCOMPARE(process.readAllStandardOutput(), QByteArray());
    QCOMPARE(process.readAllStandardError(), QByteArray());
    QCOMPARE(process.canReadLine(), false);

    // QIODevice
    QCOMPARE(process.openMode(), QIODevice::NotOpen);
    QVERIFY(!process.isOpen());
    QVERIFY(!process.isReadable());
    QVERIFY(!process.isWritable());
    QVERIFY(process.isSequential());
    QCOMPARE(process.pos(), qlonglong(0));
    QCOMPARE(process.size(), qlonglong(0));
    QVERIFY(process.atEnd());
    QCOMPARE(process.bytesAvailable(), qlonglong(0));
    QCOMPARE(process.bytesToWrite(), qlonglong(0));
    QVERIFY(!process.errorString().isEmpty());

    char c;
    QCOMPARE(process.read(&c, 1), qlonglong(-1));
    QCOMPARE(process.write(&c, 1), qlonglong(-1));

    QProcess proc2;
}

void tst_QProcess::simpleStart()
{
    qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");

    QScopedPointer<QProcess> process(new QProcess);
    QSignalSpy spy(process.data(), &QProcess::stateChanged);
    QVERIFY(spy.isValid());
    connect(process.data(), &QIODevice::readyRead, this, &tst_QProcess::readFromProcess);

    /* valgrind dislike SUID binaries(those that have the `s'-flag set), which
     * makes it fail to start the process. For this reason utilities like `ping' won't
     * start, when the auto test is run through `valgrind'. */
    process->start("testProcessNormal/testProcessNormal");
    if (process->state() != QProcess::Starting)
        QCOMPARE(process->state(), QProcess::Running);
    QVERIFY2(process->waitForStarted(5000), qPrintable(process->errorString()));
    QCOMPARE(process->state(), QProcess::Running);
    QTRY_COMPARE(process->state(), QProcess::NotRunning);

    process.reset();

    QCOMPARE(spy.size(), 3);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(spy.at(0).at(0)), QProcess::Starting);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(spy.at(1).at(0)), QProcess::Running);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(spy.at(2).at(0)), QProcess::NotRunning);
}

void tst_QProcess::startCommand()
{
    QProcess process;
    process.startCommand("testProcessSpacesArgs/nospace foo \"b a r\" baz");
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(), qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QByteArray actual = process.readAll();
    actual.remove(0, actual.indexOf('|') + 1);
    QByteArray expected = "foo|b a r|baz";
    QCOMPARE(actual, expected);
}

void tst_QProcess::startCommandEmptyString()
{
    static const char warningMsg[] =
            "QProcess::startCommand: empty or whitespace-only command was provided";
    QProcess process;

    QTest::ignoreMessage(QtWarningMsg, warningMsg);
    process.startCommand("");
    QVERIFY(!process.waitForStarted());

    QTest::ignoreMessage(QtWarningMsg, warningMsg);
    process.startCommand("   ");
    QVERIFY(!process.waitForStarted());

    QTest::ignoreMessage(QtWarningMsg, warningMsg);
    process.startCommand("\t\n");
    QVERIFY(!process.waitForStarted());
}

void tst_QProcess::startWithOpen()
{
    QProcess p;
    QTest::ignoreMessage(QtWarningMsg, "QProcess::start: program not set");
    QCOMPARE(p.open(QIODevice::ReadOnly), false);

    p.setProgram("testProcessNormal/testProcessNormal");
    QCOMPARE(p.program(), QString("testProcessNormal/testProcessNormal"));

    p.setArguments(QStringList() << "arg1" << "arg2");
    QCOMPARE(p.arguments().size(), 2);

    QVERIFY(p.open(QIODevice::ReadOnly));
    QCOMPARE(p.openMode(), QIODevice::ReadOnly);
    QVERIFY(p.waitForFinished(5000));
}

void tst_QProcess::startWithOldOpen()
{
    // similar to the above, but we start with start() actually
    // while open() is overridden to call QIODevice::open().
    // This tests the BC requirement that "it works with the old implementation"
    class OverriddenOpen : public QProcess
    {
    public:
        virtual bool open(OpenMode mode) override
        { return QIODevice::open(mode); }
    };

    OverriddenOpen p;
    p.start("testProcessNormal/testProcessNormal");
    QVERIFY(p.waitForStarted(5000));
    QVERIFY(p.waitForFinished(5000));
}

void tst_QProcess::execute()
{
    QCOMPARE(QProcess::execute("testProcessNormal/testProcessNormal",
                               QStringList() << "arg1" << "arg2"), 0);
    QCOMPARE(QProcess::execute("nonexistingexe"), -2);
}

void tst_QProcess::startDetached()
{
    QVERIFY(QProcess::startDetached("testProcessNormal/testProcessNormal",
                                    QStringList() << "arg1" << "arg2"));
    QCOMPARE(QProcess::startDetached("nonexistingexe"), false);
}

void tst_QProcess::readFromProcess()
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    QVERIFY(process);
    while (process->canReadLine()) {
        process->readLine();
    }
}

void tst_QProcess::crashTest()
{
    qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");
    QScopedPointer<QProcess> process(new QProcess);
    QSignalSpy stateSpy(process.data(), &QProcess::stateChanged);
    QVERIFY(stateSpy.isValid());
    process->start("testProcessCrash/testProcessCrash");
    QVERIFY(process->waitForStarted(5000));

    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    QSignalSpy spy(process.data(), &QProcess::errorOccurred);
    QSignalSpy spy2(process.data(), &QProcess::finished);
    QVERIFY(spy.isValid());
    QVERIFY(spy2.isValid());

    QVERIFY(process->waitForFinished(30000));

    QCOMPARE(spy.size(), 1);
    QCOMPARE(*static_cast<const QProcess::ProcessError *>(spy.at(0).at(0).constData()), QProcess::Crashed);

    QCOMPARE(spy2.size(), 1);
    QCOMPARE(*static_cast<const QProcess::ExitStatus *>(spy2.at(0).at(1).constData()), QProcess::CrashExit);

    QCOMPARE(process->exitStatus(), QProcess::CrashExit);

    // delete process;
    process.reset();

    QCOMPARE(stateSpy.size(), 3);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(stateSpy.at(0).at(0)), QProcess::Starting);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(stateSpy.at(1).at(0)), QProcess::Running);
    QCOMPARE(qvariant_cast<QProcess::ProcessState>(stateSpy.at(2).at(0)), QProcess::NotRunning);
}

void tst_QProcess::crashTest2()
{
    QProcess process;
    process.start("testProcessCrash/testProcessCrash");
    QVERIFY(process.waitForStarted(5000));

    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    QSignalSpy spy(&process, static_cast<QProcessErrorSignal>(&QProcess::errorOccurred));
    QSignalSpy spy2(&process, &QProcess::finished);

    QVERIFY(spy.isValid());
    QVERIFY(spy2.isValid());

    QObject::connect(&process, &QProcess::finished, this, &tst_QProcess::exitLoopSlot);

    QTestEventLoop::instance().enterLoop(30);
    if (QTestEventLoop::instance().timeout())
        QFAIL("Failed to detect crash : operation timed out");

    QCOMPARE(spy.size(), 1);
    QCOMPARE(*static_cast<const QProcess::ProcessError *>(spy.at(0).at(0).constData()), QProcess::Crashed);

    QCOMPARE(spy2.size(), 1);
    QCOMPARE(*static_cast<const QProcess::ExitStatus *>(spy2.at(0).at(1).constData()), QProcess::CrashExit);

    QCOMPARE(process.exitStatus(), QProcess::CrashExit);
}

void tst_QProcess::echoTest_data()
{
    QTest::addColumn<QByteArray>("input");

    QTest::newRow("1") << QByteArray("H");
    QTest::newRow("2") << QByteArray("He");
    QTest::newRow("3") << QByteArray("Hel");
    QTest::newRow("4") << QByteArray("Hell");
    QTest::newRow("5") << QByteArray("Hello");
    QTest::newRow("100 bytes") << QByteArray(100, '@');
    QTest::newRow("1000 bytes") << QByteArray(1000, '@');
    QTest::newRow("10000 bytes") << QByteArray(10000, '@');
}

void tst_QProcess::echoTest()
{
    QFETCH(QByteArray, input);

    QProcess process;
    connect(&process, &QIODevice::readyRead, this, &tst_QProcess::exitLoopSlot);

    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted(5000));

    process.write(input);

    QElapsedTimer stopWatch;
    stopWatch.start();
    do {
        QVERIFY(process.isOpen());
        QTestEventLoop::instance().enterLoop(2);
    } while (stopWatch.elapsed() < 60000 && process.bytesAvailable() < input.size());
    if (stopWatch.elapsed() >= 60000)
        QFAIL("Timed out");

    QByteArray message = process.readAll();
    QCOMPARE(message.size(), input.size());

    char *c1 = message.data();
    char *c2 = input.data();
    while (*c1 && *c2) {
        if (*c1 != *c2)
            QCOMPARE(*c1, *c2);
        ++c1;
        ++c2;
    }
    QCOMPARE(*c1, *c2);

    process.write("", 1);

    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::exitLoopSlot()
{
    QTestEventLoop::instance().exitLoop();
}

void tst_QProcess::processApplicationEvents()
{
    QCoreApplication::processEvents();
}

void tst_QProcess::echoTest2()
{

    QProcess process;
    connect(&process, &QIODevice::readyRead, this, &tst_QProcess::exitLoopSlot);

    process.start("testProcessEcho2/testProcessEcho2");
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(!process.waitForReadyRead(250));
    QCOMPARE(process.error(), QProcess::Timedout);

    process.write("Hello");
    QSignalSpy spy0(&process, &QProcess::channelReadyRead);
    QSignalSpy spy1(&process, &QProcess::readyReadStandardOutput);
    QSignalSpy spy2(&process, &QProcess::readyReadStandardError);

    QVERIFY(spy0.isValid());
    QVERIFY(spy1.isValid());
    QVERIFY(spy2.isValid());

    QElapsedTimer stopWatch;
    stopWatch.start();
    forever {
        QTestEventLoop::instance().enterLoop(1);
        if (stopWatch.elapsed() >= 30000)
            QFAIL("Timed out");
        process.setReadChannel(QProcess::StandardOutput);
        qint64 baso = process.bytesAvailable();

        process.setReadChannel(QProcess::StandardError);
        qint64 base = process.bytesAvailable();
        if (baso == 5 && base == 5)
            break;
    }

    QVERIFY(spy0.size() > 0);
    QVERIFY(spy1.size() > 0);
    QVERIFY(spy2.size() > 0);

    QCOMPARE(process.readAllStandardOutput(), QByteArray("Hello"));
    QCOMPARE(process.readAllStandardError(), QByteArray("Hello"));

    process.write("", 1);
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

#if defined(Q_OS_WIN)
void tst_QProcess::echoTestGui()
{
    QProcess process;

    process.start("testProcessEchoGui/testProcessEchoGui");


    process.write("Hello");
    process.write("q");

    QVERIFY(process.waitForFinished(50000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    QCOMPARE(process.readAllStandardOutput(), QByteArray("Hello"));
    QCOMPARE(process.readAllStandardError(), QByteArray("Hello"));
}

void tst_QProcess::testSetNamedPipeHandleState()
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start("testSetNamedPipeHandleState/testSetNamedPipeHandleState");
    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
}
#endif // Q_OS_WIN

#if defined(Q_OS_WIN)
void tst_QProcess::batFiles_data()
{
    QTest::addColumn<QString>("batFile");
    QTest::addColumn<QByteArray>("output");

    QTest::newRow("simple") << QFINDTESTDATA("testBatFiles/simple.bat") << QByteArray("Hello");
    QTest::newRow("with space") << QFINDTESTDATA("testBatFiles/with space.bat") << QByteArray("Hello");
}

void tst_QProcess::batFiles()
{
    QFETCH(QString, batFile);
    QFETCH(QByteArray, output);

    QProcess proc;

    proc.start(batFile, QStringList());

    QVERIFY(proc.waitForFinished(5000));
    QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc.exitCode(), 0);

    QVERIFY(proc.bytesAvailable() > 0);

    QVERIFY(proc.readAll().startsWith(output));
}
#endif // Q_OS_WIN

void tst_QProcess::exitStatus_data()
{
    QTest::addColumn<QStringList>("processList");
    QTest::addColumn<QList<QProcess::ExitStatus> >("exitStatus");

    QTest::newRow("normal") << (QStringList() << "testProcessNormal/testProcessNormal")
                            << (QList<QProcess::ExitStatus>() << QProcess::NormalExit);
    QTest::newRow("crash") << (QStringList() << "testProcessCrash/testProcessCrash")
                            << (QList<QProcess::ExitStatus>() << QProcess::CrashExit);

    QTest::newRow("normal-crash") << (QStringList()
                                      << "testProcessNormal/testProcessNormal"
                                      << "testProcessCrash/testProcessCrash")
                                  << (QList<QProcess::ExitStatus>()
                                      << QProcess::NormalExit
                                      << QProcess::CrashExit);
    QTest::newRow("crash-normal") << (QStringList()
                                      << "testProcessCrash/testProcessCrash"
                                      << "testProcessNormal/testProcessNormal")
                                  << (QList<QProcess::ExitStatus>()
                                      << QProcess::CrashExit
                                      << QProcess::NormalExit);
}

void tst_QProcess::exitStatus()
{
    QProcess process;
    QFETCH(QStringList, processList);
    QFETCH(QList<QProcess::ExitStatus>, exitStatus);

    QCOMPARE(exitStatus.size(), processList.size());
    for (int i = 0; i < processList.size(); ++i) {
        process.start(processList.at(i));
        QVERIFY(process.waitForStarted(5000));
        QVERIFY(process.waitForFinished(30000));

        QCOMPARE(process.exitStatus(), exitStatus.at(i));
    }
}

void tst_QProcess::loopBackTest()
{

    QProcess process;
    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted(5000));

    for (int i = 0; i < 100; ++i) {
        process.write("Hello");
        do {
            QVERIFY(process.waitForReadyRead(5000));
        } while (process.bytesAvailable() < 5);
        QCOMPARE(process.readAll(), QByteArray("Hello"));
    }

    process.write("", 1);
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::readTimeoutAndThenCrash()
{

    QProcess process;
    process.start("testProcessEcho/testProcessEcho");
    if (process.state() != QProcess::Starting)
        QCOMPARE(process.state(), QProcess::Running);

    QVERIFY(process.waitForStarted(5000));
    QCOMPARE(process.state(), QProcess::Running);

    QVERIFY(!process.waitForReadyRead(5000));
    QCOMPARE(process.error(), QProcess::Timedout);

    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    QSignalSpy spy(&process, &QProcess::errorOccurred);
    QVERIFY(spy.isValid());

    process.kill();

    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.state(), QProcess::NotRunning);

    QCOMPARE(spy.size(), 1);
    QCOMPARE(*static_cast<const QProcess::ProcessError *>(spy.at(0).at(0).constData()), QProcess::Crashed);
}

void tst_QProcess::waitForFinished()
{
    QProcess process;

    process.start("testProcessOutput/testProcessOutput");

    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);

    QString output = process.readAll();
    QCOMPARE(output.count("\n"), 10*1024);

    process.start("blurdybloop");
    QVERIFY(!process.waitForFinished());
    QCOMPARE(process.error(), QProcess::FailedToStart);
}

void tst_QProcess::deadWhileReading()
{
    QProcess process;

    process.start("testProcessDeadWhileReading/testProcessDeadWhileReading");

    QString output;

    QVERIFY(process.waitForStarted(5000));
    while (process.waitForReadyRead(5000))
        output += process.readAll();

    QCOMPARE(output.count("\n"), 10*1024);
    process.waitForFinished();
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::restartProcessDeadlock()
{
    // The purpose of this test is to detect whether restarting a
    // process in the finished() connected slot causes a deadlock
    // because of the way QProcessManager uses its locks.
    QProcess process;
    connect(&process, &QProcess::finished, this, &tst_QProcess::restartProcess);

    process.start("testProcessEcho/testProcessEcho");

    QCOMPARE(process.write("", 1), qlonglong(1));
    QVERIFY(process.waitForFinished(5000));

    QObject::disconnect(&process, &QProcess::finished, nullptr, nullptr);

    QCOMPARE(process.write("", 1), qlonglong(1));
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::restartProcess()
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    QVERIFY(process);
    process->start("testProcessEcho/testProcessEcho");
}

void tst_QProcess::closeWriteChannel()
{
    QByteArray testData("Data to read");
    QProcess more;
    more.start("testProcessEOF/testProcessEOF");

    QVERIFY(more.waitForStarted(5000));
    QVERIFY(!more.waitForReadyRead(250));
    QCOMPARE(more.error(), QProcess::Timedout);

    QCOMPARE(more.write(testData), qint64(testData.size()));

    QVERIFY(!more.waitForReadyRead(250));
    QCOMPARE(more.error(), QProcess::Timedout);

    more.closeWriteChannel();
    // During closeWriteChannel() call, we might also get an I/O completion
    // on the read pipe. So, take this into account before waiting for
    // the new incoming data.
    while (more.bytesAvailable() < testData.size())
        QVERIFY(more.waitForReadyRead(5000));
    QCOMPARE(more.readAll(), testData);

    if (more.state() == QProcess::Running)
        QVERIFY(more.waitForFinished(5000));
    QCOMPARE(more.exitStatus(), QProcess::NormalExit);
    QCOMPARE(more.exitCode(), 0);
}

void tst_QProcess::closeReadChannel()
{
    for (int i = 0; i < 10; ++i) {
        QProcess::ProcessChannel channel1 = QProcess::StandardOutput;
        QProcess::ProcessChannel channel2 = QProcess::StandardError;

        QProcess proc;
        proc.start("testProcessEcho2/testProcessEcho2");
        QVERIFY(proc.waitForStarted(5000));
        proc.closeReadChannel(i&1 ? channel2 : channel1);
        proc.setReadChannel(i&1 ? channel2 : channel1);
        proc.write("Data");

        QVERIFY(!proc.waitForReadyRead(5000));
        QVERIFY(proc.readAll().isEmpty());

        proc.setReadChannel(i&1 ? channel1 : channel2);

        while (proc.bytesAvailable() < 4 && proc.waitForReadyRead(5000))
        { }

        QCOMPARE(proc.readAll(), QByteArray("Data"));

        proc.write("", 1);
        QVERIFY(proc.waitForFinished(5000));
        QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
        QCOMPARE(proc.exitCode(), 0);
    }
}

void tst_QProcess::openModes()
{
    QProcess proc;
    QVERIFY(!proc.isOpen());
    QCOMPARE(proc.openMode(), QProcess::NotOpen);
    proc.start("testProcessEcho3/testProcessEcho3");
    QVERIFY(proc.waitForStarted(5000));
    QVERIFY(proc.isOpen());
    QCOMPARE(proc.openMode(), QProcess::ReadWrite);
    QVERIFY(proc.isReadable());
    QVERIFY(proc.isWritable());

    proc.write("Data");

    proc.closeWriteChannel();

    QVERIFY(proc.isWritable());
    QCOMPARE(proc.openMode(), QProcess::ReadWrite);

    while (proc.bytesAvailable() < 4 && proc.waitForReadyRead(5000))
    { }

    QCOMPARE(proc.readAll().constData(), QByteArray("Data").constData());

    proc.closeReadChannel(QProcess::StandardOutput);

    QCOMPARE(proc.openMode(), QProcess::ReadWrite);
    QVERIFY(proc.isReadable());

    proc.closeReadChannel(QProcess::StandardError);

    QCOMPARE(proc.openMode(), QProcess::ReadWrite);
    QVERIFY(proc.isReadable());

    proc.close();
    QVERIFY(!proc.isOpen());
    QVERIFY(!proc.isReadable());
    QVERIFY(!proc.isWritable());
    QCOMPARE(proc.state(), QProcess::NotRunning);
}

void tst_QProcess::emitReadyReadOnlyWhenNewDataArrives()
{

    QProcess proc;
    connect(&proc, &QIODevice::readyRead, this, &tst_QProcess::exitLoopSlot);
    QSignalSpy spy(&proc, &QProcess::readyRead);
    QVERIFY(spy.isValid());

    proc.start("testProcessEcho/testProcessEcho");

    QCOMPARE(spy.size(), 0);

    proc.write("A");

    QTestEventLoop::instance().enterLoop(5);
    if (QTestEventLoop::instance().timeout())
        QFAIL("Operation timed out");

    QCOMPARE(spy.size(), 1);

    QTestEventLoop::instance().enterLoop(1);
    QVERIFY(QTestEventLoop::instance().timeout());
    QVERIFY(!proc.waitForReadyRead(250));

    QObject::disconnect(&proc, &QIODevice::readyRead, nullptr, nullptr);
    proc.write("B");
    QVERIFY(proc.waitForReadyRead(5000));

    proc.write("", 1);
    QVERIFY(proc.waitForFinished(5000));
    QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc.exitCode(), 0);
}

void tst_QProcess::hardExit()
{
    QProcess proc;

    proc.start("testProcessEcho/testProcessEcho");

    QVERIFY2(proc.waitForStarted(), qPrintable(proc.errorString()));

#if defined(Q_OS_QNX)
    // QNX may lose the kill if it's delivered while the forked process
    // is doing the exec that morphs it into testProcessEcho.  It's very
    // unlikely that a normal application would do such a thing.  Make
    // sure the test doesn't accidentally try to do it.
    proc.write("A");
    QVERIFY(proc.waitForReadyRead(5000));
#endif

    proc.kill();

    QVERIFY(proc.waitForFinished(5000));
    QCOMPARE(int(proc.state()), int(QProcess::NotRunning));
    QCOMPARE(int(proc.error()), int(QProcess::Crashed));
}

void tst_QProcess::softExit()
{
    QProcess proc;
    QCOMPARE(proc.processId(), 0);
    proc.start("testSoftExit/testSoftExit");

    QVERIFY(proc.waitForStarted(10000));
    QVERIFY(proc.waitForReadyRead(10000));

    QVERIFY(proc.processId() > 0);

    proc.terminate();

    QVERIFY(proc.waitForFinished(10000));
    QCOMPARE(int(proc.state()), int(QProcess::NotRunning));
    QCOMPARE(int(proc.error()), int(QProcess::UnknownError));
}

class SoftExitProcess : public QProcess
{
    Q_OBJECT
public:
    bool waitedForFinished;

    SoftExitProcess(int n) : waitedForFinished(false), n(n), killing(false)
    {
        connect(this, &QProcess::finished, this, &SoftExitProcess::finishedSlot);

        switch (n) {
        case 0:
            setProcessChannelMode(QProcess::MergedChannels);
            connect(this, &QIODevice::readyRead, this, &SoftExitProcess::terminateSlot);
            break;
        case 1:
            connect(this, &QProcess::readyReadStandardOutput,
                    this, &SoftExitProcess::terminateSlot);
            break;
        case 2:
            connect(this, &QProcess::readyReadStandardError,
                    this, &SoftExitProcess::terminateSlot);
            break;
        case 3:
            connect(this, &QProcess::started,
                    this, &SoftExitProcess::terminateSlot);
            break;
        case 4:
            setProcessChannelMode(QProcess::MergedChannels);
            connect(this, SIGNAL(channelReadyRead(int)), this, SLOT(terminateSlot()));
            break;
        default:
            connect(this, &QProcess::stateChanged,
                    this, &SoftExitProcess::terminateSlot);
            break;
        }
    }

    void writeAfterStart(const char *buf, int count)
    {
        dataToWrite = QByteArray(buf, count);
    }

    void start(const QString &program)
    {
        QProcess::start(program);
        writePendingData();
    }

public slots:
    void terminateSlot()
    {
        writePendingData(); // In cases 3 and 5 we haven't written the data yet.
        if (killing || (n == 5 && state() != Running)) {
            // Don't try to kill the process before it is running - that can
            // be hazardous, as the actual child process might not be running
            // yet. Also, don't kill it "recursively".
            return;
        }
        killing = true;
        readAll();
        terminate();
        if ((waitedForFinished = waitForFinished(5000)) == false) {
            kill();
            if (state() != NotRunning)
                waitedForFinished = waitForFinished(5000);
        }
    }

    void finishedSlot(int, QProcess::ExitStatus)
    {
        waitedForFinished = true;
    }

private:
    void writePendingData()
    {
        if (!dataToWrite.isEmpty()) {
            write(dataToWrite);
            dataToWrite.clear();
        }
    }

private:
    int n;
    bool killing;
    QByteArray dataToWrite;
};

void tst_QProcess::softExitInSlots_data()
{
    QTest::addColumn<QString>("appName");
    QTest::addColumn<int>("signalToConnect");

    QByteArray dataTagPrefix("gui app ");
#ifndef QT_NO_WIDGETS
    for (int i = 0; i < 6; ++i) {
        QTest::newRow(dataTagPrefix + QByteArray::number(i))
                << "testGuiProcess/testGuiProcess" << i;
    }
#endif

    dataTagPrefix = "console app ";
    for (int i = 0; i < 6; ++i) {
        QTest::newRow(dataTagPrefix + QByteArray::number(i))
                << "testProcessEcho2/testProcessEcho2" << i;
    }
}

void tst_QProcess::softExitInSlots()
{
    QFETCH(QString, appName);
    QFETCH(int, signalToConnect);

    SoftExitProcess proc(signalToConnect);
    proc.writeAfterStart("OLEBOLE", 8); // include the \0
    proc.start(appName);
    QTRY_VERIFY_WITH_TIMEOUT(proc.waitedForFinished, 60000);
    QCOMPARE(proc.state(), QProcess::NotRunning);
}

void tst_QProcess::mergedChannels()
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    QCOMPARE(process.processChannelMode(), QProcess::MergedChannels);

    process.start("testProcessEcho2/testProcessEcho2");

    QVERIFY(process.waitForStarted(5000));

    {
        QCOMPARE(process.write("abc"), qlonglong(3));
        while (process.bytesAvailable() < 6)
            QVERIFY(process.waitForReadyRead(5000));
        QCOMPARE(process.readAllStandardOutput(), QByteArray("aabbcc"));
        QTest::ignoreMessage(QtWarningMsg,
            "QProcess::readAllStandardError: Called with MergedChannels");
        QCOMPARE(process.readAllStandardError(), QByteArray());
    }

    for (int i = 0; i < 100; ++i) {
        QCOMPARE(process.write("abc"), qlonglong(3));
        while (process.bytesAvailable() < 6)
            QVERIFY(process.waitForReadyRead(5000));
        QCOMPARE(process.readAll(), QByteArray("aabbcc"));
    }

    process.closeWriteChannel();
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::forwardedChannels_data()
{
    QTest::addColumn<bool>("detach");
    QTest::addColumn<int>("mode");
    QTest::addColumn<int>("inmode");
    QTest::addColumn<QByteArray>("outdata");
    QTest::addColumn<QByteArray>("errdata");

    QTest::newRow("separate")
            << false
            << int(QProcess::SeparateChannels) << int(QProcess::ManagedInputChannel)
            << QByteArray() << QByteArray();
    QTest::newRow("forwarded")
            << false
            << int(QProcess::ForwardedChannels) << int(QProcess::ManagedInputChannel)
            << QByteArray("forwarded") << QByteArray("forwarded");
    QTest::newRow("stdout")
            << false
            << int(QProcess::ForwardedOutputChannel) << int(QProcess::ManagedInputChannel)
            << QByteArray("forwarded") << QByteArray();
    QTest::newRow("stderr")
            << false
            << int(QProcess::ForwardedErrorChannel) << int(QProcess::ManagedInputChannel)
            << QByteArray() << QByteArray("forwarded");
    QTest::newRow("fwdinput")
            << false
            << int(QProcess::ForwardedErrorChannel) << int(QProcess::ForwardedInputChannel)
            << QByteArray() << QByteArray("input");
    QTest::newRow("detached-default-forwarding")
            << true
            << int(QProcess::SeparateChannels) << int(QProcess::ManagedInputChannel)
            << QByteArray("out data") << QByteArray("err data");
    QTest::newRow("detached-merged-forwarding")
            << true
            << int(QProcess::MergedChannels) << int(QProcess::ManagedInputChannel)
            << QByteArray("out data" "err data") << QByteArray();
}

void tst_QProcess::forwardedChannels()
{
    QFETCH(bool, detach);
    QFETCH(int, mode);
    QFETCH(int, inmode);
    QFETCH(QByteArray, outdata);
    QFETCH(QByteArray, errdata);

    QProcess process;
    process.start("testForwarding/testForwarding",
                  QStringList() << QString::number(mode) << QString::number(inmode)
                                << QString::number(bool(detach)));
    QVERIFY(process.waitForStarted(5000));
    QCOMPARE(process.write("input"), 5);
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished(40000));    // testForwarding has a 30s wait
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    const char *err;
    switch (process.exitCode()) {
    case 0: err = "ok"; break;
    case 1: err = "processChannelMode is wrong"; break;
    case 11: err = "inputChannelMode is wrong"; break;
    case 2: err = "failed to start"; break;
    case 3: err = "failed to write"; break;
    case 4: err = "did not finish"; break;
    case 5: err = "unexpected stdout"; break;
    case 6: err = "unexpected stderr"; break;
    case 12: err = "cannot create temp file"; break;
    case 13: err = "startDetached failed"; break;
    case 14: err = "waitForDoneFileWritten timed out"; break;
    default: err = "unknown exit code"; break;
    }
    QVERIFY2(!process.exitCode(), err);
    QCOMPARE(process.readAllStandardOutput(), outdata);
    QCOMPARE(process.readAllStandardError(), errdata);
}

void tst_QProcess::atEnd()
{
    QProcess process;

    process.start("testProcessEcho/testProcessEcho");
    process.write("abcdefgh\n");

    while (process.bytesAvailable() < 8)
        QVERIFY(process.waitForReadyRead(5000));

    QTextStream stream(&process);
    QVERIFY(!stream.atEnd());
    QString tmp = stream.readLine();
    QVERIFY(stream.atEnd());
    QCOMPARE(tmp, QString::fromLatin1("abcdefgh"));

    process.write("", 1);
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

class TestThread : public QThread
{
    Q_OBJECT
public:
    inline int code()
    {
        return exitCode;
    }

protected:
    inline void run() override
    {
        exitCode = 90210;

        QProcess process;
        connect(&process, &QProcess::finished, this, &TestThread::catchExitCode, Qt::DirectConnection);

        process.start("testProcessEcho/testProcessEcho");

        QCOMPARE(process.write("abc\0", 4), qint64(4));
        exitCode = exec();
    }

protected slots:
    inline void catchExitCode(int exitCode)
    {
        this->exitCode = exitCode;
        exit(exitCode);
    }

private:
    int exitCode;
};

void tst_QProcess::processInAThread()
{
    for (int i = 0; i < 10; ++i) {
        TestThread thread;
        thread.start();
        QVERIFY(thread.wait(10000));
        QCOMPARE(thread.code(), 0);
    }
}

void tst_QProcess::processesInMultipleThreads()
{
    if (QTestPrivate::isRunningArmOnX86())
        QSKIP("Test is too slow to run on emulator");

#if defined(Q_OS_QNX)
    QSKIP("QNX: Large amount of threads is unstable and do not finish in given time");
#endif

    for (int i = 0; i < 10; ++i) {
        // run from 1 to 10 threads, but run at least some tests
        // with more threads than the ideal
        int threadCount = i;
        if (i > 7)
            threadCount = qMax(threadCount, QThread::idealThreadCount() + 2);

        QList<TestThread *> threads(threadCount);
        QScopeGuard cleanup([&threads]() { qDeleteAll(threads); });
        for (int j = 0; j < threadCount; ++j)
            threads[j] = new TestThread;
        for (int j = 0; j < threadCount; ++j)
            threads[j]->start();
        for (int j = 0; j < threadCount; ++j)
            QVERIFY(threads[j]->wait(10000));
        for (int j = 0; j < threadCount; ++j)
            QCOMPARE(threads[j]->code(), 0);
    }
}

void tst_QProcess::waitForFinishedWithTimeout()
{
    QProcess process;

    process.start("testProcessEcho/testProcessEcho");

    QVERIFY(process.waitForStarted(5000));
    QVERIFY(!process.waitForFinished(1));

    process.write("", 1);

    QVERIFY(process.waitForFinished());
}

void tst_QProcess::waitForReadyReadInAReadyReadSlot()
{
    QProcess process;
    connect(&process, &QIODevice::readyRead, this, &tst_QProcess::waitForReadyReadInAReadyReadSlotSlot);
    connect(&process, &QProcess::finished, this, &tst_QProcess::exitLoopSlot);
    bytesAvailable = 0;

    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted(5000));

    QSignalSpy spy(&process, &QProcess::readyRead);
    QVERIFY(spy.isValid());
    process.write("foo");
    QTestEventLoop::instance().enterLoop(30);
    QVERIFY(!QTestEventLoop::instance().timeout());

    QCOMPARE(spy.size(), 1);

    process.disconnect();
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QVERIFY(process.bytesAvailable() >= bytesAvailable);
}

void tst_QProcess::waitForReadyReadInAReadyReadSlotSlot()
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    QVERIFY(process);
    bytesAvailable = process->bytesAvailable();
    process->write("bar", 4);
    QVERIFY(process->waitForReadyRead(5000));
    QVERIFY(process->bytesAvailable() > bytesAvailable);
    bytesAvailable = process->bytesAvailable();
    QTestEventLoop::instance().exitLoop();
}

void tst_QProcess::waitForBytesWrittenInABytesWrittenSlot()
{
    QProcess process;
    connect(&process, &QIODevice::bytesWritten, this, &tst_QProcess::waitForBytesWrittenInABytesWrittenSlotSlot);
    bytesAvailable = 0;

    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted(5000));

    QSignalSpy spy(&process, &QProcess::bytesWritten);
    QVERIFY(spy.isValid());
    process.write("f");
    QTestEventLoop::instance().enterLoop(30);
    QVERIFY(!QTestEventLoop::instance().timeout());

    QCOMPARE(spy.size(), 1);
    process.write("", 1);
    process.disconnect();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::waitForBytesWrittenInABytesWrittenSlotSlot()
{
    QProcess *process = qobject_cast<QProcess *>(sender());
    QVERIFY(process);
    process->write("b");
    QVERIFY(process->waitForBytesWritten(5000));
    QTestEventLoop::instance().exitLoop();
}

void tst_QProcess::spaceArgsTest_data()
{
    QTest::addColumn<QStringList>("args");
    QTest::addColumn<QString>("stringArgs");

    // arg1 | arg2
    QTest::newRow("arg1 arg2") << (QStringList() << QString::fromLatin1("arg1") << QString::fromLatin1("arg2"))
                               << QString::fromLatin1("arg1 arg2");
    // "arg1" | ar "g2
    QTest::newRow("\"\"\"\"arg1\"\"\"\" \"ar \"\"\"g2\"") << (QStringList() << QString::fromLatin1("\"arg1\"") << QString::fromLatin1("ar \"g2"))
                                                          << QString::fromLatin1("\"\"\"\"arg1\"\"\"\" \"ar \"\"\"g2\"");
    // ar g1 | a rg 2
    QTest::newRow("\"ar g1\" \"a rg 2\"") << (QStringList() << QString::fromLatin1("ar g1") << QString::fromLatin1("a rg 2"))
                                          << QString::fromLatin1("\"ar g1\" \"a rg 2\"");
    // -lar g1 | -l"ar g2"
    QTest::newRow("\"-lar g1\" \"-l\"\"\"ar g2\"\"\"\"") << (QStringList() << QString::fromLatin1("-lar g1") << QString::fromLatin1("-l\"ar g2\""))
                                                         << QString::fromLatin1("\"-lar g1\" \"-l\"\"\"ar g2\"\"\"\"");
    // ar"g1
    QTest::newRow("ar\"\"\"\"g1") << (QStringList() << QString::fromLatin1("ar\"g1"))
                                  << QString::fromLatin1("ar\"\"\"\"g1");
    // ar/g1
    QTest::newRow("ar\\g1") << (QStringList() << QString::fromLatin1("ar\\g1"))
                            << QString::fromLatin1("ar\\g1");
    // ar\g"1
    QTest::newRow("ar\\g\"\"\"\"1") << (QStringList() << QString::fromLatin1("ar\\g\"1"))
                                    << QString::fromLatin1("ar\\g\"\"\"\"1");
    // arg\"1
    QTest::newRow("arg\\\"\"\"1") << (QStringList() << QString::fromLatin1("arg\\\"1"))
                                  << QString::fromLatin1("arg\\\"\"\"1");
    // """"
    QTest::newRow("\"\"\"\"\"\"\"\"\"\"\"\"") << (QStringList() << QString::fromLatin1("\"\"\"\""))
                                              << QString::fromLatin1("\"\"\"\"\"\"\"\"\"\"\"\"");
    // """" | "" ""
    QTest::newRow("\"\"\"\"\"\"\"\"\"\"\"\" \"\"\"\"\"\"\" \"\"\"\"\"\"\"") << (QStringList() << QString::fromLatin1("\"\"\"\"") << QString::fromLatin1("\"\" \"\""))
                                                                            << QString::fromLatin1("\"\"\"\"\"\"\"\"\"\"\"\" \"\"\"\"\"\"\" \"\"\"\"\"\"\"");
    // ""  ""
    QTest::newRow("\"\"\"\"\"\"\" \"\" \"\"\"\"\"\"\" (bogus double quotes)") << (QStringList() << QString::fromLatin1("\"\"  \"\""))
                                                                              << QString::fromLatin1("\"\"\"\"\"\"\" \"\" \"\"\"\"\"\"\"");
    // ""  ""
    QTest::newRow(" \"\"\"\"\"\"\" \"\" \"\"\"\"\"\"\"   (bogus double quotes)") << (QStringList() << QString::fromLatin1("\"\"  \"\""))
                                                                                 << QString::fromLatin1(" \"\"\"\"\"\"\" \"\" \"\"\"\"\"\"\"   ");
}

static QByteArray startFailMessage(const QString &program, const QProcess &process)
{
    QByteArray result = "Process '";
    result += program.toLocal8Bit();
    result += "' failed to start: ";
    result += process.errorString().toLocal8Bit();
    return result;
}

void tst_QProcess::spaceArgsTest()
{
    QFETCH(QStringList, args);
    QFETCH(QString, stringArgs);

    auto splitString = QProcess::splitCommand(stringArgs);
    QCOMPARE(args, splitString);

    QStringList programs;
    programs << QString::fromLatin1("testProcessSpacesArgs/nospace")
             << QString::fromLatin1("testProcessSpacesArgs/one space")
             << QString::fromLatin1("testProcessSpacesArgs/two space s");

    QProcess process;

    for (int i = 0; i < programs.size(); ++i) {
        QString program = programs.at(i);
        process.start(program, args);

        QByteArray errorMessage;
        bool started = process.waitForStarted();
        if (!started)
            errorMessage = startFailMessage(program, process);
        QVERIFY2(started, errorMessage.constData());
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);

        QStringList actual = QString::fromLatin1(process.readAll()).split("|");
        QVERIFY(!actual.isEmpty());
        // not interested in the program name, it might be different.
        actual.removeFirst();

        QCOMPARE(actual, args);
    }
}

#if defined(Q_OS_WIN)

void tst_QProcess::nativeArguments()
{
    QProcess proc;

    // This doesn't actually need special quoting, so it is pointless to use
    // native arguments here, but that's not the point of this test.
    proc.setNativeArguments("hello kitty, \"*\"!");

    proc.start(QString::fromLatin1("testProcessSpacesArgs/nospace"), QStringList());

    QVERIFY2(proc.waitForStarted(), qPrintable(proc.errorString()));
    QVERIFY(proc.waitForFinished());
    QCOMPARE(proc.exitStatus(), QProcess::NormalExit);
    QCOMPARE(proc.exitCode(), 0);

    QStringList actual = QString::fromLatin1(proc.readAll()).split(QLatin1Char('|'));
    QVERIFY(!actual.isEmpty());
    // not interested in the program name, it might be different.
    actual.removeFirst();
    QStringList expected;
    expected << "hello" << "kitty," << "*!";
    QCOMPARE(actual, expected);
}

void tst_QProcess::createProcessArgumentsModifier()
{
    int calls = 0;
    const QString reversedCommand = "lamroNssecorPtset/lamroNssecorPtset";
    QProcess process;
    process.setCreateProcessArgumentsModifier([&calls] (QProcess::CreateProcessArguments *args)
    {
        calls++;
        std::reverse(args->arguments, args->arguments + wcslen(args->arguments) - 1);
    });
    process.start(reversedCommand);
    QVERIFY2(process.waitForStarted(), qUtf8Printable(process.errorString()));
    QVERIFY(process.waitForFinished());
    QCOMPARE(calls, 1);

    process.setCreateProcessArgumentsModifier(QProcess::CreateProcessArgumentModifier());
    QVERIFY(!process.waitForStarted());
    QCOMPARE(calls, 1);
}
#endif // Q_OS_WIN

#ifdef Q_OS_UNIX
static constexpr int sigs[] = { SIGABRT, SIGILL, SIGSEGV };
struct DisableCrashLogger
{
    // disable core dumps too
    tst_QProcessCrash::NoCoreDumps disableCoreDumps {};
    std::array<struct sigaction, std::size(sigs)> oldhandlers;
    DisableCrashLogger()
    {
        struct sigaction def = {};
        def.sa_handler = SIG_DFL;
        for (uint i = 0; i < std::size(sigs); ++i)
            sigaction(sigs[i], &def, &oldhandlers[i]);
    }
    ~DisableCrashLogger()
    {
        // restore them
        for (uint i = 0; i < std::size(sigs); ++i)
            sigaction(sigs[i], &oldhandlers[i], nullptr);
    }
};

QT_BEGIN_NAMESPACE
Q_AUTOTEST_EXPORT bool _qprocessUsingVfork() noexcept;
QT_END_NAMESPACE
static constexpr char messageFromChildProcess[] = "Message from the child process";
static_assert(std::char_traits<char>::length(messageFromChildProcess) <= PIPE_BUF);
static void childProcessModifier(int fd)
{
    QT_WRITE(fd, messageFromChildProcess, strlen(messageFromChildProcess));
    QT_CLOSE(fd);
}

void tst_QProcess::setChildProcessModifier_data()
{
    QTest::addColumn<bool>("detached");
    QTest::addColumn<bool>("useVfork");
    QTest::newRow("normal") << false << false;
    QTest::newRow("detached") << true << false;

#ifdef QT_BUILD_INTERNAL
    if (_qprocessUsingVfork()) {
        QTest::newRow("normal-vfork") << false << true;
        QTest::newRow("detached-vfork") << true << true;
    }
#endif
}

void tst_QProcess::setChildProcessModifier()
{
    QFETCH(bool, detached);
    QFETCH(bool, useVfork);
    int pipes[2] = { -1 , -1 };
    QVERIFY(qt_safe_pipe(pipes) == 0);

    QProcess process;
    if (useVfork)
        process.setUnixProcessParameters(QProcess::UnixProcessFlag::UseVFork);
    process.setChildProcessModifier([pipes]() {
        ::childProcessModifier(pipes[1]);
    });
    process.setProgram("testProcessNormal/testProcessNormal");
    if (detached) {
        process.startDetached();
    } else {
        process.start("testProcessNormal/testProcessNormal");
        if (process.state() != QProcess::Starting)
            QCOMPARE(process.state(), QProcess::Running);
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
        QVERIFY2(process.waitForFinished(5000), qPrintable(process.errorString()));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
    }

    char buf[sizeof messageFromChildProcess] = {};
    qt_safe_close(pipes[1]);
    QCOMPARE(qt_safe_read(pipes[0], buf, sizeof(buf)), qint64(sizeof(messageFromChildProcess)) - 1);
    QCOMPARE(buf, messageFromChildProcess);
    qt_safe_close(pipes[0]);
}

void tst_QProcess::failChildProcessModifier()
{
    static const char failureMsg[] =
            "Some error message from the child process would go here if this were a "
            "real application";
    static_assert(sizeof(failureMsg) < _POSIX_PIPE_BUF / 2,
            "Implementation detail: the length of the message is limited");

    QFETCH(bool, detached);
    QFETCH(bool, useVfork);

    QProcess process;
    if (useVfork)
        process.setUnixProcessParameters(QProcess::UnixProcessFlag::UseVFork);
    process.setChildProcessModifier([&process]() {
        process.failChildProcessModifier(failureMsg, EPERM);
    });
    process.setProgram("testProcessNormal/testProcessNormal");

    if (detached) {
        qint64 pid;
        QVERIFY(!process.startDetached(&pid));
        QCOMPARE(pid, -1);
    } else {
        process.start();
        QVERIFY(!process.waitForStarted(5000));
    }

    QString errMsg = process.errorString();
    QVERIFY2(errMsg.startsWith("Child process modifier reported error: "_L1 + failureMsg),
             qPrintable(errMsg));
    QVERIFY2(errMsg.endsWith(strerror(EPERM)), qPrintable(errMsg));
}

void tst_QProcess::throwInChildProcessModifier()
{
#ifndef __cpp_exceptions
    Q_SKIP("Exceptions disabled.");
#else
    static constexpr char What[] = "tst_QProcess::throwInChildProcessModifier()::MyException";
    struct MyException : std::exception {
        const char *what() const noexcept override { return What; }
    };
    QProcess process;
    process.setChildProcessModifier([]() {
        throw MyException();
    });
    process.setProgram("testProcessNormal/testProcessNormal");

    process.start();
    QVERIFY(!process.waitForStarted(5000));
    QCOMPARE(process.state(), QProcess::NotRunning);
    QCOMPARE(process.error(), QProcess::FailedToStart);
    QVERIFY2(process.errorString().contains("Child process modifier threw an exception"),
             qPrintable(process.errorString()));
    QVERIFY2(process.errorString().contains(What),
             qPrintable(process.errorString()));

    // try again, to ensure QProcess internal state wasn't corrupted
    process.start();
    QVERIFY(!process.waitForStarted(5000));
    QCOMPARE(process.state(), QProcess::NotRunning);
    QCOMPARE(process.error(), QProcess::FailedToStart);
    QVERIFY2(process.errorString().contains("Child process modifier threw an exception"),
             qPrintable(process.errorString()));
    QVERIFY2(process.errorString().contains(What),
             qPrintable(process.errorString()));
#endif
}

void tst_QProcess::terminateInChildProcessModifier_data()
{
    using F = std::function<void(void)>;
    QTest::addColumn<F>("function");
    QTest::addColumn<QProcess::ExitStatus>("exitStatus");
    QTest::addColumn<bool>("stderrIsEmpty");

    QTest::newRow("_exit") << F([]() { _exit(0); }) << QProcess::NormalExit << true;
    QTest::newRow("abort") << F(std::abort) << QProcess::CrashExit << true;
    QTest::newRow("sigkill") << F([]() { raise(SIGKILL); }) << QProcess::CrashExit << true;
    QTest::newRow("terminate") << F(std::terminate) << QProcess::CrashExit
                               << (std::get_terminate() == std::abort);
    QTest::newRow("crash") << F([]() { tst_QProcessCrash::crash(); }) << QProcess::CrashExit << true;
}

void tst_QProcess::terminateInChildProcessModifier()
{
    QFETCH(std::function<void(void)>, function);
    QFETCH(QProcess::ExitStatus, exitStatus);
    QFETCH(bool, stderrIsEmpty);

    // temporarily disable QTest's crash logger
    DisableCrashLogger disableCrashLogging;

    // testForwardingHelper prints to both stdout and stderr, so if we fail to
    // fail we should be able to tell too
    QProcess process;
    process.setChildProcessModifier(function);
    process.setProgram("testForwardingHelper/testForwardingHelper");
    process.setArguments({ "/dev/null" });

    // temporarily disable QTest's crash logger while starting the child process
    {
        DisableCrashLogger d;
        process.start();
    }

    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(5000), qPrintable(process.errorString()));
    QCOMPARE(process.exitStatus(), exitStatus);
    QCOMPARE(process.readAllStandardOutput(), QByteArray());

    // some environments print extra stuff to stderr when we crash
#ifndef Q_OS_QNX
    if (!QTestPrivate::isRunningArmOnX86()) {
        QByteArray standardError = process.readAllStandardError();
        QVERIFY2(standardError.isEmpty() == stderrIsEmpty,
                 "stderr was: " + standardError);
    }
#endif
}

void tst_QProcess::raiseInChildProcessModifier()
{
#ifdef QT_BUILD_INTERNAL
    // This is similar to the above, but knowing that raise() doesn't unblock
    // signals, unlike abort(), this implies that
    //  1) the raise() in the child modifier will not run our handler
    //  2) the write() to stdout after that will run
    //  3) QProcess resets the signal handlers to the defaults, then unblocks
    //  4) at that point, the signal will be delivered to the child, but our
    //     handler is no longer active so there'll be no write() to stderr
    //
    // Note for maintenance: if in the future this test causes the parent
    // process to die with SIGUSR1, it means the C library is buggy and is
    // using a cached PID in the child process after vfork().
    if (!QT_PREPEND_NAMESPACE(_qprocessUsingVfork()))
        QSKIP("QProcess will only block Unix signals when using vfork()");

    // we use SIGUSR1 because QtTest doesn't log it and because its default
    // action is termination, not core dumping
    struct SigUsr1Handler {
        SigUsr1Handler()
        {
            struct sigaction sa = {};
            sa.sa_flags = SA_RESETHAND;
            sa.sa_handler = [](int) {
                static const char msg[] = "SIGUSR1 handler was run";
                (void)write(STDERR_FILENO, msg, strlen(msg));
                raise(SIGUSR1);     // re-raise
            };
            sigaction(SIGUSR1, &sa, nullptr);
        }
        ~SigUsr1Handler() { restore(); }
        static void restore() { signal(SIGUSR1, SIG_DFL); }
    } sigUsr1Handler;

    QProcess process;

    // QProcess will block signals with UseVFork
    process.setUnixProcessParameters(QProcess::UnixProcessFlag::UseVFork |
                                     QProcess::UnixProcessFlag::ResetSignalHandlers);
    process.setChildProcessModifier([]() {
        raise(SIGUSR1);
        ::childProcessModifier(STDOUT_FILENO);
    });

    // testForwardingHelper prints to both stdout and stderr, so if we fail to
    // fail we should be able to tell too
    process.setProgram("testForwardingHelper/testForwardingHelper");
    process.setArguments({ "/dev/null" });

    process.start();
    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    QVERIFY2(process.waitForFinished(5000), qPrintable(process.errorString()));
    QCOMPARE(process.error(), QProcess::Crashed);

    // ensure the write() from the child modifier DID get run
    QCOMPARE(process.readAllStandardOutput(), messageFromChildProcess);

    // some environments print extra stuff to stderr when we crash
    if (!QTestPrivate::isRunningArmOnX86()) {
        // and write() from the SIGUSR1 handler did not
        QCOMPARE(process.readAllStandardError(), QByteArray());
    }
#else
    QSKIP("Requires QT_BUILD_INTERNAL symbols");
#endif
}

void tst_QProcess::unixProcessParameters_data()
{
    QTest::addColumn<QProcess::UnixProcessParameters>("params");
    QTest::addColumn<QString>("cmd");
    QTest::newRow("defaults") << QProcess::UnixProcessParameters{} << QString();

    auto addRow = [](const char *cmd, QProcess::UnixProcessFlags flags) {
        QProcess::UnixProcessParameters params = {};
        params.flags = flags;
        QTest::addRow("%s", cmd) << params << cmd;
    };
    using P = QProcess::UnixProcessFlag;
    addRow("reset-sighand", P::ResetSignalHandlers);
    addRow("ignore-sigpipe", P::IgnoreSigPipe);
    addRow("file-descriptors", P::CloseFileDescriptors);
    addRow("setsid", P::CreateNewSession);
    addRow("reset-ids", P::ResetIds);

    // On FreeBSD, we need to be session leader to disconnect from the CTTY
    addRow("noctty", P::DisconnectControllingTerminal | P::CreateNewSession);
}

void tst_QProcess::unixProcessParameters()
{
    QFETCH(QProcess::UnixProcessParameters, params);
    QFETCH(QString, cmd);

    // set up a few things
    struct Scope {
        int devnull;
        struct sigaction old_sigusr1, old_sigpipe;
        Scope()
        {
            int fd = open("/dev/null", O_RDONLY);
            devnull = fcntl(fd, F_DUPFD, 100);
            close(fd);

            // we ignore SIGUSR1 and reset SIGPIPE to Terminate
            struct sigaction act = {};
            sigemptyset(&act.sa_mask);
            act.sa_handler = SIG_IGN;
            sigaction(SIGUSR1, &act, &old_sigusr1);
            act.sa_handler = SIG_DFL;
            sigaction(SIGPIPE, &act, &old_sigpipe);

            // and we block SIGUSR2
            sigset_t *set = &act.sa_mask;               // reuse this sigset_t
            sigaddset(set, SIGUSR2);
            sigprocmask(SIG_BLOCK, set, nullptr);
        }
        ~Scope()
        {
            if (devnull != -1)
                dismiss();
        }
        void dismiss()
        {
            close(devnull);
            sigaction(SIGUSR1, &old_sigusr1, nullptr);
            sigaction(SIGPIPE, &old_sigpipe, nullptr);
            devnull = -1;

            sigset_t *set = &old_sigusr1.sa_mask;       // reuse this sigset_t
            sigaddset(set, SIGUSR2);
            sigprocmask(SIG_BLOCK, set, nullptr);
        }
    } scope;

    if (params.flags & QProcess::UnixProcessFlag::ResetIds) {
        if (getuid() == geteuid() && getgid() == getegid())
            qInfo("Process has identical real and effective IDs; this test will do nothing");
    }

    if (params.flags & QProcess::UnixProcessFlag::DisconnectControllingTerminal) {
        if (int fd = open("/dev/tty", O_RDONLY); fd < 0) {
            qInfo("Process has no controlling terminal; this test will do nothing");
            close(fd);
        }
    }

    QProcess process;
    process.setUnixProcessParameters(params);
    process.setStandardInputFile(QProcess::nullDevice());   // so we can't mess with SIGPIPE
    process.setProgram("testUnixProcessParameters/testUnixProcessParameters");
    process.setArguments({ cmd, QString::number(scope.devnull) });
    process.start();
    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(5000));

    const QString stdErr = process.readAllStandardError();
    QCOMPARE(stdErr, QString());
    QCOMPARE(process.readAll(), QString());
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
}

void tst_QProcess::impossibleUnixProcessParameters_data()
{
    using P = QProcess::UnixProcessParameters;
    QTest::addColumn<P>("params");
    QTest::newRow("setsid") << P{ QProcess::UnixProcessFlag::CreateNewSession };
}

void tst_QProcess::impossibleUnixProcessParameters()
{
    QFETCH(QProcess::UnixProcessParameters, params);

    QProcess process;
    if (params.flags & QProcess::UnixProcessFlag::CreateNewSession) {
        process.setChildProcessModifier([]() {
            // double setsid() should cause the second to fail
            setsid();
        });
    }
    process.setUnixProcessParameters(params);
    process.start("testProcessNormal/testProcessNormal");

    QVERIFY(!process.waitForStarted(5000));
    qDebug() << process.errorString();
}

void tst_QProcess::unixProcessParametersAndChildModifier()
{
    static constexpr char message[] = "Message from the handler function\n";
    static_assert(std::char_traits<char>::length(message) <= PIPE_BUF);
    QProcess process;
    QAtomicInt vforkControl;
    int pipes[2];

    pid_t oldpgid = getpgrp();

    QVERIFY2(pipe(pipes) == 0, qPrintable(qt_error_string()));
    auto pipeGuard0 = qScopeGuard([=] { close(pipes[0]); });
    {
        auto pipeGuard1 = qScopeGuard([=] { close(pipes[1]); });

        // verify that our modifier runs before the parameters are applied
        process.setChildProcessModifier([=, &vforkControl] {
            const char *pgidmsg = "PGID mismatch. ";
            if (getpgrp() != oldpgid)
                (void)write(pipes[1], pgidmsg, strlen(pgidmsg));
            (void)write(pipes[1], message, strlen(message));
            vforkControl.storeRelaxed(1);
        });
        auto flags = QProcess::UnixProcessFlag::CloseFileDescriptors |
                QProcess::UnixProcessFlag::CreateNewSession |
                QProcess::UnixProcessFlag::UseVFork;
        process.setUnixProcessParameters({ flags });
        process.setProgram("testUnixProcessParameters/testUnixProcessParameters");
        process.setArguments({ "file-descriptors", QString::number(pipes[1]) });
        process.start();
        QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    } // closes the writing end of the pipe

    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.readAllStandardError(), QString());
    QCOMPARE(process.readAll(), QString());

    char buf[2 * sizeof(message)];
    int r = read(pipes[0], buf, sizeof(buf));
    QVERIFY2(r >= 0, qPrintable(qt_error_string()));
    QCOMPARE(QByteArrayView(buf, r), message);

    if (haveWorkingVFork)
        QVERIFY2(vforkControl.loadRelaxed(), "QProcess doesn't appear to have used vfork()");
}

void tst_QProcess::unixProcessParametersOtherFileDescriptors()
{
    constexpr int TargetFileDescriptor = 3;
    int fd1 = open("/dev/null", O_RDONLY);
    int devnull = fcntl(fd1, F_DUPFD, 100); // instead of F_DUPFD_CLOEXEC
    close(fd1);

    auto closeFds = qScopeGuard([&] {
        close(devnull);
    });

    QProcess process;
    QProcess::UnixProcessParameters params;
    params.flags = QProcess::UnixProcessFlag::CloseFileDescriptors
            | QProcess::UnixProcessFlag::UseVFork;
    params.lowestFileDescriptorToClose = 4;
    process.setUnixProcessParameters(params);
    process.setChildProcessModifier([devnull, &process]() {
        if (dup2(devnull, TargetFileDescriptor) != TargetFileDescriptor)
            process.failChildProcessModifier("dup2", errno);
    });
    process.setProgram("testUnixProcessParameters/testUnixProcessParameters");
    process.setArguments({ "file-descriptors2", QString::number(TargetFileDescriptor),
                           QString::number(devnull) });
    process.start();

    QVERIFY2(process.waitForStarted(5000), qPrintable(process.errorString()));
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.readAllStandardError(), QString());
    QCOMPARE(process.readAll(), QString());
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
}
#endif

void tst_QProcess::exitCodeTest()
{
    for (int i = 0; i < 255; ++i) {
        QProcess process;
        process.start("testExitCodes/testExitCodes", {QString::number(i)});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitCode(), i);
        QCOMPARE(process.error(), QProcess::UnknownError);
    }
}

void tst_QProcess::failToStart()
{
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    qRegisterMetaType<QProcess::ProcessState>("QProcess::ProcessState");

    QProcess process;
    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorSpy(&process, &QProcess::errorOccurred);
    QSignalSpy finishedSpy(&process, &QProcess::finished);
    QVERIFY(stateSpy.isValid());
    QVERIFY(errorSpy.isValid());
    QVERIFY(finishedSpy.isValid());

// OS X and HP-UX have a really low default process limit (~100), so spawning
// to many processes here will cause test failures later on.
#if defined Q_OS_HPUX
   const int attempts = 15;
#elif defined Q_OS_DARWIN
   const int attempts = 15;
#else
   const int attempts = 50;
#endif

    for (int j = 0; j < 8; ++j) {
        for (int i = 0; i < attempts; ++i) {
            QCOMPARE(errorSpy.size(), j * attempts + i);
            process.start("/blurp");

            switch (j) {
            case 0:
            case 1:
                QVERIFY(!process.waitForStarted());
                break;
            case 2:
            case 3:
                QVERIFY(!process.waitForFinished());
                break;
            case 4:
            case 5:
                QVERIFY(!process.waitForReadyRead());
                break;
            case 6:
            case 7:
            default:
                QVERIFY(!process.waitForBytesWritten());
                break;
            }

            QCOMPARE(process.error(), QProcess::FailedToStart);
            QCOMPARE(errorSpy.size(), j * attempts + i + 1);
            QCOMPARE(finishedSpy.size(), 0);

            int it = j * attempts + i + 1;

            QCOMPARE(stateSpy.size(), it * 2);
            QCOMPARE(qvariant_cast<QProcess::ProcessState>(stateSpy.at(it * 2 - 2).at(0)), QProcess::Starting);
            QCOMPARE(qvariant_cast<QProcess::ProcessState>(stateSpy.at(it * 2 - 1).at(0)), QProcess::NotRunning);
        }
    }
}

void tst_QProcess::failToStartWithWait()
{
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    QProcess process;
    QEventLoop loop;
    QSignalSpy errorSpy(&process, &QProcess::errorOccurred);
    QSignalSpy finishedSpy(&process, &QProcess::finished);
    QVERIFY(errorSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    for (int i = 0; i < 50; ++i) {
        process.start("/blurp", QStringList() << "-v" << "-debug");
        process.waitForStarted();

        QCOMPARE(process.error(), QProcess::FailedToStart);
        QCOMPARE(errorSpy.size(), i + 1);
        QCOMPARE(finishedSpy.size(), 0);
    }
}

void tst_QProcess::failToStartWithEventLoop()
{
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    QProcess process;
    QEventLoop loop;
    QSignalSpy errorSpy(&process, &QProcess::errorOccurred);
    QSignalSpy finishedSpy(&process, &QProcess::finished);
    QVERIFY(errorSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    // The error signal may be emitted before start() returns
    connect(&process, &QProcess::errorOccurred, &loop, &QEventLoop::quit, Qt::QueuedConnection);


    for (int i = 0; i < 50; ++i) {
        process.start("/blurp", QStringList() << "-v" << "-debug");

        loop.exec();

        QCOMPARE(process.error(), QProcess::FailedToStart);
        QCOMPARE(errorSpy.size(), i + 1);
        QCOMPARE(finishedSpy.size(), 0);
    }
}

void tst_QProcess::failToStartEmptyArgs_data()
{
    QTest::addColumn<int>("startOverload");
    QTest::newRow("start(QString, QStringList, OpenMode)") << 0;
    QTest::newRow("start(OpenMode)") << 1;
}

void tst_QProcess::failToStartEmptyArgs()
{
    QFETCH(int, startOverload);
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");

    QProcess process;
    QSignalSpy errorSpy(&process, static_cast<QProcessErrorSignal>(&QProcess::errorOccurred));
    QVERIFY(errorSpy.isValid());

    switch (startOverload) {
    case 0:
        process.start(QString(), QStringList(), QIODevice::ReadWrite);
        break;
    case 1:
        process.start(QIODevice::ReadWrite);
        break;
    default:
        QFAIL("Unhandled QProcess::start overload.");
    };

    QVERIFY(!process.waitForStarted());
    QCOMPARE(errorSpy.size(), 1);
    QCOMPARE(process.error(), QProcess::FailedToStart);
}

void tst_QProcess::removeFileWhileProcessIsRunning()
{
    QFile file(m_temporaryDir.path() + QLatin1String("/removeFile.txt"));
    QVERIFY(file.open(QFile::WriteOnly));

    QProcess process;
    process.start("testProcessEcho/testProcessEcho");

    QVERIFY(process.waitForStarted(5000));

    QVERIFY(file.remove());

    process.write("", 1);
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::setEnvironment_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("value");

    QTest::newRow("setting-empty") << "tst_QProcess" << "";
    QTest::newRow("setting") << "tst_QProcess" << "value";

#ifdef Q_OS_WIN
    QTest::newRow("unsetting") << "PROMPT" << QString();
    QTest::newRow("overriding") << "PROMPT" << "value";
#else
    QTest::newRow("unsetting") << "PATH" << QString();
    QTest::newRow("overriding") << "PATH" << "value";
#endif
}

void tst_QProcess::setEnvironment()
{
    // make sure our environment variables are correct
    QVERIFY(qgetenv("tst_QProcess").isEmpty());
    QVERIFY(!qgetenv("PATH").isEmpty());
#ifdef Q_OS_WIN
    QVERIFY(!qgetenv("PROMPT").isEmpty());
#endif

    QFETCH(QString, name);
    QFETCH(QString, value);
    QString executable = QDir::currentPath() + "/testProcessEnvironment/testProcessEnvironment";

    {
        QProcess process;
        QStringList environment = QProcess::systemEnvironment();
        if (value.isNull()) {
            int pos;
            QRegularExpression rx(name + "=.*");
#ifdef Q_OS_WIN
            rx.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
#endif
            while ((pos = environment.indexOf(rx)) != -1)
                environment.removeAt(pos);
        } else {
            environment.append(name + '=' + value);
        }
        process.setEnvironment(environment);
        process.start(executable, QStringList() << name);

        QVERIFY(process.waitForFinished());
        if (value.isNull())
            QCOMPARE(process.exitCode(), 1);
        else if (!value.isEmpty())
            QCOMPARE(process.exitCode(), 0);

        QCOMPARE(process.readAll(), value.toLocal8Bit());
    }

    // re-do the test but set the environment twice, to make sure
    // that the latter addition overrides
    // this test doesn't make sense in unsetting
    if (!value.isNull()) {
        QProcess process;
        QStringList environment = QProcess::systemEnvironment();
        environment.prepend(name + "=This is not the right value");
        environment.append(name + '=' + value);
        process.setEnvironment(environment);
        process.start(executable, QStringList() << name);

        QVERIFY(process.waitForFinished());
        if (!value.isEmpty())
            QCOMPARE(process.exitCode(), 0);

        QCOMPARE(process.readAll(), value.toLocal8Bit());
    }
}

void tst_QProcess::setProcessEnvironment_data()
{
    setEnvironment_data();
}

void tst_QProcess::setProcessEnvironment()
{
    // make sure our environment variables are correct
    QVERIFY(qgetenv("tst_QProcess").isEmpty());
    QVERIFY(!qgetenv("PATH").isEmpty());
#ifdef Q_OS_WIN
    QVERIFY(!qgetenv("PROMPT").isEmpty());
#endif

    QFETCH(QString, name);
    QFETCH(QString, value);
    QString executable = QDir::currentPath() + "/testProcessEnvironment/testProcessEnvironment";

    {
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        if (value.isNull())
            environment.remove(name);
        else
            environment.insert(name, value);
        process.setProcessEnvironment(environment);
        process.start(executable, QStringList() << name);

        QVERIFY(process.waitForFinished());
        if (value.isNull())
            QCOMPARE(process.exitCode(), 1);
        else if (!value.isEmpty())
            QCOMPARE(process.exitCode(), 0);

        QCOMPARE(process.readAll(), value.toLocal8Bit());
    }
}

void tst_QProcess::environmentIsSorted()
{
    QProcessEnvironment env;
    env.insert(QLatin1String("a"), QLatin1String("foo_a"));
    env.insert(QLatin1String("B"), QLatin1String("foo_B"));
    env.insert(QLatin1String("c"), QLatin1String("foo_c"));
    env.insert(QLatin1String("D"), QLatin1String("foo_D"));
    env.insert(QLatin1String("e"), QLatin1String("foo_e"));
    env.insert(QLatin1String("F"), QLatin1String("foo_F"));
    env.insert(QLatin1String("Path"), QLatin1String("foo_Path"));
    env.insert(QLatin1String("SystemRoot"), QLatin1String("foo_SystemRoot"));

    const QStringList envlist = env.toStringList();

#ifdef Q_OS_WIN32
    // The environment block passed to CreateProcess "[Requires that] All strings in the
    // environment block must be sorted alphabetically by name. The sort is case-insensitive,
    // Unicode order, without regard to locale."
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms682009(v=vs.85).aspx
    // So on Windows we sort that way.
    const QStringList expected = { QLatin1String("a=foo_a"),
                                   QLatin1String("B=foo_B"),
                                   QLatin1String("c=foo_c"),
                                   QLatin1String("D=foo_D"),
                                   QLatin1String("e=foo_e"),
                                   QLatin1String("F=foo_F"),
                                   QLatin1String("Path=foo_Path"),
                                   QLatin1String("SystemRoot=foo_SystemRoot") };
#else
    const QStringList expected = { QLatin1String("B=foo_B"),
                                   QLatin1String("D=foo_D"),
                                   QLatin1String("F=foo_F"),
                                   QLatin1String("Path=foo_Path"),
                                   QLatin1String("SystemRoot=foo_SystemRoot"),
                                   QLatin1String("a=foo_a"),
                                   QLatin1String("c=foo_c"),
                                   QLatin1String("e=foo_e") };
#endif
    QCOMPARE(envlist, expected);
}

void tst_QProcess::systemEnvironment()
{
    QVERIFY(!QProcess::systemEnvironment().isEmpty());
    QVERIFY(!QProcessEnvironment::systemEnvironment().isEmpty());

    QVERIFY(QProcessEnvironment::systemEnvironment().contains("PATH"));
    QVERIFY(!QProcess::systemEnvironment().filter(QRegularExpression("^PATH=", QRegularExpression::CaseInsensitiveOption)).isEmpty());
}

void tst_QProcess::spaceInName()
{
    QProcess process;
    process.start("test Space In Name/testSpaceInName", QStringList());
    QVERIFY(process.waitForStarted());
    process.write("", 1);
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

void tst_QProcess::lockupsInStartDetached()
{
    // Check that QProcess doesn't cause a lock up at this program's
    // exit if a thread was started and we tried to run a program that
    // doesn't exist. Before Qt 4.2, this used to lock up on Unix due
    // to calling ::exit instead of ::_exit if execve failed.

    QObject *dummy = new QObject(this);
    QHostInfo::lookupHost(QString("something.invalid"), dummy, SLOT(deleteLater()));
    QProcess::execute("yjhbrty");
    QProcess::startDetached("yjhbrty");
}

void tst_QProcess::atEnd2()
{
    QProcess process;

    process.start("testProcessEcho/testProcessEcho");
    process.write("Foo\nBar\nBaz\nBodukon\nHadukan\nTorwukan\nend\n");
    process.putChar('\0');
    QVERIFY(process.waitForFinished());
    QList<QByteArray> lines;
    while (!process.atEnd()) {
        lines << process.readLine();
    }
    QCOMPARE(lines.size(), 7);
}

void tst_QProcess::waitForReadyReadForNonexistantProcess()
{
    // Start a program that doesn't exist, process events and then try to waitForReadyRead
    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");

    QProcess process;
    QSignalSpy errorSpy(&process, &QProcess::errorOccurred);
    QSignalSpy finishedSpy(&process, &QProcess::finished);
    QVERIFY(errorSpy.isValid());
    QVERIFY(finishedSpy.isValid());

    QVERIFY(!process.waitForReadyRead()); // used to crash
    process.start("doesntexist");
    QVERIFY(!process.waitForReadyRead());
    QCOMPARE(errorSpy.size(), 1);
    QCOMPARE(errorSpy.at(0).at(0).toInt(), 0);
    QCOMPARE(finishedSpy.size(), 0);
}

void tst_QProcess::setStandardInputFile()
{
    static const char data[] = "A bunch\1of\2data\3\4\5\6\7...";
    QProcess process;
    QFile file(m_temporaryDir.path() + QLatin1String("/data-sif"));

    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data, sizeof data);
    file.close();

    process.setStandardInputFile(file.fileName());
    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted());
    QCOMPARE(errorOccurredSpy.size(), 0);
    QCOMPARE(stateSpy.size(), 2);
    QCOMPARE(stateSpy[0][0].value<QProcess::ProcessState>(), QProcess::Starting);
    QCOMPARE(stateSpy[1][0].value<QProcess::ProcessState>(), QProcess::Running);
    stateSpy.clear();

    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
        QByteArray all = process.readAll();
    QCOMPARE(all.size(), int(sizeof data) - 1); // testProcessEcho drops the ending \0
    QVERIFY(all == data);

    QProcess process2;
    process2.setStandardInputFile(QProcess::nullDevice());
    process2.start("testProcessEcho/testProcessEcho");
    QVERIFY(process2.waitForFinished());
    all = process2.readAll();
    QCOMPARE(all.size(), 0);
}

void tst_QProcess::setStandardInputFileFailure()
{
    QProcess process;
    process.setStandardInputFile(nonExistentFileName);

    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(!process.waitForStarted());

    QCOMPARE(errorOccurredSpy.size(), 1);
    QCOMPARE(errorOccurredSpy[0][0].value<QProcess::ProcessError>(), QProcess::FailedToStart);

    QCOMPARE(stateSpy.size(), 2);
    QCOMPARE(stateSpy[0][0].value<QProcess::ProcessState>(), QProcess::Starting);
    QCOMPARE(stateSpy[1][0].value<QProcess::ProcessState>(), QProcess::NotRunning);
}

void tst_QProcess::setStandardOutputFile_data()
{
    QTest::addColumn<QProcess::ProcessChannel>("channelToTest");
    QTest::addColumn<QProcess::ProcessChannelMode>("channelMode");
    QTest::addColumn<bool>("append");

    QTest::newRow("stdout-truncate") << QProcess::StandardOutput
                                     << QProcess::SeparateChannels
                                     << false;
    QTest::newRow("stdout-append") << QProcess::StandardOutput
                                   << QProcess::SeparateChannels
                                   << true;

    QTest::newRow("stderr-truncate") << QProcess::StandardError
                                     << QProcess::SeparateChannels
                                     << false;
    QTest::newRow("stderr-append") << QProcess::StandardError
                                   << QProcess::SeparateChannels
                                   << true;

    QTest::newRow("merged-truncate") << QProcess::StandardOutput
                                     << QProcess::MergedChannels
                                     << false;
    QTest::newRow("merged-append") << QProcess::StandardOutput
                                   << QProcess::MergedChannels
                                   << true;
}

void tst_QProcess::setStandardOutputFile()
{
    static const char data[] = "Original data. ";
    static const char testdata[] = "Test data.";

    QFETCH(QProcess::ProcessChannel, channelToTest);
    QFETCH(QProcess::ProcessChannelMode, channelMode);
    QFETCH(bool, append);

    QIODevice::OpenMode mode = append ? QIODevice::Append : QIODevice::Truncate;

    // create the destination file with data
    QFile file(m_temporaryDir.path() + QLatin1String("/data-stdof-") + QLatin1String(QTest::currentDataTag()));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data, sizeof data - 1);
    file.close();

    // run the process
    QProcess process;
    process.setProcessChannelMode(channelMode);
    if (channelToTest == QProcess::StandardOutput)
        process.setStandardOutputFile(file.fileName(), mode);
    else
        process.setStandardErrorFile(file.fileName(), mode);

    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    process.start("testProcessEcho2/testProcessEcho2");
    QVERIFY(process.waitForStarted());
    QCOMPARE(errorOccurredSpy.size(), 0);
    QCOMPARE(stateSpy.size(), 2);
    QCOMPARE(stateSpy[0][0].value<QProcess::ProcessState>(), QProcess::Starting);
    QCOMPARE(stateSpy[1][0].value<QProcess::ProcessState>(), QProcess::Running);
    stateSpy.clear();

    process.write(testdata, sizeof testdata);
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    // open the file again and verify the data
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray all = file.readAll();
    file.close();

    int expectedsize = sizeof testdata - 1;
    if (mode == QIODevice::Append) {
        QVERIFY(all.startsWith(data));
        expectedsize += sizeof data - 1;
    }
    if (channelMode == QProcess::MergedChannels) {
        expectedsize += sizeof testdata - 1;
    } else {
        QVERIFY(all.endsWith(testdata));
    }

    QCOMPARE(all.size(), expectedsize);
}

void tst_QProcess::setStandardOutputFileFailure()
{
    QFETCH(QProcess::ProcessChannel, channelToTest);
    QFETCH(QProcess::ProcessChannelMode, channelMode);
    QFETCH(bool, append);

    QIODevice::OpenMode mode = append ? QIODevice::Append : QIODevice::Truncate;

    // run the process
    QProcess process;
    process.setProcessChannelMode(channelMode);
    if (channelToTest == QProcess::StandardOutput)
        process.setStandardOutputFile(nonExistentFileName, mode);
    else
        process.setStandardErrorFile(nonExistentFileName, mode);

    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    process.start("testProcessEcho2/testProcessEcho2");
    QVERIFY(!process.waitForStarted());
    QCOMPARE(errorOccurredSpy.size(), 1);
    QCOMPARE(errorOccurredSpy[0][0].value<QProcess::ProcessError>(), QProcess::FailedToStart);
    QCOMPARE(stateSpy.size(), 2);
    QCOMPARE(stateSpy[0][0].value<QProcess::ProcessState>(), QProcess::Starting);
    QCOMPARE(stateSpy[1][0].value<QProcess::ProcessState>(), QProcess::NotRunning);
}

void tst_QProcess::setStandardOutputFileNullDevice()
{
    static const char testdata[] = "Test data.";

    QProcess process;
    process.setStandardOutputFile(QProcess::nullDevice());
    process.start("testProcessEcho2/testProcessEcho2");
    process.write(testdata, sizeof testdata);
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(process.bytesAvailable(), Q_INT64_C(0));

    QVERIFY(!QFileInfo(QProcess::nullDevice()).isFile());
}

void tst_QProcess::setStandardOutputFileAndWaitForBytesWritten()
{
    static const char testdata[] = "Test data.";

    QFile file(m_temporaryDir.path() + QLatin1String("/data-stdofawfbw"));
    QProcess process;
    process.setStandardOutputFile(file.fileName());
    process.start("testProcessEcho2/testProcessEcho2");
    QVERIFY2(process.waitForStarted(), qPrintable(process.errorString()));
    process.write(testdata, sizeof testdata);
    process.waitForBytesWritten();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    // open the file again and verify the data
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray all = file.readAll();
    file.close();

    QCOMPARE(all, QByteArray::fromRawData(testdata, sizeof testdata - 1));
}

void tst_QProcess::setStandardOutputProcess_data()
{
    QTest::addColumn<bool>("merged");
    QTest::addColumn<bool>("waitForBytesWritten");
    QTest::newRow("separate") << false << false;
    QTest::newRow("separate with waitForBytesWritten") << false << true;
    QTest::newRow("merged") << true << false;
}

void tst_QProcess::setStandardOutputProcess()
{
    QProcess source;
    QProcess intermediate;
    QProcess sink;

    QFETCH(bool, merged);
    QFETCH(bool, waitForBytesWritten);
    source.setProcessChannelMode(merged ? QProcess::MergedChannels : QProcess::SeparateChannels);
    source.setStandardOutputProcess(&intermediate);
    intermediate.setStandardOutputProcess(&sink);

    source.start("testProcessEcho2/testProcessEcho2");
    intermediate.setProgram("testProcessEcho/testProcessEcho");
    QVERIFY(intermediate.startDetached());
    sink.start("testProcessEcho2/testProcessEcho2");

    QByteArray data("Hello, World");
    source.write(data);
    if (waitForBytesWritten)
        source.waitForBytesWritten();
    source.closeWriteChannel();
    QVERIFY(source.waitForFinished());
    QCOMPARE(source.exitStatus(), QProcess::NormalExit);
    QCOMPARE(source.exitCode(), 0);
    QVERIFY(sink.waitForFinished());
    QCOMPARE(sink.exitStatus(), QProcess::NormalExit);
    QCOMPARE(sink.exitCode(), 0);
    QByteArray all = sink.readAll();

    if (!merged)
        QCOMPARE(all, data);
    else
        QCOMPARE(all, QByteArray("HHeelllloo,,  WWoorrlldd"));
}

void tst_QProcess::fileWriterProcess()
{
    const QByteArray line = QByteArrayLiteral(" -- testing testing 1 2 3\n");
    QByteArray stdinStr;
    stdinStr.reserve(5000 * (4 + line.size()) + 1);
    for (int i = 0; i < 5000; ++i) {
        stdinStr += QByteArray::number(i);
        stdinStr += line;
    }

    QElapsedTimer stopWatch;
    stopWatch.start();
    const QString fileName = m_temporaryDir.path() + QLatin1String("/fileWriterProcess.txt");
    const QString binary = QDir::currentPath() + QLatin1String("/fileWriterProcess/fileWriterProcess");

    do {
        if (QFile::exists(fileName))
            QVERIFY(QFile::remove(fileName));
        QProcess process;
        process.setWorkingDirectory(m_temporaryDir.path());
        process.start(binary, {}, QIODevice::ReadWrite | QIODevice::Text);
        process.write(stdinStr);
        process.closeWriteChannel();
        while (process.bytesToWrite()) {
            QVERIFY(stopWatch.elapsed() < 3500);
            QVERIFY(process.waitForBytesWritten(2000));
        }
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QCOMPARE(QFile(fileName).size(), qint64(stdinStr.size()));
    } while (stopWatch.elapsed() < 3000);
}

void tst_QProcess::detachedProcessParameters_data()
{
    QTest::addColumn<QString>("outChannel");
    QTest::newRow("none") << QString();
    QTest::newRow("stdout") << QString("stdout");
    QTest::newRow("stderr") << QString("stderr");
}

void tst_QProcess::detachedProcessParameters()
{
    QFETCH(QString, outChannel);
    qint64 pid;

    QFile infoFile(m_temporaryDir.path() + QLatin1String("/detachedinfo.txt"));
    if (infoFile.exists())
        QVERIFY(infoFile.remove());
    QFile channelFile(m_temporaryDir.path() + QLatin1String("detachedinfo2.txt"));
    if (channelFile.exists())
        QVERIFY(channelFile.remove());

    QString workingDir = QDir::currentPath() + "/testDetached";

    QVERIFY(QFile::exists(workingDir));

    QVERIFY(qgetenv("tst_QProcess").isEmpty());
    QByteArray envVarValue("foobarbaz");
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("tst_QProcess"), QString::fromUtf8(envVarValue));

    QProcess process;
    process.setProgram(QDir::currentPath() + QLatin1String("/testDetached/testDetached"));
#ifdef Q_OS_WIN
    int modifierCalls = 0;
    process.setCreateProcessArgumentsModifier(
        [&modifierCalls] (QProcess::CreateProcessArguments *) { modifierCalls++; });
#endif
    QStringList args(infoFile.fileName());
    if (!outChannel.isEmpty()) {
        args << QStringLiteral("--out-channel=") + outChannel;
        if (outChannel == "stdout")
            process.setStandardOutputFile(channelFile.fileName());
        else if (outChannel == "stderr")
            process.setStandardErrorFile(channelFile.fileName());
    }
    process.setArguments(args);
    process.setWorkingDirectory(workingDir);
    process.setProcessEnvironment(environment);
    QVERIFY(process.startDetached(&pid));

    QFileInfo fi(infoFile);
    fi.setCaching(false);
    //The guard counter ensures the test does not hang if the sub process fails.
    //Instead, the test will fail when trying to open & verify the sub process output file.
    for (int guard = 0; guard < 100 && fi.size() == 0; guard++) {
        QTest::qSleep(100);
    }

    QVERIFY(infoFile.open(QIODevice::ReadOnly | QIODevice::Text));
    QString actualWorkingDir = QString::fromUtf8(infoFile.readLine()).trimmed();
    QByteArray processIdString = infoFile.readLine().trimmed();
    QByteArray actualEnvVarValue = infoFile.readLine().trimmed();
    QByteArray infoFileContent;
    if (!outChannel.isEmpty()) {
        infoFile.seek(0);
        infoFileContent = infoFile.readAll();
    }
    infoFile.close();
    infoFile.remove();

    if (!outChannel.isEmpty()) {
        QVERIFY(channelFile.open(QIODevice::ReadOnly | QIODevice::Text));
        QByteArray channelContent = channelFile.readAll();
        channelFile.close();
        channelFile.remove();
        QCOMPARE(channelContent, infoFileContent);
    }

    bool ok = false;
    qint64 actualPid = processIdString.toLongLong(&ok);
    QVERIFY(ok);

    QCOMPARE(actualWorkingDir, workingDir);
    QCOMPARE(actualPid, pid);
    QCOMPARE(actualEnvVarValue, envVarValue);
#ifdef Q_OS_WIN
    QCOMPARE(modifierCalls, 1);
#endif
}

void tst_QProcess::switchReadChannels()
{
    const char data[] = "ABCD";

    QProcess process;

    process.start("testProcessEcho2/testProcessEcho2");
    process.write(data);
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    for (int i = 0; i < 4; ++i) {
        process.setReadChannel(QProcess::StandardOutput);
        QCOMPARE(process.read(1), QByteArray(&data[i], 1));
        process.setReadChannel(QProcess::StandardError);
        QCOMPARE(process.read(1), QByteArray(&data[i], 1));
    }

    process.ungetChar('D');
    process.setReadChannel(QProcess::StandardOutput);
    process.ungetChar('D');
    process.setReadChannel(QProcess::StandardError);
    QCOMPARE(process.read(1), QByteArray("D"));
    process.setReadChannel(QProcess::StandardOutput);
    QCOMPARE(process.read(1), QByteArray("D"));
}

void tst_QProcess::discardUnwantedOutput()
{
    QProcess process;

    process.setProgram("testProcessEcho2/testProcessEcho2");
    process.start(QIODevice::WriteOnly);
    process.write("Hello, World");
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    process.setReadChannel(QProcess::StandardOutput);
    QCOMPARE(process.bytesAvailable(), Q_INT64_C(0));
    process.setReadChannel(QProcess::StandardError);
    QCOMPARE(process.bytesAvailable(), Q_INT64_C(0));
}

// Q_OS_WIN - setWorkingDirectory will chdir before starting the process on unices
void tst_QProcess::setWorkingDirectory()
{
    QProcess process;
    process.setWorkingDirectory(m_temporaryDir.path());

    // use absolute path because on Windows, the executable is relative to the parent's CWD
    // while on Unix with fork it's relative to the child's (with posix_spawn, it could be either).
    process.start(QFileInfo("testSetWorkingDirectory/testSetWorkingDirectory").absoluteFilePath());

    QVERIFY2(process.waitForFinished(), process.errorString().toLocal8Bit());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    QByteArray workingDir = process.readAllStandardOutput();
    QCOMPARE(QDir(m_temporaryDir.path()).canonicalPath(), QDir(workingDir.constData()).canonicalPath());
}

void tst_QProcess::setNonExistentWorkingDirectory()
{
    QProcess process;
    process.setWorkingDirectory(nonExistentFileName);

    QSignalSpy stateSpy(&process, &QProcess::stateChanged);
    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    // use absolute path because on Windows, the executable is relative to the parent's CWD
    // while on Unix with fork it's relative to the child's (with posix_spawn, it could be either).
    process.start(QFileInfo("testSetWorkingDirectory/testSetWorkingDirectory").absoluteFilePath());

    QVERIFY(!process.waitForFinished());
    QCOMPARE(errorOccurredSpy.size(), 1);
    QCOMPARE(process.error(), QProcess::FailedToStart);
    QCOMPARE(stateSpy.size(), 2);
    QCOMPARE(stateSpy[0][0].value<QProcess::ProcessState>(), QProcess::Starting);
    QCOMPARE(stateSpy[1][0].value<QProcess::ProcessState>(), QProcess::NotRunning);

#ifdef Q_OS_UNIX
    QVERIFY2(process.errorString().startsWith("chdir:"), process.errorString().toLocal8Bit());
#endif
}

void tst_QProcess::detachedSetNonExistentWorkingDirectory()
{
    QProcess process;
    process.setWorkingDirectory(nonExistentFileName);

    QSignalSpy errorOccurredSpy(&process, &QProcess::errorOccurred);

    // use absolute path because on Windows, the executable is relative to the parent's CWD
    // while on Unix with fork it's relative to the child's (with posix_spawn, it could be either).
    process.setProgram(QFileInfo("testSetWorkingDirectory/testSetWorkingDirectory").absoluteFilePath());

    qint64 pid = -1;
    QVERIFY(!process.startDetached(&pid));
    QCOMPARE(pid, -1);
    QCOMPARE(process.error(), QProcess::FailedToStart);
    QVERIFY(process.errorString() != "Unknown error");

    QCOMPARE(errorOccurredSpy.size(), 1);
    QCOMPARE(process.error(), QProcess::FailedToStart);

#ifdef Q_OS_UNIX
    QVERIFY2(process.errorString().startsWith("chdir:"), process.errorString().toLocal8Bit());
#endif
}

void tst_QProcess::startFinishStartFinish()
{
    QProcess process;

    for (int i = 0; i < 3; ++i) {
        QCOMPARE(process.state(), QProcess::NotRunning);

        process.start("testProcessOutput/testProcessOutput");
        QVERIFY(process.waitForReadyRead(10000));
        QCOMPARE(QString::fromLatin1(process.readLine().trimmed()),
                 QString("0 -this is a number"));
        if (process.state() != QProcess::NotRunning) {
            QVERIFY(process.waitForFinished(10000));
            QCOMPARE(process.exitStatus(), QProcess::NormalExit);
            QCOMPARE(process.exitCode(), 0);
        }
    }
}

void tst_QProcess::invalidProgramString_data()
{
    QTest::addColumn<QString>("programString");
    QTest::newRow("null string") << QString();
    QTest::newRow("empty string") << QString("");
}

void tst_QProcess::invalidProgramString()
{
    QFETCH(QString, programString);
    QProcess process;

    qRegisterMetaType<QProcess::ProcessError>("QProcess::ProcessError");
    QSignalSpy spy(&process, &QProcess::errorOccurred);
    QVERIFY(spy.isValid());

    process.start(programString);
    QCOMPARE(process.error(), QProcess::FailedToStart);
    QCOMPARE(spy.size(), 1);

    QVERIFY(!QProcess::startDetached(programString));
}

void tst_QProcess::onlyOneStartedSignal()
{
    qRegisterMetaType<QProcess::ExitStatus>("QProcess::ExitStatus");
    QProcess process;

    QSignalSpy spyStarted(&process,  &QProcess::started);
    QSignalSpy spyFinished(&process, &QProcess::finished);

    QVERIFY(spyStarted.isValid());
    QVERIFY(spyFinished.isValid());

    process.start("testProcessNormal/testProcessNormal");
    QVERIFY(process.waitForStarted(5000));
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(spyStarted.size(), 1);
    QCOMPARE(spyFinished.size(), 1);

    spyStarted.clear();
    spyFinished.clear();

    process.start("testProcessNormal/testProcessNormal");
    QVERIFY(process.waitForFinished(5000));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
    QCOMPARE(spyStarted.size(), 1);
    QCOMPARE(spyFinished.size(), 1);
}

class BlockOnReadStdOut : public QObject
{
    Q_OBJECT
public:
    BlockOnReadStdOut(QProcess *process)
    {
        connect(process, &QProcess::readyReadStandardOutput, this, &BlockOnReadStdOut::block);
    }

public slots:
    void block()
    {
        QThread::sleep(std::chrono::seconds{1});
    }
};

void tst_QProcess::finishProcessBeforeReadingDone()
{
    QProcess process;
    BlockOnReadStdOut blocker(&process);
    QEventLoop loop;
    connect(&process, &QProcess::finished, &loop, &QEventLoop::quit);
    process.start("testProcessOutput/testProcessOutput");
    QVERIFY(process.waitForStarted());
    loop.exec();
    QStringList lines = QString::fromLocal8Bit(process.readAllStandardOutput()).split(
            QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
    QVERIFY(!lines.isEmpty());
    QCOMPARE(lines.last(), QStringLiteral("10239 -this is a number"));
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

//-----------------------------------------------------------------------------
void tst_QProcess::waitForStartedWithoutStart()
{
    QProcess process;
    QVERIFY(!process.waitForStarted(5000));
}

//-----------------------------------------------------------------------------
void tst_QProcess::startStopStartStop()
{
    // we actually do start-stop x 3 :-)
    QProcess process;
    process.start("testProcessNormal/testProcessNormal");
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    process.start("testExitCodes/testExitCodes", QStringList() << "1");
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 1);

    process.start("testProcessNormal/testProcessNormal");
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);
}

//-----------------------------------------------------------------------------
void tst_QProcess::startStopStartStopBuffers_data()
{
    QTest::addColumn<QProcess::ProcessChannelMode>("channelMode1");
    QTest::addColumn<QProcess::ProcessChannelMode>("channelMode2");

    QTest::newRow("separate-separate") << QProcess::SeparateChannels << QProcess::SeparateChannels;
    QTest::newRow("separate-merged")   << QProcess::SeparateChannels << QProcess::MergedChannels;
    QTest::newRow("merged-separate")   << QProcess::MergedChannels   << QProcess::SeparateChannels;
    QTest::newRow("merged-merged")     << QProcess::MergedChannels   << QProcess::MergedChannels;
    QTest::newRow("merged-forwarded")  << QProcess::MergedChannels   << QProcess::ForwardedChannels;
}

void tst_QProcess::startStopStartStopBuffers()
{
    QFETCH(QProcess::ProcessChannelMode, channelMode1);
    QFETCH(QProcess::ProcessChannelMode, channelMode2);

    QProcess process;
    process.setProcessChannelMode(channelMode1);
    process.start("testProcessHang/testProcessHang");
    QVERIFY2(process.waitForReadyRead(), process.errorString().toLocal8Bit());
    if (channelMode1 == QProcess::SeparateChannels || channelMode1 == QProcess::ForwardedOutputChannel) {
        process.setReadChannel(QProcess::StandardError);
        if (process.bytesAvailable() == 0)
            QVERIFY(process.waitForReadyRead());
        process.setReadChannel(QProcess::StandardOutput);
    }

    // We want to test that the write buffer still has bytes after the child
    // exits. We can do that by writing data until the OS stops consuming data,
    // indicating that the pipe buffers are full. The initial value of 128 kB
    // should make this loop typicall run only once; the worst case I know of
    // is Linux, which defaults to 64 kB of buffer.

    QByteArray chunk(128 * 1024, 'a');
    do {
        process.write(chunk);
        QVERIFY(process.bytesToWrite() > 0);
        process.waitForBytesWritten(1);
    } while (process.bytesToWrite() == 0);
    chunk = {};
    process.kill();

    QVERIFY(process.waitForFinished());

#ifndef Q_OS_WIN
    // confirm that our buffers are still full
    // Note: this doesn't work on Windows because our buffers are drained into
    // QWindowsPipeWriter before being sent to the child process and are lost
    // in waitForFinished() -> processFinished() -> cleanup().
    QVERIFY(process.bytesToWrite() > 0);
    QVERIFY(process.bytesAvailable() > 0); // channelMode1 is not ForwardedChannels
    if (channelMode1 == QProcess::SeparateChannels || channelMode1 == QProcess::ForwardedOutputChannel) {
        process.setReadChannel(QProcess::StandardError);
        QVERIFY(process.bytesAvailable() > 0);
        process.setReadChannel(QProcess::StandardOutput);
    }
#endif

    process.setProcessChannelMode(channelMode2);
    process.start("testProcessEcho2/testProcessEcho2", {}, QIODevice::ReadWrite | QIODevice::Text);

    // the buffers should now be empty
    QCOMPARE(process.bytesToWrite(), qint64(0));
    QCOMPARE(process.bytesAvailable(), qint64(0));
    process.setReadChannel(QProcess::StandardError);
    QCOMPARE(process.bytesAvailable(), qint64(0));
    process.setReadChannel(QProcess::StandardOutput);

    process.write("line3\n");
    process.closeWriteChannel();
    QVERIFY(process.waitForFinished());
    QCOMPARE(process.exitStatus(), QProcess::NormalExit);
    QCOMPARE(process.exitCode(), 0);

    if (channelMode2 == QProcess::MergedChannels) {
        QCOMPARE(process.readAll(), QByteArray("lliinnee33\n\n"));
    } else if (channelMode2 != QProcess::ForwardedChannels) {
        QCOMPARE(process.readAllStandardOutput(), QByteArray("line3\n"));
        if (channelMode2 == QProcess::SeparateChannels)
            QCOMPARE(process.readAllStandardError(), QByteArray("line3\n"));
    }
}

void tst_QProcess::processEventsInAReadyReadSlot_data()
{
    QTest::addColumn<bool>("callWaitForReadyRead");

    QTest::newRow("no waitForReadyRead") << false;
    QTest::newRow("waitForReadyRead") << true;
}

void tst_QProcess::processEventsInAReadyReadSlot()
{
    // Test whether processing events in a readyReadXXX slot crashes. (QTBUG-48697)
    QFETCH(bool, callWaitForReadyRead);
    QProcess process;
    QObject::connect(&process, &QProcess::readyReadStandardOutput,
                     this, &tst_QProcess::processApplicationEvents);
    process.start("testProcessEcho/testProcessEcho");
    QVERIFY(process.waitForStarted());
    const QByteArray data(156, 'x');
    process.write(data.constData(), data.size() + 1);
    if (callWaitForReadyRead)
        QVERIFY(process.waitForReadyRead());
    if (process.state() == QProcess::Running)
        QVERIFY(process.waitForFinished());
}

enum class ChdirMode {
    None = 0,
    InParent,
    InChild
};
Q_DECLARE_METATYPE(ChdirMode)

void tst_QProcess::startFromCurrentWorkingDir_data()
{
    qRegisterMetaType<ChdirMode>();
    QTest::addColumn<QString>("programPrefix");
    QTest::addColumn<ChdirMode>("chdirMode");
    QTest::addColumn<bool>("success");

    constexpr bool IsWindows = true
#ifdef Q_OS_UNIX
            && false
#endif
            ;

    // baseline: trying to execute the directory, this can't possibly succeed!
    QTest::newRow("plain-same-cwd") << QString() << ChdirMode::None << false;

    // cross-platform behavior: neither OS searches the setWorkingDirectory()
    // dir without "./"
    QTest::newRow("plain-child-chdir") << QString() << ChdirMode::InChild << false;

    // cross-platform behavior: both OSes search the parent's CWD with "./"
    QTest::newRow("prefixed-parent-chdir") << "./" << ChdirMode::InParent << true;

    // opposite behaviors: Windows searches the parent's CWD and Unix searches
    // the child's with "./"
    QTest::newRow("prefixed-child-chdir") << "./" << ChdirMode::InChild << !IsWindows;

    // Windows searches the parent's CWD without "./"
    QTest::newRow("plain-parent-chdir") << QString() << ChdirMode::InParent << IsWindows;
}

void tst_QProcess::startFromCurrentWorkingDir()
{
    QFETCH(QString, programPrefix);
    QFETCH(ChdirMode, chdirMode);
    QFETCH(bool, success);

    QProcess process;
    qRegisterMetaType<QProcess::ProcessError>();
    QSignalSpy errorSpy(&process, &QProcess::errorOccurred);
    QVERIFY(errorSpy.isValid());

    // both the dir name and the executable name
    const QString target = QStringLiteral("testProcessNormal");
    process.setProgram(programPrefix + target);

#ifdef Q_OS_UNIX
    // Reset PATH, to be sure it doesn't contain . or the empty path.
    // We can't do this on Windows because DLLs are searched in PATH
    // and Windows always searches "." anyway.
    auto restoreEnv = qScopeGuard([old = qgetenv("PATH")] {
        qputenv("PATH", old);
    });
    qputenv("PATH", "/");
#endif

    switch (chdirMode) {
    case ChdirMode::InParent: {
        auto restoreCwd = qScopeGuard([old = QDir::currentPath()] {
            QDir::setCurrent(old);
        });
        QVERIFY(QDir::setCurrent(target));
        process.start();
        break;
    }
    case ChdirMode::InChild:
        process.setWorkingDirectory(target);
        Q_FALLTHROUGH();
    case ChdirMode::None:
        process.start();
        break;
    }

    QCOMPARE(process.waitForStarted(), success);
    QCOMPARE(errorSpy.size(), int(!success));
    if (success) {
        QVERIFY(process.waitForFinished());
    } else {
        QCOMPARE(process.error(), QProcess::FailedToStart);
    }
}

QTEST_MAIN(tst_QProcess)
#include "tst_qprocess.moc"
