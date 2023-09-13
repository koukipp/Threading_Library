.DEFAULT_GOAL := run_main
TARGETS = main matmul matmul_multilevel matmul_yield create_bench matmul_sem atexit_example

run_main: clean compile_exec


compile_exec:
	for t in $(TARGETS); do\
		gcc -Wall lib/list.c lib/threads.c $$t.c -o $$t.out -lpthread ;\
	done
compile_exec_atexit:
	for t in $(TARGETS); do\
		gcc -Wall lib/list.c -DATEXIT_HANDLER lib/threads.c $$t.c -o $$t.out -lpthread ;\
	done
clean:
	rm -f *.out
	rm -f *.o
	rm -f *.so

