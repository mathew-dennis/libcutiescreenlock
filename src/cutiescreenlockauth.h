#pragma once

#include <QObject>
#include <QSettings>
#include <QTimer>
#include <QThread>
#include <QDateTime>
#include <QDBusConnection>

// Runs the blocking PAM call off the UI thread so a slow/locked-out auth
// stack never freezes the compositor.
class PamAuthWorker : public QObject {
	Q_OBJECT
    public:
	explicit PamAuthWorker(QObject *parent = nullptr);

    public Q_SLOTS:
	void authenticate(const QString &username, const QString &password);

    Q_SIGNALS:
	void finished(bool success, const QString &error);
};

// The real screen-lock engine. Owns:
//  - PAM authentication for "password" mode (the device's real Unix account
//    password, via the setuid unix_chkpwd helper - no special privileges
//    needed).
//  - Salted+stretched local hashes for "pin"/"pattern" mode.
//  - The org.cutie_shell.CutieScreenLock D-Bus service that CutieScreenLock (the
//    client type in this same module) talks to.
//
// Intentionally NOT registered as a QML type. There must be exactly one
// instance system-wide - it owns the PAM worker thread and the D-Bus
// service registration. It's a plain C++ class meant to be constructed
// once, in cutie-panel's main.cpp, and otherwise left alone; every other
// consumer (including cutie-panel's own Lockscreen.qml) should go through
// the CutieScreenLock QML type instead, even in-process.
class CutieScreenLockAuthority : public QObject {
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.cutie_shell.CutieScreenLock")

	Q_SCRIPTABLE Q_PROPERTY(bool authenticating READ authenticating NOTIFY
					 authenticatingChanged)
	Q_SCRIPTABLE Q_PROPERTY(int failedAttempts READ failedAttempts NOTIFY
					 failedAttemptsChanged)
	Q_SCRIPTABLE Q_PROPERTY(int lockoutSecondsRemaining READ
					 lockoutSecondsRemaining NOTIFY
						 lockoutSecondsRemainingChanged)
	Q_SCRIPTABLE Q_PROPERTY(
		QString method READ method WRITE setMethod NOTIFY methodChanged)

    public:
	explicit CutieScreenLockAuthority(QObject *parent = nullptr);
	~CutieScreenLockAuthority();

	bool authenticating() const { return m_authenticating; }
	int failedAttempts() const { return m_failedAttempts; }
	int lockoutSecondsRemaining() const;
	QString method() const { return m_method; }
	void setMethod(const QString &method);

	Q_SCRIPTABLE Q_INVOKABLE void authenticatePassword(const QString &password);
	Q_SCRIPTABLE Q_INVOKABLE bool verifyPin(const QString &pin);
	Q_SCRIPTABLE Q_INVOKABLE bool verifyPattern(const QString &patternSequence);
	Q_SCRIPTABLE Q_INVOKABLE void setPin(const QString &pin);
	Q_SCRIPTABLE Q_INVOKABLE void setPattern(const QString &patternSequence);
	Q_SCRIPTABLE Q_INVOKABLE bool hasCredentialConfigured() const;

    Q_SIGNALS:
	Q_SCRIPTABLE void authenticatingChanged();
	Q_SCRIPTABLE void failedAttemptsChanged();
	Q_SCRIPTABLE void lockoutSecondsRemainingChanged();
	Q_SCRIPTABLE void methodChanged();
	Q_SCRIPTABLE void passwordAuthResult(bool success, const QString &error);
	Q_SCRIPTABLE void unlocked();

    private Q_SLOTS:
	void onPamFinished(bool success, const QString &error);
	void onLockoutTick();

    private:
	QString hashSecret(const QString &secret, const QByteArray &salt) const;
	void registerFailure();
	void registerSuccess();

	bool m_authenticating = false;
	int m_failedAttempts = 0;
	QString m_method;

	// Backed by ~/.config/Cutie Community Project/CutieScreenLock.conf,
	// permissions forced to 0600 in the constructor since it holds
	// PIN/pattern hashes.
	QSettings m_settings;

	QThread m_pamThread;
	PamAuthWorker *m_pamWorker;

	QTimer m_lockoutTimer;
	qint64 m_lockoutUntilEpoch = 0;
};
