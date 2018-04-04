/*
 * SH7720 Setup
 *
 *  Copyright (C) 2007  Markus Brunner, Mark Jonas
 *  Copyright (C) 2009  Paul Mundt
 *
 *  Based on arch/sh/kernel/cpu/sh4/setup-sh7750.c:
 *
 *  Copyright (C) 2006  Paul Mundt
 *  Copyright (C) 2006  Jamie Lenehan
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/io.h>
#include <linux/serial_sci.h>
#include <asm/rtc.h>

static struct resource rtc_resources[] = {
	[0] = {
		.start	= 0xa413fec0,
		.end	= 0xa413fec0 + 0x28 - 1,
		.flags	= IORESOURCE_IO,
	},
	[1] = {
		/* Shared Period/Carry/Alarm IRQ */
		.start	= 20,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct sh_rtc_platform_info rtc_info = {
	.capabilities	= RTC_CAP_4_DIGIT_YEAR,
};

static struct platform_device rtc_device = {
	.name		= "sh-rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resources),
	.resource	= rtc_resources,
	.dev		= {
		.platform_data = &rtc_info,
	},
};

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xa4430000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 80, 80, 80, 80 },
	}, {
		.mapbase	= 0xa4438000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs           = { 81, 81, 81, 81 },
	}, {

		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct resource usb_ohci_resources[] = {
	[0] = {
		.start	= 0xA4428000,
		.end	= 0xA44280FF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 67,
		.end	= 67,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 usb_ohci_dma_mask = 0xffffffffUL;
static struct platform_device usb_ohci_device = {
	.name		= "sh_ohci",
	.id		= -1,
	.dev = {
		.dma_mask		= &usb_ohci_dma_mask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(usb_ohci_resources),
	.resource	= usb_ohci_resources,
};

static struct resource usbf_resources[] = {
	[0] = {
		.name	= "sh_udc",
		.start	= 0xA4420000,
		.end	= 0xA44200FF,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.name	= "sh_udc",
		.start	= 65,
		.end	= 65,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device usbf_device = {
	.name		= "sh_udc",
	.id		= -1,
	.dev = {
		.dma_mask		= NULL,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(usbf_resources),
	.resource	= usbf_resources,
};

static struct platform_device *sh7720_devices[] __initdata = {
	&rtc_device,
	&sci_device,
	&usb_ohci_device,
	&usbf_device,
};

static int __init sh7720_devices_setup(void)
{
	return platform_add_devices(sh7720_devices,
				    ARRAY_SIZE(sh7720_devices));
}
__initcall(sh7720_devices_setup);

enum {
	UNUSED = 0,

	/* interrupt sources */
	TMU0, TMU1, TMU2, RTC,
	WDT, REF_RCMI, SIM,
	IRQ0, IRQ1, IRQ2, IRQ3,
	USBF_SPD, TMU_SUNI, IRQ5, IRQ4,
	DMAC1, LCDC, SSL,
	ADC, DMAC2, USBFI, CMT,
	SCIF0, SCIF1,
	PINT07, PINT815, TPU, IIC,
	SIOF0, SIOF1, MMC, PCC,
	USBHI, AFEIF,
	H_UDI,
};

static struct intc_vect vectors[] __initdata = {
	/* IRQ0->5 are handled in setup-sh3.c */
	INTC_VECT(TMU0, 0x400),       INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2, 0x440),       INTC_VECT(RTC, 0x480),
	INTC_VECT(RTC, 0x4a0),	      INTC_VECT(RTC, 0x4c0),
	INTC_VECT(SIM, 0x4e0),	      INTC_VECT(SIM, 0x500),
	INTC_VECT(SIM, 0x520),	      INTC_VECT(SIM, 0x540),
	INTC_VECT(WDT, 0x560),        INTC_VECT(REF_RCMI, 0x580),
	/* H_UDI cannot be masked */  INTC_VECT(TMU_SUNI, 0x6c0),
	INTC_VECT(USBF_SPD, 0x6e0),   INTC_VECT(DMAC1, 0x800),
	INTC_VECT(DMAC1, 0x820),      INTC_VECT(DMAC1, 0x840),
	INTC_VECT(DMAC1, 0x860),      INTC_VECT(LCDC, 0x900),
#if defined(CONFIG_CPU_SUBTYPE_SH7720)
	INTC_VECT(SSL, 0x980),
#endif
	INTC_VECT(USBFI, 0xa20),      INTC_VECT(USBFI, 0xa40),
	INTC_VECT(USBHI, 0xa60),
	INTC_VECT(DMAC2, 0xb80),      INTC_VECT(DMAC2, 0xba0),
	INTC_VECT(ADC, 0xbe0),        INTC_VECT(SCIF0, 0xc00),
	INTC_VECT(SCIF1, 0xc20),      INTC_VECT(PINT07, 0xc80),
	INTC_VECT(PINT815, 0xca0),    INTC_VECT(SIOF0, 0xd00),
	INTC_VECT(SIOF1, 0xd20),      INTC_VECT(TPU, 0xd80),
	INTC_VECT(TPU, 0xda0),        INTC_VECT(TPU, 0xdc0),
	INTC_VECT(TPU, 0xde0),        INTC_VECT(IIC, 0xe00),
	INTC_VECT(MMC, 0xe80),        INTC_VECT(MMC, 0xea0),
	INTC_VECT(MMC, 0xec0),        INTC_VECT(MMC, 0xee0),
	INTC_VECT(CMT, 0xf00),        INTC_VECT(PCC, 0xf60),
	INTC_VECT(AFEIF, 0xfe0),
};

static struct intc_prio_reg prio_registers[] __initdata = {
	{ 0xA414FEE2UL, 0, 16, 4, /* IPRA */ { TMU0, TMU1, TMU2, RTC } },
	{ 0xA414FEE4UL, 0, 16, 4, /* IPRB */ { WDT, REF_RCMI, SIM, 0 } },
	{ 0xA4140016UL, 0, 16, 4, /* IPRC */ { IRQ3, IRQ2, IRQ1, IRQ0 } },
	{ 0xA4140018UL, 0, 16, 4, /* IPRD */ { USBF_SPD, TMU_SUNI, IRQ5, IRQ4 } },
	{ 0xA414001AUL, 0, 16, 4, /* IPRE */ { DMAC1, 0, LCDC, SSL } },
	{ 0xA4080000UL, 0, 16, 4, /* IPRF */ { ADC, DMAC2, USBFI, CMT } },
	{ 0xA4080002UL, 0, 16, 4, /* IPRG */ { SCIF0, SCIF1, 0, 0 } },
	{ 0xA4080004UL, 0, 16, 4, /* IPRH */ { PINT07, PINT815, TPU, IIC } },
	{ 0xA4080006UL, 0, 16, 4, /* IPRI */ { SIOF0, SIOF1, MMC, PCC } },
	{ 0xA4080008UL, 0, 16, 4, /* IPRJ */ { 0, USBHI, 0, AFEIF } },
};

static DECLARE_INTC_DESC(intc_desc, "sh7720", vectors, NULL,
		NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	register_intc_controller(&intc_desc);
	plat_irq_setup_sh3();
}
