VCPKG_ROOT = ./vcpkg
VCPKG = $(VCPKG_ROOT)/vcpkg
PACKAGES = $(VCPKG_ROOT)/downloads

BUILD_MAKE_ROOT = build/make
BUILD_XCODE_ROOT = build/xcode

.PHONY: build
build: $(BUILD_MAKE_ROOT)
	cmake --build $(BUILD_MAKE_ROOT)

.PHONY: clean
clean:
	rm -rf $(BUILD_MAKE_ROOT)
	rm -rf $(BUILD_XCODE_ROOT)


$(BUILD_MAKE_ROOT): $(PACKAGES)
	cmake -B $@ -S . -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake 

$(BUILD_XCODE_ROOT): $(PACKAGES)
	cmake -G Xcode -B $@ -S . -DCMAKE_TOOLCHAIN_FILE=$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake
	cmake -G Xcode $(BUILD_XCODE_ROOT)

$(VCPKG):
	git submodule init
	git submodule update
	$(VCPKG_ROOT)/bootstrap-vcpkg.sh -disableMetrics

$(VCPKG_ROOT)/downloads: $(VCPKG)
	$(VCPKG) install --recurse "aws-sdk-cpp[sqs]"
