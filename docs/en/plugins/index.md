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
