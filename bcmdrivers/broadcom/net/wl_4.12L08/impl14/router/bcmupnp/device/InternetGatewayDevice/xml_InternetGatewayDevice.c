/*
 * Broadcom UPnP module, xml_InternetGatewayDevice.c
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: xml_InternetGatewayDevice.c 241192 2011-02-17 21:52:25Z gmo $
 */
#include <upnp.h>

char xml_InternetGatewayDevice[] =
	"<?xml version=\"1.0\"?>\r\n"
	"<root xmlns=\"urn:schemas-upnp-org:device-1-0\">\r\n"
	"\t<specVersion>\r\n"
	"\t\t<major>1</major>\r\n"
	"\t\t<minor>0</minor>\r\n"
	"\t</specVersion>\r\n"
	"\t<device>\r\n"
	"\t\t<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType>\r\n"
	"\t\t<friendlyName>router</friendlyName>\r\n"
	"\t\t<manufacturer>Broadcom</manufacturer>\r\n"
	"\t\t<manufacturerURL>http://www.broadcom.com</manufacturerURL>\r\n"
	"\t\t<modelDescription>Gateway</modelDescription>\r\n"
	"\t\t<modelName>router</modelName>\r\n"
	"\t\t<modelNumber>947xx</modelNumber>\r\n"
	"\t\t<serialNumber>0000001</serialNumber>\r\n"
	"\t\t<modelURL>http://www.broadcom.com</modelURL>\r\n"
	"\t\t<UDN>uuid:eb9ab5b2-981c-4401-a20e-b7bcde359dbb</UDN>\r\n"
	"\t\t<serviceList>\r\n"
	"\t\t\t<service>\r\n"
	"\t\t\t\t<serviceType>urn:schemas-upnp-org:service:Layer3Forwarding:1</serviceType>\r\n"
	"\t\t\t\t<serviceId>urn:upnp-org:serviceId:L3Forwarding1</serviceId>\r\n"
	"\t\t\t\t<SCPDURL>/x_layer3forwarding.xml</SCPDURL>\r\n"
	"\t\t\t\t<controlURL>/control?Layer3Forwarding</controlURL>\r\n"
	"\t\t\t\t<eventSubURL>/event?Layer3Forwarding</eventSubURL>\r\n"
	"\t\t\t</service>\r\n"
	"\t\t</serviceList>\r\n"
	"\t\t<deviceList>\r\n"
	"\t\t\t<device>\r\n"
	"\t\t\t\t<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType>\r\n"
	"\t\t\t\t<friendlyName>WANDevice</friendlyName>\r\n"
	"\t\t\t\t<manufacturer>Broadcom</manufacturer>\r\n"
	"\t\t\t\t<manufacturerURL>http://www.broadcom.com</manufacturerURL>\r\n"
	"\t\t\t\t<modelDescription>Gateway</modelDescription>\r\n"
	"\t\t\t\t<modelName>router</modelName>\r\n"
	"\t\t\t\t<modelURL>http://www.broadcom.com</modelURL>\r\n"
	"\t\t\t\t<UDN>uuid:e1f05c9d-3034-4e4c-af82-17cdfbdcc077</UDN>\r\n"
	"\t\t\t\t<serviceList>\r\n"
	"\t\t\t\t\t<service>\r\n"
	"\t\t\t\t\t\t<serviceType>urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1</serviceType>\r\n"
	"\t\t\t\t\t\t<serviceId>urn:upnp-org:serviceId:WANCommonIFC1</serviceId>\r\n"
	"\t\t\t\t\t\t<SCPDURL>/x_wancommoninterfaceconfig.xml</SCPDURL>\r\n"
	"\t\t\t\t\t\t<controlURL>/control?WANCommonInterfaceConfig</controlURL>\r\n"
	"\t\t\t\t\t\t<eventSubURL>/event?WANCommonInterfaceConfig</eventSubURL>\r\n"
	"\t\t\t\t\t</service>\r\n"
	"\t\t\t\t</serviceList>\r\n"
	"\t\t\t\t<deviceList>\r\n"
	"\t\t\t\t\t<device>\r\n"
	"\t\t\t\t\t\t<deviceType>urn:schemas-upnp-org:device:WANConnectionDevice:1</deviceType>\r\n"
	"\t\t\t\t\t\t<friendlyName>WAN Connection Device</friendlyName>\r\n"
	"\t\t\t\t\t\t<manufacturer>Broadcom</manufacturer>\r\n"
	"\t\t\t\t\t\t<manufacturerURL>http://www.broadcom.com</manufacturerURL>\r\n"
	"\t\t\t\t\t\t<modelDescription>Gateway</modelDescription>\r\n"
	"\t\t\t\t\t\t<modelName>router</modelName>\r\n"
	"\t\t\t\t\t\t<modelURL>http://www.broadcom.com</modelURL>\r\n"
	"\t\t\t\t\t\t<UDN>uuid:1995cf2d-d4b1-4fdb-bf84-8e59d2066198</UDN>\r\n"
	"\t\t\t\t\t\t<serviceList>\r\n"
	"\t\t\t\t\t\t\t<service>\r\n"
	"\t\t\t\t\t\t\t\t<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>\r\n"
	"\t\t\t\t\t\t\t\t<serviceId>urn:upnp-org:serviceId:WANIPConn1</serviceId>\r\n"
	"\t\t\t\t\t\t\t\t<SCPDURL>/x_wanipconnection.xml</SCPDURL>\r\n"
	"\t\t\t\t\t\t\t\t<controlURL>/control?WANIPConnection</controlURL>\r\n"
	"\t\t\t\t\t\t\t\t<eventSubURL>/event?WANIPConnection</eventSubURL>\r\n"
	"\t\t\t\t\t\t\t</service>\r\n"
	"\t\t\t\t\t\t</serviceList>\r\n"
	"\t\t\t\t\t</device>\r\n"
	"\t\t\t\t</deviceList>\r\n"
	"\t\t\t</device>\r\n"
	"\t\t</deviceList>\r\n"
	"\t\t<presentationURL>http://255.255.255.255</presentationURL>\r\n"
	"\t</device>\r\n"
	"</root>\r\n"
	"\r\n";
