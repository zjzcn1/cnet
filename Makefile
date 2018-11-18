BUILD_DIR = ./build

all: build
	cd $(BUILD_DIR);  make -j3
clean:
	rm -rf $(BUILD_DIR)
build:
	mkdir $(BUILD_DIR);cd $(BUILD_DIR); cmake ..
