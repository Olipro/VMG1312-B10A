/*===========================================================================
FILE:
   GobiUSBNet.c

DESCRIPTION:
   Qualcomm USB Network device for Gobi 3000
   
FUNCTIONS:
   GatherEndpoints
   GobiSuspend
   GobiResume
   GobiNetDriverBind
   GobiNetDriverUnbind
   GobiUSBNetURBCallback
   GobiUSBNetTXTimeout
   GobiUSBNetAutoPMThread
   GobiUSBNetStartXmit
   GobiUSBNetOpen
   GobiUSBNetStop
   GobiUSBNetProbe
   GobiUSBNetModInit
   GobiUSBNetModExit

Copyright (c) 2011, Code Aurora Forum. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Code Aurora Forum nor
      the names of its contributors may be used to endorse or promote
      products derived from this software without specific prior written
      permission.


THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
===========================================================================*/

//---------------------------------------------------------------------------
// Include Files
//---------------------------------------------------------------------------


#include "Structs.h"
#include "QMIDevice.h"
#include "QMI.h"
#include <linux/kmod.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/workqueue.h>
#include <linux/mii.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/usb/usbnet.h>

//-----------------------------------------------------------------------------
// Definitions
//-----------------------------------------------------------------------------

// Version Information
#define DRIVER_VERSION "2011-07-29-1026"
#define DRIVER_AUTHOR "Qualcomm Innovation Center"
#define DRIVER_DESC "GobiNet"

// Debug flag
int debug = 0;

// Allow user interrupts
int interruptible = 0;

// Number of IP packets which may be queued up for transmit
int txQueueLength = 100;

// Class should be created during module init, so needs to be global
static struct class * gpClass;

/*===========================================================================
METHOD:
   GatherEndpoints (Public Method)

DESCRIPTION:
   Enumerate endpoints

PARAMETERS
   pIntf          [ I ] - Pointer to usb interface

RETURN VALUE:
   sEndpoints structure
              NULL for failure
===========================================================================*/
sEndpoints * GatherEndpoints( struct usb_interface * pIntf )
{
   int numEndpoints;
   int endpointIndex;
   sEndpoints * pOut;
   struct usb_host_endpoint * pEndpoint = NULL;
   
   pOut = kzalloc( sizeof( sEndpoints ), GFP_ATOMIC );
   if (pOut == NULL)
   {
      DBG( "unable to allocate memory\n" );
      return NULL;
   }

   pOut->mIntfNum = pIntf->cur_altsetting->desc.bInterfaceNumber;
   
   // Scan endpoints
   numEndpoints = pIntf->cur_altsetting->desc.bNumEndpoints;
   DBG("numEndpoints = %d\n", numEndpoints);
   for (endpointIndex = 0; endpointIndex < numEndpoints; endpointIndex++)
   {
      pEndpoint = pIntf->cur_altsetting->endpoint + endpointIndex;
      if (pEndpoint == NULL)
      {
         DBG( "invalid endpoint %u\n", endpointIndex );
         kfree( pOut );
         return NULL;
      }
      
      if (usb_endpoint_dir_in( &pEndpoint->desc ) == true
      &&  usb_endpoint_xfer_int( &pEndpoint->desc ) == true)
      {
         pOut->mIntInEndp = pEndpoint->desc.bEndpointAddress;
      }
      else if (usb_endpoint_dir_in( &pEndpoint->desc ) == true
      &&  usb_endpoint_xfer_int( &pEndpoint->desc ) == false)
      {
         pOut->mBlkInEndp = pEndpoint->desc.bEndpointAddress;
      }
      else if (usb_endpoint_dir_in( &pEndpoint->desc ) == false
      &&  usb_endpoint_xfer_int( &pEndpoint->desc ) == false)
      {
         pOut->mBlkOutEndp = pEndpoint->desc.bEndpointAddress;
      }
   }

   if (pOut->mIntInEndp == 0
   ||  pOut->mBlkInEndp == 0
   ||  pOut->mBlkOutEndp == 0)
   {
      DBG( "One or more endpoints missing\n" );
      kfree( pOut );
      return NULL;
   }

   DBG( "intf %u\n", pOut->mIntfNum );
   DBG( "   int in  0x%02x\n", pOut->mIntInEndp );
   DBG( "   blk in  0x%02x\n", pOut->mBlkInEndp );
   DBG( "   blk out 0x%02x\n", pOut->mBlkOutEndp );

   return pOut;
}

/*===========================================================================
METHOD:
   GobiSuspend (Public Method)

DESCRIPTION:
   Stops QMI traffic while device is suspended

PARAMETERS
   pIntf          [ I ] - Pointer to interface
   powerEvent     [ I ] - Power management event

RETURN VALUE:
   int - 0 for success
         negative errno for failure
===========================================================================*/
int GobiSuspend( 
   struct usb_interface *     pIntf,
   pm_message_t               powerEvent )
{
   struct usbnet * pDev;
   sGobiUSBNet * pGobiDev;
   
   if (pIntf == 0)
   {
      return -ENOMEM;
   }
   
#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   // Is this autosuspend or system suspend?
   //    do we allow remote wakeup?
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ) && defined(CONFIG_PM))
   if (pDev->udev->auto_pm == 0)
#else
   //if ((powerEvent.event & PM_EVENT_AUTO) == 0)
   if ((powerEvent.event & 0x0400) == 0)
#endif
   {
      DBG( "device suspended to power level %d\n", 
           powerEvent.event );
      GobiSetDownReason( pGobiDev, DRIVER_SUSPENDED );
   }
   else
   {
      DBG( "device autosuspend\n" );
   }
     
   if (powerEvent.event & PM_EVENT_SUSPEND)
   {
      // Stop QMI read callbacks
      KillRead( pGobiDev );
      //pDev->udev->reset_resume = 0;
      
      // Store power state to avoid duplicate resumes
      pIntf->dev.power.power_state.event = powerEvent.event;
   }
   else
   {
      // Other power modes cause QMI connection to be lost
      //pDev->udev->reset_resume = 1;
   }
   
   // Run usbnet's suspend function
   return usbnet_suspend( pIntf, powerEvent );
}
   
/*===========================================================================
METHOD:
   GobiResume (Public Method)

DESCRIPTION:
   Resume QMI traffic or recreate QMI device

PARAMETERS
   pIntf          [ I ] - Pointer to interface

RETURN VALUE:
   int - 0 for success
         negative errno for failure
===========================================================================*/
int GobiResume( struct usb_interface * pIntf )
{
   struct usbnet * pDev;
   sGobiUSBNet * pGobiDev;
   int nRet;
   int oldPowerState;
   
   if (pIntf == 0)
   {
      return -ENOMEM;
   }
   
#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   oldPowerState = pIntf->dev.power.power_state.event;
   pIntf->dev.power.power_state.event = PM_EVENT_ON;
   DBG( "resuming from power mode %d\n", oldPowerState );

   if (oldPowerState & PM_EVENT_SUSPEND)
   {
      // It doesn't matter if this is autoresume or system resume
      GobiClearDownReason( pGobiDev, DRIVER_SUSPENDED );
   
      nRet = usbnet_resume( pIntf );
      if (nRet != 0)
      {
         DBG( "usbnet_resume error %d\n", nRet );
         return nRet;
      }

      // Restart QMI read callbacks
      nRet = StartRead( pGobiDev );
      if (nRet != 0)
      {
         DBG( "StartRead error %d\n", nRet );
         return nRet;
      }

      // Kick Auto PM thread to process any queued URBs
      complete( &pGobiDev->mAutoPM.mThreadDoWork );
   }
   else
   {
      DBG( "nothing to resume\n" );
      return 0;
   }
   
   return nRet;
}

/*===========================================================================
METHOD:
   GobiNetDriverBind (Public Method)

DESCRIPTION:
   Setup in and out pipes

PARAMETERS
   pDev           [ I ] - Pointer to usbnet device
   pIntf          [ I ] - Pointer to interface

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int GobiNetDriverBind( 
   struct usbnet *         pDev, 
   struct usb_interface *  pIntf )
{
   int numEndpoints;
   int endpointIndex;
   struct usb_host_endpoint * pEndpoint = NULL;
   struct usb_host_endpoint * pIn = NULL;
   struct usb_host_endpoint * pOut = NULL;
   
   // Verify one altsetting
   if (pIntf->num_altsetting != 1)
   {
      DBG( "invalid num_altsetting %u\n", pIntf->num_altsetting );
      return -ENODEV;
   }

   // Verify correct interface (0 or 5)
#if 1
   DBG("\n\n\nbInterfaceNumber = %d\n\n\n", pIntf->cur_altsetting->desc.bInterfaceNumber);
   if (pIntf->cur_altsetting->desc.bInterfaceNumber != 4) 
   {
      DBG( "invalid interface %d, pIntf->cur_altsetting->desc.bInterfaceNumber  = %d\n", 
           pIntf->cur_altsetting->desc.bInterfaceNumber, pIntf->cur_altsetting->desc.bInterfaceNumber );
      return -ENODEV;
   }
#endif
   
   // Collect In and Out endpoints
   numEndpoints = pIntf->cur_altsetting->desc.bNumEndpoints;
   for (endpointIndex = 0; endpointIndex < numEndpoints; endpointIndex++)
   {
      pEndpoint = pIntf->cur_altsetting->endpoint + endpointIndex;
      if (pEndpoint == NULL)
      {
         DBG( "invalid endpoint %u\n", endpointIndex );
         return -ENODEV;
      }
      
      if (usb_endpoint_dir_in( &pEndpoint->desc ) == true
      &&  usb_endpoint_xfer_int( &pEndpoint->desc ) == false)
      {
         pIn = pEndpoint;
      }
      else if (usb_endpoint_dir_out( &pEndpoint->desc ) == true)
      {
         pOut = pEndpoint;
      }
   }
   
   if (pIn == NULL || pOut == NULL)
   {
      DBG( "invalid endpoints\n" );
      return -ENODEV;
   }

   if (usb_set_interface( pDev->udev, 
                          pIntf->cur_altsetting->desc.bInterfaceNumber,
                          0 ) != 0)
   {
      DBG( "unable to set interface\n" );
      return -ENODEV;
   }

   pDev->in = usb_rcvbulkpipe( pDev->udev,
                   pIn->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK );
   pDev->out = usb_sndbulkpipe( pDev->udev,
                   pOut->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK );
                   
   DBG( "in %x, out %x\n", 
        pIn->desc.bEndpointAddress, 
        pOut->desc.bEndpointAddress );

   // In later versions of the kernel, usbnet helps with this
#if (LINUX_VERSION_CODE <= KERNEL_VERSION( 2,6,23 ))
   pIntf->dev.platform_data = (void *)pDev;
#endif

   return 0;
}

/*===========================================================================
METHOD:
   GobiNetDriverUnbind (Public Method)

DESCRIPTION:
   Deregisters QMI device (Registration happened in the probe function)

PARAMETERS
   pDev           [ I ] - Pointer to usbnet device
   pIntfUnused    [ I ] - Pointer to interface

RETURN VALUE:
   None
===========================================================================*/
static void GobiNetDriverUnbind( 
   struct usbnet *         pDev, 
   struct usb_interface *  pIntf)
{
   sGobiUSBNet * pGobiDev = (sGobiUSBNet *)pDev->data[0];

   // Should already be down, but just in case...
   netif_carrier_off( pDev->net );

   DeregisterQMIDevice( pGobiDev );
   
#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,29 ))
   kfree( pDev->net->netdev_ops );
   pDev->net->netdev_ops = NULL;
#endif

#if (LINUX_VERSION_CODE <= KERNEL_VERSION( 2,6,23 ))
   pIntf->dev.platform_data = NULL;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,19 ))
   pIntf->needs_remote_wakeup = 0;
#endif

   kfree( pGobiDev->mpEndpoints );
   kfree( pGobiDev );
   pGobiDev = NULL;
}

/*===========================================================================
METHOD:
   GobiUSBNetURBCallback (Public Method)

DESCRIPTION:
   Write is complete, cleanup and signal that we're ready for next packet

PARAMETERS
   pURB     [ I ] - Pointer to sAutoPM struct

RETURN VALUE:
   None
===========================================================================*/
void GobiUSBNetURBCallback( struct urb * pURB )
{
   unsigned long activeURBflags;
   sAutoPM * pAutoPM = (sAutoPM *)pURB->context;
   if (pAutoPM == NULL)
   {
      // Should never happen
      DBG( "bad context\n" );
      return;
   }

   if (pURB->status != 0)
   {
      // Note that in case of an error, the behaviour is no different
      DBG( "urb finished with error %d\n", pURB->status );
   }

   // Remove activeURB (memory to be freed later)
   spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

   // EAGAIN used to signify callback is done
   pAutoPM->mpActiveURB = ERR_PTR( -EAGAIN );

   spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

   complete( &pAutoPM->mThreadDoWork );
   
   usb_free_urb( pURB );
}

/*===========================================================================
METHOD:
   GobiUSBNetTXTimeout (Public Method)

DESCRIPTION:
   Timeout declared by the net driver.  Stop all transfers

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   None
===========================================================================*/
void GobiUSBNetTXTimeout( struct net_device * pNet )
{
   struct sGobiUSBNet * pGobiDev;
   sAutoPM * pAutoPM;
   sURBList * pURBListEntry;
   unsigned long activeURBflags, URBListFlags;
   struct usbnet * pDev = netdev_priv( pNet );
   struct urb * pURB;

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return;
   }
   pAutoPM = &pGobiDev->mAutoPM;

   DBG( "\n" );

   // Grab a pointer to active URB
   spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
   pURB = pAutoPM->mpActiveURB;
   spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

   // Stop active URB
   if (pURB != NULL)
   {
      usb_kill_urb( pURB );
   }

   // Cleanup URB List
   spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );

   pURBListEntry = pAutoPM->mpURBList;
   while (pURBListEntry != NULL)
   {
      pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
      atomic_dec( &pAutoPM->mURBListLen );
      usb_free_urb( pURBListEntry->mpURB );
      kfree( pURBListEntry );
      pURBListEntry = pAutoPM->mpURBList;
   }

   spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

   complete( &pAutoPM->mThreadDoWork );

   return;
}

/*===========================================================================
METHOD:
   GobiUSBNetAutoPMThread (Public Method)

DESCRIPTION:
   Handle device Auto PM state asynchronously
   Handle network packet transmission asynchronously

PARAMETERS
   pData     [ I ] - Pointer to sAutoPM struct

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int GobiUSBNetAutoPMThread( void * pData )
{
   unsigned long activeURBflags, URBListFlags;
   sURBList * pURBListEntry;
   int status;
   struct usb_device * pUdev;
   sAutoPM * pAutoPM = (sAutoPM *)pData;
   struct urb * pURB;

   if (pAutoPM == NULL)
   {
      DBG( "passed null pointer\n" );
      return -EINVAL;
   }
   
   pUdev = interface_to_usbdev( pAutoPM->mpIntf );

   DBG( "traffic thread started\n" );

   while (pAutoPM->mbExit == false)
   {
      // Wait for someone to poke us
      wait_for_completion_interruptible( &pAutoPM->mThreadDoWork );

      // Time to exit?
      if (pAutoPM->mbExit == true)
      {
         // Stop activeURB
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
         pURB = pAutoPM->mpActiveURB;
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

         if (pURB != NULL)
         {
            usb_kill_urb( pURB );
         }
         // Will be freed in callback function

         // Cleanup URB List
         spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );

         pURBListEntry = pAutoPM->mpURBList;
         while (pURBListEntry != NULL)
         {
            pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
            atomic_dec( &pAutoPM->mURBListLen );
            usb_free_urb( pURBListEntry->mpURB );
            kfree( pURBListEntry );
            pURBListEntry = pAutoPM->mpURBList;
         }

         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

         break;
      }
      
      // Is our URB active?
      spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );

      // EAGAIN used to signify callback is done
      if (IS_ERR( pAutoPM->mpActiveURB ) 
      &&  PTR_ERR( pAutoPM->mpActiveURB ) == -EAGAIN )
      {
         pAutoPM->mpActiveURB = NULL;

         // Restore IRQs so task can sleep
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         
         // URB is done, decrement the Auto PM usage count
         usb_autopm_put_interface( pAutoPM->mpIntf );

         // Lock ActiveURB again
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
      }

      if (pAutoPM->mpActiveURB != NULL)
      {
         // There is already a URB active, go back to sleep
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         continue;
      }
      
      // Is there a URB waiting to be submitted?
      spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
      if (pAutoPM->mpURBList == NULL)
      {
         // No more URBs to submit, go back to sleep
         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         continue;
      }

      // Pop an element
      pURBListEntry = pAutoPM->mpURBList;
      pAutoPM->mpURBList = pAutoPM->mpURBList->mpNext;
      atomic_dec( &pAutoPM->mURBListLen );
      spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

      // Set ActiveURB
      pAutoPM->mpActiveURB = pURBListEntry->mpURB;
      spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );

      // Tell autopm core we need device woken up
      status = usb_autopm_get_interface( pAutoPM->mpIntf );
      if (status < 0)
      {
         DBG( "unable to autoresume interface: %d\n", status );

         // likely caused by device going from autosuspend -> full suspend
         if (status == -EPERM)
         {
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ) && defined(CONFIG_PM))
            pUdev->auto_pm = 0;
#endif
            GobiSuspend( pAutoPM->mpIntf, PMSG_SUSPEND );
         }

         // Add pURBListEntry back onto pAutoPM->mpURBList
         spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
         pURBListEntry->mpNext = pAutoPM->mpURBList;
         pAutoPM->mpURBList = pURBListEntry;
         atomic_inc( &pAutoPM->mURBListLen );
         spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );
         
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
         pAutoPM->mpActiveURB = NULL;
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         
         // Go back to sleep
         continue;
      }

      // Submit URB
      status = usb_submit_urb( pAutoPM->mpActiveURB, GFP_KERNEL );
      if (status < 0)
      {
         // Could happen for a number of reasons
         DBG( "Failed to submit URB: %d.  Packet dropped\n", status );
         spin_lock_irqsave( &pAutoPM->mActiveURBLock, activeURBflags );
         usb_free_urb( pAutoPM->mpActiveURB );
         pAutoPM->mpActiveURB = NULL;
         spin_unlock_irqrestore( &pAutoPM->mActiveURBLock, activeURBflags );
         usb_autopm_put_interface( pAutoPM->mpIntf );

         // Loop again
         complete( &pAutoPM->mThreadDoWork );
      }
      
      kfree( pURBListEntry );
   }   
   
   DBG( "traffic thread exiting\n" );
   pAutoPM->mpThread = NULL;
   return 0;
}      

/*===========================================================================
METHOD:
   GobiUSBNetStartXmit (Public Method)

DESCRIPTION:
   Convert sk_buff to usb URB and queue for transmit

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   NETDEV_TX_OK on success
   NETDEV_TX_BUSY on error
===========================================================================*/
int GobiUSBNetStartXmit( 
   struct sk_buff *     pSKB,
     struct net_device *  pNet )
{
   unsigned long URBListFlags;
   struct sGobiUSBNet * pGobiDev;
   sAutoPM * pAutoPM;
   sURBList * pURBListEntry, ** ppURBListEnd;
   void * pURBData;
   struct usbnet * pDev = netdev_priv( pNet );
   
   DBG( "\n" );
   
   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return NETDEV_TX_BUSY;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return NETDEV_TX_BUSY;
   }
   pAutoPM = &pGobiDev->mAutoPM;
   
   if (GobiTestDownReason( pGobiDev, DRIVER_SUSPENDED ) == true)
   {
      // Should not happen
      DBG( "device is suspended\n" );
      dump_stack();
      return NETDEV_TX_BUSY;
   }
   
   // Convert the sk_buff into a URB

   // Check if buffer is full
   if (atomic_read( &pAutoPM->mURBListLen ) >= txQueueLength)
   {
      DBG( "not scheduling request, buffer is full\n" );
      return NETDEV_TX_BUSY;
   }

   // Allocate URBListEntry
   pURBListEntry = kmalloc( sizeof( sURBList ), GFP_ATOMIC );
   if (pURBListEntry == NULL)
   {
      DBG( "unable to allocate URBList memory\n" );
      return NETDEV_TX_BUSY;
   }
   pURBListEntry->mpNext = NULL;

   // Allocate URB
   pURBListEntry->mpURB = usb_alloc_urb( 0, GFP_ATOMIC );
   if (pURBListEntry->mpURB == NULL)
   {
      DBG( "unable to allocate URB\n" );
      kfree( pURBListEntry );
      return NETDEV_TX_BUSY;
   }

   // Allocate URB transfer_buffer
   pURBData = kmalloc( pSKB->len, GFP_ATOMIC );
   if (pURBData == NULL)
   {
      DBG( "unable to allocate URB data\n" );
      usb_free_urb( pURBListEntry->mpURB );
      kfree( pURBListEntry );
      return NETDEV_TX_BUSY;
   }
   // Fill will SKB's data
   memcpy( pURBData, pSKB->data, pSKB->len );

   usb_fill_bulk_urb( pURBListEntry->mpURB,
                      pGobiDev->mpNetDev->udev,
                      pGobiDev->mpNetDev->out,
                      pURBData,
                      pSKB->len,
                      GobiUSBNetURBCallback,
                      pAutoPM );

   // Free the transfer buffer on last reference dropped
   //pURBListEntry->mpURB->transfer_flags |= URB_FREE_BUFFER;
   pURBListEntry->mpURB->transfer_flags |= 0x0100;

   // Aquire lock on URBList
   spin_lock_irqsave( &pAutoPM->mURBListLock, URBListFlags );
   
   // Add URB to end of list
   ppURBListEnd = &pAutoPM->mpURBList;
   while ((*ppURBListEnd) != NULL)
   {
      ppURBListEnd = &(*ppURBListEnd)->mpNext;
   }
   *ppURBListEnd = pURBListEntry;
   atomic_inc( &pAutoPM->mURBListLen );

   spin_unlock_irqrestore( &pAutoPM->mURBListLock, URBListFlags );

   complete( &pAutoPM->mThreadDoWork );

   // Start transfer timer
   pNet->trans_start = jiffies;
   // Free SKB
   dev_kfree_skb_any( pSKB );

   return NETDEV_TX_OK;
}

/*===========================================================================
METHOD:
   GobiUSBNetOpen (Public Method)

DESCRIPTION:
   Wrapper to usbnet_open, correctly handling autosuspend
   Start AutoPM thread

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int GobiUSBNetOpen( struct net_device * pNet )
{
   int status = 0;
   struct sGobiUSBNet * pGobiDev;
   struct usbnet * pDev = netdev_priv( pNet );
   
   if (pDev == NULL)
   {
      DBG( "failed to get usbnet device\n" );
      return -ENXIO;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   DBG( "\n" );

   // Start the AutoPM thread
   pGobiDev->mAutoPM.mpIntf = pGobiDev->mpIntf;
   pGobiDev->mAutoPM.mbExit = false;
   pGobiDev->mAutoPM.mpURBList = NULL;
   pGobiDev->mAutoPM.mpActiveURB = NULL;
   spin_lock_init( &pGobiDev->mAutoPM.mURBListLock );
   spin_lock_init( &pGobiDev->mAutoPM.mActiveURBLock );
   atomic_set( &pGobiDev->mAutoPM.mURBListLen, 0 );
   init_completion( &pGobiDev->mAutoPM.mThreadDoWork );
   
   pGobiDev->mAutoPM.mpThread = kthread_run( GobiUSBNetAutoPMThread, 
                                               &pGobiDev->mAutoPM, 
                                               "GobiUSBNetAutoPMThread" );
   if (IS_ERR( pGobiDev->mAutoPM.mpThread ))
   {
      DBG( "AutoPM thread creation error\n" );
      return PTR_ERR( pGobiDev->mAutoPM.mpThread );
   }

   // Allow traffic
   GobiClearDownReason( pGobiDev, NET_IFACE_STOPPED );

   // Pass to usbnet_open if defined
   if (pGobiDev->mpUSBNetOpen != NULL)
   {
      status = pGobiDev->mpUSBNetOpen( pNet );
   
      // If usbnet_open was successful enable Auto PM
      if (status == 0)
      {
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,33 ))
         usb_autopm_enable( pGobiDev->mpIntf );
#else
         usb_autopm_put_interface( pGobiDev->mpIntf );
#endif
      }
   }
   else
   {
      DBG( "no USBNetOpen defined\n" );
   }
   
   return status;
}

/*===========================================================================
METHOD:
   GobiUSBNetStop (Public Method)

DESCRIPTION:
   Wrapper to usbnet_stop, correctly handling autosuspend
   Stop AutoPM thread

PARAMETERS
   pNet     [ I ] - Pointer to net device

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int GobiUSBNetStop( struct net_device * pNet )
{
   struct sGobiUSBNet * pGobiDev;
   struct usbnet * pDev = netdev_priv( pNet );

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      return -ENXIO;
   }
   
   pGobiDev = (sGobiUSBNet *)pDev->data[0];
   if (pGobiDev == NULL)
   {
      DBG( "failed to get QMIDevice\n" );
      return -ENXIO;
   }

   // Stop traffic
   GobiSetDownReason( pGobiDev, NET_IFACE_STOPPED );

   // Tell traffic thread to exit
   pGobiDev->mAutoPM.mbExit = true;
   complete( &pGobiDev->mAutoPM.mThreadDoWork );
   
   // Wait for it to exit
   while( pGobiDev->mAutoPM.mpThread != NULL )
   {
      msleep( 100 );
   }
   DBG( "thread stopped\n" );

   // Pass to usbnet_stop, if defined
   if (pGobiDev->mpUSBNetStop != NULL)
   {
      return pGobiDev->mpUSBNetStop( pNet );
   }
   else
   {
      return 0;
   }
}

/*=========================================================================*/
// Struct driver_info
/*=========================================================================*/
static const struct driver_info GobiNetInfo = 
{
   .description   = "GobiNet Ethernet Device",
   .flags         = FLAG_ETHER,
   .bind          = GobiNetDriverBind,
   .unbind        = GobiNetDriverUnbind,
   .data          = 1,
};

/*=========================================================================*/
// Qualcomm Gobi 3000 VID/PIDs
/*=========================================================================*/
static const struct usb_device_id GobiVIDPIDTable [] =
{
   //ZyXEL
   { 
      USB_DEVICE( 0x0586, 0x3443),
      .driver_info = (unsigned long)&GobiNetInfo 
   },
   //Terminating entry
   { }
};

MODULE_DEVICE_TABLE( usb, GobiVIDPIDTable );

/*===========================================================================
METHOD:
   GobiUSBNetProbe (Public Method)

DESCRIPTION:
   Run usbnet_probe
   Setup QMI device

PARAMETERS
   pIntf        [ I ] - Pointer to interface
   pVIDPIDs     [ I ] - Pointer to VID/PID table

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
int GobiUSBNetProbe( 
   struct usb_interface *        pIntf, 
   const struct usb_device_id *  pVIDPIDs )
{
   int status;
   struct usbnet * pDev;
   sGobiUSBNet * pGobiDev;
   sEndpoints * pEndpoints;
   int pipe;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,29 ))
   struct net_device_ops * pNetDevOps;
#endif


   pEndpoints = GatherEndpoints( pIntf );
   if (pEndpoints == NULL)
   {
      return -ENODEV;
   }      

   status = usbnet_probe( pIntf, pVIDPIDs );
   if (status < 0)
   {
      DBG( "usbnet_probe failed %d\n", status );
      return status;
   }

#if (LINUX_VERSION_CODE >= KERNEL_VERSION( 2,6,19 ))
   pIntf->needs_remote_wakeup = 1;
#endif

#if (LINUX_VERSION_CODE > KERNEL_VERSION( 2,6,23 ))
   pDev = usb_get_intfdata( pIntf );
#else
   pDev = (struct usbnet *)pIntf->dev.platform_data;
#endif

   if (pDev == NULL || pDev->net == NULL)
   {
      DBG( "failed to get netdevice\n" );
      usbnet_disconnect( pIntf );
      kfree( pEndpoints );
      return -ENXIO;
   }

   pGobiDev = kmalloc( sizeof( sGobiUSBNet ), GFP_KERNEL );
   if (pGobiDev == NULL)
   {
      DBG( "falied to allocate device buffers" );
      usbnet_disconnect( pIntf );
      kfree( pEndpoints );
      return -ENOMEM;
   }
   
   pDev->data[0] = (unsigned long)pGobiDev;
   
   pGobiDev->mpNetDev = pDev;
   pGobiDev->mpEndpoints = pEndpoints;

   // Clearing endpoint halt is a magic handshake that brings 
   // the device out of low power (airplane) mode
   // NOTE: FCC verification should be done before this, if required
   pipe = usb_sndbulkpipe( pGobiDev->mpNetDev->udev,
                           pGobiDev->mpEndpoints->mBlkOutEndp );
   usb_clear_halt( pGobiDev->mpNetDev->udev, pipe );

   // Overload PM related network functions
#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,29 ))
   pGobiDev->mpUSBNetOpen = pDev->net->open;
   pDev->net->open = GobiUSBNetOpen;
   pGobiDev->mpUSBNetStop = pDev->net->stop;
   pDev->net->stop = GobiUSBNetStop;
   pDev->net->hard_start_xmit = GobiUSBNetStartXmit;
   pDev->net->tx_timeout = GobiUSBNetTXTimeout;
#else
   pNetDevOps = kmalloc( sizeof( struct net_device_ops ), GFP_KERNEL );

   if (pNetDevOps == NULL)
   {
      DBG( "falied to allocate net device ops" );
      usbnet_disconnect( pIntf );
      return -ENOMEM;
   }
   memcpy( pNetDevOps, pDev->net->netdev_ops, sizeof( struct net_device_ops ) );
   
   pGobiDev->mpUSBNetOpen = pNetDevOps->ndo_open;
   pNetDevOps->ndo_open = GobiUSBNetOpen;
   pGobiDev->mpUSBNetStop = pNetDevOps->ndo_stop;
   pNetDevOps->ndo_stop = GobiUSBNetStop;
   pNetDevOps->ndo_start_xmit = GobiUSBNetStartXmit;
   pNetDevOps->ndo_tx_timeout = GobiUSBNetTXTimeout;

   pDev->net->netdev_ops = pNetDevOps;
#endif

#if (LINUX_VERSION_CODE < KERNEL_VERSION( 2,6,31 ))
   memset( &(pGobiDev->mpNetDev->stats), 0, sizeof( struct net_device_stats ) );
#else
   memset( &(pGobiDev->mpNetDev->net->stats), 0, sizeof( struct net_device_stats ) );
#endif

   pGobiDev->mpIntf = pIntf;
   memset( &(pGobiDev->mMEID), '0', 14 );
   
   DBG( "Mac Address:\n" );
   PrintHex( &pGobiDev->mpNetDev->net->dev_addr[0], 6 );

   pGobiDev->mbQMIValid = false;
   memset( &pGobiDev->mQMIDev, 0, sizeof( sQMIDev ) );
   pGobiDev->mQMIDev.mbCdevIsInitialized = false;

   pGobiDev->mQMIDev.mpDevClass = gpClass;
   
   init_completion( &pGobiDev->mAutoPM.mThreadDoWork );
   spin_lock_init( &pGobiDev->mQMIDev.mClientMemLock );

   // Default to device down
   pGobiDev->mDownReason = 0;
   GobiSetDownReason( pGobiDev, NO_NDIS_CONNECTION );
   //
   GobiSetDownReason( pGobiDev, NET_IFACE_STOPPED );

   // Register QMI
#if 1
	status = RegisterQMIDevice( pGobiDev );
   if (status != 0)
   {
       //usbnet_disconnect() will call GobiNetDriverUnbind() which will call
       //DeregisterQMIDevice() to clean up any partially created QMI device
      usbnet_disconnect( pIntf );
      return status;
   }
   
#endif
   // Success
   return 0;
}

static struct usb_driver GobiNet =
{
   .name       = "GobiNet",
   .id_table   = GobiVIDPIDTable,
   .probe      = GobiUSBNetProbe,
   .disconnect = usbnet_disconnect,
   .suspend    = GobiSuspend,
   .resume     = GobiResume,
   .supports_autosuspend = true,
};

/*===========================================================================
METHOD:
   GobiUSBNetModInit (Public Method)

DESCRIPTION:
   Initialize module
   Create device class
   Register out usb_driver struct

RETURN VALUE:
   int - 0 for success
         Negative errno for error
===========================================================================*/
static int __init GobiUSBNetModInit( void )
{
   gpClass = class_create( THIS_MODULE, "GobiQMI" );
   if (IS_ERR( gpClass ) == true)
   {
      DBG( "error at class_create %ld\n",
           PTR_ERR( gpClass ) );
      return -ENOMEM;
   }

   // This will be shown whenever driver is loaded
   printk( KERN_INFO "%s: %s\n", DRIVER_DESC, DRIVER_VERSION );

   return usb_register( &GobiNet );
}
module_init( GobiUSBNetModInit );

/*===========================================================================
METHOD:
   GobiUSBNetModExit (Public Method)

DESCRIPTION:
   Deregister module
   Destroy device class

RETURN VALUE:
   void
===========================================================================*/
static void __exit GobiUSBNetModExit( void )
{
   usb_deregister( &GobiNet );

   class_destroy( gpClass );
}
module_exit( GobiUSBNetModExit );

MODULE_VERSION( DRIVER_VERSION );
MODULE_AUTHOR( DRIVER_AUTHOR );
MODULE_DESCRIPTION( DRIVER_DESC );
MODULE_LICENSE("Dual BSD/GPL");
 

#ifdef bool
#undef bool
#endif

module_param( debug, bool, S_IRUGO | S_IWUSR );
MODULE_PARM_DESC( debug, "Debuging enabled or not" );

module_param( interruptible, bool, S_IRUGO | S_IWUSR );
MODULE_PARM_DESC( interruptible, "Listen for and return on user interrupt" );

module_param( txQueueLength, int, S_IRUGO | S_IWUSR );
MODULE_PARM_DESC( txQueueLength, 
                  "Number of IP packets which may be queued up for transmit" );

