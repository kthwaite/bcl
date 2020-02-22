bcl: main.c
	@gcc main.c -o bcl -O2 -Werror -Wall

.PHONY: debug
debug: main.c
	@gcc main.c -o bcl -O0 -g -Wall -Werror

.PHONY: install
install: bcl
	cp bcl "$(HOME)/.local/bin"

.PHONY: format
format: main.c
	clang-format -i main.c -style="{BasedOnStyle: llvm, IndentWidth: 4}"

.PHONY: clean
clean:
	rm -rf *.o *.out bcl
