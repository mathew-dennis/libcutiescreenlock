# libcutiescreenlock

`libcutiescreenlock` provides Cutie Shell with screen-lock authentication and reusable QML controls for entering a PIN or pattern. The authentication engine runs in the Cutie Panel process (which handles the lockscreen) and publishes one session D-Bus service. Applications use the `Cutie.ScreenLock` QML module as the client API.

## Features

- **Password authentication:** Checks the current Unix account password through PAM. PAM work runs on a worker thread so a slow authentication stack does not block the UI. Account status checks are also performed.
- **PIN authentication:** Stores a random 16-byte salt and a stretched SHA-256 hash in the user's CutieScreenLock settings file. The PIN itself is not stored.
- **Pattern authentication:** Stores the same form of salted, stretched hash for a pattern sequence. The QML component emits node indices joined by hyphens, such as `0-1-4-7`.
- **Attempt lockout:** After five failed attempts, authentication is temporarily blocked with an increasing delay, capped at five minutes. A successful authentication clears the failure count.
- **QML controls:** `PinPad` and `PatternLock` provide ready-to-use PIN and pattern entry interfaces.
- **Service availability tracking:** The QML client watches for the D-Bus service to appear or disappear and updates its `available` property.

## Architecture

The library has two parts:

1. `CutieScreenLockAuthority` is the C++ authentication engine. Construct exactly one instance in the Cutie Panel process. It owns the PAM worker and registers `org.cutie_shell.CutieScreenLock` on the session bus at `/CutieScreenLock`.
2. `CutieScreenLock` is the QML-facing D-Bus client. It is registered by the `Cutie.ScreenLock` QML module. Settings and lock-screen UIs should use this client rather than creating another authority or implementing D-Bus calls themselves.

The PIN and pattern settings are stored using `QSettings` in the user-scope `Cutie Community Project/CutieScreenLock` configuration file. The authority attempts to restrict that file to owner read/write permissions.

## Build and install

The project uses CMake and Qt 6. Required development dependencies are Qt 6 Core, QML and D-Bus, plus PAM headers and library (`libpam0g-dev` on Debian-based systems).

```sh
cmake -S . -B build
cmake --build build
cmake --install build
```

The install provides the `cutiescreenlock` shared library and `cutiescreenlockauth.h`, along with the `Cutie.ScreenLock` QML module under Qt's QML import path.

## Integrating the authority

Link the Cutie Panel executable to `cutiescreenlock` and create one authority for the lifetime of the process:

```cpp
#include "cutiescreenlockauth.h"

int main(int argc, char **argv)
{
    // Initialize the Qt application first.
    CutieScreenLockAuthority screenLockAuthority;
    // Continue with the panel event loop.
}
```

The authority requires the `cutie-lockscreen` PAM service. The packaged service file includes the distribution's standard `common-auth` and `common-account` stacks:

```text
@include common-auth
@include common-account
```

Install it as `/etc/pam.d/cutie-lockscreen` (the Debian package supplies this file). The password mode authenticates the Unix user running the authority process.

## QML API

Import the module and create a client object:

```qml
import Cutie.ScreenLock

CutieScreenLock {
    id: screenLock
}
```

### `CutieScreenLock` properties

| Property | Type | Meaning |
| --- | --- | --- |
| `method` | `string` | Configured method: normally `none`, `password`, `pin`, or `pattern`. Returns `none` if the service is unavailable. |
| `authenticating` | `bool` | Whether an asynchronous PAM password authentication is in progress. |
| `lockoutSecondsRemaining` | `int` | Seconds remaining in the attempt lockout, or zero when not locked out. |
| `available` | `bool` | Whether the authority's D-Bus service is currently reachable. |

### `CutieScreenLock` methods

| Method | Return | Description |
| --- | --- | --- |
| `setMethod(method)` | `bool` | Sets the configured method if the service is available. |
| `authenticatePassword(password)` | `void` | Starts PAM authentication asynchronously. Read the result from `passwordAuthResult`. |
| `verifyPin(pin)` | `bool` | Verifies a PIN synchronously. Returns false if unavailable, locked out, or incorrect. |
| `verifyPattern(patternSequence)` | `bool` | Verifies a pattern sequence synchronously. Returns false if unavailable, locked out, or incorrect. |
| `setPin(pin)` | `bool` | Stores a new PIN hash if the service is available. |
| `setPattern(patternSequence)` | `bool` | Stores a new pattern hash if the service is available. |
| `hasCredentialConfigured()` | `bool` | Reports whether the credential for the active PIN/pattern method is configured. Password mode is always considered configured. |

### `CutieScreenLock` signals

| Signal | Meaning |
| --- | --- |
| `methodChanged()` | The configured method changed, or the service appeared. |
| `authenticatingChanged()` | PAM authentication activity changed. |
| `lockoutSecondsRemainingChanged()` | The lockout timer changed. |
| `availableChanged()` | The service appeared or disappeared. |
| `passwordAuthResult(bool success, string error)` | Result of the asynchronous password check. `error` is empty on success. |
| `unlocked()` | An authentication succeeded. |

Example password flow:

```qml
Connections {
    target: screenLock
    function onPasswordAuthResult(success, error) {
        if (success)
            console.log("Unlocked")
        else
            console.log("Authentication failed:", error)
    }
}

Button {
    text: "Unlock"
    enabled: screenLock.available && !screenLock.authenticating
    onClicked: screenLock.authenticatePassword(passwordField.text)
}
```

### `PinPad`

`PinPad` is a QML item that displays a numeric keypad, entry dots and backspace. It is available after importing `Cutie.ScreenLock`.

| Property / signal | Type | Description |
| --- | --- | --- |
| `pinLength` | `int` | Current number of entered digits. |
| `requiredLength` | `int` | Number of digits that triggers `pinEntered`; defaults to 4. |
| `maxPinLength` | `int` | Maximum accepted digits; defaults to 8. |
| `pinEntered(pin)` | signal | Emitted when the input reaches `requiredLength`. |
| `reset()` | method | Clears the entered PIN. |

Example:

```qml
PinPad {
    onPinEntered: pin => {
        const accepted = screenLock.verifyPin(pin)
        reset()
    }
}
```

### `PatternLock`

`PatternLock` displays a 3×3 grid by default and emits the selected node indices, numbered left-to-right and top-to-bottom from 0 to 8.

| Property / signal | Type | Description |
| --- | --- | --- |
| `gridSize` | `int` | Number of rows and columns; defaults to 3. |
| `nodeRadius` | `real` | Display radius of each node; defaults to 14. |
| `hitRadius` | `real` | Pointer distance used to select a node; defaults to 40. |
| `minimumLength` | `int` | Minimum number of nodes before a pattern is emitted; defaults to 4. |
| `patternEntered(sequence)` | signal | Emitted with visited node indices joined by `-`. |
| `reset()` | method | Clears the current pattern and drag state. |

The component has a default size of 280×280. For example, a path through nodes 0, 1, 4 and 7 emits `"0-1-4-7"`.

```qml
PatternLock {
    onPatternEntered: sequence => {
        const accepted = screenLock.verifyPattern(sequence)
        reset()
    }
}
```

## C++ API

`cutiescreenlockauth.h` exposes `CutieScreenLockAuthority`. It is a plain C++ `QObject`, not a QML type. Its public methods mirror the service operations:

- `method()` / `setMethod(method)`
- `authenticating()`, `failedAttempts()`, `lockoutSecondsRemaining()`
- `authenticatePassword(password)`
- `verifyPin(pin)`, `verifyPattern(patternSequence)`
- `setPin(pin)`, `setPattern(patternSequence)`
- `hasCredentialConfigured()`

Its signals include `methodChanged`, `authenticatingChanged`, `failedAttemptsChanged`, `lockoutSecondsRemainingChanged`, `passwordAuthResult(success, error)`, and `unlocked`.

## Notes

- The client and authority communicate over the **session** D-Bus. The authority must be running in the same user session as its clients.
- `authenticatePassword` is asynchronous. PIN and pattern verification are synchronous calls to the service.
- The current PIN/pattern hash uses 100,000 rounds of SHA-256 as a dependency-free stretching fallback. The source comments recommend replacing it with a dedicated password hashing algorithm such as Argon2 or PBKDF2-HMAC when an appropriate dependency is available.
- `setMethod` accepts a string; callers should use the supported values `none`, `password`, `pin`, and `pattern`.
