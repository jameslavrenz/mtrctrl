BUILD_HOST := build/host
BUILD_FW := build/fw
OPENOCD ?= openocd
PYTHON ?= python3

.PHONY: all test firmware flash hwtest check-target cppcheck clean

all: test firmware

test:
	cmake -S . -B $(BUILD_HOST) -G Ninja
	cmake --build $(BUILD_HOST)
	ctest --test-dir $(BUILD_HOST) --output-on-failure

firmware:
	cmake -S . -B $(BUILD_FW) -G Ninja --toolchain cmake/arm-none-eabi.cmake
	cmake --build $(BUILD_FW)

flash: firmware
	$(OPENOCD) -f interface/stlink.cfg -f firmware/openocd/nucleo_f446re.cfg \
		-c "program $(CURDIR)/$(BUILD_FW)/firmware/mtrctrl.elf verify reset exit"

hwtest:
	$(PYTHON) tools/hwtest.py

check-target: flash hwtest

cppcheck:
	cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
		--suppress=missingIncludeSystem --suppress=unmatchedSuppression \
		--suppress=shadowFunction --suppress=functionStatic \
		--suppress=constParameterPointer \
		--inline-suppr -I src -I firmware/app -I firmware/platform -I firmware/rtos \
		--std=c++20 src firmware/app firmware/platform firmware/rtos

clean:
	rm -rf $(BUILD_HOST) $(BUILD_FW)
