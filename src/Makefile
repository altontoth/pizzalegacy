.PHONY: all release format tags

all:
	@env -u MAKEFLAGS cmake --build ../build/debug -j

release:
	@env -u MAKEFLAGS cmake --build ../build/release -j

format:
	cmake --build ../build/debug --target format

tags:
	cmake --build ../build/debug --target tags
