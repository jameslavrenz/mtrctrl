# Vendored dependency versions

These trees are subsets copied for a reproducible offline build. They are not
git submodules. Do not edit them.

| Component | Source | Revision |
| --- | --- | --- |
| FreeRTOS-Kernel | https://github.com/FreeRTOS/FreeRTOS-Kernel tag V11.3.2 | `640d0c43705e64cd53f46b31d882de6cd8433933` |
| CMSIS Core (from STM32CubeF4) | https://github.com/STMicroelectronics/STM32CubeF4 tag v1.28.3 | `94cae6e83f00e276a11957e7833c01ac3d0bd7af` |
| cmsis_device_f4 | https://github.com/STMicroelectronics/cmsis_device_f4 | `3c77349ce04c8af401454cc51f85ea9a50e34fc1` (pinned by CubeF4 v1.28.3) |
| stm32f4xx_hal_driver | https://github.com/STMicroelectronics/stm32f4xx_hal_driver release v1.8.5 | `b6f0ed3829f3829eb358a2e7417d80bba1a42db7` (pinned by CubeF4 v1.28.3) |

FreeRTOS tag V11.3.2 reports `tskKERNEL_VERSION_NUMBER` as `"V11.1.0+"`. That
string is what is in the tagged sources; this tree does not patch it.

Licenses: FreeRTOS MIT (`FreeRTOS-Kernel/LICENSE.md`), CMSIS Apache-2.0
(`cmsis/LICENSE.txt`), ST device and HAL BSD-3-Clause (their `LICENSE.md`).
