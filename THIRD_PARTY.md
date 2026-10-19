# Credits and licenses

Application source is distributed under GPLv2; see LICENSE.

- OpenOrbis PS4 Toolchain 0.5.4: https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain
- OpenOrbis LibOrbisPkg (commit pinned in Dockerfile): https://github.com/OpenOrbis/LibOrbisPkg
- ps4-libjbc (commit pinned in Dockerfile): https://github.com/bucanero/ps4-libjbc
- cJSON 1.7.19: MIT, license in `third_party/cjson/LICENSE`.
- stb_image (commit 2c980bb59875b0d32144a71867fbdebb2f77cd20): public domain/MIT, `third_party/stb/LICENSE`.
- Noto Sans: SIL Open Font License, `assets/FONT-LICENSE.txt`.
- Native prototype and BGFT ABI reference: https://github.com/ItsJokerZz/UnityOrbisBridge (GPLv2).
- Installation-location registry research reference: https://github.com/ReSo7200/DirectPackageInstaller-Enhanced/blob/DN8/Payload/main_experimental.c
- Packed launcher import mapping: https://github.com/OpenOrbis/create-fself/blob/master/pkg/oelf/OELFStrangeLibs.go
- BGFT task stop/unregister reference: https://github.com/flatz/ps4_remote_pkg_installer/blob/master/installer.c
- Mozilla CA certificate store, curl conversion dated 2026-09-25: MPL 2.0,
  `assets/cacert.pem` and `assets/cacert-LICENSE.txt`.
  Source: https://curl.se/ca/cacert-2026-09-25.pem (121 certificates).
  SHA-256: `a41b5d356aea97a529fe27e0f7316d2f9d946d75927476cf9cf1b90637d00505`.
  Provenance and conversion license: https://curl.se/docs/caextract.html.
  License text: https://www.mozilla.org/media/MPL/2.0/index.txt.
  Update the dated source, checksum, and certificate-count test together when
  replacing this bundle. Updates ship with application packages; the application
  does not download trust anchors or accept server-provided certificates as roots.
- Native HTTPS certificate ABI and PEM format research reference:
  https://github.com/Xyhlo/SSPI/blob/dc6cb98ee0b1c68d67bfee581fcbcb241246f456/app/NativeHttp.cs.
  OpenOrbis 0.5.4 declares `SslPem` and `sceHttpsLoadCert`; the reference confirms
  an array of pointers to PEM records, with CRLF line endings and retained storage.
  The implementation here uses the existing OpenOrbis declarations and a separate
  small parser; no code is copied from that reference.

The new application icon and SVG artwork were created for this project.
