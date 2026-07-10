#include "cutiescreenlockclient.h"

#include <QDBusConnection>
#include <QDBusReply>

static const char *kService = "org.cutie_shell.CutieScreenLock";
static const char *kPath = "/CutieScreenLock";
static const char *kInterface = "org.cutie_shell.CutieScreenLock";

CutieScreenLock::CutieScreenLock(QObject *parent)
	: QObject(parent)
	, m_iface(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		  QString::fromLatin1(kInterface), QDBusConnection::sessionBus())
{
	QDBusConnection bus = QDBusConnection::sessionBus();

	bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		    QString::fromLatin1(kInterface),
		    QStringLiteral("methodChanged"), this,
		    SIGNAL(methodChanged()));

	bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		    QString::fromLatin1(kInterface),
		    QStringLiteral("unlocked"), this, SIGNAL(unlocked()));

	bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		    QString::fromLatin1(kInterface),
		    QStringLiteral("passwordAuthResult"), this,
		    SIGNAL(passwordAuthResult(bool, QString)));

	bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		    QString::fromLatin1(kInterface),
		    QStringLiteral("authenticatingChanged"), this,
		    SLOT(_refreshAuthenticating()));

	bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kPath),
		    QString::fromLatin1(kInterface),
		    QStringLiteral("lockoutSecondsRemainingChanged"), this,
		    SLOT(_refreshLockoutSecondsRemaining()));

	// cutie-settings and cutie-panel start independently - whichever
	// starts first must not get stuck assuming the other is permanently
	// absent. This watches the actual bus for the service coming and
	// going, so `available` (and everything gated on it in QML) updates
	// live instead of being frozen at whatever was true when this object
	// happened to be constructed.
	m_serviceWatcher = new QDBusServiceWatcher(
		QString::fromLatin1(kService), bus,
		QDBusServiceWatcher::WatchForRegistration |
			QDBusServiceWatcher::WatchForUnregistration,
		this);
	connect(m_serviceWatcher, &QDBusServiceWatcher::serviceRegistered, this,
		&CutieScreenLock::_onServiceRegistered);
	connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
		this, &CutieScreenLock::_onServiceUnregistered);
}

bool CutieScreenLock::available() const
{
	return m_iface.isValid();
}

void CutieScreenLock::_onServiceRegistered()
{
	// The service just appeared - re-check validity (QDBusInterface
	// doesn't always self-update the instant this fires) and refresh
	// everything that may have been serving stale defaults while it was
	// down.
	Q_EMIT availableChanged();
	Q_EMIT methodChanged();
	Q_EMIT authenticatingChanged();
	Q_EMIT lockoutSecondsRemainingChanged();
}

void CutieScreenLock::_onServiceUnregistered()
{
	Q_EMIT availableChanged();
}

QString CutieScreenLock::method() const
{
	if (!m_iface.isValid())
		return QStringLiteral("none");
	return m_iface.property("method").toString();
}

bool CutieScreenLock::authenticating() const
{
	if (!m_iface.isValid())
		return false;
	return m_iface.property("authenticating").toBool();
}

int CutieScreenLock::lockoutSecondsRemaining() const
{
	if (!m_iface.isValid())
		return 0;
	return m_iface.property("lockoutSecondsRemaining").toInt();
}

void CutieScreenLock::_refreshAuthenticating()
{
	Q_EMIT authenticatingChanged();
}

void CutieScreenLock::_refreshLockoutSecondsRemaining()
{
	Q_EMIT lockoutSecondsRemainingChanged();
}

bool CutieScreenLock::setMethod(const QString &method)
{
	if (!m_iface.isValid())
		return false;
	m_iface.setProperty("method", method);
	return true;
}

void CutieScreenLock::authenticatePassword(const QString &password)
{
	if (!m_iface.isValid()) {
		Q_EMIT passwordAuthResult(false, QStringLiteral("CutieScreenLock service unavailable"));
		return;
	}
	// Fire-and-forget: the real result comes back via the relayed
	// passwordAuthResult signal, same as when this was in-process.
	m_iface.asyncCall(QStringLiteral("authenticatePassword"), password);
}

bool CutieScreenLock::verifyPin(const QString &pin)
{
	if (!m_iface.isValid())
		return false;
	QDBusReply<bool> reply = m_iface.call(QStringLiteral("verifyPin"), pin);
	return reply.isValid() && reply.value();
}

bool CutieScreenLock::verifyPattern(const QString &patternSequence)
{
	if (!m_iface.isValid())
		return false;
	QDBusReply<bool> reply =
		m_iface.call(QStringLiteral("verifyPattern"), patternSequence);
	return reply.isValid() && reply.value();
}

bool CutieScreenLock::setPin(const QString &pin)
{
	if (!m_iface.isValid())
		return false;
	m_iface.call(QStringLiteral("setPin"), pin);
	return true;
}

bool CutieScreenLock::setPattern(const QString &patternSequence)
{
	if (!m_iface.isValid())
		return false;
	m_iface.call(QStringLiteral("setPattern"), patternSequence);
	return true;
}

bool CutieScreenLock::hasCredentialConfigured()
{
	if (!m_iface.isValid())
		return false;
	QDBusReply<bool> reply =
		m_iface.call(QStringLiteral("hasCredentialConfigured"));
	return reply.isValid() && reply.value();
}