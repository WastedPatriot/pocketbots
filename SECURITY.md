# Security

pocketbots is a keychain. The firmware switches the radio off at boot. It does not contain a Wi-Fi password, an API token, or a socket.

## Reporting

Use a [GitHub security advisory](../../security/advisories/new) if the report should stay private. A public issue is fine for anything that is already obvious from the source. Please say whether the board is V1 or V1.1, and what you did just before it misbehaved.

## Secrets

`firmware/include/secrets.example.h` is the committed file, and it is empty. Copy it to `firmware/include/secrets.h` if you turn the radio on later. `secrets.h` is gitignored.

The same ignore list covers `.env`, `.env.*`, `credentials.h`, `*.pem` and `*.key`. Don't commit those, and don't paste a password into a file that is tracked.

## Scope

The device talks to its own display, touch controller, motion sensor and battery pin. A fault can stall the glass or run the cell down. It does not reach anything else on your network unless you add that, and the credentials for that stay in `secrets.h`.
