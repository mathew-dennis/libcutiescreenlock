#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QDBusInterface>
#include <QDBusServiceWatcher>

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
	// Whether cutie-panel's service is reachable right now. Backed by a
	// QDBusServiceWatcher, not a one-off check - cutie-settings and
	// cutie-panel are separate processes with no guaranteed start order,
	// so this can legitimately flip from false to true (or back) after
	// this object already exists. A plain Q_INVOKABLE here previously
	// meant any `enabled: lockAuthClient.isAvailable()` binding in QML
	// only ever evaluated once and then froze - if cutie-panel wasn't up
	// yet at that exact instant, the binding stayed false forever even
	// after cutie-panel started. This property + NOTIFY fixes that.
	Q_PROPERTY(bool available READ available NOTIFY availableChanged)

    public:
	explicit CutieScreenLock(QObject *parent = nullptr);

	QString method() const;
	bool authenticating() const;
	int lockoutSecondsRemaining() const;
	bool available() const;

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
	void availableChanged();
	void passwordAuthResult(bool success, const QString &error);
	void unlocked();

    private Q_SLOTS:
	void _refreshAuthenticating();
	void _refreshLockoutSecondsRemaining();
	void _onServiceRegistered();
	void _onServiceUnregistered();

    private:
	QDBusInterface m_iface;
	QDBusServiceWatcher *m_serviceWatcher;
};