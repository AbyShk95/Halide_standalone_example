ifndef HEXAGON_SDK_ROOT
$(error HEXAGON_SDK_ROOT not set)
endif

HEXAGON_TOOLS_VER ?= 8.4.11
HEXAGON_TOOLS ?= $(HEXAGON_SDK_ROOT)/tools/HEXAGON_Tools/$(HEXAGON_TOOLS_VER)
HEXAGON_QAIC ?= $(HEXAGON_SDK_ROOT)/ipc/fastrpc/qaic/Ubuntu16/qaic
HEXAGON_CC ?= $(HEXAGON_SDK_ROOT)/tools/HEXAGON_Tools/$(HEXAGON_TOOLS_VER)/Tools/bin/hexagon-clang
HEXAGON_CXX ?= $(HEXAGON_SDK_ROOT)/tools/HEXAGON_Tools/$(HEXAGON_TOOLS_VER)/Tools/bin/hexagon-clang++
HEXAGON_AR ?= $(HEXAGON_SDK_ROOT)/tools/HEXAGON_Tools/$(HEXAGON_TOOLS_VER)/Tools/bin/hexagon-ar
HALIDE_ROOT ?= $(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/
ANDROID_NDK_ROOT ?= $(HEXAGON_SDK_ROOT)/tools/android-ndk-r19c
ANDROID_ARM64_TOOLCHAIN ?= $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/linux-x86_64
ANDROID_CXXFLAGS ?= -I $(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/include/ -I$(HEXAGON_SDK_ROOT)/incs/stddef/ -I$(HEXAGON_SDK_ROOT)/incs/a1std/ -I$(HEXAGON_SDK_ROOT)/incs/qlist/ -I$(HEXAGON_SDK_ROOT)/incs/ -I. -target aarch64-linux-android21 -Wall -O0 -g -stdlib=libc++ -std=c++11
ANDROID_LDFLAGS ?= -L $(HEXAGON_SDK_ROOT)/ipc/fastrpc/remote/ship/android_aarch64 -Lrpc -llog -fPIE -pie -L dsp/generated/ -lcdsprpc -lresize_stub
HEXAGON_CFLAGS ?= -mhvx -mhvx-length=128B -O2 -mv68 -I $(HEXAGON_SDK_ROOT)/incs/stddef/ -I $(HEXAGON_SDK_ROOT)/incs/ -I $(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/include/
GENERATOR_CXXFLAGS ?= -I $(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/include/ -stdlib=libc++ -std=c++11 -O3 -g -fno-rtti -rdynamic
GENERATOR_LDFLAGS ?= -L $(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/lib/ -lHalide -lpthread -ldl -lz -lrt -lm

CPP_DEPS = main.cpp

.PHONY: all clean flash

all: dsp/generated/resizeHalide_generator dsp/generated/resizeHalide.generator.o \
	 dsp/generated/resize_skel.c dsp/generated/libresize_stub.so dsp/generated/resize_dsp_halide.o \
	 dsp/generated/resize_skel.o dsp/generated/libresize_skel.so resize_android

# Halide generator
dsp/generated/resizeHalide_generator: generator/resizeHalide_generator.cpp
	mkdir -p dsp/generated
	clang++ generator/resizeHalide_generator.cpp $(HALIDE_ROOT)/tools/GenGen.cpp $(GENERATOR_CXXFLAGS) -o dsp/generated/resizeHalide_generator $(GENERATOR_LDFLAGS)

dsp/generated/resizeHalide.generator.o: dsp/generated/resizeHalide_generator
	export LD_LIBRARY_PATH=$(HEXAGON_SDK_ROOT)/tools/HALIDE_Tools/2.3.03/Halide/lib/ && \
	./dsp/generated/resizeHalide_generator -n resizeHalide.generator auto_schedule=true -p ${HALIDE_ROOT}/lib/libauto_schedule.so -s Adams2019 -g resizeHalide -o dsp/generated/ -e o,h,cpp -f resizeHalide target=hexagon-32-qurt-hvx_128-hvx_v68-debug

dsp/generated/resize_skel.c: rpc/resize.idl
	$(HEXAGON_QAIC) -I $(HEXAGON_SDK_ROOT)/incs/stddef -I $(HEXAGON_SDK_ROOT)/incs/ rpc/resize.idl -o dsp/generated/

dsp/generated/libresize_stub.so: dsp/generated/resize_stub.c
	$(ANDROID_ARM64_TOOLCHAIN)/bin/clang -target aarch64-linux-android21 -L $(HEXAGON_SDK_ROOT)/ipc/fastrpc/remote/ship/android_aarch64/ -I$(HEXAGON_SDK_ROOT)/incs/ -I$(HEXAGON_SDK_ROOT)/incs/stddef/ -fsigned-char -stdlib=libc++ dsp/generated/resize_stub.c -llog -fPIE -lcdsprpc -Wl,-soname,libresize_stub.so -shared -o dsp/generated/libresize_stub.so

dsp/generated/resize_dsp_halide.o: dsp/resize_dsp_halide.c
	$(HEXAGON_CC) $(HEXAGON_CFLAGS) -I dsp/generated -fPIC dsp/resize_dsp_halide.c -c -o dsp/generated/resize_dsp_halide.o

dsp/generated/resize_skel.o: dsp/generated/resize_skel.c
	$(HEXAGON_CC) $(HEXAGON_CFLAGS) -fPIC dsp/generated/resize_skel.c -c -o dsp/generated/resize_skel.o

dsp/generated/libresize_skel.so: dsp/generated/resize_skel.o dsp/generated/resize_dsp_halide.o dsp/generated/resizeHalide.generator.o
	$(HEXAGON_CC) $(HEXAGON_CFLAGS) -fPIC -mG0lib -G0 -shared -lc -lstdc++ -Wl,--whole-archive -Wl,--start-group dsp/generated/resizeHalide.generator.o dsp/generated/resize_dsp_halide.o dsp/generated/resize_skel.o -Wl,--end-group -o dsp/generated/libresize_skel.so

resize_android: $(CPP_DEPS)
	LD_LIBRARY_PATH=dsp/generated/ $(ANDROID_ARM64_TOOLCHAIN)/bin/clang++ $(CPP_DEPS) -I dsp/generated/ $(ANDROID_CXXFLAGS) $(ANDROID_LDFLAGS) -o resize_android 

flash:
	adb push resize_android /data/local/tmp/
	adb push dsp/generated/libresize_stub.so /data/local/tmp/
	adb push dsp/generated/libresize_skel.so /data/local/tmp/
	adb push $(HALIDE_ROOT)/lib/arm-64-android/libhalide_hexagon_host.so /data/local/tmp/
	adb push $(HALIDE_ROOT)/lib/v62/libhalide_hexagon_remote_skel.so /data/local/tmp/
	adb shell chmod +x /data/local/tmp/resize_android
	adb shell LD_LIBRARY_PATH=/data/local/tmp/ ADSP_LIBRARY_PATH=/data/local/tmp/ /data/local/tmp/resize_android

clean:
	rm -rf resize_android ./dsp/generated ./dsp/*.o
