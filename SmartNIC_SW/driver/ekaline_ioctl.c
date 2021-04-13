/* This file should be included from function smartnic_ioctl(struct file *f, unsigned int op, unsigned long data) at file main.c */

#define EKA_FPGA_RESET_TIME_THR 50000
eka_ioctl_t eka_ioctl;

if (copy_from_user(&eka_ioctl, (void*)data, sizeof(eka_ioctl_t)))  return -EFAULT;
PRINTK("EKALINE DEBUG: EKA IOCTL cmd=%u\n",eka_ioctl.cmd);

uint64_t ekaSnDriverVerNum = EKA_VER_NUM;

int i;
uint64_t ekaIoctlNif;
uint64_t ekaIoctlSess;


switch (eka_ioctl.cmd) {
 case EKA_DUMP:
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.eka_session), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_session[eka_ioctl.session_num]), */
   /* 			  sizeof(eka_session_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num, */
   /* 	    eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* }		 */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.eka_debug), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_debug), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n",eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_igmp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_arp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_all_rx_udp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   break;
 case EKA_GET_NIF_STATE:	
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.eka_debug), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->eka_debug), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_igmp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_igmp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_arp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_arp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   /* if ((rc = copy_to_user((void __user *) &(eka_ioctl.drop_all_rx_udp), */
   /* 			  &(pDevExt->nif[eka_ioctl.nif_num].eka_private_data->drop_all_rx_udp), */
   /* 			  sizeof(uint8_t)))) { */
   /*   PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: DUMP nif %d, session %d failed\n", */
   /* 	    eka_ioctl.nif_num,eka_ioctl.session_num); */
   /*   return -EFAULT; */
   /* } */
   break;
 case EKA_SET:
   ekaIoctlNif = eka_ioctl.paramA;
   ekaIoctlSess = eka_ioctl.paramB;
   
   if (ekaIoctlNif > SC_NO_NETWORK_INTERFACES || ekaIoctlSess > EKA_SESSIONS_PER_NIF) {
     PRINTK("EKALINE ERROR: SMARTNIC_EKALINE_DATA: Wrong nif %d or session %d \n",
	    ekaIoctlNif,ekaIoctlSess);
   }
   
   memcpy(&pDevExt->nif[ekaIoctlNif].eka_private_data->eka_session[ekaIoctlSess],
	  &eka_ioctl.eka_session,
	  sizeof(eka_session_t));

   PRINTK("EKALINE DEBUG: EKA_SET nif %d, session %d, active=%u\n",
   	  ekaIoctlNif,ekaIoctlSess,eka_ioctl.eka_session.active);
   break;
 case EKA_VERSION:
   /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
   if (strlen(EKA_SN_DRIVER_BUILD_TIME) > EKA_BUILD_TIME_STRING_LEN - 1) {
     PRINTK("EKALINE ERROR: strlen(EKA_SN_DRIVER_BUILD_TIME) %d > EKA_BUILD_TIME_STRING_LEN - 1 (%d)\n",
	    strlen(EKA_SN_DRIVER_BUILD_TIME), EKA_BUILD_TIME_STRING_LEN - 1);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) eka_ioctl.paramA,
			  EKA_SN_DRIVER_BUILD_TIME,
			  strlen(EKA_SN_DRIVER_BUILD_TIME)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_VERSION failed\n");
     return -EFAULT;
   }	
   PRINTK("EKA_VERSION: %s\n",EKA_SN_DRIVER_BUILD_TIME);
   /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */
   if (strlen(EKA_SN_DRIVER_RELEASE_NOTE) > EKA_RELEASE_STRING_LEN - 1) {
     PRINTK("EKALINE ERROR: strlen(EKA_SN_DRIVER_RELEASE_NOTE) %d > EKA_RELEASE_STRING_LEN - 1 (%d)\n",
	    strlen(EKA_SN_DRIVER_RELEASE_NOTE), EKA_RELEASE_STRING_LEN - 1);
     return -EFAULT;
   }
   if ((rc = copy_to_user((void __user *) eka_ioctl.paramB,
			  EKA_SN_DRIVER_RELEASE_NOTE,
			  strlen(EKA_SN_DRIVER_RELEASE_NOTE)))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_RELEASE failed\n");
     return -EFAULT;
   }
   PRINTK("EKA_RELEASE: %s\n",EKA_SN_DRIVER_RELEASE_NOTE);
   /* ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ */

   if ((rc = copy_to_user((void __user *) eka_ioctl.paramC,
			  &ekaSnDriverVerNum,
			  8))) {
     PRINTK("EKALINE DEBUG: SMARTNIC_EKALINE_DATA: EKA_VER_NUM (%d) failed\n",ekaSnDriverVerNum);
     return -EFAULT;
   }
   PRINTK("EKA_VER_NUM: %d\n",ekaSnDriverVerNum);
   break;
 case EKA_DEBUG_ON:
   pDevExt->eka_debug = 1;
   PRINTK("EKALINE DEBUG: eka_debug = ON\n");
   break;
 case EKA_DEBUG_OFF:
   pDevExt->eka_debug = 0;
   PRINTK("EKALINE DEBUG: eka_debug = OFF\n");
   break;
 case EKA_DROP_IGMP_ON:
   pDevExt->eka_drop_igmp = 1;
   PRINTK("EKALINE DEBUG: drop_igmp = ON\n");
   break;
 case EKA_DROP_IGMP_OFF:
   pDevExt->eka_drop_igmp = 0;
   PRINTK("EKALINE DEBUG: drop_igmp = OFF\n");
   break;
 case EKA_DROP_ARP_ON:
   pDevExt->eka_drop_arp = 1;
   PRINTK("EKALINE DEBUG: drop_arp = ON\n");
   break;
 case EKA_DROP_ARP_OFF:
   pDevExt->eka_drop_arp = 0;
   PRINTK("EKALINE DEBUG: drop_arp = OFF\n");
   break;
 case EKA_UDP_DROP_ON:
   pDevExt->eka_drop_all_rx_udp = 1;
   PRINTK("EKALINE DEBUG: drop_all_rx_udp = ON\n");
   break;
 case EKA_UDP_DROP_OFF:
   pDevExt->eka_drop_all_rx_udp = 0;
   PRINTK("EKALINE DEBUG: drop_all_rx_udp = OFF\n");
   break;
 case EKA_IOREMAP_WC: {
   if ((rc = copy_to_user((void __user *) eka_ioctl.paramA,
			  &pDevExt->ekaline_wc_addr,
			  sizeof(pDevExt->ekaline_wc_addr)))) {
     PRINTK("%s:%u: EKALINE DEBUG: EKA_IOREMAP_WC failed\n",__FILE__,__LINE__);
     return -EFAULT;
   }
   PRINTK("EKALINE DEBUG: EKA_IOREMAP_WC pDevExt->ekaline_wc_addr = 0x%016llx", pDevExt->ekaline_wc_addr);
 }
   break;
 case EKA_GET_IGMP_STATE: {
   struct sc_multicast_subscription_t* dstAddr = (struct sc_multicast_subscription_t*)eka_ioctl.paramA;
   PRINTK("%s:%u: EKALINE DEBUG: EKA_GET_IGMP_STATE: dstAddr %px == wcattr.paramA 0x%llx\n",
	  __FILE__,__LINE__,
	  dstAddr,
	  eka_ioctl.paramA
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
