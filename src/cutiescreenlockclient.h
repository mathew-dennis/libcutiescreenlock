#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QDBusInterface>

// The single QML-facing screen-lock API. Talks to CutieScreenLockAuthority's
// org.cutie_shell.CutieScreenLock D-Bus service (owned by cutie-panel) over the
// session bus. Every consumer - cutie-panel's own Lockscreen.qml included -
// goes through this, so there is exactly one client implementation instead
// of each app hand-rolling its own D-Bus calls.
//
// import Cutie.ScreenLock
// CutieScreenLock { id: screenLock }
class CutieScreenLock : public QObject {
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(QString method READ method NOTIFY methodChanged)
	Q_PROPERTY(bool authenticating READ authenticating NOTIFY
			   authenticatingChanged)
	Q_PROPERTY(int lockoutSecondsRemaining READ lockoutSecondsRemaining
			   NOTIFY lockoutSecondsRemainingChanged)

    public:
	explicit CutieScreenLock(QObject *parent = nullptr);

	QString method() const;
	bool authenticating() const;
	int lockoutSecondsRemaining() const;

	Q_INVOKABLE bool isAvailable() const;
	Q_INVOKABLE bool setMethod(const QString &method);

	// Async - result arrives via passwordAuthResult(). Mirrors
	// authenticatingChanged so QML can show a spinner during the PAM
	// round-trip the same way it always could when this lived in-process.
	Q_INVOKABLE void authenticatePassword(const QString &password);

	Q_INVOKABLE bool verifyPin(const QString &pin);
	Q_INVOKABLE bool verifyPattern(const QString &patternSequence);
	Q_INVOKABLE bool setPin(const QString &pin);
	Q_INVOKABLE bool setPattern(const QString &patternSequence);
	Q_INVOKABLE bool hasCredentialConfigured();

    Q_SIGNALS:
	void methodChanged();
	void authenticatingChanged();
	void lockoutSecondsRemainingChanged();
	void passwordAuthResult(bool success, const QString &error);
	void unlocked();

    private Q_SLOTS:
	void _refreshAuthenticating();
	void _refreshLockoutSecondsRemaining();

    private:
	QDBusInterface m_iface;
};
