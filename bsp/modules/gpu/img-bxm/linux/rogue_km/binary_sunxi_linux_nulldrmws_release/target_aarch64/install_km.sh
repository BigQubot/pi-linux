KERNELVERSION=5.15.147
MOD_DESTDIR=/lib/modules/5.15.147/extra
check_module_directory /lib/modules/5.15.147
install_file pvrsrvkm.ko ${MOD_DESTDIR}/pvrsrvkm.ko "kernel_module" 0644 0:0
