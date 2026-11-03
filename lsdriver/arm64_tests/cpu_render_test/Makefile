obj-m += cpu_render_test.o
cpu_render_test-y := cpu_render_test_main.o

ccflags-y += -std=gnu11
ccflags-y += -O2
ccflags-y += -Wno-unused-parameter
ccflags-y += -fno-stack-protector

cpu_render_test_main.o: $(src)/cpu_render_test_main.c FORCE
	$(call if_changed_dep,cc_o_c)
