/* This file should be included from function smartnic_ioctl(struct file *f, unsigned int op, unsigned long data) at file main.c */

#define EKA_FPGA_RESET_TIME_THR 50000
eka_ioctl_t eka_ioctl;


if (copy_from_user(&eka_ioctl, (void*)data, sizeof(eka_ioctl_t)))  return -EFAULT;
PRINTK("EKALINE DEBUG: EKA IOCTL to nif %d, session %d, cmd=%u\n",eka_ioctl.nif_num,eka_ioctl.session_num,eka_ioctl.cmd);

uint64_t ekaSnDriverVerNum = EKA_VER_NUM;

if (data == NULL) {
  PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: data = NULL");
  return -EFAULT;
 }

/* eka_session_t* eka_session = &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_session[eka_ioctl.session_num]); */
/* char* eka_version = (char*)&(pDevExt->nif[0].eka_private_data->eka_version); // version is taken from feth0 */
/* char* eka_release = (char*)&(pDevExt->nif[0].eka_private_data->eka_release); // release is taken from feth0 */
/* if (eka_session == NULL) { */
/*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DevExt->nif[2].eka_private_data == NULL\n"); */
/*   return -EFAULT; */
/*  } */
int i;

switch (eka_ioctl.cmd) {
 case EKA_DUMP:
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->eka_session),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_session[eka_ioctl.session_num]),
			  sizeof(eka_session_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,
	    eka_ioctl.session_num);
     return -EFAULT;
   }		
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->eka_debug),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_debug),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_igmp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_arp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_all_rx_udp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   break;
 case EKA_GET_NIF_STATE:	
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->eka_debug),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_debug),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_igmp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_arp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->drop_all_rx_udp),
			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp),
			  sizeof(uint8_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }
   break;
 case EKA_SET:
   if ((rc = copy_from_user(&(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_session[eka_ioctl.session_num]),
			    (void*) &(((eka_ioctl_t*)data)->eka_session),
			    sizeof(eka_session_t)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: SET nif %d, session %d failed\n",
	    eka_ioctl.nif_num,eka_ioctl.session_num);
     return -EFAULT;
   }	
   PRINTK("EKALINE DEBUG: EKA_SET nif %d, session %d, active=%u\n",
	  eka_ioctl.nif_num,eka_ioctl.session_num,eka_ioctl.eka_session.active);
   break;
 case EKA_VERSION:
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->eka_version),
			  (char*)&(pDevExt->nif[0].eka_private_data->eka_version),
			  64))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_VERSION failed\n");
     return -EFAULT;
   }	
   PRINTK("EKA_VERSION: %s\n",(char*)&(pDevExt->nif[0].eka_private_data->eka_version));

   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->eka_release),
			  (char*)&(pDevExt->nif[0].eka_private_data->eka_release),
			  256))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_RELEASE failed\n");
     return -EFAULT;
   }
   PRINTK("EKA_RELEASE: %s\n",(char*)&(pDevExt->nif[0].eka_private_data->eka_release));

   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->wcattr.bar0_wc_va),
			  &ekaSnDriverVerNum,
			  8))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_VER_NUM (%d) failed\n",ekaSnDriverVerNum);
     return -EFAULT;
   }
   PRINTK("EKA_VER_NUM: %d\n",ekaSnDriverVerNum);
   break;
 case EKA_DEBUG_ON:
   for (i=0;i<8;i++) pDevExt->nif[i].eka_private_data->eka_debug = 1;
   PRINTK("EKALINE DEBUG: eka_debug = ON\n");
   break;
 case EKA_DEBUG_OFF:
   for (i=0;i<8;i++) pDevExt->nif[i].eka_private_data->eka_debug = 0;
   PRINTK("EKALINE DEBUG: eka_debug = OFF\n");
   break;
 case EKA_DROP_IGMP_ON:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp = 1;
   PRINTK("EKALINE DEBUG: drop_igmp = ON\n");
   break;
 case EKA_DROP_IGMP_OFF:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp = 0;
   PRINTK("EKALINE DEBUG: drop_igmp = OFF\n");
   break;
 case EKA_DROP_ARP_ON:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp = 1;
   PRINTK("EKALINE DEBUG: drop_arp = ON\n");
   break;
 case EKA_DROP_ARP_OFF:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp = 0;
   PRINTK("EKALINE DEBUG: drop_arp = OFF\n");
   break;
 case EKA_UDP_DROP_ON:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp = 1;
   PRINTK("EKALINE DEBUG: drop_all_rx_udp = ON\n");
   break;
 case EKA_UDP_DROP_OFF:
   pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp = 0;
   PRINTK("EKALINE DEBUG: drop_all_rx_udp = OFF\n");
   break;
 case EKA_IOREMAP_WC: {
   if ((rc = copy_to_user((void __user *) &(((eka_ioctl_t*)data)->wcattr.bar0_wc_va),
			  &pDevExt->ekaline_wc_addr,
			  sizeof(pDevExt->ekaline_wc_addr)))) {
     PRINTK("%s:%u: EKALINE DEBUG: EKA_IOREMAP_WC failed\n",__FILE__,__LINE__);
     return -EFAULT;
   }
   PRINTK("EKALINE DEBUG: EKA_IOREMAP_WC pDevExt->ekaline_wc_addr = 0x%016llx", pDevExt->ekaline_wc_addr);
 }
   break;
 case EKA_GET_IGMP_STATE: {
   PRINTK("%s:%u: EKALINE DEBUG: EKA_GET_IGMP_STATE\n",__FILE__,__LINE__);

   struct sc_multicast_subscription_t* dstAddr = (struct sc_multicast_subscription_t*)((eka_ioctl_t*)data)->wcattr.bar0_pa;
   PRINTK("%s:%u: EKALINE DEBUG: EKA_GET_IGMP_STATE: dstAddr %p == wcattr.bar0_pa 0x%llx\n",
	  __FILE__,__LINE__,
	  dstAddr,
	  ((eka_ioctl_t*)data)->wcattr.bar0_pa
	  );
   if ((rc = copy_to_user((void __user *) dstAddr,
			  &pDevExt->igmpContext.global_subscriber,
			  sizeof(pDevExt->igmpContext.global_subscriber)))) {
     PRINTK("%s:%u: EKALINE DEBUG: EKA_GET_IGMP_STATE failed\n",__FILE__,__LINE__);
     return -EFAULT;
   }
   PRINTK("EKALINE DEBUG: EKA_GET_IGMP_STATE copied %u bytes",
	  sizeof(pDevExt->igmpContext.global_subscriber));
 }
   break;

 default:
   PRINTK("EKALINE DEBUG: Unknown IOCTL cmd %u\n",eka_ioctl.cmd);

   return -EINVAL;
 }

return 0;
