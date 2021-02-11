#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x14522340, "module_layout" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0x4f1939c7, "per_cpu__current_task" },
	{ 0x5a34a45c, "__kmalloc" },
	{ 0x7ee91c1d, "_spin_trylock" },
	{ 0xf9a482f9, "msleep" },
	{ 0x1e0c2be4, "ioremap_wc" },
	{ 0x6980fe91, "param_get_int" },
	{ 0xd3243be0, "napi_disable" },
	{ 0x25ec1b28, "strlen" },
	{ 0xd2037915, "dev_set_drvdata" },
	{ 0x6fff393f, "time_to_tm" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0x950ffff2, "cpu_online_mask" },
	{ 0x79aa04a2, "get_random_bytes" },
	{ 0xc89eabe4, "pcie_set_readrq" },
	{ 0xd691cba2, "malloc_sizes" },
	{ 0x52760ca9, "getnstimeofday" },
	{ 0xa30682, "pci_disable_device" },
	{ 0x799c50a, "param_set_ulong" },
	{ 0xc7a4fbed, "rtnl_lock" },
	{ 0xf417ff07, "pci_disable_msix" },
	{ 0x51154c2a, "skb_copy_and_csum_dev" },
	{ 0x865e3dca, "netif_carrier_on" },
	{ 0x973873ab, "_spin_lock" },
	{ 0xf77cb70a, "netif_carrier_off" },
	{ 0xfa0d49c7, "__register_chrdev" },
	{ 0xd3364703, "x86_dma_fallback_dev" },
	{ 0x81721ef7, "call_netdevice_notifiers" },
	{ 0xeae3dfd6, "__const_udelay" },
	{ 0x102b9c3, "pci_release_regions" },
	{ 0x8c009d8e, "linkwatch_fire_event" },
	{ 0x6a9f26c9, "init_timer_key" },
	{ 0x3758301, "mutex_unlock" },
	{ 0xff964b25, "param_set_int" },
	{ 0x712aa29b, "_spin_lock_irqsave" },
	{ 0x1932bfb8, "nonseekable_open" },
	{ 0x7d11c268, "jiffies" },
	{ 0xe8367c2d, "free_cpumask_var" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xd7f0902b, "__netdev_alloc_skb" },
	{ 0xffc7c184, "__init_waitqueue_head" },
	{ 0xfe7c4287, "nr_cpu_ids" },
	{ 0x41344088, "param_get_charp" },
	{ 0xaf559063, "pci_set_master" },
	{ 0xe83fea1, "del_timer_sync" },
	{ 0xde0bdcff, "memset" },
	{ 0x724781d5, "pci_enable_pcie_error_reporting" },
	{ 0x9f1019bd, "pci_set_dma_mask" },
	{ 0x7b3d21a1, "pci_enable_msix" },
	{ 0x37befc70, "jiffies_to_msecs" },
	{ 0x944d5135, "mutex_lock_interruptible" },
	{ 0x4bf79039, "__mutex_init" },
	{ 0xea147363, "printk" },
	{ 0xcf08c5b6, "kthread_stop" },
	{ 0x2fa5a500, "memcmp" },
	{ 0xa9fcf31d, "wait_for_completion_interruptible" },
	{ 0x7bd0a577, "free_netdev" },
	{ 0x7ec9bfbc, "strncpy" },
	{ 0x85f8a266, "copy_to_user" },
	{ 0xdb3b96d5, "register_netdev" },
	{ 0x208f6781, "fasync_helper" },
	{ 0xb4390f9a, "mcount" },
	{ 0xaf8d5e94, "netif_receive_skb" },
	{ 0x85abc85f, "strncmp" },
	{ 0x8e3c9cc3, "vprintk" },
	{ 0x16305289, "warn_slowpath_null" },
	{ 0xae290fb6, "pci_bus_write_config_dword" },
	{ 0xfee8a795, "mutex_lock" },
	{ 0x4b07e779, "_spin_unlock_irqrestore" },
	{ 0x45450063, "mod_timer" },
	{ 0x1902adf, "netpoll_trap" },
	{ 0x859c6dc7, "request_threaded_irq" },
	{ 0xc7a24d76, "sysfs_format_mac" },
	{ 0x1615b190, "dev_kfree_skb_any" },
	{ 0x520ee4c8, "pci_find_capability" },
	{ 0x78764f4e, "pv_irq_ops" },
	{ 0x42c8de35, "ioremap_nocache" },
	{ 0xbb0c1c6f, "__napi_schedule" },
	{ 0xc5aa6d66, "pci_bus_read_config_dword" },
	{ 0xf0fdf6cb, "__stack_chk_fail" },
	{ 0x3bd1b1f6, "msecs_to_jiffies" },
	{ 0x108e8985, "param_get_uint" },
	{ 0xafbc0d15, "alloc_netdev_mq" },
	{ 0x1000e51, "schedule" },
	{ 0x6b2dc060, "dump_stack" },
	{ 0x5771028f, "napi_complete_done" },
	{ 0xa44ad274, "wait_for_completion_interruptible_timeout" },
	{ 0xd55704ee, "eth_type_trans" },
	{ 0x266c7c38, "wake_up_process" },
	{ 0x68f7c535, "pci_unregister_driver" },
	{ 0x7f8bdd3a, "ether_setup" },
	{ 0x91766c09, "param_get_ulong" },
	{ 0x2044fa9e, "kmem_cache_alloc_trace" },
	{ 0xe52947e7, "__phys_addr" },
	{ 0x6ad065f4, "param_set_charp" },
	{ 0x642e54ac, "__wake_up" },
	{ 0xb1e5b010, "__netif_napi_add" },
	{ 0xd2965f6f, "kthread_should_stop" },
	{ 0x1d2e87c6, "do_gettimeofday" },
	{ 0xe3b405a9, "pci_disable_pcie_error_reporting" },
	{ 0x44f2f7ce, "find_get_pid" },
	{ 0x9c55cec, "schedule_timeout_interruptible" },
	{ 0x37a0cba, "kfree" },
	{ 0xc185e3ce, "kthread_create" },
	{ 0xc911f7f0, "remap_pfn_range" },
	{ 0x236c8c64, "memcpy" },
	{ 0x6d090f30, "pci_request_regions" },
	{ 0x33d92f9a, "prepare_to_wait" },
	{ 0x27f7c562, "send_sig_info" },
	{ 0x3285cc48, "param_set_uint" },
	{ 0x5f07b9f3, "__pci_register_driver" },
	{ 0x9ccb2622, "finish_wait" },
	{ 0xf7184fa4, "get_pid_task" },
	{ 0x4cbbd171, "__bitmap_weight" },
	{ 0x73618816, "unregister_netdev" },
	{ 0xe456bd3a, "complete" },
	{ 0x9edbecae, "snprintf" },
	{ 0xbc0d78f9, "__netif_schedule" },
	{ 0xd110dab, "dev_queue_xmit" },
	{ 0x207b7e2c, "skb_put" },
	{ 0xa12add91, "pci_enable_device" },
	{ 0xb02504d8, "pci_set_consistent_dma_mask" },
	{ 0xe99d2a98, "eth_mac_addr" },
	{ 0x8b3fdb57, "irq_set_affinity_hint" },
	{ 0x3302b500, "copy_from_user" },
	{ 0xa92a43c, "dev_get_drvdata" },
	{ 0xa9943164, "pci_find_ext_capability" },
	{ 0x6e720ff2, "rtnl_unlock" },
	{ 0x6e9681d2, "dma_ops" },
	{ 0xf20dabd8, "free_irq" },
	{ 0xe914e41e, "strcpy" },
	{ 0x91e97f1b, "alloc_cpumask_var" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "0A0236269BC94359D967CCD");

static const struct rheldata _rheldata __used
__attribute__((section(".rheldata"))) = {
	.rhel_major = 6,
	.rhel_minor = 10,
	.rhel_release = 754,
};
#ifdef RETPOLINE
	MODULE_INFO(retpoline, "Y");
#endif
