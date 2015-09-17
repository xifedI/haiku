/*
 * Copyright 2011 Michael Lotz <mmlr@mlotz.ch>
 * Distributed under the terms of the MIT license.
 */


#include "QuirkyDevices.h"

#include "HIDWriter.h"

#include <string.h>

#include <usb/USB_hid.h>


static status_t
sixaxis_init(usb_device device, const usb_configuration_info *config,
	size_t interfaceIndex)
{
	TRACE_ALWAYS("found SIXAXIS controller, putting it in operational mode\n");

	// an extra get_report is required for the SIXAXIS to become operational
	uint8 dummy[18];
	status_t result = gUSBModule->send_request(device, USB_REQTYPE_INTERFACE_IN
			| USB_REQTYPE_CLASS, B_USB_REQUEST_HID_GET_REPORT,
		(B_USB_REQUEST_HID_FEATURE_REPORT << 8) | 0xf2 /* ? */, interfaceIndex,
		sizeof(dummy), dummy, NULL);
	if (result != B_OK) {
		TRACE_ALWAYS("failed to set operational mode: %s\n", strerror(result));
	}

	return result;
}


static status_t
sixaxis_build_descriptor(HIDWriter &writer)
{
	writer.BeginCollection(COLLECTION_APPLICATION,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_JOYSTICK);

	main_item_data_converter converter;
	converter.flat_data = 0; // defaults
	converter.main_data.array_variable = 1;
	converter.main_data.no_preferred = 1;

	writer.SetReportID(1);

	// unknown / padding
	writer.DefineInputPadding(1, 8);

	// digital button state on/off
	writer.DefineInputData(19, 1, converter.main_data, 0, 1,
		B_HID_USAGE_PAGE_BUTTON, 1);

	// padding to 32 bit boundary
	writer.DefineInputPadding(13, 1);

	// left analog stick
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_X);
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_Y);

	// right analog stick
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_X);
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_Y);

	// unknown / padding
	writer.DefineInputPadding(4, 8);

	// pressure sensitive button states
	writer.DefineInputData(12, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_VNO, B_HID_UID_GD_VNO);

	// unknown / padding / operation mode / battery status / connection ...
	writer.DefineInputPadding(15, 8);

	// accelerometer x, y and z
	writer.DefineInputData(3, 16, converter.main_data, 0, UINT16_MAX,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_VX);

	// gyroscope
	writer.DefineInputData(1, 16, converter.main_data, 0, UINT16_MAX,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_VBRZ);

	return writer.EndCollection();
}


static status_t
xbox360_build_descriptor(HIDWriter &writer)
{
	writer.BeginCollection(COLLECTION_APPLICATION,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_JOYSTICK);

	main_item_data_converter converter;
	converter.flat_data = 0; // defaults
	converter.main_data.array_variable = 1;
	converter.main_data.no_preferred = 1;

	// unknown / padding / byte count
	writer.DefineInputPadding(2, 8);

	// dpad / buttons
	writer.DefineInputData(11, 1, converter.main_data, 0, 1,
		B_HID_USAGE_PAGE_BUTTON, 1);
	writer.DefineInputPadding(1, 1);
	writer.DefineInputData(4, 1, converter.main_data, 0, 1,
		B_HID_USAGE_PAGE_BUTTON, 12);

	// triggers
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_Z);
	writer.DefineInputData(1, 8, converter.main_data, 0, 255,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_RZ);

	// sticks
	writer.DefineInputData(2, 16, converter.main_data, -32768, 32767,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_X);
	writer.DefineInputData(2, 16, converter.main_data, -32768, 32767,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_X);

	// unknown / padding
	writer.DefineInputPadding(6, 8);

	return writer.EndCollection();
}


static status_t
wacom_init(usb_device device, const usb_configuration_info *config,
	size_t interfaceIndex)
{
	status_t result;

	TRACE_ALWAYS("found Wacom device, setting it to Wacom mode\n");

	// set protocol to report protocol
	result = gUSBModule->send_request(device, USB_REQTYPE_INTERFACE_OUT
			| USB_REQTYPE_CLASS, B_USB_REQUEST_HID_SET_PROTOCOL, 1,
		interfaceIndex, 0, NULL, NULL);
	if (result != B_OK)
		TRACE_ALWAYS("failed to set report protocol: %s\n", strerror(result));

	// set the device to Wacom mode
	int tryCount;
	char reportData[2] = { 0x02, 0x02 };	// Report ID, Mode
	char returnData[2] = { 0x00, 0x00 };
	for (tryCount = 0; tryCount < 5; tryCount++) {
		result = gUSBModule->send_request(device, USB_REQTYPE_INTERFACE_OUT
				| USB_REQTYPE_CLASS, B_USB_REQUEST_HID_SET_REPORT,
			(B_USB_REQUEST_HID_FEATURE_REPORT << 8) | reportData[0],
			interfaceIndex, sizeof(reportData), reportData, NULL);
		if (result != B_OK)
			TRACE_ALWAYS("failed to send feature report: %s\n",
					strerror(result));

		result = gUSBModule->send_request(device, USB_REQTYPE_INTERFACE_IN
				| USB_REQTYPE_CLASS, B_USB_REQUEST_HID_GET_REPORT,
			(B_USB_REQUEST_HID_FEATURE_REPORT << 8) | reportData[0],
			interfaceIndex, sizeof(returnData), returnData, NULL);
		if (result != B_OK)
			TRACE_ALWAYS("failed to get feature report: %s\n",
					strerror(result));

		TRACE("returnData: %u - %u\n", returnData[0], returnData[1]);

		if (returnData[1] == reportData[1]) {
			TRACE_ALWAYS("device successfully set to Wacom mode\n");
			break;
		}
	}

	TRACE("number of tries: %u\n", tryCount + 1);

	if (tryCount > 4) {
		TRACE_ALWAYS("device failed to set to Wacom mode\n");
	}

	return result;
}


static status_t
wacom_build_descriptor(HIDWriter &writer)
{
	// Builds a descriptor for the Wacom Graphire 2 pen

	writer.BeginCollection(COLLECTION_APPLICATION,
		B_HID_USAGE_PAGE_DIGITIZER, B_HID_UID_DIG_DIGITIZER);

	main_item_data_converter converter;
	converter.flat_data = 0; // defaults
	converter.main_data.array_variable = 1;
	converter.main_data.relative = 0;
	converter.main_data.no_preferred = 1;

	writer.SetReportID(1);	// TODO: really needed?

	// Report ID
	writer.DefineInputPadding(1, 8);

	// Stylus tip / button 1 / button 2
	writer.DefineInputData(3, 1, converter.main_data, 0, 1,
		B_HID_USAGE_PAGE_BUTTON, 0x1);

	// Padding
	writer.DefineInputPadding(1, 2);

	// Device ID	TODO: can be used to switch between tip and eraser
	writer.DefineInputPadding(1, 2);

	// Proximity
	writer.DefineInputData(1, 1, converter.main_data, 0, 1,
		B_HID_USAGE_PAGE_DIGITIZER, B_HID_UID_DIG_IN_RANGE);

	// X position
	writer.DefineInputData(1, 16, converter.main_data, 0, 65535,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_X);

	// Y position
	writer.DefineInputData(1, 16, converter.main_data, 0, 65535,
		B_HID_USAGE_PAGE_GENERIC_DESKTOP, B_HID_UID_GD_Y);

	// Stylus pressure
	writer.DefineInputData(1, 10, converter.main_data, 0, 1023,
		B_HID_USAGE_PAGE_DIGITIZER, B_HID_UID_DIG_TIP_PRESSURE);

	// Padding (total packet size: 8 bytes)
	writer.DefineInputPadding(1, 6);

	return writer.EndCollection();
}


usb_hid_quirky_device gQuirkyDevices[] = {
	{
		// The Sony SIXAXIS controller (PS3) needs a GET_REPORT to become
		// operational which we do in sixaxis_init. Also the normal report
		// descriptor doesn't contain items for the motion sensing data
		// and pressure sensitive buttons, so we constrcut a new report
		// descriptor that includes those extra items.
		0x054c, 0x0268, USB_INTERFACE_CLASS_HID, 0, 0,
		sixaxis_init, sixaxis_build_descriptor
	},

	{
		// XBOX 360 controllers aren't really HID (marked vendor specific).
		// They therefore don't provide a HID/report descriptor either. The
		// input stream is HID-like enough though. We therefore claim support
		// and build a report descriptor of our own.
		0, 0, 0xff /* vendor specific */, 0x5d /* XBOX controller */, 0x01,
		NULL, xbox360_build_descriptor
	},

	{
		0x056a, 0, USB_HID_DEVICE_CLASS, B_USB_HID_INTERFACE_BOOT_SUBCLASS,
		0, wacom_init, wacom_build_descriptor
	}
};

int32 gQuirkyDeviceCount
	= sizeof(gQuirkyDevices) / sizeof(gQuirkyDevices[0]);
