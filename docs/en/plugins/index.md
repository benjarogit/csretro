# Plugin authors

The client is not playable yet. The server side is an AMXX module (vendored
NextClientServerApi; rebrand follows the gates).

## What exists today

- Module and test plugin on the test-server path (see [Test server](../server/testserver.md)).
- Building the server API: [Build server](../build/server.md).

## What a plugin cannot do yet

- A stable, fully rebranded `csr_` native surface and generated native tables
  arrive with later gates (plan G08/G11). Until then the vendored API in the tree
  is the technical reference, not finished public plugin documentation.

There is no second docs tree. When native tables are generated, they land under
`docs/de` and `docs/en` and on the Page.

## Develop against the current API

Use the vendored [Pawn include](https://github.com/benjarogit/csretro/blob/main/server/api/addons/amxmodx/scripting/include/next_client_api.inc) as the exact API contract. It currently uses upstream `ncl_` names. Do not invent `csr_` replacements before the protocol/rebrand gate. Metamod-R loads the server stack; this AMXX include is not a general Metamod plugin SDK.

Wait for `ncl_client_api_ready`, check readiness and supported features, then use the relevant sandbox, sprite, viewmodel, sound or material operation. Handle disconnects and unavailable clients. Test compilation and behavior against the same server/module revision in the [test server](../server/testserver.md). A server-side module loading successfully does not establish working x64 client-side rendering or authentication.

Generated native references must be derived from the include when the API changes. The [feature guide](../players/features.md) explains player-visible scope; [CREDITS](https://github.com/benjarogit/csretro/blob/main/CREDITS.md) owns attribution.
