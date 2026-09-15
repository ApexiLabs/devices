# Changelog

## [0.2.0](https://github.com/ApexiLabs/devices/compare/v0.1.0...v0.2.0) (2026-09-15)


### Features

* add guarded signed OTA and release evidence for APE-82 ([#20](https://github.com/ApexiLabs/devices/issues/20)) ([ce8fa2e](https://github.com/ApexiLabs/devices/commit/ce8fa2ea86f4f3bb742343768da43833d8ba6b27))
* add identity-bound logger provisioning ([#12](https://github.com/ApexiLabs/devices/issues/12)) ([3bd3b38](https://github.com/ApexiLabs/devices/commit/3bd3b382c530592aa9513b62b30799475baa108d))
* add optional MQTT live upload ([f5ee0f7](https://github.com/ApexiLabs/devices/commit/f5ee0f7b3c73f423983476894dbc2dcc05853f4f))
* add optional MQTT live upload ([2de39b4](https://github.com/ApexiLabs/devices/commit/2de39b4305aa489fb3f3f58a5c4ac1219bcba8dc))
* add optional remote logger management and store-forward ([#7](https://github.com/ApexiLabs/devices/issues/7)) ([9a3192e](https://github.com/ApexiLabs/devices/commit/9a3192ed3c78de1d3efa26b4289db1b36fbede32))
* consolidate functional Logger and Dash firmware ([#23](https://github.com/ApexiLabs/devices/issues/23)) ([5fcf279](https://github.com/ApexiLabs/devices/commit/5fcf27913021c5a1a6d73e133626929791264e3b))
* generalize sensor configuration and release automation ([65ff1ef](https://github.com/ApexiLabs/devices/commit/65ff1ef7d745d32f1fde33f1898c2773019f7f28))
* harden ESP32 production security gates ([#13](https://github.com/ApexiLabs/devices/issues/13)) ([5864b73](https://github.com/ApexiLabs/devices/commit/5864b7312f6cd51c63bc57bcc794979f4a1850ba))
* isolate ESP32 HTTPS uploads from sampling for APE-76 ([#19](https://github.com/ApexiLabs/devices/issues/19)) ([989dd5c](https://github.com/ApexiLabs/devices/commit/989dd5c36cd73faac71c327829cee895bca2a763))
* make optional hardware configurable ([17249bb](https://github.com/ApexiLabs/devices/commit/17249bb0325fdb0a47a07452c4c607b742bcebb2))
* make optional hardware configurable ([64c697a](https://github.com/ApexiLabs/devices/commit/64c697a83c5accc924586015939313830f917d17))
* report observed logger diagnostics for APE-83 ([#17](https://github.com/ApexiLabs/devices/issues/17)) ([9310ea4](https://github.com/ApexiLabs/devices/commit/9310ea4afac965b2ca0910dfb746b8e43fe5f2c7))
* report planned session assignments ([#8](https://github.com/ApexiLabs/devices/issues/8)) ([87ce5a6](https://github.com/ApexiLabs/devices/commit/87ce5a6ef57c93be2853a2d295981430aa0565b8))
* rotate logger app bearers safely ([#14](https://github.com/ApexiLabs/devices/issues/14)) ([924b862](https://github.com/ApexiLabs/devices/commit/924b862dd2703c7ec64277db702e1ee931a43adf))
* version live mqtt payloads ([20aa5b9](https://github.com/ApexiLabs/devices/commit/20aa5b9423432f7e5c9a65148accb65306a62488))
* version live mqtt payloads ([f5c43fa](https://github.com/ApexiLabs/devices/commit/f5c43faea73f4ac4d65b88d85ede704ba8b6c34c))


### Bug Fixes

* fail closed on invalid live MQTT identity ([a1a951c](https://github.com/ApexiLabs/devices/commit/a1a951c291d0d385bd4d6343548ff0f04d6be896))
* keep production upload secrets out of git ([46df4e5](https://github.com/ApexiLabs/devices/commit/46df4e501e5bfe70651c5f5d8f9d0436bb30ebca))
* make store-forward recovery deterministic ([#9](https://github.com/ApexiLabs/devices/issues/9)) ([f09d93f](https://github.com/ApexiLabs/devices/commit/f09d93f3d5a4fed3735e7fa87e00164f8ba218dd))
* resolve APE-78 and APE-85 review failures ([#16](https://github.com/ApexiLabs/devices/issues/16)) ([71baea8](https://github.com/ApexiLabs/devices/commit/71baea8ecfcf3939173b07718bb21f353e931e42))
* retain provisioning reservations after serial errors ([#18](https://github.com/ApexiLabs/devices/issues/18)) ([299ffc1](https://github.com/ApexiLabs/devices/commit/299ffc141daf709ca736fc588a1786d4352cba5d))
